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

// --- ZMIENNE DO MENU ---
    int menuCursorIndex;
    int menuScrollOffset;
    bool inSubMenu;
    bool inSubSubMenu; // <--- NOWA ZMIENNA (do zagnieżdżenia ON/OFF w testach)
    int subMenuCursor; 
    int subSubMenuCursor; // <--- NOWA ZMIENNA
    const int MENU_MAX_ITEMS = 7; // <--- ZMIANA Z 6 NA 7

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
    
    // --- SYSTEM POPUP DLA WIZUALIZACJI ---
    void showPopup(String line1, String line2);
    bool getPopupActive() { return popupActive; }
    String getPopupL1() { return popupL1; }
    String getPopupL2() { return popupL2; }

    // --- POBIERANIE DANYCH DLA JSON ---
    int getMenuSubCursor() { return subMenuCursor; }
    int getEditHour() { return editHour; }
    int getEditMinute() { return editMinute; }
    int getClockPartIndex() { return clockPartIndex; }

    int getMenuCursor() { 
        if (inSubSubMenu) return subSubMenuCursor + 1; 
        if (inSubMenu) return subMenuCursor + 1; 
        return menuCursorIndex + 1; 
    } 
    
    int getMenuPage() { 
        if (!inSubMenu) return 0; 
        if (menuCursorIndex == 0) return isEditingConfig ? 11 : 1; 
        if (menuCursorIndex == 1) return 2; 
        if (menuCursorIndex == 2) return inSubSubMenu ? 30 : 3; 
        if (menuCursorIndex == 3) return 4; 
        if (menuCursorIndex == 4) return 40; 
        if (menuCursorIndex == 5) return isEditingConfig ? 51 : 5; 
        if (menuCursorIndex == 6) return 6; 
        return 99; 
    }

  private: // (Przenosimy flagi Popup na dół)
    bool popupActive = false;
    String popupL1 = "";
    String popupL2 = "";
};
#endif