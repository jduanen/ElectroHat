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
*  - From client to server:
*   * {"applName": <String>, "libVersion": <String>, "ipAddr": <String>,
*      "ssid": <String>, "passwd": <String>, "RSSI": <Int>, "el": <Boolean>,
*      "randomSequence": <Boolean>, "sequenceNumber": <Int>, "sequenceDelay": <Int>,
*      "led": <Boolean>, "ledPattern"}
*  - From server to client:
*   * {"msgType": "el", "state": <Boolean>}
*   * {"msgType": "sequence", "sequenceNumber: <Int>, sequenceDelay: <Int>}
*   * {"msgType": "saveConf", "ssid": document.getElementById("ssid").value,
*      "passwd": <String>, "elState": <Boolean>, "randomSequence": <Boolean>,
*      "sequenceNumber": <Int>, "sequenceDelay": <Int>, "ledState": <Boolean>}
*   * {"msgType": "randomSequence", "state": <Boolean>}
*   * {"msgType": "led", "state": <Boolean>}
**** TODO add Pixels stuff
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


#define APPL_NAME           "ElectroHat"
#define APPL_VERSION        "1.0.0"

#define VERBOSE             1

#define CONFIG_PATH         "/config.json"

#define WIFI_AP_SSID        "ElectroHat"
 
#define WEB_SERVER_PORT     80

#define STD_WAIT            35

#define STARTUP_EL_SEQUENCE 0
#define STARTUP_EL_DELAY    100

#define EH_HTML_PATH        "/index.html"
#define EH_STYLE_PATH       "/style.css"
#define EH_SCRIPTS_PATH     "/scripts.js"

#define VALID_ENTRY(doc, key)    (doc.containsKey(key) && !doc[key].isNull())

//#define MAX_LED_PATTERNS    ?

#define STARTUP_LED_DELAY   100
#define STARTUP_LED_DELTA   64
#define STARTUP_LED_COLOR   0xFFFFFF
#define STARTUP_LED_RANDOM  false
#define STARTUP_LED_PATTERN 0
//// TODO add custom pixels description
#define STARTUP_LED_BIDIR   true


typedef struct {
    String          ssid;
    String          passwd;
    bool            elState;
    bool            randomSequence;
    unsigned short  sequenceNumber;
    unsigned short  sequenceDelay;
    bool            ledState;
    uint32_t        ledDelay;
    unsigned short  ledDelta;
    uint32_t        ledColor;
    bool            randomPattern;
    unsigned short  patternNumber;
    //// TODO add custom pixels description
    bool            bidir;
} ConfigState;

ConfigState configState = {
    String(WLAN_SSID),
    String(rot47(WLAN_PASS)),
    true,
    false,
    STARTUP_EL_SEQUENCE,
    STARTUP_EL_DELAY,
    true,
    STARTUP_LED_DELAY,
    STARTUP_LED_DELTA,
    STARTUP_LED_COLOR,
    STARTUP_LED_RANDOM,
    STARTUP_LED_PATTERN,
    //// TODO add custom pixels description
    STARTUP_LED_BIDIR
};

ElWires elWires;

WebServices webSvcs(APPL_NAME, WEB_SERVER_PORT);

NeoPixelRing ring = NeoPixelRing();

char *patternNames[LED_COUNT];

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
    }
    return(String());
};

String webpageMsgHandler(const JsonDocument& wsMsg) {
    if (VERBOSE) { //// TMP TMP TMP
        Serial.println("MSG:");
        serializeJsonPretty(wsMsg, Serial);
    }
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
    } else if (msgType.equalsIgnoreCase("saveConf")) {
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
    
        serializeJsonPretty(cs.configJsonDoc, Serial);
        cs.saveConfig();
    } else if (msgType.equalsIgnoreCase("led")) {
        configState.ledState = wsMsg["state"];
        if (!configState.ledState) {
            ring.clear();
        }
    } else if (msgType.equalsIgnoreCase("reboot")) {
        println("REBOOTING...");
        reboot();
    }

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
    Serial.println(msg);  //// TMP TMP TMP
    return(msg);
};

WebPageDef webPage = {
    EH_HTML_PATH,
    EH_SCRIPTS_PATH,
    EH_STYLE_PATH,
    webpageProcessor,
    webpageMsgHandler
};


void config() {
    bool dirty = false;
    cs.open(CONFIG_PATH);

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
    if (dirty) {
        serializeJsonPretty(cs.configJsonDoc, Serial);
        cs.saveConfig();
    }

    configState.ssid = cs.configJsonDoc["ssid"].as<String>();
    configState.passwd = cs.configJsonDoc["passwd"].as<String>();
    configState.elState = cs.configJsonDoc["elState"].as<bool>();
    configState.randomSequence = cs.configJsonDoc["randomSequence"].as<bool>();
    configState.sequenceNumber = cs.configJsonDoc["sequenceNumber"].as<unsigned int>();
    configState.sequenceDelay = cs.configJsonDoc["sequenceDelay"].as<unsigned int>();
    configState.ledState = cs.configJsonDoc["ledState"].as<bool>();
    if (VERBOSE) {
        println("Config File:");
        serializeJsonPretty(cs.configJsonDoc, Serial);
        cs.listFiles(CONFIG_PATH);
        cs.printConfig();
//        serializeJsonPretty(cs.configJsonDoc, Serial);
        println("");
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

    ring.setDelay(configState.ledDelay);
    ring.setCustomDelta(configState.ledDelta);
    ring.setColor(configState.ledColor);
    ring.enableRandomPattern(configState.randomPattern);
    ring.selectPattern(configState.patternNumber);
    //// TODO initialize custom pattern
    ring.setCustomBidir(configState.bidir);

    Serial.println("LED Delay: " + String(ring.getDelay()));
    Serial.println("Custom Pixel Color Delta: " + String(ring.getCustomDelta()));
    Serial.println("Selected LED Color: 0x" + String(ring.getColor(), HEX));
    Serial.println("Random Pattern enable: " + String(ring.randomPattern()));
    int n = ring.getNumPatterns();
    if (n < 1) {
        Serial.println("getPatternNames failed");
    } else {
        Serial.println("Number of Patterns: " + String(n));
    }
    int numPatterns = ring.getPatternNames(patternNames, LED_COUNT);
    Serial.print("Pattern Names (" + String(numPatterns) + "): ");
    for (int i = 0; (i < numPatterns); i++) {
        if (i > 0) {
            Serial.print(", ");
        }
        Serial.print(patternNames[i]);
    }
    Serial.println(".");
    Serial.println("Selected Pattern: " + String(ring.getSelectedPattern()));
    ColorRange colorRanges[LED_COUNT] = {};
    ring.getCustomPixels(0xFF, colorRanges, LED_COUNT);
    for (int i = 0; (i < LED_COUNT); i++) {
        Serial.println("    Pixel: " + String(i) + \
            ", startColor: 0x" + String(colorRanges[i].startColor, HEX) + \
            ", endColor: 0x" + String(colorRanges[i].endColor, HEX));
    }
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

    //// FIXME TMP TMP TMP
    if (false) {
        // clear the local file system
        cs.format();
    }
    if (true) {
        Serial.println("Local Files:");
        cs.listFiles("/");
    }

    config();

    wiFiConnect(configState.ssid, rot47(configState.passwd), WIFI_AP_SSID);

    if (!webSvcs.addPage(webPage)) {
        Serial.println("ERROR: failed to add common page; continuing anyway");
    }

    initElWires();
    Serial.println("EL wires function started");

    webSvcs.updateClients();

    initLEDs();
    Serial.println("NeoPixels function started");

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
