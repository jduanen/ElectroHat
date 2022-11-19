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
*      "patternDelay": <Int>, "patternColor": <Int>, "customColors": [[<Int>,<Int>],...],
*      "customBidir": <Boolean>, "customDelta": <Int>}
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
*      "patternColor": <Int>, "customColors": [[<Int>,<Int>],...],
*      "customBidir": <Boolean>, "customDelta": <Int>}
*   * {"msgType": "reboot"}
*
* Notes:
*  - I2C defaults: SDA=GPIO4 -> D2, SCL=GPIO5 -> D1
*    * XIAO ESP32-C3: SDA: GPIO6/D4/pin5; SCL: GPIO7/D5/pin6
*    * NodeMCU D1: SDA=GPIO4/D2/pin13; SCL=GPIO5/D1/pin14
*  - connect to http://<ipaddr>/index.html for web interface
*  - connect to http://<ipaddr>/update for OTA update of firmware
* 
* TODO
*  - ?
*
****************************************************************************/

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "wifi.h"
#include "sequences.h"

#include <WiFiUtilities.h>
#include <ConfigService.h>
#include <WebServices.h>
#include <ElWires.h>
#include <NeoPixelRing.h>

#define VERBOSE                 0

#define APPL_NAME               "ElectroHat"
#define APPL_VERSION            "1.1.0"

#define CONFIG_FILE_PATH        "/config.json"
#define CS_DOC_SIZE             1024

#define WIFI_AP_SSID            "ElectroHat"
 
#define WEB_SERVER_PORT         80

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
    String          ssid;
    String          passwd;

    bool            elState;
    bool            randomSequence;
    unsigned short  sequenceNumber;
    uint32_t        sequenceDelay;

    bool            ledState;
    bool            randomPattern;
    unsigned short  patternNumber;
    uint32_t        patternDelay;
    uint32_t        patternColor;
    bool            customBidir;
    unsigned short  customDelta;
    ColorRange      customColors[NUM_LEDS];
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

WebServices webSvcs(APPL_NAME, WEB_SERVER_PORT);

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
    if (true) { //// TMP TMP TMP
        Serial.println("MSG:");
        if (false) {
            serializeJsonPretty(wsMsg, Serial);
        } else {
            serializeJson(wsMsg, Serial);
        }
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
        configState.patternNumber = ring.getSelectedPattern();
        configState.patternDelay = ring.getDelay();
        configState.patternColor = ring.getColor();
        configState.customBidir = wsMsg["customBidir"];
        configState.customDelta = wsMsg["customDelta"];
    serializeJson(wsMsg, Serial);
    } else if (msgType.equalsIgnoreCase("customColors")) {
        //// TODO think about first setting LEDs and then query them to set the configState
        json2CustomColors(configState.customColors, wsMsg["customColors"]);
        updateCustomColorLeds(configState.customColors);
    } else if (msgType.equalsIgnoreCase("randomPattern")) {
        ring.enableRandomPattern(wsMsg["state"]);
        configState.randomPattern = wsMsg["state"];
    } else if (msgType.equalsIgnoreCase("saveConf")) {
        //// TODO just load from wsMsg to configState struct and then use it to load the cs.doc -- or the converse
        String ssidStr = String(wsMsg["ssid"]);
        configState.ssid = ssidStr;
        SET_CONFIG(cs, "ssid", ssidStr);
        String passwdStr = String(wsMsg["passwd"]);
        configState.passwd = passwdStr;
        SET_CONFIG(cs, "passwd", passwdStr);

        configState.elState = wsMsg["elState"];
        SET_CONFIG(cs, "elState", wsMsg["elState"]);
        configState.randomSequence = wsMsg["randomSequence"];
        SET_CONFIG(cs, "randomSequence", wsMsg["randomSequence"]);
        configState.sequenceNumber = wsMsg["sequenceNumber"];
        SET_CONFIG(cs, "sequenceNumber", wsMsg["sequenceNumber"]);
        configState.sequenceDelay = wsMsg["sequenceDelay"];
        SET_CONFIG(cs, "sequenceDelay", wsMsg["sequenceDelay"]);

        configState.ledState = wsMsg["ledState"];
        SET_CONFIG(cs, "ledState", wsMsg["ledState"]);
        configState.randomSequence = wsMsg["randomPattern"];
        SET_CONFIG(cs, "randomPattern", wsMsg["randomPattern"]);
        configState.patternNumber = wsMsg["patternNumber"];
        SET_CONFIG(cs, "patternNumber", wsMsg["patternNumber"]);
        configState.patternDelay = wsMsg["patternDelay"];
        SET_CONFIG(cs, "patternDelay", wsMsg["patternDelay"]);
        configState.patternColor = wsMsg["patternColor"];
        SET_CONFIG(cs, "patternColor", wsMsg["patternColor"]);
        json2CustomColors(configState.customColors, wsMsg["customColors"]);
        SET_CONFIG(cs, "customColors", wsMsg["customColors"]);
        configState.customBidir = wsMsg["customBidir"];
        SET_CONFIG(cs, "customBidir", wsMsg["customBidir"]);
        configState.customDelta = wsMsg["customDelta"];
        SET_CONFIG(cs, "customDelta", wsMsg["customDelta"]);
        if (!cs.saveConfig()) {
            Serial.println("ERROR: Failed to write config file");
        }
        if (true) {
            Serial.println("Config File: XXXXXXXXXXXXXXXXXX");
            serializeJson(*(cs.doc), Serial);
            cs.listFiles("/");
            cs.printConfig();
            Serial.println("...\nXXXXXXXXXXXXXXXXX\n");
        }
    } else if (msgType.equalsIgnoreCase("reboot")) {
        println("REBOOTING...");
        reboot();
    } else {
        Serial.println("ERROR: unknown WS message type -- " + msgType);
    }

    // send contents of configState (which should reflect the state of the HW)
    String msg = ", \"libVersion\": \"" + webSvcs.libVersion + "\"";
    msg += ", \"ipAddr\": \"" + WiFi.localIP().toString() + "\"";
    msg += ", \"ssid\": \"" + configState.ssid + "\"";
    msg += ", \"passwd\": \"" + configState.passwd + "\"";
    msg += ", \"RSSI\": " + String(WiFi.RSSI());
    msg += ", \"el\": \"" + String(configState.elState ? "true" : "false") + "\"";
    msg += ", \"randomSequence\": \"" + String(configState.randomSequence ? "true" : "false") + "\"";
    msg += ", \"sequenceNumber\": " + String(configState.sequenceNumber);
    msg += ", \"sequenceDelay\": " + String(configState.sequenceDelay);
    msg += ", \"led\": \"" + String(configState.ledState ? "true" : "false") + "\"";
    msg += ", \"randomPattern\": \"" + String(configState.randomPattern ? "true" : "false") + "\"";
    msg += ", \"patternNumber\": " + String(configState.patternNumber);
    msg += ", \"patternDelay\": " + String(configState.patternDelay);
    msg += ", \"patternColor\": " + String(configState.patternColor);
    msg += ", \"customBidir\": " + String(configState.customBidir);
    msg += ", \"customDelta\": " + String(configState.customDelta);
    msg += ", \"customColors\": " + customColors2String(configState.customColors);
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

void updateCustomColorLeds(ColorRange customColors[]) {
    for (int p = 0; (p < NUM_LEDS); p++) {
        ring.enableCustomPixels((1 << p), customColors[p].startColor, customColors[p].endColor);
    }
};

void json2CustomColors(ColorRange customColors[], JsonVariantConst colorRanges) {
    //// TODO figure out a better way to do this
    uint32_t cc[NUM_LEDS][2];
    copyArray(colorRanges, cc);
    //// TODO use foreach?
    for (int i = 0; (i < NUM_LEDS); i++) {
        ColorRange cr = {cc[i][0], cc[i][1]};
        customColors[i] = cr;
    }
};

void customColors2Json(JsonArray colorRanges, ColorRange customColors[]) {
    //// TODO figure out a better way to do this
    int cc[NUM_LEDS][2];
    //// TODO use foreach?
    for (int i = 0; (i < NUM_LEDS); i++) {
        cc[i][0] = customColors[i].startColor;
        cc[i][1] = customColors[i].endColor;
    }
    if (!copyArray(cc, colorRanges)) {
        Serial.println("ERROR: customColors2Json copyArray failure");
    }
};

void printCustomColors(ColorRange colorRanges[]) {
    Serial.print("Custom Colors: [");
    for (int i = 0; (i < NUM_LEDS); i++) {
        if (i > 0) {
            Serial.print(", ");
        }
        Serial.print("[0x" + String(colorRanges[i].startColor, HEX) + ", 0x" + String(colorRanges[i].endColor, HEX) + "]");
    }
    Serial.println("]");
};

String customColors2String(ColorRange customColors[]) {
    String ccStr = "[";
    for (int i = 0; (i < NUM_LEDS); i++) {
        if (i != 0) {
            ccStr += ",";
        }
        ccStr += "[" + String(customColors[i].startColor) + "," +  String(customColors[i].endColor) + "]";
    }
    ccStr += "]";
    return ccStr;
};

void config() {
    unsigned short  sequenceNumber;
    unsigned short  sequenceDelay;
    bool dirty = false;

    if (false) {
        Serial.println("Pre-config File:");
        serializeJson(*(cs.doc), Serial);
    }

    if (false) {  //// TMP TMP TMP
        // disregard the contents of the saved config file
        deserializeJson(*(cs.doc), "{}");
    }

    // Use values from local struct if corresponding value not found in config doc
    INIT_CONFIG(cs, "ssid", configState.ssid);
    INIT_CONFIG(cs, "passwd", configState.passwd);
    INIT_CONFIG(cs, "elState", configState.elState);
    INIT_CONFIG(cs, "randomSequence", configState.randomSequence);
    INIT_CONFIG(cs, "sequenceNumber", configState.sequenceNumber);
    INIT_CONFIG(cs, "sequenceDelay", configState.sequenceDelay);
    INIT_CONFIG(cs, "ledState", configState.ledState);
    INIT_CONFIG(cs, "randomPattern", configState.randomPattern);
    INIT_CONFIG(cs, "patternNumber", configState.patternNumber);
    INIT_CONFIG(cs, "patternDelay", configState.patternDelay);
    INIT_CONFIG(cs, "patternColor", configState.patternColor);
    INIT_CONFIG(cs, "customBidir", configState.customBidir);
    INIT_CONFIG(cs, "customDelta", configState.customDelta);
    if (!cs.validEntry("customColors")) {
        JsonArray arr = (*(cs.doc)).createNestedArray("customColors");
        customColors2Json(arr, configState.customColors);
        dirty = true;
    }
    if (dirty) {
        cs.saveConfig();
    }

    GET_CONFIG(configState.ssid, cs, "ssid", String);
    GET_CONFIG(configState.passwd, cs, "passwd", String);
    GET_CONFIG(configState.elState, cs, "elState", bool);
    GET_CONFIG(configState.randomSequence, cs, "randomSequence", bool);
    GET_CONFIG(configState.sequenceNumber, cs, "sequenceNumber", unsigned int);
    GET_CONFIG(configState.sequenceDelay, cs, "sequenceDelay", unsigned int);
    GET_CONFIG(configState.ledState, cs, "ledState", bool);
    GET_CONFIG(configState.randomPattern, cs, "randomPattern", bool);
    GET_CONFIG(configState.patternNumber, cs, "patternNumber", unsigned int);
    GET_CONFIG(configState.patternDelay, cs, "patternDelay", unsigned int);
    GET_CONFIG(configState.patternColor, cs, "patternColor", unsigned int);
    GET_CONFIG(configState.customBidir, cs, "customBidir", bool);
    GET_CONFIG(configState.customDelta, cs, "customDelta", unsigned int);
    json2CustomColors(configState.customColors, (*(cs.doc))["customColors"]);
    if (false) {
        Serial.println("Config File: vvvvvvvvvvvvvvvvvvv");
        serializeJson(*(cs.doc), Serial);
        cs.listFiles("/");
        cs.printConfig();
        Serial.println("...\n^^^^^^^^^^^^^^^^^^^^^\n");
    }
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
    printCustomColors(colorRanges);
    Serial.println("Custom Pattern Bidirectional: " + String(ring.getCustomBidir()));

    ring.fill(ring.makeColor(255, 0, 0));  // RED = all sub-pixels on
    delay(500);
    ring.fill(ring.makeColor(0, 255, 0));  // GREEN = all sub-pixels on
    delay(500);
    ring.fill(ring.makeColor(0, 0, 255));  // BLUE = all sub-pixels on
    delay(500);
    ring.clear();
}

void setup() { 
    delay(500);
    Serial.begin(115200);
    delay(500);
    Serial.println("\nBEGIN");

    //// TMP TMP TMP
    if (false) {
        // clear the local file system
        cs.format();
    }

    //// TMP TMP TMP
    if (true) {
        // clear the config file
        Serial.print("Contents of config file: ");
        cs.printConfig();
        Serial.println("\nWrite empty json object to config file: " + String(CONFIG_FILE_PATH));
        deserializeJson(*(cs.doc), "{}");
        cs.saveConfig();
        Serial.print("Contents of empty config file: ");
        cs.printConfig();
        Serial.println("^^^^^^^^^^^^^^^^^^^^^^^^^");
    }

    config();

    //// TMP TMP TMP
    if (true) {
        Serial.println("Local Files:");
        cs.listFiles("/");
    }
    Serial.println("Config File Contents:");
    cs.printConfig();

    wiFiConnect(configState.ssid, rot47(configState.passwd), WIFI_AP_SSID);

    if (!webSvcs.addPage(webPage)) {
        Serial.println("ERROR: failed to add common page; continuing anyway");
    }

    initElWires();
    Serial.println("EL wires function started");

    initLEDs();
    Serial.println("NeoPixels function started");

    webSvcs.updateClients();

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

    loopCnt += 1;
};
