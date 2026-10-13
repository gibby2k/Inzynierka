#ifndef SYSTEM_CONTROLLER_H
#define SYSTEM_CONTROLLER_H

#include <Arduino.h>
#include "SystemState.h"
#include "AccessManager.h"

// --- KONFIGURACJA CZASÓW ---
// #define TIME_T1_PRE_ALARM  20000 // T1 = 20 sekund
// #define TIME_T2_VERIFY     40000 // T2 = 40 sekund

extern uint16_t timeT1; // czas w sekundach
extern uint16_t timeT2; // czas w sekundach

#define PIN_BUILTIN_LED 2

class SystemController {
  private:
    unsigned long timerStart;
    unsigned long lastBlink;
    bool ledState;
    int detectedLineId;

    // --- ZMIENNE DO MENU (Tylko raz!) ---
    int menuCursorIndex;
    int menuScrollOffset;
    bool inSubMenu;
    int subMenuCursor; 
    const int MENU_MAX_ITEMS = 6;

    void activateFireAlarm();
    void resetSystem();

    // --- METODY DO MENU ---
    void enterMenu();
    // void exitMenu();
    void executeMenuAction();

    bool isEditingConfig;

    int editHour = 12;
    int editMinute = 0;
    int clockPartIndex = 0; // 0 = godziny, 1 = minuty

  public:
    SystemController();
    void begin();
    void loop();
    void onSensorSignal(int lineId);
    void onAckPressed();
    void onResetPressed();
    void handleInput(char key); 
    int getRemainingTime();
    void exitMenu();
    
    int getMenuCursor() { 
        // Dodajemy +1, aby kursor na wizualizacji miał logiczne wartości 1-6 (zamiast 0-5)
        if (inSubMenu) return subMenuCursor + 1; 
        return menuCursorIndex + 1; 
    } 
    
    int getMenuPage() { 
        // 0 = Główne menu
        if (!inSubMenu) return 0; 
        
        // Jeśli jesteśmy w podmenu, zgadujemy ekran na podstawie tego, w co weszliśmy:
        if (menuCursorIndex == 2) return 2; // "TESTY" -> Ekran 2
        if (menuCursorIndex == 3) return 4; // "TRYB PRACY/KONFIGURACJA" (Czasy T1/T2) -> Ekran 4
        if (menuCursorIndex == 4) return 1; // "CZAS/DATA" -> Ekran 1
        if (menuCursorIndex == 5) return 3; // "INFORMACJE" -> Ekran 3
        
        return 99; // Inne / Tryb edycji
    }
};
#endif