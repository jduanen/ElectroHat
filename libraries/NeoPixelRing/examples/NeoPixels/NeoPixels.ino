/*
* ElectroHat NeoPixel library example application
*/

#include "NeoPixelRing.h"


#define NEO_PIXEL_RING_VERSION  "1.0"

#define VERBOSE             1

#define LED_PIN             15
#define LED_COUNT           16
#define DEF_LED_BRIGHTNESS  50  // max = 255

#define COLOR_WIPE      0
#define COLOR_FILL      1
#define MARQUEE         2
#define RAINBOW_MARQUEE 3
#define RAINBOW         4
#define CUSTOM          5
#define NUM_PATTERNS    6


uint32_t loopCnt = 0;

NeoPixelRing ring = NeoPixelRing();

char *names[LED_COUNT];


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
    ring.enableCustomPixels(0x03C0, ring.makeColor(0, 255, 255), ring.makeColor(0, 255, 255));
    ring.enableCustomPixels(0xC003, ring.makeColor(255, 0, 255), ring.makeColor(255, 0, 255));
    ring.enableCustomPixels(0x003C, ring.makeColor(255, 0, 0), ring.makeColor(0, 255, 0));
    ring.enableCustomPixels(0x3C00, ring.makeColor(0, 255, 0), ring.makeColor(255, 0, 0));
    n = ring.getEnabledCustomPixels();
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
            p = (p + 1) % NUM_PATTERNS;
            Serial.println("P: " + String(p) + ", Pattern: " + names[p] + ", Loop: " + String(loopCnt));
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
