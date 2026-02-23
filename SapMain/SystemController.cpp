#include "SystemController.h"
#include "Config.h"
#include "SystemState.h"
#include "DsoManager.h"
#include "VentManager.h"
#include "LcdManager.h"
#include "SensorManager.h"
#include "AccessManager.h" 
#include <WiFi.h>
#include "SapNetwork.h"
#include "EventLogger.h"


extern void changeState(SystemState newState, String name, String zone, String source, String user);
extern String nazwyLinii[]; 
extern int nagraniaLinii[]; 
extern LcdManager lcdMgr;     
extern AccessManager accessMgr;
extern String currentZone, currentSource, currentUser;
extern SapNetworkManager netMgr;
extern EventLogger logger;


SystemController::SystemController() {
    timerStart = 0;
    lastBlink = 0;
    ledState = false;
    detectedLineId = -1;
    
    // Zmienne menu
    menuCursorIndex = 0;
    menuScrollOffset = 0;
    inSubMenu = false;
    subMenuCursor = 0;
}

void SystemController::begin() {
    pinMode(PIN_BUILTIN_LED, OUTPUT);
    digitalWrite(PIN_BUILTIN_LED, LOW);
    Serial.println("SystemController: Start logiki PPOZ.");
}

void SystemController::loop() {
        extern SystemState currentState;
        unsigned long now = millis();

        if (currentState == STATE_PRE_ALARM) {
            if (now - lastBlink > 500) {
                lastBlink = now;
                ledState = !ledState;
                digitalWrite(PIN_BUILTIN_LED, ledState ? HIGH : LOW);
            }
            if (now - timerStart > (timeT1 * 1000)) {
                activateFireAlarm();
            }
        else if (currentState == STATE_VERIFICATION) {
            digitalWrite(PIN_BUILTIN_LED, HIGH);
            if (now - timerStart > (timeT2 * 1000)) {
                activateFireAlarm();
            }
        }
        else if (currentState == STATE_IDLE) {
            digitalWrite(PIN_BUILTIN_LED, LOW);
        }
    }
}

void SystemController::handleInput(char key) {
    extern SystemState currentState;

    accessMgr.resetTimer();

    // --- 1. TRYB LOGOWANIA (Wpisywanie hasła) ---
    if (accessMgr.isLoginInProgress()) {
        if (key >= '0' && key <= '9') {
            accessMgr.addDigit(key);
            lcdMgr.showLoginScreen(accessMgr.getBufferMasked());
        } 
        else if (key == 'C') { // (C) ESC / DELETE
            if (accessMgr.getBufferMasked().length() > 0) {
                accessMgr.removeLastDigit(); 
                lcdMgr.showLoginScreen(accessMgr.getBufferMasked());
            } else {
                accessMgr.stopLoginSequence();
                // Jeśli anulujemy logowanie będąc w menu, wracamy do menu
                if (currentState == STATE_MENU) lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
                else changeState(currentState, currentZone, currentZone, currentSource, currentUser);
            }
        }
        else if (key == '#') { // (#) ENTER - Zatwierdź hasło
            bool ok = accessMgr.tryLogin();
            if (ok) {
                lcdMgr.showInfo("ZALOGOWANO", "POZIOM P" + String((int)accessMgr.getLevel()));
                logger.addEvent("LOGOWAN.", "P" + String((int)accessMgr.getLevel()));
                delay(1500);
                
                // Po udanym logowaniu wracamy do menu
                if (currentState == STATE_MENU) lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
                else changeState(currentState, currentZone, currentZone, currentSource, currentUser);
                
            } else {
                lcdMgr.showInfo("BLEDNE HASLO", "SPROBUJ PONOWNIE");
                delay(1500);
                accessMgr.startLoginSequence();
                lcdMgr.showLoginScreen("");
            }
        }
        return; 
    }

    // --- 2. TRYB MENU (Nawigacja) ---
    if (currentState == STATE_MENU) {
        if (inSubMenu) {
            // --- GLOBALNY PRZYCISK 'C' (WSTECZ) W PODMENU ---
            if (key == 'C') { 
                if (menuCursorIndex == 0 && isEditingConfig) {
                    // Odrzucenie zmian
                    extern uint16_t timeT1, timeT2;
                    extern Preferences configPrefs;
                    timeT1 = configPrefs.getUShort("t1", 30);
                    timeT2 = configPrefs.getUShort("t2", 60);
                    isEditingConfig = false;
                    lcdMgr.showConfigMenu(subMenuCursor, timeT1, timeT2);
                } else {
                    // Normalne wyjście z podmenu na listę
                    inSubMenu = false;
                    if (menuCursorIndex == 3) stopDso(); // Uciszamy DSO przy wyjściu
                    lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
                }
            }
            
            // --- Logika dla: 1. KONFIGURACJA ---
            else if (menuCursorIndex == 0) {
                extern uint16_t timeT1, timeT2;
                extern Preferences configPrefs;

                if (!isEditingConfig) {
                    // Lista wyboru (T1 lub T2) - podajemy timeT1 i timeT2 do rysowania
                    if (key == '2') { subMenuCursor = 0; lcdMgr.showConfigMenu(subMenuCursor, timeT1, timeT2); }
                    else if (key == '8') { subMenuCursor = 1; lcdMgr.showConfigMenu(subMenuCursor, timeT1, timeT2); }
                    else if (key == '#') { 
                        // Wejście w edycję konkretnego czasu
                        isEditingConfig = true; 
                        lcdMgr.showConfigEdit(subMenuCursor, subMenuCursor == 0 ? timeT1 : timeT2); 
                    }
                } else {
                    // Zmiana wartości wybranego czasu
                    uint16_t &currentVal = (subMenuCursor == 0) ? timeT1 : timeT2;
                    
                    if (key == '2') { // STRZAŁKA W GÓRĘ (+5 sek)
                        if (currentVal < 300) currentVal += 5; // Max 5 minut
                        lcdMgr.showConfigEdit(subMenuCursor, currentVal);
                    }
                    else if (key == '8') { // STRZAŁKA W DÓŁ (-5 sek)
                        if (currentVal > 10) currentVal -= 5;  // Min 10 sekund
                        lcdMgr.showConfigEdit(subMenuCursor, currentVal);
                    }
                    else if (key == '#') { // ZAPISZ
                        if (subMenuCursor == 0) configPrefs.putUShort("t1", timeT1);
                        else configPrefs.putUShort("t2", timeT2);
                        
                        logger.addEvent("KONFIG.", "ZMIANA"); 
                        
                        isEditingConfig = false;
                        lcdMgr.showInfo("ZAPISANO!", "NOWY CZAS");
                        delay(1000);
                        lcdMgr.showConfigMenu(subMenuCursor, timeT1, timeT2); // <--- ZMIANA TUTAJ
                    }
                }
            }

            // --- Logika dla: 2. PAMIEC ZDARZEN ---
            else if (menuCursorIndex == 1) {
                if (key == '2') { // STRZAŁKA W GÓRĘ -> Nowsze zdarzenie
                    if (subMenuCursor > 0) {
                        subMenuCursor--;
                        LogEvent ev;
                        logger.getEvent(subMenuCursor, ev);
                        lcdMgr.showEventLog(ev, subMenuCursor, logger.getCount());
                    }
                }
                else if (key == '8') { // STRZAŁKA W DÓŁ -> Starsze zdarzenie
                    if (subMenuCursor < logger.getCount() - 1) {
                        subMenuCursor++;
                        LogEvent ev;
                        logger.getEvent(subMenuCursor, ev);
                        lcdMgr.showEventLog(ev, subMenuCursor, logger.getCount());
                    }
                }
            }

            // --- Logika dla: 4. TEST DSO (Indeks 3) ---
            else if (menuCursorIndex == 3) { 
                if (key == '2') { // STRZAŁKA W GÓRĘ
                    subMenuCursor = 0; // OFF
                    lcdMgr.showTestDsoMenu(subMenuCursor);
                }
                else if (key == '8') { // STRZAŁKA W DÓŁ
                    subMenuCursor = 1; // ON
                    lcdMgr.showTestDsoMenu(subMenuCursor);
                }
                else if (key == '#') { // ENTER - Akcja!
                    if (subMenuCursor == 1) {
                        playDsoMessage(4); 
                        logger.addEvent("TEST DSO", "ON"); 
                    } else {
                        stopDso(); 
                        logger.addEvent("TEST DSO", "OFF"); 
                    }
                }
            }
            else if (menuCursorIndex == 4) { // logika dla 5. ZEGAR
                bool ntp = netMgr.isTimeSynced();
                
                if (!isEditingConfig) {
                    // Pozwalamy na edycję P3 ZAWSZE, nawet jeśli NTP działa (P3 ma priorytet)
                    if (key == '#') { 
                        if ((int)accessMgr.getLevel() < 3) {
                            lcdMgr.showInfo("BRAK UPRAWNIEN", "WYMAGANY POZ. P3");
                            delay(1500);
                            lcdMgr.showClockEdit(-1, editHour, editMinute, netMgr.isTimeSynced());
                        } else {
                            isEditingConfig = true;
                            clockPartIndex = 0; 
                            lcdMgr.showClockEdit(clockPartIndex, editHour, editMinute, netMgr.isTimeSynced());
                        }
                    }
                } else {
                    // Jesteśmy w trybie edycji (tylko P3 i BRAK NTP)
                    if (key == '2') { // GÓRA
                        if (clockPartIndex == 0) editHour = (editHour + 1) % 24;
                        else editMinute = (editMinute + 1) % 60;
                        lcdMgr.showClockEdit(clockPartIndex, editHour, editMinute, ntp);
                    }
                    else if (key == '8') { // DÓŁ
                        if (clockPartIndex == 0) editHour = (editHour == 0) ? 23 : editHour - 1;
                        else editMinute = (editMinute == 0) ? 59 : editMinute - 1;
                        lcdMgr.showClockEdit(clockPartIndex, editHour, editMinute, ntp);
                    }
                    else if (key == '4' || key == '6') { // Zmiana między godziną a minutą
                        clockPartIndex = (clockPartIndex == 0) ? 1 : 0;
                        lcdMgr.showClockEdit(clockPartIndex, editHour, editMinute, ntp);
                    }
                    if (key == '#') { // ZATWIERDŹ
                        struct tm tm;
                        tm.tm_year = 2026 - 1900; // Ustawiamy stały rok Twojej pracy inż.
                        tm.tm_mon = 1; tm.tm_mday = 23; 
                        tm.tm_hour = editHour;
                        tm.tm_min = editMinute;
                        tm.tm_sec = 0;
                        time_t t = mktime(&tm);
                        struct timeval now = { .tv_sec = t };
                        settimeofday(&now, NULL); 
                        
                        // --- KLUCZOWY ZAPIS DO PAMIĘCI ---
                        extern Preferences configPrefs;
                        configPrefs.putInt("h", editHour);
                        configPrefs.putInt("m", editMinute);
                        
                        logger.addEvent("ZEGAR", "USTAW.");
                        isEditingConfig = false;
                        lcdMgr.showInfo("ZAPISANO CZAS", "W PAMIECI NVS");
                        delay(1500);
                        lcdMgr.showClockEdit(-1, editHour, editMinute, netMgr.isTimeSynced());
                    }
                }
            }
            
            // --- Logika dla: 6. INFO SIEC (Indeks 5) ---
            else if (menuCursorIndex == 5) { 
                if (key == '2') { // STRZAŁKA W GÓRĘ -> Strona 1
                    subMenuCursor = 0; 
                    lcdMgr.showInfoNetwork(subMenuCursor, WiFi.localIP().toString(), WiFi.RSSI(), WiFi.status() == WL_CONNECTED, netMgr.isMqttConnected());
                }
                else if (key == '8') { // STRZAŁKA W DÓŁ -> Strona 2
                    subMenuCursor = 1; 
                    lcdMgr.showInfoNetwork(subMenuCursor, WiFi.localIP().toString(), WiFi.RSSI(), WiFi.status() == WL_CONNECTED, netMgr.isMqttConnected());
                }
            }
            
        } else {
            // Nawigacja po głównej liście Menu
            if (key == '2') { // STRZAŁKA W GÓRĘ
                if (menuCursorIndex > 0) {
                    menuCursorIndex--;
                    if (menuCursorIndex < menuScrollOffset) menuScrollOffset = menuCursorIndex;
                    lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
                }
            } 
            else if (key == '8') { // STRZAŁKA W DÓŁ
                if (menuCursorIndex < MENU_MAX_ITEMS - 1) {
                    menuCursorIndex++;
                    if (menuCursorIndex >= menuScrollOffset + 2) menuScrollOffset = menuCursorIndex - 1;
                    lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
                }
            }
            else if (key == 'C') { // (C) ESC - WYJŚCIE Z MENU DO EKRANU GŁÓWNEGO
                exitMenu();
            }
            else if (key == '#') { // (#) ENTER - WEJŚCIE W OPCJĘ
                inSubMenu = true;
                executeMenuAction(); 
            }
        }
        return; 
    }

    // --- 3. TRYB NORMALNY (Czuwanie/Alarm) ---
    if (key == 'A') {  // (A) ACK (Potwierdzenie)
        if (accessMgr.canAck()) onAckPressed();
    }
    else if (key == 'B') {  // (B) RESET
        if (accessMgr.canReset()) {
            onResetPressed();
        } else {
            accessMgr.startLoginSequence();
            lcdMgr.showLoginScreen(""); 
        }
    }
    else if (key == 'D') {  // (D) MENU
        if (accessMgr.getLevel() >= LEVEL_USER) { 
            enterMenu();
        } else {
            accessMgr.startLoginSequence();
            lcdMgr.showLoginScreen(""); 
        }
    }
    else if (key == '*') {  // (*) WYLOGUJ (Szybkie wylogowanie)
        if (accessMgr.getLevel() > LEVEL_GUEST) {
            accessMgr.logout();
            logger.addEvent("WYLOG.", "P1"); 
            lcdMgr.showInfo("WYLOGOWANO", "POZIOM P1");
            delay(1500);
            changeState(currentState, currentZone, currentZone, currentSource, currentUser);
        }
    }
}

void SystemController::onSensorSignal(int lineId) {
    extern SystemState currentState;
    if (currentState == STATE_IDLE || currentState == STATE_MENU) {
        detectedLineId = lineId;
        timerStart = millis();
        inSubMenu = false; // Resetujemy na wypadek ewakuacji z menu
        changeState(STATE_PRE_ALARM, "ALARM WSTEPNY", nazwyLinii[lineId], "CZUJKA", "AUTO");
    }
}

void SystemController::onAckPressed() {
    extern SystemState currentState;
    if (currentState == STATE_PRE_ALARM) {
        timerStart = millis();
        changeState(STATE_VERIFICATION, "WERYFIKACJA", nazwyLinii[detectedLineId], "OPERATOR", "FIZ");
    }
}

void SystemController::onResetPressed() {
    extern SystemState currentState;
    if (currentState == STATE_VERIFICATION || currentState == STATE_FIRE_ALARM) {
        resetSystem();
    }
}

void SystemController::activateFireAlarm() {
    changeState(STATE_FIRE_ALARM, "POZAR !!!", nazwyLinii[detectedLineId], "SYSTEM", "AUTO");
    setVent(true);
    playDsoMessage(nagraniaLinii[detectedLineId]);
    digitalWrite(PIN_BUILTIN_LED, LOW);
}

void SystemController::resetSystem() {
    setVent(false);
    stopDso();
    digitalWrite(PIN_BUILTIN_LED, LOW);
    resetSensorLatches();
    logger.addEvent("RESET", "SYSTEMU"); // Wyświetli: "05.RESET SYSTEMU"
    changeState(STATE_IDLE, "CZUWANIE", "CALY_OBIEKT", "SYSTEM", "AUTO");
}

int SystemController::getRemainingTime() {
    extern SystemState currentState;
    unsigned long duration = 0;
    if (currentState == STATE_PRE_ALARM) duration = (timeT1 * 1000);
    else if (currentState == STATE_VERIFICATION) duration = (timeT2 * 1000);
    else return 0;
    
    unsigned long passed = millis() - timerStart;
    if (passed >= duration) return 0;
    return (duration - passed) / 1000;
}

void SystemController::enterMenu() {
    menuCursorIndex = 0;
    menuScrollOffset = 0;
    inSubMenu = false;
    changeState(STATE_MENU, "MENU_SYSTEMOWE", "MENU", "OPERATOR", "FIZ");
    lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
}

void SystemController::exitMenu() {
    changeState(STATE_IDLE, "CZUWANIE", "CALY_OBIEKT", "SYSTEM", "AUTO");
}

void SystemController::executeMenuAction() {
    subMenuCursor = 0; 
    
    switch (menuCursorIndex) {
        case 0:
            // 1.KONFIGURACJA (Tylko P3)
            if ((int)accessMgr.getLevel() < 3) {
                lcdMgr.showInfo("BRAK UPRAWNIEN", "WYMAGANY POZ. P3");
                delay(1500);
                inSubMenu = false;
                
                // Odpalamy okienko logowania!
                accessMgr.startLoginSequence();
                lcdMgr.showLoginScreen("");
            } else {
                extern uint16_t timeT1, timeT2;
                isEditingConfig = false;
                subMenuCursor = 0;
                lcdMgr.showConfigMenu(subMenuCursor, timeT1, timeT2); 
            }
            break;
            
        case 1:
            // 2.PAMIETNIK
            if (logger.getCount() == 0) {
                lcdMgr.showInfo("PAMIETNIK", "BRAK WPISOW...");
                delay(1500);
                inSubMenu = false;
                lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
            } else {
                LogEvent ev;
                logger.getEvent(subMenuCursor, ev);
                lcdMgr.showEventLog(ev, subMenuCursor, logger.getCount());
            }
            break;
            
        case 2:
            // 3.RESET PAM.ZD. (Tylko P3)
            if ((int)accessMgr.getLevel() < 3) {
                lcdMgr.showInfo("BRAK UPRAWNIEN", "WYMAGANY POZ. P3");
                delay(1500);
                inSubMenu = false;
                
                // Odpalamy okienko logowania!
                accessMgr.startLoginSequence();
                lcdMgr.showLoginScreen("");
            } else {
                lcdMgr.showInfo("FORMATOWANIE...", "PROSZE CZEKAC");
                logger.clearLogs();
                delay(1000);
                logger.addEvent("FORMAT", "SERWIS"); 
                lcdMgr.showInfo("PAMIETNIK", "WYCZYSZCZONY!");
                delay(1500);
                lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
                inSubMenu = false;
            }
            break;
            
        case 3:
            // 4.TEST DSO - To było wcześniej pod case 2!
            lcdMgr.showTestDsoMenu(subMenuCursor); 
            break;
            
        case 4: // 5. ZEGAR (Tylko P3)
            if ((int)accessMgr.getLevel() < 3) {
                // Jeśli poziom jest za niski (P1 lub P2)
                lcdMgr.showInfo("BRAK UPRAWNIEN", "WYMAGANY POZ. P3");
                delay(1500);
                inSubMenu = false; // Nie wchodzimy jeszcze do podmenu
                
                // Wywołujemy procedurę logowania
                accessMgr.startLoginSequence();
                lcdMgr.showLoginScreen("");
            } else {
                // Jeśli poziom to P3, wchodzimy prosto do zegara
                struct tm timeinfo;
                bool ntp = netMgr.isTimeSynced();
                
                if(!getLocalTime(&timeinfo, 10)) {
                    editHour = 12;
                    editMinute = 0;
                } else {
                    editHour = timeinfo.tm_hour;
                    editMinute = timeinfo.tm_min;
                }
                
                clockPartIndex = 0;
                isEditingConfig = false; // Startujemy w trybie podglądu (strzałka niewidoczna)
                lcdMgr.showClockEdit(-1, editHour, editMinute, ntp);
            }
            break;
            
        case 5:
            // 6.INFO SIEC - To było wcześniej pod case 4!
            lcdMgr.showInfoNetwork(subMenuCursor, WiFi.localIP().toString(), WiFi.RSSI(), WiFi.status() == WL_CONNECTED, netMgr.isMqttConnected());
            break;
    }
}
