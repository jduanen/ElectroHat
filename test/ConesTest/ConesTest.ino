/***************************************************************************

 ElectroHat Cones Test Program
 
****************************************************************************/
#include <Arduino.h>
//#include <WiFiUtilities.h>
//#include "wifi.h"
//#include <ConfigService.h>
#include "sequences.h"
#include "ElWires.h"


#define STARTUP_EL_SEQUENCE 0
#define STARTUP_EL_SPEED    100


typedef struct {
//    String         ssid;
//    String         passwd;
    bool           elState;
    bool           randomSequence;
    unsigned short sequenceNumber;
    unsigned short sequenceSpeed;
} ConfigState;


ConfigState configState = {
//    String(WLAN_SSID),
//    String(rot47(WLAN_PASS)),
    true,
    false,
    STARTUP_EL_SEQUENCE,
    STARTUP_EL_SPEED
};

ElWires elWires(true);


void setup() {
    Serial.begin(9600);
    delay(500);
    Serial.println("\nBEGIN");

    Serial.println("ElWire Lib Version: " + elWires.libVersion);
    elWires.start();
    Serial.println("Init EL Wires");
    elWires.clear();
    elWires.setSequence(configState.sequenceNumber, configState.sequenceSpeed);
    Serial.println("Number of Sequences: " + String(elWires.numSequences()));
    Serial.println("Random Sequence Enabled: " + String(elWires.randomSequence() ? "Yes" : "No"));
    Serial.println("Sequence Number: " + String(elWires.sequenceNumber()));
    Serial.println("Sequence Speed: " + String(elWires.sequenceSpeed()));

    Serial.println("READY");
}

int cnt = 0;

void loop() {
    if (true) {
        elWires.run();
    } else {
        Serial.println(cnt);
        elWires.writeAll(cnt & 0xFF);
        delay(1000);
        cnt += 1;
    }
}
