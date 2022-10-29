/*
* ElectroHat NeoPixel library example application
*/

#include "NeoPixelRing.h"


#define NEO_PIXEL_RING_VERSION  "1.0"

#define VERBOSE             1

#define LED_PIN             15
#define LED_COUNT           16
#define DEF_LED_BRIGHTNESS  50  // max = 255

#define COLOR_WIPE  0
#define MARQUEE     1
#define RAINBOW     2


uint32_t loopCnt = 0;

NeoPixelRing ring = NeoPixelRing();


void setup() {
    delay(500);
    Serial.begin(19200);
    delay(500);
    Serial.println("\nBEGIN");

    ring.test(NeoPixelRing::ALL_WHITE);
    delay(500);

    Serial.println("Delay: " + String(ring.getDelay()));
    ring.setDelay(250);
    Serial.println("Delay: " + String(ring.getDelay()));
    Serial.println("# Patterns: " + String(ring.getNumPatterns()));
    Serial.println("Selected Pattern: " + String(ring.getSelectedPattern()));
    ring.selectPattern(1);
    Serial.println("Selected Pattern: " + String(ring.getSelectedPattern()));
    ring.setColor(ring.makeColor(255, 0, 255));
    Serial.println("Selected Color: 0x" + String(ring.getColor(), HEX));
    int n = ring.getNumPatterns();
    Serial.println("Number of Patterns: " + String(n));
    char *names[n];
    int numPatterns = ring.getPatternNames(names, n);
    if (numPatterns < 1) {
        Serial.println("getPatternNames failed");
    } else {
        Serial.print("Pattern Names (" + String(numPatterns) + "): ");
        for (int i = 0; (i < numPatterns); i++) {
            if (i > 0) {
                Serial.print(", ");
            }
            Serial.print(names[i]);
        }
        Serial.println(".");
    }

    Serial.println("\nSTART");
    ring.clear();
}

int p = 0;
void loop() {
    if ((loopCnt % 256) == 0) {
        p = (p + 1) % 3;
        Serial.println("P: " + String(p) + ", Loop: " + String(loopCnt));
        ring.clear();
    }
    ring.selectPattern(p);
    int d;
    switch (p) {
        case COLOR_WIPE:
            d = 50;
            break;
        case MARQUEE:
            d = 60;
            break;
        case RAINBOW:
            d = 50;
            break;
        default:
            d = 10;
    }
    ring.setDelay(d);
    ring.run();
    loopCnt += 1;
}
