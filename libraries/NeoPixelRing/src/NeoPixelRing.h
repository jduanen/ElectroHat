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

#define PATTERN_FUNC(func)  [](NeoPixelRing* npr) { return npr->func();}

class NeoPixelRing;
using PatternFunc = void (*)(NeoPixelRing*);

typedef struct {
    PatternFunc func;
    char *name;
} Patterns;


class NeoPixelRing {
public:
    String libVersion = NEO_PIXEL_RING_VERSION;

    enum LED_TEST {
        ALL_WHITE = 0,
        ALL_RED   = 1,
        ALL_GREEN = 2,
        ALL_BLUE  = 3,
        ALL_OFF   = 4,
        ALL_BLINK = 5
    };

    NeoPixelRing();
    NeoPixelRing(byte brightness);
    void test(byte test);
    void clear();
    void run();
    uint32_t makeColor(byte r, byte g, byte b);
    void setColor(uint32_t color);
    uint32_t getColor();
    void setDelay(uint32_t delay);
    uint32_t getDelay();
    byte getNumPatterns();
    byte getPatternNames(char *namePtrs[], byte number);
    byte getSelectedPattern();
    bool selectPattern(byte patternNum);

private:
    Adafruit_NeoPixel *_ring = new Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

    static byte const _NUM_PATTERNS = 3;

    byte _pixelNum = 0;
    uint32_t _color = 0x000000;
    uint32_t _delay = 0;
    byte _patternNum = 0;
    unsigned long _nextRunTime = 0;
    uint32_t _firstPixelHue = 0;
    unsigned long _loopCnt = 0;

    void _create(byte brightness);
    void _colorWipe();
    void _marquee();
    void _rainbow();

//    const PatternFunc _patterns[_NUM_PATTERNS] = {
    const Patterns _patterns[_NUM_PATTERNS] = {
        {PATTERN_FUNC(_colorWipe), "Color Wipe"},
        {PATTERN_FUNC(_marquee), "Marquee"},
        {PATTERN_FUNC(_rainbow), "Rainbow"}
    };
};

#include "NeoPixelRing.hpp"
