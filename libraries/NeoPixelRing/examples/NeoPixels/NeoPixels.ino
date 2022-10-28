/*
* ElectroHat NeoPixel library example application
*/

#include "NeoPixelRing.h"


#define NEO_PIXEL_RING_VERSION  "1.0"

#define VERBOSE             1

#define LED_PIN             15
#define LED_COUNT           16
#define DEF_LED_BRIGHTNESS  50  // max = 255


uint32_t    loopCnt = 0;

NeoPixelRing ring;


void setup() {
    delay(500);
    Serial.begin(19200);
    delay(500);
    Serial.println("\nBEGIN");

    /*
    Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
    for (int i = 0; (i < strip.numPixels()); i++) {
        strip.setPixelColor(i, strip.Color(255,0,255));
        strip.show();
        delay(200);
    }
    */
    Serial.println("*******");

    ring = NeoPixelRing();

    ring.test(LED_TEST_ALL_BLUE);
    delay(1000);
    Serial.println("\nSTART");
}

void loop() {
    ring.test(LED_TEST_ALL_WHITE);
    Serial.println("AAA");
    delay(1000);
    ring.test(LED_TEST_ALL_RED);
    Serial.println("BBB");
    delay(1000);
    ring.test(LED_TEST_ALL_OFF);
    Serial.println("CCC");
    delay(1000);

    if ((loopCnt & 10000) == 0) {
        Serial.println("Loop: " + String(loopCnt));
    }
    loopCnt += 1;
}

    /*
    // fill along the length of the strip in R, G, and B
    println("Color Wipe: R");
    colorWipe(strip.Color(255,   0,   0), 50);
    println("Color Wipe: G");
    colorWipe(strip.Color(  0, 255,   0), 50);
    println("Color Wipe: B");
    colorWipe(strip.Color(  0,   0, 255), 50);

    // theater marquee effect in W, R, and B -- all in half brightness
    println("Theater Marquee: W");
    theaterChase(strip.Color(127, 127, 127), 50);
    println("Theater Marquee: R");
    theaterChase(strip.Color(127,   0,   0), 50);
    println("Theater Marquee: B");
    theaterChase(strip.Color(  0,   0, 127), 50);

    // flowing rainbow cycle along the whole strip
    println("Rainbow");
    rainbow(10);

    // rainbow cycle along the whole strip
    println("RainbowCycle");
    rainbowCycle(10);

    // rainbow-enhanced theaterChase variant
    println("Theater Chase Rainbow");
    theaterChaseRainbow(50);
    */
