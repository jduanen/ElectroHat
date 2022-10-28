/*
* NeoPixel LED Ring library
*/

NeoPixelRing::NeoPixelRing() {
    _create(DEF_LED_BRIGHTNESS);
}

NeoPixelRing::NeoPixelRing(byte brightness) {
    _create(brightness);
}

void NeoPixelRing::_create(byte brightness) {
    _ring->begin();
    _ring->clear();
    _ring->show();  // turn off all pixels
    _ring->setBrightness(brightness);
    _ring->setPixelColor(1, 255, 0, 255);
}

void NeoPixelRing::test(byte test) {
    uint32_t c;

    switch (test) {
    case LED_TEST_ALL_WHITE:
        c = _ring->Color(255, 255, 255);
        Serial.println("WHT: 0x" + String(c, HEX));
        _ring->fill(c, 0, LED_COUNT);
        Serial.println("XXX");
        _ring->show();
        Serial.println("YYY");
        break;
    case LED_TEST_ALL_RED:
        Serial.println("RED");
        _ring->fill(_ring->Color(255, 0, 0), 0, LED_COUNT);
        _ring->show();
        break;
    case LED_TEST_ALL_GREEN:
        Serial.println("GRN");
        _ring->fill(_ring->Color(0, 255, 0), 0, LED_COUNT);
        _ring->show();
        break;
    case LED_TEST_ALL_BLUE:
        Serial.println("BLU");
        _ring->fill(_ring->Color(0, 0, 255), 0, LED_COUNT);
        _ring->show();
        break;
    case LED_TEST_ALL_OFF:
        Serial.println("OFF");
        _ring->clear();
        _ring->show();
        break;
    case LED_TEST_ALL_BLINK:
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
    Serial.println("ZZZ");
};
