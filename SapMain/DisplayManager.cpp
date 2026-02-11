#include "DisplayManager.h"
#include "Config.h"
#include <TM1637Display.h>

// Inicjalizacja obiektu biblioteki
TM1637Display display(PIN_DISP_CLK, PIN_DISP_DIO);

// Zmienne do symulacji zegara
unsigned long lastTick = 0;
bool colonState = false; // Do migania dwukropkiem

// Startujemy od godziny 12:00:00
int simHour = 12;
int simMinute = 0;
int simSecond = 0;

void initDisplay() {
    display.setBrightness(0x0f); // Maksymalna jasność (0x0f)
    
    // Test startowy: 88:88
    uint8_t data[] = { 0xff, 0xff, 0xff, 0xff };
    display.setSegments(data);
    delay(500);
    display.clear();
    
    Serial.println("DisplayManager: TM1637 zainicjalizowany (Start 12:00).");
}

void updateDisplayClock() {
    // 1. Logika upływu czasu (symulacja RTC)
    // Wykonuje się co 1000ms (1 sekunda)
    if (millis() - lastTick > 1000) {
        lastTick = millis();
        simSecond++;
        colonState = !colonState; // Zmień stan dwukropka (migaj)

        // Przeliczanie czasu
        if (simSecond >= 60) {
            simSecond = 0;
            simMinute++;
        }
        if (simMinute >= 60) {
            simMinute = 0;
            simHour++;
        }
        if (simHour >= 24) {
            simHour = 0;
        }
    }

    // 2. Formatowanie do wyświetlenia (HHMM)
    int displayValue = (simHour * 100) + simMinute;

    // 3. Wyświetlenie z migającym dwukropkiem
    // 0b01000000 (lub 0x40) włącza środkowy dwukropek w większości modułów TM1637
    if (colonState) {
        display.showNumberDecEx(displayValue, 0b01000000, true); 
    } else {
        display.showNumberDecEx(displayValue, 0, true);
    }
}

void clearDisplay() {
    display.clear();
}