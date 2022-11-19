/*
* NeoPixel LED Ring library
*/

#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>


#define NEO_PIXEL_RING_VERSION  "1.1"

#define LED_PIN             15

#define DEF_LED_BRIGHTNESS  50  // max = 255

#ifndef UNUSED_ANALOG
#define UNUSED_ANALOG       A0
#endif /* UNUSED_ANALOG */

#define MAX_PATTERNS        6
#define PATTERN_FUNC(func)  [](NeoPixelRing<numLeds>* npr) {return npr->func();}

typedef struct {
    uint32_t startColor;
    uint32_t endColor;
} ColorRange;


template<uint8_t numLeds>
class NeoPixelRing {
public:
    String libVersion = NEO_PIXEL_RING_VERSION;

    NeoPixelRing();
    NeoPixelRing(byte brightness);

    byte getNumLeds();
    byte getBrightness();
    void setBrightness(byte brightness);
    uint32_t getColor();
    void setColor(uint32_t color);
    uint32_t getDelay();
    void setDelay(uint32_t delay);
    byte getNumPatterns();
    byte getPatternNames(char *namePtrs[], byte number);
    byte getSelectedPattern();
    bool selectPattern(byte patternNum);
    void enableRandomPattern(bool enable);
    bool randomPattern();
    uint32_t getEnabledCustomPixels();
    void enableCustomPixels(uint32_t pixels, uint32_t startColor, uint32_t endColor);
    void disableCustomPixels(uint32_t pixels);
    void getCustomPixels(uint32_t pixels, ColorRange colorRanges[], int size);
    uint32_t getCustomDelta();
    void setCustomDelta(uint32_t delta);
    bool getCustomBidir();
    void setCustomBidir(bool bidir);
    void getPatternDelays(uint16_t minDelays[], uint16_t maxDelays[], uint16_t defDelays[], byte number);

    uint32_t makeColor(byte r, byte g, byte b);
    void fill(uint32_t color);
    void clear();
    unsigned long run();

private:
    using PatternFuncPtr = void (*)(NeoPixelRing<numLeds>*);
    typedef struct {
        PatternFuncPtr  methodPtr;
        char            *name;
        uint16_t        minDelay;
        uint16_t        maxDelay;
        uint16_t        defDelay;
    } Patterns;

    Adafruit_NeoPixel *_ring = new Adafruit_NeoPixel(numLeds, LED_PIN, NEO_GRB + NEO_KHZ800);

    byte _brightness = DEF_LED_BRIGHTNESS;
    uint32_t _color = 0x000000;
    uint32_t _patternDelay = 0;
    byte _patternNum = 0;
    uint32_t _firstPixelHue = 0;
    bool _randomPattern = false;
    byte _pixelNum = 0;
    unsigned long _nextRunTime = 0;
    unsigned long _loopCnt = 0;
    uint32_t _customPixelEnables = 0;
    ColorRange _colorRanges[numLeds];
    int _customDelta = 64;
    bool _customBidir = false;
    const uint8_t _numPatterns = MAX_PATTERNS;
    Patterns _patterns[MAX_PATTERNS] = {
        {PATTERN_FUNC(NeoPixelRing<numLeds>::_rainbowMarquee), (char *)"Rainbow Marquee", 1, 300, 64},
        {PATTERN_FUNC(NeoPixelRing<numLeds>::_rainbow), (char *)"Rainbow", 1, 50, 8},
        {PATTERN_FUNC(NeoPixelRing<numLeds>::_colorWipe), (char *)"Color Wipe", 30, 300, 64},
        {PATTERN_FUNC(NeoPixelRing<numLeds>::_colorFill), (char *)"Color Fill", 0, 0, 0},
        {PATTERN_FUNC(NeoPixelRing<numLeds>::_marquee), (char *)"Marquee", 10, 150, 64},
        {PATTERN_FUNC(NeoPixelRing<numLeds>::_custom), (char *)"Custom", 1, 400, 32}
    };

    void _create(byte brightness);

    void _generateRandomPixel();
    void _splitColor(byte *rPtr, byte *gPtr, byte *bPtr, uint32_t c);

    void _rainbowMarquee();
    void _rainbow();
    void _colorWipe();
    void _colorFill();
    void _marquee();
    void _custom();
};

#include "NeoPixelRing.hpp"
