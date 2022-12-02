/*
* ElectroHat NeoPixel library example application
*/

#include "NeoPixelRing.h"


#define VERBOSE         1

#define LED_COUNT       16

#define RAINBOW_MARQUEE 0
#define RAINBOW         1
#define COLOR_WIPE      2
#define COLOR_FILL      3
#define MARQUEE         4
#define CUSTOM          5


uint32_t loopCnt = 0;

NeoPixelRing<LED_COUNT> ring;

int numPatterns;
char **namePtrs;


void setup() {
    delay(500);
    Serial.begin(19200);
    delay(500);
    Serial.println("\nBEGIN");

    Serial.println("# of LEDs: " + String(ring.getNumLeds()));
    ring.setColor(0xFFFFFF);  // White: all on
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
    int numPatterns = ring.getNumPatterns();
    Serial.println("Number of Patterns: " + String(numPatterns));
    namePtrs = new char *[numPatterns];
    int num = ring.getPatternNames(namePtrs, numPatterns);
    if (num < 1) {
        Serial.println("getPatternNames failed");
    } else {
        if (num != numPatterns) {
            Serial.println("getPatternNames mismatch: " + String(num) + " != " + String(numPatterns));
        }
        Serial.print("Pattern Names (" + String(numPatterns) + "): ");
        for (int i = 0; (i < numPatterns); i++) {
            if (i > 0) {
                Serial.print(", ");
            }
            Serial.print(namePtrs[i]);
        }
        Serial.println(".");
    }
    ring.enableCustomPixels(0x03C0, ring.makeColor(0, 255, 255), ring.makeColor(0, 255, 255));
    ring.enableCustomPixels(0xC003, ring.makeColor(255, 0, 255), ring.makeColor(255, 0, 255));
    ring.enableCustomPixels(0x003C, ring.makeColor(255, 0, 0), ring.makeColor(0, 255, 0));
    ring.enableCustomPixels(0x3C00, ring.makeColor(0, 255, 0), ring.makeColor(255, 0, 0));
    int n = ring.getEnabledCustomPixels();
    Serial.println("Currently enabled Pixels: 0x" + String(n, HEX));

    ColorRange ranges[8] = {};
    ring.getCustomPixels(0xFF, ranges, 8);
    for (int i = 0; (i < 8); i++) {
        Serial.println("    Pixel: " + String(i) + \
            ", startColor: 0x" + String(ranges[i].startColor, HEX) + \
            ", endColor: 0x" + String(ranges[i].endColor, HEX));
    }
    ring.setCustomDelta(128);
    n = ring.getCustomDelta();
    Serial.println("Custom Pixel Delta: " + String(n));
    bool b = ring.getCustomBidir();
    Serial.println("Custom Bidirectional: " + String(b));
    ring.setCustomBidir(true);
    Serial.println("Current Custom Bidirectional: " + String(b));

    ring.enableRandomPattern(false);
    b = ring.randomPattern();
    Serial.println("Random Pattern enable: " + String(b));

    Serial.println("\nSTART");
    ring.clear();
}

int p = 0;

uint32_t colors[] = {
    ring.makeColor(0, 0, 255),
    ring.makeColor(0, 255, 0),
    ring.makeColor(255, 0, 0),
    ring.makeColor(255, 0, 255),
    ring.makeColor(255, 255, 0),
    ring.makeColor(255, 255, 255)
};

int numColors = (sizeof(colors) / 4);

int c = 0;

void loop() {
    int d;

    if (false) {
        if ((loopCnt % 512) == 0) {
            p = (p + 1) % numPatterns;
            Serial.println("P: " + String(p) + ", Pattern: " + String(namePtrs[p]) + ", Loop: " + String(loopCnt));
            ring.clear();
            c += 1;
        }
    } else {
        p = CUSTOM;
    }
    
    ring.selectPattern(p);

    switch (p) {
        case COLOR_WIPE:
            d = 250;
            ring.setColor(colors[c % numColors]);
            break;
        case COLOR_FILL:
            d = 300;
            ring.setColor(colors[loopCnt % numColors]);
            break;
        case MARQUEE:
            d = 75;
//            ring.setColor(colors[loopCnt % numColors]);
            break;
        case RAINBOW_MARQUEE:
            d = 75;
            break;
        case RAINBOW:
            d = 100;
            break;
        case CUSTOM:
            d = 10;
            break;
        default:
            d = 100;
    }
    ring.setDelay(d);
    unsigned long waitTime = ring.run();
    if (waitTime > 0) {
        delay(waitTime);
    }
    loopCnt += 1;
}
