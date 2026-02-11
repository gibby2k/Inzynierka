#include "DisplayManager.h"
#include "Config.h"
#include <TM1637Display.h>

TM1637Display display(PIN_DISP_CLK, PIN_DISP_DIO);

// Zmienne zegara
unsigned long lastTick = 0;
bool colonState = false;
int simHour = 12;
int simMinute = 0;
int simSecond = 0;

void initDisplay() {
    display.setBrightness(0x0f);
    // Test startowy
    uint8_t data[] = { 0xff, 0xff, 0xff, 0xff };
    display.setSegments(data);
    delay(500);
    display.clear();
    Serial.println("DisplayManager: TM1637 zainicjalizowany.");
}

void updateDisplayClock() {
    // 1. Aktualizacja czasu (symulacja)
    if (millis() - lastTick > 1000) {
        lastTick = millis();
        simSecond++;
        colonState = !colonState;

        if (simSecond >= 60) { simSecond = 0; simMinute++; }
        if (simMinute >= 60) { simMinute = 0; simHour++; }
        if (simHour >= 24)  { simHour = 0; }
    }

    // 2. Wyświetlanie (HH:MM) z migającym dwukropkiem
    int displayValue = (simHour * 100) + simMinute;
    if (colonState) {
        display.showNumberDecEx(displayValue, 0b01000000, true); 
    } else {
        display.showNumberDecEx(displayValue, 0, true);
    }
}

// --- PROSTE ODLICZANIE (00:SS) ---
void updateDisplayCountdown(int seconds) {
    // Zabezpieczenie
    if (seconds < 0) seconds = 0;
    
    // Formatowanie: Chcemy widzieć "00:10", "00:09" itd.
    // TM1637 przyjmuje format liczbowy, więc robimy (0 * 100) + sekundy
    int displayValue = seconds; 

    // showNumberDecEx(liczba, kropki, zera_wiodące)
    // 0b01000000 włącza dwukropek
    // true włącza zera wiodące (żeby było 00:09 a nie :9)
    display.showNumberDecEx(displayValue, 0b01000000, true);
}

void clearDisplay() {
    display.clear();
}