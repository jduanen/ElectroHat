/***************************************************************************
*
* ElectroHat application
* 
* Drives eight 9' EL wires using the AFD Driver board, drives a ring of 16
*  NeoPixels, and provides a web-server-based GUI.
* 
* N.B. This is insecure in that it passes the WiFi password in the clear -- you've been warned...
*       (actually, it's slightly obfuscated, but essentially in the clear)
*
* WebSocket interface
* - initialization
*   * server
*     - has hard-coded defaults that get overridden by values in the local config file
*     - initializes HW based on config values, then offers the web page and waits for WS messages
*   * client
*     - initializes GUI with default values, connects to server, and sends a "query" WS message
*       * initView() gets called by onOpen() when "index.html" is opened
*       * initView() is in "index.html" so that it can get compile-time constants
*   * server
*     - gets query WS message and sends the full config state to the client in a WS message
*   * client
*     - receives WS message from server and updates all the GUI elements with received data
* - on GUI interactions
*   * client
*     - on click, update local GUI, and send a WS message with the update to the server
*   * server
*     - on receiving an update message, changes the HW, and updates the config state
*       * on receipt of a "save" message, updates the config state, and write it to the local config file
* - WS messages from client to server (full config state):
*   * {"applName": <String>, "libVersion": <String>, "ipAddr": <String>,
*      "ssid": <String>, "passwd": <String>, "RSSI": <Int>, "el": <Boolean>,
*      "randomSequence": <Boolean>, "sequenceNumber": <Int>, "sequenceDelay": <Int>,
*      "led": <Boolean>, "randomPattern": <Boolean>, "patternNumber": <Int>,
*      "patternDelay": <Int>, "patternColor": <Int>, "customBidir": <Boolean>,
*      "customDelta": <Int>, "customColors": [[<Int>,<Int>],...]}
* - WS messages from server to client (dependent on what's clicked on the GUI):
*   * {"msgType": "query"}
*   * {"msgType": "el", "state": <Boolean>}
*   * {"msgType": "sequence", "sequenceNumber": <Int>, "sequenceDelay": <Int>}
*   * {"msgType": "randomSequence", "state": <Boolean>}
*   * {"msgType": "led", "state": <Boolean>}
*   * {"msgType": "pattern", "patternNumber": <Int>, "patternDelay": <Int>,
*      "patternColor": <Int>, "customBidir": <Boolean>, "customDelta": <Int>}
*   * {"msgType": "customColors", "customColors": [[<Int>,<Int>],...]}
*   * {"msgType": "randomPattern", "state": <Boolean>}
*   * {"msgType": "saveConf", "ssid": document.getElementById("ssid").value,
*      "passwd": <String>, "elState": <Boolean>, "randomSequence": <Boolean>,
*      "sequenceNumber": <Int>, "sequenceDelay": <Int>, "ledState": <Boolean>,
*      "randomPattern": <Boolean>, "patternNumber": <Int>, "patternDelay": <Int>,
*      "patternColor": <Int>, "customBidir": <Boolean>, "customDelta": <Int>}
*   * {"msgType": "reboot"}
*
* Notes:
*  - I2C defaults: SDA=GPIO4 -> D2, SCL=GPIO5 -> D1
*    * XIAO ESP32-C3: SDA: GPIO6/D4/pin5; SCL: GPIO7/D5/pin6
*    * NodeMCU D1: SDA=GPIO4/D2/pin13; SCL=GPIO5/D1/pin14
*  - connect to http://<ipaddr>/index.html for web interface
*  - connect to http://<ipaddr>/update for OTA update of firmware
*  - there seems to be a bug in the WS code that fails silently on messages
*    longer than 512B
*    * to work around this (rather than fix it) I removed the custom colors
*      from the saveConf message, and use the current values (which should be
*      kept up to date anyway).
* 
* TODO
*  - ?
*
****************************************************************************/

#include <Arduino.h>
//#include <Adafruit_NeoPixel.h>

#include "wifi.h"
#include "sequences.h"

#include <WiFiUtilities.h>
#include <ConfigService.h>
#include <WebServices.h>
#include <ElWires.h>
#include <NeoPixelRing.h>

#define VERBOSE                 0

#define APPL_NAME               "ElectroHat"
#define APPL_VERSION            "1.2.0"

#define CONFIG_FILE_PATH        "/config.json"
#define CS_DOC_SIZE             1536

#define WIFI_AP_SSID            "ElectroHat"
 
#define WEB_SERVER_PORT         80
#define MAX_WS_MSG_SIZE         1536

#define STARTUP_EL_SEQUENCE     0
#define STARTUP_EL_DELAY        100

#define EH_HTML_PATH            "/index.html"
#define EH_STYLE_PATH           "/style.css"
#define EH_SCRIPTS_PATH         "/scripts.js"

#define STARTUP_RANDOM_PATTERN  false
#define STARTUP_PATTERN_NUM     0
#define STARTUP_PATTERN_DELAY   100
#define STARTUP_PATTERN_COLOR   0xFFFFFF
#define STARTUP_CUSTOM_BIDIR    true
#define STARTUP_CUSTOM_DELTA    64

#define NUM_LEDS                16
#define LED_BRIGHT              8


typedef struct {
    String      ssid;
    String      passwd;

    bool        elState;
    bool        randomSequence;
    uint16_t    sequenceNumber;
    uint32_t    sequenceDelay;

    bool        ledState;
    bool        randomPattern;
    uint16_t    patternNumber;
    uint32_t    patternDelay;
    uint32_t    patternColor;
    bool        customBidir;
    uint16_t    customDelta;
    uint32_t    customColors[NUM_LEDS][2];
} ConfigState;

ConfigState configState = {
    String(WLAN_SSID),
    String(rot47(WLAN_PASS)),

    true,
    false,
    STARTUP_EL_SEQUENCE,
    STARTUP_EL_DELAY,

    true,
    STARTUP_RANDOM_PATTERN,
    STARTUP_PATTERN_NUM,
    STARTUP_PATTERN_DELAY,
    STARTUP_PATTERN_COLOR,
    STARTUP_CUSTOM_BIDIR,
    STARTUP_CUSTOM_DELTA,
    {
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080},
        {0x808080, 0x808080}
    }
};


ConfigService cs(CS_DOC_SIZE, CONFIG_FILE_PATH);

WebServices<MAX_WS_MSG_SIZE> webSvcs(APPL_NAME, WEB_SERVER_PORT);

ElWires elWires;

NeoPixelRing<NUM_LEDS> ring(LED_BRIGHT);


char *patternNamePtrs[MAX_PATTERNS];
uint16_t patternMinDelays[MAX_PATTERNS];
uint16_t patternMaxDelays[MAX_PATTERNS];
uint16_t patternDefDelays[MAX_PATTERNS];

unsigned int loopCnt = 0;


void(* reboot)(void) = 0;

void print(String str) {
    if (VERBOSE) {
        Serial.print(str);
    }
};

void println(String str) {
    if (VERBOSE) {
        Serial.println(str);
    }
};

String webpageProcessor(const String& var) {
    if (var == "APPL_NAME") {
        return (String(APPL_NAME));
    } else if (var == "VERSION") {
        return (String(APPL_VERSION));
    } else if (var == "LIB_VERSION") {
        return (webSvcs.libVersion);
    } else if (var == "IP_ADDR") {
        return (WiFi.localIP().toString());
    } else if (var == "SSID") {
        return (configState.ssid);
    } else if (var == "RSSI") {
        return (String(WiFi.RSSI()));
    } else if (var == "WIFI_MODE") {
        return (getWiFiMode());
    } else if (var == "WIFI_AP_SSID") {
        return (WIFI_AP_SSID);
    } else if (var == "CONNECTION") {
        return (WiFi.SSID());
    } else if (var == "EL_STATE") {
        return (configState.elState ? "ON" : "OFF");
    } else if (var == "SEQUENCES_VERSION") {
        return (elWires.libVersion);
    } else if (var == "NUM_LEDS") {
        return (String(NUM_LEDS));
    } else if (var == "NUMBER_OF_SEQUENCES") {
        return (String(elWires.numSequences()));
    } else if (var == "LED_STATE") {
        return (configState.ledState ? "ON" : "OFF");
    } else if (var == "NUMBER_OF_PATTERNS") {
        return (String(ring.getNumPatterns()));
    } else if (var == "PATTERN_NAMES") {
        String patternNames = "";
        for (int i = 0; (i < ring.getNumPatterns()); i++) {
            if (i > 0) {
                patternNames.concat(String(","));
            }
            patternNames.concat(String("\"") + patternNamePtrs[i] + String("\""));
        }
        return patternNames;
    } else if (var == "PATTERN_MIN_DELAYS") {
        String minDelays = "";
        for (int i = 0; (i < ring.getNumPatterns()); i++) {
            if (i > 0) {
                minDelays.concat(String(","));
            }
            minDelays.concat(patternMinDelays[i]);
        }
        return minDelays;
    } else if (var == "PATTERN_MAX_DELAYS") {
        String maxDelays = "";
        for (int i = 0; (i < ring.getNumPatterns()); i++) {
            if (i > 0) {
                maxDelays.concat(String(","));
            }
            maxDelays.concat(patternMaxDelays[i]);
        }
        return maxDelays;
    } else if (var == "PATTERN_DEF_DELAYS") {
        String defDelays = "";
        for (int i = 0; (i < ring.getNumPatterns()); i++) {
            if (i > 0) {
                defDelays.concat(String(","));
            }
            defDelays.concat(patternDefDelays[i]);
        }
        return defDelays;
    } else if (var == "CUSTOM_DELTA") {
        return String(configState.customDelta);
    }
    return(String());
};

String webpageMsgHandler(const JsonDocument& wsMsg) {
    if (false) { //// TMP TMP TMP
        Serial.println("MSG:");
        if (false) {
            serializeJsonPretty(wsMsg, Serial);
        } else {
            serializeJson(wsMsg, Serial);
        }
        Serial.println("");
    }

    // handle changes in GUI; update HW and reflect it in the configState
    String msgType = String(wsMsg["msgType"]);
    if (msgType.equalsIgnoreCase("query")) {
        // NOP
    } else if (msgType.equalsIgnoreCase("el")) {
        configState.elState = wsMsg["state"];
        if (!configState.elState) {
            elWires.clear();
        }
    } else if (msgType.equalsIgnoreCase("sequence")) {
        elWires.setSequence(wsMsg["sequenceNumber"], wsMsg["sequenceDelay"]);
        configState.sequenceNumber = elWires.sequenceNumber();
        configState.sequenceDelay = elWires.sequenceDelay();
    } else if (msgType.equalsIgnoreCase("randomSequence")) {
        elWires.enableRandomSequence(wsMsg["state"]);
        configState.randomSequence = wsMsg["state"];
    } else if (msgType.equalsIgnoreCase("led")) {
        configState.ledState = wsMsg["state"];
        if (!configState.ledState) {
            ring.clear();
        }
    } else if (msgType.equalsIgnoreCase("pattern")) {
        ring.selectPattern(wsMsg["patternNumber"]);
        ring.setDelay(wsMsg["patternDelay"]);
        ring.setColor(wsMsg["patternColor"]);
        configState.patternNumber = wsMsg["patternNumber"];
        configState.patternDelay = wsMsg["patternDelay"];
        configState.patternColor = wsMsg["patternColor"];
        configState.customBidir = wsMsg["customBidir"];
        configState.customDelta = wsMsg["customDelta"];
    } else if (msgType.equalsIgnoreCase("customColors")) {
//        Serial.println("==>>"); printTuples("Tuples: [", configState.customColors);  //// TMP TMP TMP
        copyArray(wsMsg["customColors"], configState.customColors);
//        Serial.println("<<=="); printTuples("Tuples: [", configState.customColors);  //// TMP TMP TMP
        updateCustomColorLeds(configState.customColors);
    } else if (msgType.equalsIgnoreCase("randomPattern")) {
        ring.enableRandomPattern(wsMsg["state"]);
        configState.randomPattern = wsMsg["state"];
    } else if (msgType.equalsIgnoreCase("saveConf")) {
        String jsonStr;
        JSON_START(jsonStr);
        jsonStr.concat("\"ssid\": \"" + configState.ssid + "\", ");
        jsonStr.concat("\"passwd\": \"" + configState.passwd + "\", ");
        jsonStr.concat("\"elState\": " + String(configState.elState ? true : false) + ", ");
        jsonStr.concat("\"randomSequence\": " + String(configState.randomSequence ? true : false) + ", ");
        jsonStr.concat("\"sequenceNumber\": " + String(configState.sequenceNumber) + ", ");
        jsonStr.concat("\"sequenceDelay\": " + String(configState.sequenceDelay) + ", ");
        jsonStr.concat("\"ledState\": " + String(configState.ledState ? true : false) + ", ");
        jsonStr.concat("\"randomPattern\": " + String(configState.randomPattern ? true : false) + ", ");
        jsonStr.concat("\"patternDelay\": " + String(configState.patternDelay) + ", ");
        jsonStr.concat("\"patternNumber\": " + String(configState.patternNumber) + ", ");
        jsonStr.concat("\"patternColor\": " + String(configState.patternColor) + ", ");
        jsonStr.concat("\"customBidir\": " + String(configState.customBidir ? true : false) + ", ");
        jsonStr.concat("\"customDelta\": " + String(configState.customDelta) + ", ");
        /*
        jsonStr.concat("\"\": \"" + configState. + "\", ");
        jsonStr.concat("\"\": " + String(configState. ? true : false) + ", ");
        jsonStr.concat("\"\": " + String(configState.) + ", ");
        */

        jsonStr.concat("\"customColors\": [");
        //// TODO decide if I should use tuples2String() here
        for (int i = 0; (i < NUM_LEDS); i++) {
            if (i > 0) {
                jsonStr.concat(", ");
            }
            jsonStr.concat("[" + String(configState.customColors[i][0]) + ", " + String(configState.customColors[i][1]) + "]");
        }
        jsonStr.concat("]}");
        JSON_END(jsonStr);

        if (!cs.setConfig(jsonStr)) {
            Serial.println("ERROR: Failed to save config to file");
        }
        if (false) {  //// TMP TMP TMP
            Serial.println("Config File: XXXXXXXXXXXXXXXXXX");
            cs.listFiles("/");
            cs.printConfigFile();
            Serial.println("...\nXXXXXXXXXXXXXXXXX\n");
        }
    } else if (msgType.equalsIgnoreCase("reboot")) {
        println("REBOOTING...");
        reboot();
    } else if (msgType.equalsIgnoreCase("update")) {
        //// FIXME
        Serial.println("FIXME do the right thing here");
    } else {
        Serial.println("ERROR: unknown WS message type -- " + msgType);
    }

    // send contents of configState (which should reflect the state of the HW)
    String msg = ", \"libVersion\": \"" + webSvcs.libVersion + "\"";
    msg.concat(", \"ipAddr\": \""); msg.concat(WiFi.localIP().toString() + "\"");
    msg.concat(", \"ssid\": \""); msg.concat(configState.ssid + "\"");
    msg.concat(", \"passwd\": \""); msg.concat(configState.passwd + "\"");
    msg.concat(", \"RSSI\": "); msg.concat(WiFi.RSSI());
    msg.concat(", \"el\": "); msg.concat(configState.elState);
    msg.concat(", \"randomSequence\": "); msg.concat(configState.randomSequence);
    msg.concat(", \"sequenceNumber\": "); msg.concat(configState.sequenceNumber);
    msg.concat(", \"sequenceDelay\": "); msg.concat(configState.sequenceDelay);
    msg.concat(", \"led\": "); msg.concat(configState.ledState);
    msg.concat(", \"randomPattern\": "); msg.concat(configState.randomPattern);
    msg.concat(", \"patternNumber\": "); msg.concat(configState.patternNumber);
    msg.concat(", \"patternDelay\": "); msg.concat(configState.patternDelay);
    msg.concat(", \"patternColor\": "); msg.concat(configState.patternColor);
    msg.concat(", \"customBidir\": "); msg.concat(configState.customBidir);
    msg.concat(", \"customDelta\": "); msg.concat(configState.customDelta);
    msg.concat(", \"customColors\": "); msg.concat(tuples2String(configState.customColors));
    if (false) {  //// TMP TMP TMP
        Serial.println(msg);
    }
    return(msg);
};

WebPageDef webPage = {
    EH_HTML_PATH,
    EH_SCRIPTS_PATH,
    EH_STYLE_PATH,
    webpageProcessor,
    webpageMsgHandler
};

void updateCustomColorLeds(uint32_t tuples[][2]) {
    for (int p = 0; (p < NUM_LEDS); p++) {
        ring.enableCustomPixels((1 << p), tuples[p][0], tuples[p][1]);
    }
};

String tuples2String(uint32_t tuples[][2]) {
    String ccStr = "[";
    //// use forall instead?
    for (int i = 0; (i < NUM_LEDS); i++) {
        if (i != 0) {
            ccStr += ",";
        }
        ccStr.concat("[" + String(tuples[i][0]) + "," +  String(tuples[i][1]) + "]");
    }
    ccStr.concat("]");
    return ccStr;
};

void printTuples(String hdr, uint32_t tuples[][2]) {
    Serial.print(hdr);
    //// use forall/iterator instead?
    for (int i = 0; (i < NUM_LEDS); i++) {
        if (i > 0) {
            Serial.print(", ");
        }
        Serial.print("[0x" + String(tuples[i][0], HEX) + ", 0x" + String(tuples[i][1], HEX) + "]");
    }
    Serial.println("]");
};

void printConfigState(ConfigState *statePtr) {
    Serial.println("configState:");
    Serial.println("    ssid:\t\t " + statePtr->ssid);
    Serial.println("    passwd:\t\t " + statePtr->passwd);
    Serial.println("    elState:\t\t " + String(statePtr->elState));
    Serial.println("    randomSequence:\t " + String(statePtr->sequenceNumber));
    Serial.println("    sequenceNumber:\t " + String(statePtr->sequenceNumber));
    Serial.println("    sequenceDelay:\t " + String(statePtr->sequenceDelay));
    Serial.println("    ledState:\t\t " + String(statePtr->ledState));
    Serial.println("    randomPattern:\t " + String(statePtr->randomPattern));
    Serial.println("    patternNumber:\t " + String(statePtr->patternNumber));
    Serial.println("    patternDelay:\t " + String(statePtr->patternDelay));
    Serial.println("    patternColor:\t " + String(statePtr->patternColor));
    Serial.println("    customBidir:\t " + String(statePtr->customBidir));
    Serial.println("    customDelta:\t " + String(statePtr->customDelta));
    printTuples("    customColors:\t ", statePtr->customColors);
};

void stop() {
    Serial.println("STOP");
    while (true) {
        wdt_reset();
        delay(100);
    }
};

void config() {
    uint16_t  sequenceNumber;
    uint16_t  sequenceDelay;

    if (false) {
        Serial.println("Pre-config File:");
        serializeJson(*(cs.docPtr), Serial);
    }

    if (false) {  //// TMP TMP TMP
        Serial.println("Clearing out the config file");
        deserializeJson(*(cs.docPtr), "{}");
    }

    cs.printConfigFile();    //// TMP TMP TMP
    Serial.println("CS Mem: " + String(CS_DOC(cs).memoryUsage()));  //// TMP TMP TMP

    // overwrite defaults in the config state struct with any values found in the config file
    configState.ssid = cs.getConfigValue<String>("ssid", configState.ssid);
    configState.passwd = cs.getConfigValue<String>("passwd", configState.passwd);
    configState.elState = cs.getConfigValue<bool>("elState", configState.elState);
    configState.randomSequence = cs.getConfigValue<bool>("randomSequence", configState.randomSequence);
    configState.sequenceNumber = cs.getConfigValue<uint16_t>("sequenceNumber", configState.sequenceNumber);
    configState.sequenceDelay = cs.getConfigValue<uint32_t>("sequenceDelay", configState.sequenceDelay);
    configState.ledState = cs.getConfigValue<bool>("ledState", configState.ledState);
    configState.randomPattern = cs.getConfigValue<bool>("randomPattern", configState.randomPattern);
    configState.patternNumber = cs.getConfigValue<uint16_t>("patternNumber", configState.patternNumber);
    configState.patternDelay = cs.getConfigValue<uint32_t>("patternDelay", configState.patternDelay);
    configState.patternColor = cs.getConfigValue<uint32_t>("patternColor", configState.patternColor);
    configState.customBidir = cs.getConfigValue<bool>("customBidir", configState.customBidir);
    configState.customDelta = cs.getConfigValue<uint16_t>("customDelta", configState.customDelta);

    uint16_t numColors = (*(cs.docPtr))["customColors"].size();
    if (!cs.validEntry("customColors")) {
        cs.docPtr->createNestedArray("customColors");
        numColors = 0;
    }
    if (numColors != NUM_LEDS) {
        Serial.println("ERROR: wrong number of customColors");
        //// FIXME
    }
    if (cs.validEntry("customColors") && (numColors == NUM_LEDS)) {
        copyArray((*(cs.docPtr))["customColors"], configState.customColors);
        if (cs.docPtr->overflowed()) {
            Serial.println("ERROR: overflowed config JsonDocument");
        }
        numColors = (*(cs.docPtr))["customColors"].size();
        if (numColors != NUM_LEDS) {
            Serial.println("ERROR: incorrect number of customColors: " + String(numColors));
        }
    }
    if (cs.validEntry("customColors") && !cs.docPtr->overflowed() && (numColors == NUM_LEDS)) {
        Serial.println("USING CUSTOM COLORS FROM CONFIG FILE");
        copyArray(CS_DOC(cs)["customColors"], configState.customColors);
    }
    //// TMP TMP TMP
    printConfigState(&configState);
};

void initElWires() {
    println("Init EL Wires");
    elWires.clear();
    elWires.setSequence(configState.sequenceNumber, configState.sequenceDelay);
    println("Number of Sequences: " + String(elWires.numSequences()));
    println("Random Sequence Enabled: " + String(elWires.randomSequence() ? "Yes" : "No"));
    println("Sequence Number: " + String(elWires.sequenceNumber()));
    println("Sequence Delay: " + String(elWires.sequenceDelay()));
};

#define BLINK_IPA_REPEAT                1
#define BLINK_IPA_REPEAT_DELAY          500   // 500ms
#define BLINK_IPA_INTER_BYTE_DELAY      5000  // 5s

void lightLEDs(uint32_t color, uint16_t enables) {
    for (int i = 0; (i < 16); i++) {
        if ((enables >> i) & 1) {
            ring.ring->setPixelColor(i, color);
            ring.ring->show();
        }
    }
}

void blinkIPA(IPAddress ipAddr) {
    uint32_t colors[4] = {
        ring.makeColor(255, 0,   0),    // Red
        ring.makeColor(0,   255, 0),    // Green
        ring.makeColor(0,   0, 255),    // Blue
        ring.makeColor(255, 0, 255)     // Magenta
    };

    ring.clear();
    for (int i = 0; (i < BLINK_IPA_REPEAT); i++) {
        for (int n = 0; (n < 4); n++) {
            lightLEDs(ring.makeColor(255, 255, 255), 0xF00F);
            lightLEDs(ring.makeColor(0, 0, 0), 0x0FF0);
            uint16_t enbs = ((ipAddr[n] << 4) & 0x0FF0);
            lightLEDs(colors[n], enbs);
            delay(BLINK_IPA_INTER_BYTE_DELAY);
        }
        delay(BLINK_IPA_REPEAT_DELAY);
    }
    ring.clear();
};

void initLEDs() {
    ring.clear();
    ring.fill(ring.makeColor(255, 255, 255));  // WHITE = all sub-pixels on

    ring.setDelay(configState.patternDelay);
    ring.setCustomDelta(configState.customDelta);
    ring.setColor(configState.patternColor);
    ring.enableRandomPattern(configState.randomPattern);
    ring.selectPattern(configState.patternNumber);
    updateCustomColorLeds(configState.customColors);
    ring.setCustomBidir(configState.customBidir);

    Serial.println("LED Delay: " + String(ring.getDelay()));
    Serial.println("Custom Pixel Color Delta: " + String(ring.getCustomDelta()));
    Serial.println("Selected LED Color: 0x" + String(ring.getColor(), HEX));
    Serial.println("Random Pattern enable: " + String(ring.randomPattern()));
    int num = ring.getNumPatterns();
    Serial.println("Number of Patterns: " + String(num));
    int numPatterns = ring.getPatternNames(patternNamePtrs, MAX_PATTERNS);
    if (numPatterns < 1) {
        Serial.println("getPatternNames failed");
    } else {
        if (numPatterns != num) {
            Serial.println("getPatternNames mismatch: " + String(numPatterns) + " != " + String(num));
        }
        Serial.print("Pattern Names (" + String(numPatterns) + "): ");
        for (int i = 0; (i < numPatterns); i++) {
            if (i > 0) {
                Serial.print(", ");
            }
            Serial.print(patternNamePtrs[i]);
        }
    }
    ring.getPatternDelays(patternMinDelays, patternMaxDelays, patternDefDelays, MAX_PATTERNS);
    Serial.println("Selected Pattern: " + String(ring.getSelectedPattern()));
    ColorRange colorRanges[NUM_LEDS] = {};
    ring.getCustomPixels(0xFFFF, colorRanges, NUM_LEDS);
    Serial.println("Custom Pattern Bidirectional: " + String(ring.getCustomBidir()));

    // test all colors/leds work
    ring.fill(ring.makeColor(255, 0, 0));  // RED = all sub-pixels on
    delay(200);
    ring.fill(ring.makeColor(0, 255, 0));  // GREEN = all sub-pixels on
    delay(200);
    ring.fill(ring.makeColor(0, 0, 255));  // BLUE = all sub-pixels on
    delay(200);
    ring.clear();

    // blink the IPAddress
    blinkIPA(WiFi.localIP());
}

void setup() { 
    delay(500);
    Serial.begin(115200);
    delay(500);
    Serial.println("\nBEGIN");

    //// TMP TMP TMP
    if (false) {
        // clear the local file system
        cs.formatFS();
    }

    //// TMP TMP TMP
    if (false) {
        // clear the config file
        Serial.print("Contents of config file: ");
        cs.printConfigFile();
        Serial.println("\nWrite empty json object to config file: " + String(CONFIG_FILE_PATH));
        deserializeJson(*(cs.docPtr), "{}");
        cs.saveConfig();
        Serial.print("Contents of empty config file: ");
        cs.printConfigFile();
        Serial.println("^^^^^^^^^^^^^^^^^^^^^^^^^");
    }

    config();

    //// TMP TMP TMP
    if (true) {
        Serial.println("Local Files:");
        cs.listFiles("/");
    }
    Serial.println("Config File Contents:");
    cs.printConfigFile();

    wiFiConnect(configState.ssid, rot47(configState.passwd), WIFI_AP_SSID);

    if (!webSvcs.addPage(webPage)) {
        Serial.println("ERROR: failed to add common page; continuing anyway");
    }
    webSvcs.addPage(CONFIG_FILE_PATH, "application/json");

    initElWires();
    Serial.println("EL wires function started");

    initLEDs();
    Serial.println("NeoPixels function started");

    webSvcs.updateClients();  //// FIXME remove this?

    Serial.println("READY");
};

void loop() {
    uint32_t elWaitTime, ledWaitTime, waitTime;

    webSvcs.run();

    if (configState.elState) {
        elWaitTime = elWires.run();
    }
    if (configState.ledState) {
        ledWaitTime = ring.run();
    }

    waitTime = min(elWaitTime, ledWaitTime);
    if (waitTime > 10) {
        delay(waitTime);
    }

    if (false) {
        Serial.println("EL: " + String(elWaitTime) + ", LED: " + String(ledWaitTime));
    }

    loopCnt++;
};
