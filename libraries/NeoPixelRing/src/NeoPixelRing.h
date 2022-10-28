/*
* NeoPixel LED Ring library
*/

#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>


#define NEO_PIXEL_RING_VERSION  "1.0"

#define LED_PIN             15
#define LED_COUNT           16
#define DEF_LED_BRIGHTNESS  50  // max = 255


#define LED_TEST_ALL_WHITE  0
#define LED_TEST_ALL_RED    1
#define LED_TEST_ALL_GREEN  2
#define LED_TEST_ALL_BLUE   3
#define LED_TEST_ALL_OFF    4
#define LED_TEST_ALL_BLINK  5


class NeoPixelRing {
public:
    String libVersion = NEO_PIXEL_RING_VERSION;
    NeoPixelRing();
    NeoPixelRing(byte brightness);
    void run();
    void test(byte test);

private:
    Adafruit_NeoPixel *_ring = new Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

    void _create(byte brightness);
};

#include "NeoPixelRing.hpp"
