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


#define APPL_NAME               "ElectroHat"
#define APPL_VERSION            "1.0.0"

#define VERBOSE                 1

#define CONFIG_PATH             "/config.json"

#define WIFI_AP_SSID            "ElectroHat"
 
#define WEB_SERVER_PORT         80

#define STD_WAIT                35

#define STARTUP_EL_SEQUENCE     0
#define STARTUP_EL_DELAY        100

#define EH_HTML_PATH            "/index.html"
#define EH_STYLE_PATH           "/style.css"
#define EH_SCRIPTS_PATH         "/scripts.js"

#define VALID_ENTRY(doc, key)   (doc.containsKey(key) && !doc[key].isNull())

#define MAX_PATTERNS            10

#define STARTUP_RANDOM_PATTERN  false
#define STARTUP_PATTERN_NUM     0
#define STARTUP_PATTERN_DELAY   100
#define STARTUP_PATTERN_COLOR   0xFFFFFF
#define STARTUP_CUSTOM_BIDIR    true
#define STARTUP_CUSTOM_DELTA    64

#define NUM_LEDS                LED_COUNT


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
    ColorRange      customColors[NUM_LEDS];
    bool            customBidir;
    unsigned short  customDelta;
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
    },
    STARTUP_CUSTOM_BIDIR,
    STARTUP_CUSTOM_DELTA
};

ElWires elWires;

WebServices webSvcs(APPL_NAME, WEB_SERVER_PORT);

NeoPixelRing ring = NeoPixelRing();

char *patternNamePtrs[MAX_PATTERNS];
uint16_t patternMinDelays[MAX_PATTERNS];
uint16_t patternMaxDelays[MAX_PATTERNS];

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
        //// TODO just load from wsMsg to configState struct and then use it to load the cs.configJsonDoc -- or the converse
        String ssidStr = String(wsMsg["ssid"]);
        configState.ssid = ssidStr;
        cs.configJsonDoc["ssid"] = ssidStr;
        String passwdStr = String(wsMsg["passwd"]);
        configState.passwd = passwdStr;
        cs.configJsonDoc["passwd"] = passwdStr;

        configState.elState = wsMsg["elState"];
        cs.configJsonDoc["elState"] = wsMsg["elState"];
        configState.randomSequence = wsMsg["randomSequence"];
        cs.configJsonDoc["randomSequence"] = wsMsg["randomSequence"];
        configState.sequenceNumber = wsMsg["sequenceNumber"];
        cs.configJsonDoc["sequenceNumber"] = wsMsg["sequenceNumber"];
        configState.sequenceDelay = wsMsg["sequenceDelay"];
        cs.configJsonDoc["sequenceDelay"] = wsMsg["sequenceDelay"];

        configState.ledState = wsMsg["ledState"];
        cs.configJsonDoc["ledState"] = wsMsg["ledState"];
        configState.randomSequence = wsMsg["randomPattern"];
        cs.configJsonDoc["randomPattern"] = wsMsg["randomPattern"];
        configState.patternNumber = wsMsg["patternNumber"];
        cs.configJsonDoc["patternNumber"] = wsMsg["patternNumber"];
        configState.patternDelay = wsMsg["patternDelay"];
        cs.configJsonDoc["patternDelay"] = wsMsg["patternDelay"];
        configState.patternColor = wsMsg["patternColor"];
        cs.configJsonDoc["patternColor"] = wsMsg["patternColor"];
        json2CustomColors(configState.customColors, wsMsg["customColors"]);
        cs.configJsonDoc["customColors"] = wsMsg["customColors"];
        configState.customBidir = wsMsg["customBidir"];
        cs.configJsonDoc["customBidir"] = wsMsg["customBidir"];
        configState.customDelta = wsMsg["customDelta"];
        cs.configJsonDoc["customDelta"] = wsMsg["customDelta"];
        if (!cs.saveConfig()) {
            Serial.println("ERROR: Failed to write config file");
        }
    } else if (msgType.equalsIgnoreCase("reboot")) {
        println("REBOOTING...");
        reboot();
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
    msg += ", \"customColors\": " + customColors2String(configState.customColors);
    msg += ", \"customBidir\": " + String(configState.customBidir);
    msg += ", \"customDelta\": " + String(configState.customDelta);
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

void customColors2Json(JsonVariant colorRanges, ColorRange customColors[]) {
    //// TODO figure out a better way to do this
    uint32_t cc[NUM_LEDS][2];
    //// TODO use foreach?
    for (int i = 0; (i < NUM_LEDS); i++) {
        cc[i][0] = customColors[i].startColor;
        cc[i][1] = customColors[i].endColor;
    }
    copyArray(cc, colorRanges);
};

void printCustomColors(ColorRange colorRanges[]) {
    Serial.print("Custom Colors: [");
    for (int i = 0; (i < LED_COUNT); i++) {
        if (i > 0) {
            Serial.print(", ");
        }
        Serial.print("[0x" + String(colorRanges[i].startColor, HEX) + ", 0x" + String(colorRanges[i].endColor, HEX) + "]");
    }
    Serial.println("]");
}

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
    bool dirty = false;
    cs.open(CONFIG_PATH);

    if (false) {  //// TMP TMP TMP
        // disregard the contents of the saved config file
        deserializeJson(cs.configJsonDoc, "{}");
    }

    unsigned short  sequenceNumber;
    unsigned short  sequenceDelay;

    if (!VALID_ENTRY(cs.configJsonDoc, "ssid")) {
        cs.configJsonDoc["ssid"] = configState.ssid;
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "passwd")) {
        cs.configJsonDoc["passwd"] = configState.passwd;
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "elState")) {
        cs.configJsonDoc["elState"] = configState.elState;
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "randomSequence")) {
        cs.configJsonDoc["randomSequence"] = configState.randomSequence;
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "sequenceNumber")) {
        cs.configJsonDoc["sequenceNumber"] = configState.sequenceNumber;
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "sequenceDelay")) {
        cs.configJsonDoc["sequenceDelay"] = configState.sequenceDelay;
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "ledState")) {
        cs.configJsonDoc["ledState"] = configState.ledState;
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "randomPattern")) {
        cs.configJsonDoc["randomPattern"] = configState.randomPattern;
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "patternNumber")) {
        cs.configJsonDoc["patternNumber"] = configState.randomPattern;
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "patternDelay")) {
        cs.configJsonDoc["patternDelay"] = configState.patternDelay;
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "patternColor")) {
        cs.configJsonDoc["patternColor"] = configState.patternColor;
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "customColors")) {
        JsonArray arr = cs.configJsonDoc.createNestedArray("customColors");
        customColors2Json(arr, configState.customColors);
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "customBidir")) {
        cs.configJsonDoc["customBidir"] = configState.customBidir;
        dirty = true;
    }
    if (!VALID_ENTRY(cs.configJsonDoc, "customDelta")) {
        cs.configJsonDoc["customDelta"] = configState.customDelta;
        dirty = true;
    }
    if (dirty) {
        cs.saveConfig();
    }

    configState.ssid = cs.configJsonDoc["ssid"].as<String>();
    configState.passwd = cs.configJsonDoc["passwd"].as<String>();
    configState.elState = cs.configJsonDoc["elState"].as<bool>();
    configState.randomSequence = cs.configJsonDoc["randomSequence"].as<bool>();
    configState.sequenceNumber = cs.configJsonDoc["sequenceNumber"].as<unsigned int>();
    configState.sequenceDelay = cs.configJsonDoc["sequenceDelay"].as<unsigned int>();
    configState.ledState = cs.configJsonDoc["ledState"].as<bool>();
    configState.randomPattern = cs.configJsonDoc["randomPattern"].as<bool>();
    configState.patternNumber = cs.configJsonDoc["patternNumber"].as<unsigned int>();
    configState.patternDelay = cs.configJsonDoc["patternDelay"].as<unsigned int>();
    configState.patternColor = cs.configJsonDoc["patternColor"].as<unsigned int>();
    json2CustomColors(configState.customColors, cs.configJsonDoc["customColors"]);
    configState.customBidir = cs.configJsonDoc["customBidir"].as<bool>();
    configState.customDelta = cs.configJsonDoc["customDelta"].as<unsigned int>();
    if (VERBOSE) {
        println("Config File:");
        serializeJson(cs.configJsonDoc, Serial);
        cs.listFiles("/");
        cs.printConfig();
        println("\n");
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
        Serial.println(".");
    }
    ring.getPatternDelays(patternMinDelays, patternMaxDelays, MAX_PATTERNS);
    Serial.println("Selected Pattern: " + String(ring.getSelectedPattern()));
    ColorRange colorRanges[LED_COUNT] = {};
    ring.getCustomPixels(0xFFFF, colorRanges, LED_COUNT);
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
    Serial.begin(19200);
    delay(500);
    Serial.println("\nBEGIN");

    //// TMP TMP TMP
    if (false) {
        // clear the local file system
        cs.format();
    }

    config();

    //// TMP TMP TMP
    if (true) {
        Serial.println("Local Files:");
        cs.listFiles("/");
    }
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
