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
*      "patternColor": <Int>, "customBidir": <Boolean>, "customDelta": <Int>,
*      "customColors": [[<Int>,<Int>],...]}
*   * {"msgType": "randomPattern", "state": <Boolean>}
*   * {"msgType": "saveConf", "ssid": document.getElementById("ssid").value,
*      "passwd": <String>, "elState": <Boolean>, "randomSequence": <Boolean>,
*      "sequenceNumber": <Int>, "sequenceDelay": <Int>, "ledState": <Boolean>,
*      "randomPattern": <Boolean>, "patternNumber": <Int>, "patternDelay": <Int>,
*      "patternColor": <Int>, "customBidir": <Boolean>, "customDelta": <Int>,
*      "customColors": [[<Int>,<Int>],...]}
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
#define APPL_VERSION            "1.1.0"

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
    if (true) { //// TMP TMP TMP
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
        configState.patternNumber = ring.getSelectedPattern();
        configState.patternDelay = ring.getDelay();
        configState.patternColor = ring.getColor();
        configState.customBidir = wsMsg["customBidir"];
        configState.customDelta = wsMsg["customDelta"];
        copyArray(wsMsg["customColors"], configState.customColors);
/*
    } else if (msgType.equalsIgnoreCase("customColors")) {
        //// TODO think about first setting LEDs and then query them to set the configState
        Serial.println("==>>"); printTuples("Tuples: [", configState.customColors);
        copyArray(wsMsg["customColors"], configState.customColors);
        Serial.println("<<=="); printTuples("Tuples: [", configState.customColors);
        updateCustomColorLeds(configState.customColors);
*/
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
        configState.customBidir = wsMsg["customBidir"];
        SET_CONFIG(cs, "customBidir", wsMsg["customBidir"]);
        configState.customDelta = wsMsg["customDelta"];
        SET_CONFIG(cs, "customDelta", wsMsg["customDelta"]);

//        copyArray(wsMsg["customColors"], configState.customColors);
        if (!copyArray(configState.customColors, CS_DOC(cs)["customColors"])) {
            Serial.println("ERROR: copyArray failed in saveConf handler");
        }

        /*
        //// FIXME find a better way to do this
        // N.B. customColors field not in this message, should already be updated
        for (int i = 0; (i < NUM_LEDS); i++) {
            CS_DOC(cs)["customColors"][i][0] = configState.customColors[i][0];
            CS_DOC(cs)["customColors"][i][1] = configState.customColors[i][1];
        }
        */

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
    if (true) {  //// TMP TMP TMP
        Serial.println("+++++++++++++++++++++++++++");
        Serial.println(msg);
        Serial.println("+++++++++++++++++++++++++++");
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
        serializeJson(*(cs.doc), Serial);
    }

    if (false) {  //// TMP TMP TMP
        Serial.println("Clearing out the config file");
        deserializeJson(*(cs.doc), "{}");
    }

    cs.printConfig();    //// TMP TMP TMP
    Serial.println("CS Mem: " + String(CS_DOC(cs).memoryUsage()));  //// TMP TMP TMP

    // overwrite defaults in the config state struct with any values found in the config file
    INIT_STATE(configState.ssid, cs, "ssid", String);
    INIT_STATE(configState.passwd, cs, "passwd", String);
    INIT_STATE(configState.elState, cs, "elState", bool);
    INIT_STATE(configState.randomSequence, cs, "randomSequence", bool);
    INIT_STATE(configState.sequenceNumber, cs, "sequenceNumber", uint16_t);
    INIT_STATE(configState.sequenceDelay, cs, "sequenceDelay", uint32_t);
    INIT_STATE(configState.ledState, cs, "ledState", bool);
    INIT_STATE(configState.randomPattern, cs, "randomPattern", bool);
    INIT_STATE(configState.patternNumber, cs, "patternNumber", uint16_t);
    INIT_STATE(configState.patternDelay, cs, "patternDelay", uint32_t);
    INIT_STATE(configState.patternColor, cs, "patternColor", uint32_t);
    INIT_STATE(configState.customBidir, cs, "customBidir", bool);
    INIT_STATE(configState.customDelta, cs, "customDelta", uint16_t);
    uint16_t numElems = CS_DOC(cs)["customColors"].size();
    //// TMP TMP TMP
    Serial.println(">> ArraySize: " + String(numElems) + ", OVFL: " + String(cs.doc->overflowed()) + ", VALID: " + cs.validEntry("customColors"));
    if (numElems != NUM_LEDS) {
        /*
        CS_DOC(cs)["customColors"].clear();
        CS_DOC(cs).createNestedArray("customColors");
        if (!copyArray(configState.customColors, CS_DOC(cs)["customColors"])) {
            Serial.println("ERROR: copyArray failed in config");
        }
        */
        for (int i = NUM_LEDS; (i < numElems); i++) {
            CS_DOC(cs)["customColors"].remove(i);
            /*
            CS_DOC(cs)["customColors"][i].clear();
            CS_DOC(cs)["customColors"][numElems][0] = 0;
            CS_DOC(cs)["customColors"][numElems][1] = 0;
            */
        }
        /*
        Serial.println("CS Mem:: " + String(CS_DOC(cs).memoryUsage()));
        cs.doc->garbageCollect();
        Serial.println("CS Mem:: " + String(CS_DOC(cs).memoryUsage()));
        cs.doc->shrinkToFit();
        Serial.println("CS Mem::: " + String(CS_DOC(cs).memoryUsage()));
        */
        numElems = NUM_LEDS;
    }
    //// TMP TMP TMP
    Serial.println(">>>> ArraySize: " + String(numElems) + ", OVFL: " + String(cs.doc->overflowed()) + ", VALID: " + cs.validEntry("customColors"));
    if (cs.validEntry("customColors") && !cs.doc->overflowed() && (numElems == NUM_LEDS)) {
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
    if (false) {
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


/*

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
    Serial.println("B: "); printTuples("Tuples: [", configState.customColors); Serial.println("\n");
    copyArray(CS_DOC(cs)["customColors"], configState.customColors);
    Serial.println("C: "); printTuples("Tuples: [", configState.customColors); Serial.println("\n");
    if (true) {
        Serial.println("Config File: vvvvvvvvvvvvvvvvvvv");
        serializeJson(*(cs.doc), Serial);
        cs.listFiles("/");
        cs.printConfig();
        Serial.println("...\n^^^^^^^^^^^^^^^^^^^^^\n");
    }
*/