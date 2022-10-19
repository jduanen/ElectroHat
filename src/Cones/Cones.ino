/***************************************************************************
*
* ElectroHat Cones application
* 
* Drives eight 9' EL wires using the AFD Driver board
* 
* N.B. This is insecure in that it passes the WiFi password in the clear -- you've been warned...
*       (actually, it's slightly obfuscated, but essentially in the clear)
*
* WebSocket interface
*  - From client to server:
*   * {"applName": <String>, "libVersion": <String>, "ipAddr": <String>,
*      "ssid": <String>, "passwd": <String>, "RSSI": <Int>, "el": <Boolean>,
*      "randomSequence": <Boolean>, "sequenceNumber": <Int>, "sequenceSpeed": <Int>}
*  - From server to client:
*   * {"msgType": "el", "state": <Boolean>}
*   * {"msgType": "sequence", "sequenceNumber: <Int>, sequenceSpeed: <Int>}
*   * {"msgType": "saveConf", "ssid": document.getElementById("ssid").value,
*      "passwd": <String>, "elState": <Boolean>, "randomSequence": <Boolean>,
*      "sequenceNumber": <Int>, "sequenceSpeed": <Int>}
*   * {"msgType": "randomSequence", "state": <Boolean>}
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

#include "wifi.h"
#include "sequences.h"

#include <WiFiUtilities.h>
#include <ConfigService.h>
#include <WebServices.h>
#include <ElWires.h>


#define APPL_NAME           "ElectroHatCones"
#define APPL_VERSION        "1.0.0"

#define VERBOSE             1

#define CONFIG_PATH         "/config.json"

#define WIFI_AP_SSID        "ElectroHat"
 
#define WEB_SERVER_PORT     80

#define STD_WAIT            35

#define STARTUP_EL_SEQUENCE 0
#define STARTUP_EL_SPEED    100

#define EH_HTML_PATH        "/index.html"
#define EH_STYLE_PATH       "/style.css"
#define EH_SCRIPTS_PATH     "/scripts.js"

#define VALID_ENTRY(doc, key)    (doc.containsKey(key) && !doc[key].isNull())


typedef struct {
    String         ssid;
    String         passwd;
    bool           elState;
    bool           randomSequence;
    unsigned short sequenceNumber;
    unsigned short sequenceSpeed;
} ConfigState;

ConfigState configState = {
    String(WLAN_SSID),
    String(rot47(WLAN_PASS)),
    true,
    false,
    STARTUP_EL_SEQUENCE,
    STARTUP_EL_SPEED
};

ElWires elWires;

WebServices webSvcs(APPL_NAME, WEB_SERVER_PORT);


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
        elWires.setSequence(wsMsg["sequenceNumber"], wsMsg["sequenceSpeed"]);
        configState.sequenceNumber = elWires.sequenceNumber();
        configState.sequenceSpeed = elWires.sequenceSpeed();
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
        configState.sequenceSpeed = wsMsg["sequenceSpeed"];
        cs.configJsonDoc["sequenceSpeed"] = wsMsg["sequenceSpeed"];
    
        serializeJsonPretty(cs.configJsonDoc, Serial);
        cs.saveConfig();
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
    msg += ", \"sequenceSpeed\": " + String(configState.sequenceSpeed);
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
    unsigned short  sequenceSpeed;

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
    if (!VALID_ENTRY(cs.configJsonDoc, "sequenceSpeed")) {
        cs.configJsonDoc["sequenceSpeed"] = configState.sequenceSpeed;
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
    configState.sequenceSpeed = cs.configJsonDoc["sequenceSpeed"].as<unsigned int>();
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
    elWires.setSequence(configState.sequenceNumber, configState.sequenceSpeed);
    println("Number of Sequences: " + String(elWires.numSequences()));
    println("Random Sequence Enabled: " + String(elWires.randomSequence() ? "Yes" : "No"));
    println("Sequence Number: " + String(elWires.sequenceNumber()));
    println("Sequence Speed: " + String(elWires.sequenceSpeed()));
};

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

    Serial.println("READY");
};

void loop() {
    webSvcs.run();

    if (configState.elState) {
        elWires.run();
    } else {
        // MAGIC NUMBER: measured to approximate constant delay through loop
        delayMicroseconds(100);
    }
    delay(1);
};

