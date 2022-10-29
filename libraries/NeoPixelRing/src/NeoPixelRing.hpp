/*
* NeoPixel LED Ring library
*/

NeoPixelRing::NeoPixelRing() {
    _create(DEF_LED_BRIGHTNESS);
};

NeoPixelRing::NeoPixelRing(byte brightness) {
    _create(brightness);
};

void NeoPixelRing::_create(byte brightness) {
    _ring->begin();
    _ring->clear();
    _ring->show();  // turn off all pixels
    _ring->setBrightness(brightness);
    _ring->setPixelColor(1, 255, 0, 255);
};

void NeoPixelRing::test(byte test) {
    uint32_t c;

    switch (test) {
    case NeoPixelRing::ALL_WHITE:
        c = _ring->Color(255, 255, 255);
        _ring->fill(c, 0, LED_COUNT);
        _ring->show();
        break;
    case NeoPixelRing::ALL_RED:
        _ring->fill(_ring->Color(255, 0, 0), 0, LED_COUNT);
        _ring->show();
        break;
    case NeoPixelRing::ALL_GREEN:
        _ring->fill(_ring->Color(0, 255, 0), 0, LED_COUNT);
        _ring->show();
        break;
    case NeoPixelRing::ALL_BLUE:
        _ring->fill(_ring->Color(0, 0, 255), 0, LED_COUNT);
        _ring->show();
        break;
    case NeoPixelRing::ALL_OFF:
        _ring->clear();
        _ring->show();
        break;
    case NeoPixelRing::ALL_BLINK:
        int i;
        uint32_t c[LED_COUNT];
        for (i = 0; (i < LED_COUNT); i++) {
            c[i] = _ring->getPixelColor(i);
        }
        for (int n = 0; (n < 60); n++) {
            _ring->clear();
            delay(500);
            for (i = 0; (i < LED_COUNT); i++) {
                _ring->setPixelColor(i, c[i]);
            }
            _ring->show();
            delay(1000);
        }
        break;
    default:
        Serial.println("Invalid test: " + String(test));
        return;
    }
};

void NeoPixelRing::clear() {
    _ring->clear();
};

// Fill pixels sequentially with the selected color, with the selected delay
//  time (in msec) between each illumination.
// Clears the pixels at the start of every cycle.
void NeoPixelRing::_colorWipe() {
    _nextRunTime = millis() + _delay;
    if (_pixelNum == 0) {
        _ring->clear();
    }
    _ring->setPixelColor(_pixelNum, _color);
    _ring->show();
};

// Fill all pixels with the selected color
// Doesn't clear the pixels first.
void NeoPixelRing::_colorFill() {
    _nextRunTime = millis() + _delay;
    _ring->fill(_color, 0, _ring->numPixels());
    _ring->show();
};

// Movie-marquee-like chasing rainbow lights that precess
void NeoPixelRing::_marquee() {
    _nextRunTime = millis() + _delay;
    _ring->clear();
    int n = _ring->numPixels();
    for (int i = (_loopCnt % 3); (i < n); i += 3) {
        int hue = _firstPixelHue + i * 65536L / n;
        uint32_t rgbColor = _ring->gamma32(_ring->ColorHSV(hue));
//        Serial.println("> " + String(i) + ", 0x" + String(rgbColor, HEX));
        _ring->setPixelColor(i, rgbColor);
        _ring->show();
    }
    _firstPixelHue += 256;
};

// Loop through color wheel over all pixels.  Just changes Hue value and keeps
//  Saturation and Brightness at max (i.e., 255).  Bumps starting pixel's Hue
//  by 256 each iteration.
void NeoPixelRing::_rainbow() {
    _nextRunTime = millis() + _delay;
    _ring->clear();
    _ring->rainbow(_firstPixelHue);
    _ring->show();
    _firstPixelHue += 256;
};

void NeoPixelRing::run() {
    unsigned long now = millis();
    if (now > _nextRunTime) {
        Serial.println("Warning: " + String(now - _nextRunTime) + " msec late");
    } else {
        if (_nextRunTime > now) {
            delay(_nextRunTime - now);
        }
    }
    _patterns[_patternNum].func(this);
    _loopCnt += 1;
    _pixelNum = _loopCnt % _ring->numPixels();
};

uint32_t NeoPixelRing::makeColor(byte r, byte g, byte b) {
    return _ring->Color(r, g, b);
};

void NeoPixelRing::setColor(uint32_t color) {
    _color = color;
};

uint32_t NeoPixelRing::getColor() {
    return _color;
};

void NeoPixelRing::setDelay(uint32_t delay) {
    _delay = delay;
};

uint32_t NeoPixelRing::getDelay() {
    return _delay;
};

byte NeoPixelRing::getNumPatterns() {
    return _NUM_PATTERNS;
};

//// FIXME use the right kind of args to capture both dims
byte NeoPixelRing::getPatternNames(char *namePtrs[], byte number) {
    if (number < _NUM_PATTERNS) {
        return -1;
    }
    for (int i = 0; (i < _NUM_PATTERNS); i++) {
        namePtrs[i] = _patterns[i].name;
    }
    return _NUM_PATTERNS;
};

byte NeoPixelRing::getSelectedPattern() {
    return _patternNum;
};

bool NeoPixelRing::selectPattern(byte patternNum) {
    if (patternNum >= _NUM_PATTERNS) {
        return true;
    }
    _patternNum = patternNum;
    return false;
};
