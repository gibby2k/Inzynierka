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
extern String currentZone, currentSource, currentUser, currentName;
extern SapNetworkManager netMgr;
extern EventLogger logger;
extern String actionSource;
extern bool ntpEnabled;

SystemController::SystemController() {
    timerStart = 0;
    lastBlink = 0;
    ledState = false;
    detectedLineId = -1;
    
    menuCursorIndex = 0;
    menuScrollOffset = 0;
    inSubMenu = false;
    inSubSubMenu = false; 
    subMenuCursor = 0;
    subSubMenuCursor = 0;
}

void SystemController::begin() {
    pinMode(PIN_BUILTIN_LED, OUTPUT);
    digitalWrite(PIN_BUILTIN_LED, LOW);
    Serial.println("SystemController: Start logiki PPOZ.");
}

// NOWA FUNKCJA - Wirtualny POPUP sprzężony z WWW
void SystemController::showPopup(String line1, String line2) {
    popupActive = true;
    popupL1 = line1;
    popupL2 = line2;
    
    extern SystemState currentState;
    netMgr.publishStatus(currentState, currentName, currentZone, currentSource, currentUser);
    
    lcdMgr.showInfo(line1, line2);
    delay(1500);
    
    popupActive = false;
    netMgr.publishStatus(currentState, currentName, currentZone, currentSource, currentUser);
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

    // --- NOWY KOD: AUTOMATYCZNE ODŚWIEŻANIE ZEGARA W MENU ---
    extern bool ntpEnabled;
    if (currentState == STATE_MENU && inSubMenu && menuCursorIndex == 5 && !isEditingConfig && !popupActive) {
        static unsigned long lastMenuClockUpdate = 0;
        if (now - lastMenuClockUpdate > 1000) { // Skanowanie czasu co 1 sekundę
            lastMenuClockUpdate = now;
            struct tm timeinfo;
            // Pobieramy aktualny wewnętrzny czas ESP32 (bez blokowania pętli)
            if(getLocalTime(&timeinfo, 0)) {
                // Jeśli zmieniła się godzina lub minuta, aktualizujemy ekran i zmienne
                if (editHour != timeinfo.tm_hour || editMinute != timeinfo.tm_min) {
                    editHour = timeinfo.tm_hour;
                    editMinute = timeinfo.tm_min;
                    
                    // Odświeżenie fizycznego LCD
                    lcdMgr.showZegarMenu(subMenuCursor, ntpEnabled, editHour, editMinute);
                    
                    // (Zmiana zmiennych editHour i editMinute sprawi, że najbliższy
                    //  pakiet MQTT do panelu WWW automatycznie prześle nową godzinę)
                }
            }
        }
    }
}

void SystemController::handleInput(char key) {
    extern SystemState currentState;
    accessMgr.resetTimer();

    if (accessMgr.isLoginInProgress()) {
        if (key >= '0' && key <= '9') {
            accessMgr.addDigit(key);
            lcdMgr.showLoginScreen(accessMgr.getBufferMasked());
        } 
        else if (key == 'C') { 
            if (accessMgr.getBufferMasked().length() > 0) {
                accessMgr.removeLastDigit(); 
                lcdMgr.showLoginScreen(accessMgr.getBufferMasked());
            } else {
                accessMgr.stopLoginSequence();
                if (currentState == STATE_MENU) lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
                else changeState(currentState, currentName, currentZone, actionSource, "P" + String((int)accessMgr.getLevel()));
            }
        }
        else if (key == '#') { 
            bool ok = accessMgr.tryLogin();
            if (ok) {
                logger.addEvent("LOGOWAN.", "P" + String((int)accessMgr.getLevel()));
                showPopup("ZALOGOWANO", "POZIOM P" + String((int)accessMgr.getLevel()));
                
                if (currentState == STATE_MENU) lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
                else changeState(currentState, currentName, currentZone, actionSource, "P" + String((int)accessMgr.getLevel()));
            } else {
                showPopup("BLEDNE HASLO", "SPROBUJ PONOWNIE");
                accessMgr.startLoginSequence();
                lcdMgr.showLoginScreen("");
            }
        }
        return; 
    }

    if (currentState == STATE_MENU) {
        if (inSubSubMenu) {
            if (key == 'C') { 
                inSubSubMenu = false;
                stopDso(); setVent(false);
                lcdMgr.showTestowanieMenu(subMenuCursor);
            }
            else if (menuCursorIndex == 2) { 
                if (key == '2') { subSubMenuCursor = 0; lcdMgr.showTestOnOffMenu(subMenuCursor == 0 ? "TEST DSO" : "TEST VENT", subSubMenuCursor); }
                else if (key == '8') { subSubMenuCursor = 1; lcdMgr.showTestOnOffMenu(subMenuCursor == 0 ? "TEST DSO" : "TEST VENT", subSubMenuCursor); }
                else if (key == '#') {
                    if (subMenuCursor == 0) { 
                        if (subSubMenuCursor == 1) { playDsoMessage(4); logger.addEvent("TEST DSO", "ON"); showPopup("TEST DSO", "WLACZONY (ON)"); } 
                        else { stopDso(); logger.addEvent("TEST DSO", "OFF"); showPopup("TEST DSO", "WYLACZONY (OFF)"); }
                    } else { 
                        if (subSubMenuCursor == 1) { setVent(true); logger.addEvent("TEST VEN.", "ON"); showPopup("TEST WENT.", "WLACZONY (ON)"); } 
                        else { setVent(false); logger.addEvent("TEST VEN.", "OFF"); showPopup("TEST WENT.", "WYLACZONY (OFF)"); }
                    }
                    
                    // UWAGA: Usunąłem tutaj 'inSubSubMenu = false;' 
                    // Dzięki temu zostajemy w podmenu ON/OFF po aktywacji!
                    lcdMgr.showTestOnOffMenu(subMenuCursor == 0 ? "TEST DSO" : "TEST VENT", subSubMenuCursor);
                }
            }
        }
        else if (inSubMenu) {
            if (key == 'C') { 
                if (menuCursorIndex == 0 && isEditingConfig) {
                    extern uint16_t timeT1, timeT2;
                    extern Preferences configPrefs;
                    timeT1 = configPrefs.getUShort("t1", 30); timeT2 = configPrefs.getUShort("t2", 60);
                    isEditingConfig = false;
                    lcdMgr.showConfigMenu(subMenuCursor, timeT1, timeT2);
                } else {
                    inSubMenu = false;
                    stopDso(); setVent(false);
                    lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
                }
            }
            else if (menuCursorIndex == 0) { 
                extern uint16_t timeT1, timeT2; extern Preferences configPrefs;
                if (!isEditingConfig) {
                    if (key == '2') { subMenuCursor = 0; lcdMgr.showConfigMenu(subMenuCursor, timeT1, timeT2); }
                    else if (key == '8') { subMenuCursor = 1; lcdMgr.showConfigMenu(subMenuCursor, timeT1, timeT2); }
                    else if (key == '#') { isEditingConfig = true; lcdMgr.showConfigEdit(subMenuCursor, subMenuCursor == 0 ? timeT1 : timeT2); }
                } else {
                    uint16_t &currentVal = (subMenuCursor == 0) ? timeT1 : timeT2;
                    if (key == '2') { if (currentVal < 300) currentVal += 5; lcdMgr.showConfigEdit(subMenuCursor, currentVal); }
                    else if (key == '8') { if (currentVal > 10) currentVal -= 5; lcdMgr.showConfigEdit(subMenuCursor, currentVal); }
                    else if (key == '#') { 
                        if (subMenuCursor == 0) configPrefs.putUShort("t1", timeT1); else configPrefs.putUShort("t2", timeT2);
                        logger.addEvent("KONFIG.", "ZMIANA"); 
                        isEditingConfig = false;
                        showPopup("ZAPISANO!", "NOWY CZAS T" + String(subMenuCursor + 1));
                        lcdMgr.showConfigMenu(subMenuCursor, timeT1, timeT2); 
                    }
                }
            }
            else if (menuCursorIndex == 1) { 
                if (key == '2') { subMenuCursor = 0; lcdMgr.showBlokowanieMenu(subMenuCursor, isLineBlocked()); }
                else if (key == '8') { subMenuCursor = 1; lcdMgr.showBlokowanieMenu(subMenuCursor, isLineBlocked()); }
                else if (key == '#') { 
                    bool newState = (subMenuCursor == 1);
                    setLineBlocked(newState); 
                    logger.addEvent("BLOKADA", newState ? "ON" : "OFF"); 
                    showPopup("BLOKOWANIE", newState ? "ZAKTYWOWANO" : "WYLACZONO");
                    lcdMgr.showBlokowanieMenu(subMenuCursor, isLineBlocked());
                }
            }
            else if (menuCursorIndex == 2) { 
                if (key == '2') { subMenuCursor = 0; lcdMgr.showTestowanieMenu(subMenuCursor); }
                else if (key == '8') { subMenuCursor = 1; lcdMgr.showTestowanieMenu(subMenuCursor); }
                else if (key == '#') { 
                    inSubSubMenu = true; subSubMenuCursor = 0;
                    lcdMgr.showTestOnOffMenu(subMenuCursor == 0 ? "TEST DSO" : "TEST VENT", subSubMenuCursor);
                }
            }
            else if (menuCursorIndex == 3) { 
                if (key == '2') { 
                    if (subMenuCursor > 0) {
                        subMenuCursor--;
                        LogEvent ev; logger.getEvent(subMenuCursor, ev);
                        lcdMgr.showEventLog(ev, subMenuCursor, logger.getCount());
                    }
                }
                else if (key == '8') { 
                    if (subMenuCursor < logger.getCount() - 1) {
                        subMenuCursor++;
                        LogEvent ev; logger.getEvent(subMenuCursor, ev);
                        lcdMgr.showEventLog(ev, subMenuCursor, logger.getCount());
                    }
                }
            }
            else if (menuCursorIndex == 4) { 
                if (key == '2' || key == '4' || key == '6' || key == '8') { 
                    subMenuCursor = (subMenuCursor == 0) ? 1 : 0;
                    lcdMgr.showResetConfirm(subMenuCursor);
                }
                else if (key == '#') { 
                    if (subMenuCursor == 1) { 
                        showPopup("FORMATOWANIE...", "PROSZE CZEKAC");
                        logger.clearLogs();
                        logger.addEvent("FORMAT", "SERWIS"); 
                        showPopup("PAMIETNIK", "WYCZYSZCZONY!");
                    }
                    inSubMenu = false;
                    lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
                }
            }
            else if (menuCursorIndex == 5) { 
                if (!isEditingConfig) {
                    if (key == '2') { subMenuCursor = 0; lcdMgr.showZegarMenu(subMenuCursor, ntpEnabled, editHour, editMinute); }
                    else if (key == '8') { subMenuCursor = 1; lcdMgr.showZegarMenu(subMenuCursor, ntpEnabled, editHour, editMinute); }
                    else if (key == '#') { 
                        if (subMenuCursor == 0) { // Przełączanie TAK/NIE dla NTP
                            ntpEnabled = !ntpEnabled;
                            extern Preferences configPrefs;
                            configPrefs.putBool("ntp", ntpEnabled);
                            
                            if (ntpEnabled) {
                                showPopup("SYNC NTP...", "PROSZE CZEKAC");
                                configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
                                delay(2000);
                                showPopup("CZAS SYNCH.", "OK!");
                                logger.addEvent("ZEGAR", "NTP ON");
                                
                                struct tm timeinfo;
                                if(getLocalTime(&timeinfo, 10)) { editHour = timeinfo.tm_hour; editMinute = timeinfo.tm_min; }
                            } else {
                                showPopup("SYNC NTP", "WYLACZONA");
                                logger.addEvent("ZEGAR", "NTP OFF");
                            }
                            lcdMgr.showZegarMenu(subMenuCursor, ntpEnabled, editHour, editMinute);
                        } 
                        else if (subMenuCursor == 1) { // Wejście w ręczną edycję
                            if (ntpEnabled) {
                                showPopup("ZABLOKOWANE", "WYLACZ SYNC NTP");
                                lcdMgr.showZegarMenu(subMenuCursor, ntpEnabled, editHour, editMinute);
                            } else {
                                isEditingConfig = true; clockPartIndex = 0; 
                                lcdMgr.showClockEdit(clockPartIndex, editHour, editMinute); // <-- POPRAWIONE (3 arg)
                            }
                        }
                    }
                    else if (key == 'A') { // --- PRZYCISK: WYMUSZENIE SYNC NTP ---
                        showPopup("SYNC NTP...", "PROSZE CZEKAC");
                        configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
                        delay(2000);
                        showPopup("SYNC NTP", "ZAKONCZONA!");
                        logger.addEvent("ZEGAR", "SYNC NTP");
                        
                        struct tm timeinfo;
                        if(getLocalTime(&timeinfo, 10)) { editHour = timeinfo.tm_hour; editMinute = timeinfo.tm_min; }
                        lcdMgr.showZegarMenu(subMenuCursor, ntpEnabled, editHour, editMinute); // <-- POPRAWIONE (wraca do menu Zegara)
                    }
                } else { // TRYB EDYCJI RECZNEJ
                    if (key == '2') { 
                        if (clockPartIndex == 0) editHour = (editHour + 1) % 24; else editMinute = (editMinute + 1) % 60;
                        lcdMgr.showClockEdit(clockPartIndex, editHour, editMinute); // <-- POPRAWIONE (3 arg)
                    }
                    else if (key == '8') { 
                        if (clockPartIndex == 0) editHour = (editHour == 0) ? 23 : editHour - 1; else editMinute = (editMinute == 0) ? 59 : editMinute - 1;
                        lcdMgr.showClockEdit(clockPartIndex, editHour, editMinute); // <-- POPRAWIONE (3 arg)
                    }
                    else if (key == '4' || key == '6') { 
                        clockPartIndex = (clockPartIndex == 0) ? 1 : 0;
                        lcdMgr.showClockEdit(clockPartIndex, editHour, editMinute); // <-- POPRAWIONE (3 arg)
                    }
                    if (key == '#') { 
                        struct tm tm;
                        tm.tm_year = 2026 - 1900; tm.tm_mon = 1; tm.tm_mday = 23; 
                        tm.tm_hour = editHour; tm.tm_min = editMinute; tm.tm_sec = 0;
                        tm.tm_isdst = -1; // Zabezpieczenie strefy czasowej
                        time_t t = mktime(&tm); struct timeval now = { .tv_sec = t }; settimeofday(&now, NULL); 
                        
                        extern Preferences configPrefs;
                        configPrefs.putInt("h", editHour); configPrefs.putInt("m", editMinute);
                        
                        logger.addEvent("ZEGAR", "USTAW.");
                        isEditingConfig = false;
                        showPopup("ZAPISANO CZAS", "W PAMIECI NVS");
                        lcdMgr.showZegarMenu(subMenuCursor, ntpEnabled, editHour, editMinute); // <-- POPRAWIONE
                    }
                }
            }
            else if (menuCursorIndex == 6) { 
                if (key == '2') { subMenuCursor = 0; lcdMgr.showInfoNetwork(subMenuCursor, WiFi.localIP().toString(), WiFi.RSSI(), WiFi.status() == WL_CONNECTED, netMgr.isMqttConnected()); }
                else if (key == '8') { subMenuCursor = 1; lcdMgr.showInfoNetwork(subMenuCursor, WiFi.localIP().toString(), WiFi.RSSI(), WiFi.status() == WL_CONNECTED, netMgr.isMqttConnected()); }
            }
        } 
        else {
            if (key == '2') { 
                if (menuCursorIndex > 0) {
                    menuCursorIndex--;
                    if (menuCursorIndex < menuScrollOffset) menuScrollOffset = menuCursorIndex;
                    lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
                }
            } 
            else if (key == '8') { 
                if (menuCursorIndex < MENU_MAX_ITEMS - 1) {
                    menuCursorIndex++;
                    if (menuCursorIndex >= menuScrollOffset + 2) menuScrollOffset = menuCursorIndex - 1;
                    lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
                }
            }
            else if (key == 'C') { exitMenu(); }
            else if (key == '#') { 
                inSubMenu = true;
                executeMenuAction(); 
            }
        }
        return; 
    }

    if (key == 'A') {  
        if (accessMgr.canAck()) onAckPressed();
    }
    else if (key == 'B') {  
        if (accessMgr.canReset()) {
            onResetPressed();
        } else {
            accessMgr.startLoginSequence();
            lcdMgr.showLoginScreen(""); 
        }
    }
    else if (key == 'D') {  
        if (accessMgr.getLevel() >= LEVEL_USER) { 
            enterMenu();
        } else {
            accessMgr.startLoginSequence();
            lcdMgr.showLoginScreen(""); 
        }
    }
    else if (key == '*') {  
        if (accessMgr.getLevel() > LEVEL_GUEST) {
            accessMgr.logout();
            logger.addEvent("WYLOG.", "P1"); 
            showPopup("WYLOGOWANO", "POZIOM P1");
            changeState(currentState, currentName, currentZone, actionSource, "P1");
        }
    }
}

void SystemController::onSensorSignal(int lineId) {
    extern SystemState currentState;
    if (currentState == STATE_IDLE || currentState == STATE_MENU) {
        detectedLineId = lineId; timerStart = millis();
        inSubMenu = false; inSubSubMenu = false; 
        changeState(STATE_PRE_ALARM, "ALARM WSTEPNY", nazwyLinii[lineId], "FIZYCZNIE", "SYSTEM");
    }
}

void SystemController::onAckPressed() {
    extern SystemState currentState;
    if (currentState == STATE_PRE_ALARM) {
        timerStart = millis();
        changeState(STATE_VERIFICATION, "WERYFIKACJA", nazwyLinii[detectedLineId], actionSource, "P" + String((int)accessMgr.getLevel()));
    }
}

void SystemController::onResetPressed() {
    extern SystemState currentState;
    if (currentState == STATE_VERIFICATION || currentState == STATE_FIRE_ALARM) { resetSystem(); }
}

void SystemController::activateFireAlarm() {
    changeState(STATE_FIRE_ALARM, "POZAR !!!", nazwyLinii[detectedLineId], "SYSTEM", "SYSTEM");
    setVent(true); playDsoMessage(nagraniaLinii[detectedLineId]); digitalWrite(PIN_BUILTIN_LED, LOW);
}

void SystemController::resetSystem() {
    setVent(false); stopDso(); digitalWrite(PIN_BUILTIN_LED, LOW); resetSensorLatches();
    logger.addEvent("RESET", "SYSTEMU"); 
    changeState(STATE_IDLE, "CZUWANIE", "CALY_OBIEKT", actionSource, "P" + String((int)accessMgr.getLevel()));
}

int SystemController::getRemainingTime() {
    extern SystemState currentState; unsigned long duration = 0;
    if (currentState == STATE_PRE_ALARM) duration = (timeT1 * 1000);
    else if (currentState == STATE_VERIFICATION) duration = (timeT2 * 1000);
    else return 0;
    
    unsigned long passed = millis() - timerStart;
    if (passed >= duration) return 0;
    return (duration - passed) / 1000;
}

void SystemController::enterMenu() {
    menuCursorIndex = 0; menuScrollOffset = 0; inSubMenu = false; inSubSubMenu = false;
    changeState(STATE_MENU, "MENU_SYSTEMOWE", "MENU", actionSource, "P" + String((int)accessMgr.getLevel()));
    lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
}

void SystemController::exitMenu() {
    changeState(STATE_IDLE, "CZUWANIE", "CALY_OBIEKT", actionSource, "P" + String((int)accessMgr.getLevel()));
}

void SystemController::executeMenuAction() {
    subMenuCursor = 0; 
    
    switch (menuCursorIndex) {
        case 0: 
            if ((int)accessMgr.getLevel() < 3) {
                showPopup("BRAK UPRAWNIEN", "WYMAGANY POZ. P3");
                inSubMenu = false; accessMgr.startLoginSequence(); lcdMgr.showLoginScreen("");
            } else {
                extern uint16_t timeT1, timeT2; isEditingConfig = false; lcdMgr.showConfigMenu(subMenuCursor, timeT1, timeT2); 
            }
            break;
        case 1: lcdMgr.showBlokowanieMenu(subMenuCursor, isLineBlocked()); break;
        case 2: lcdMgr.showTestowanieMenu(subMenuCursor); break;
        case 3: 
            if (logger.getCount() == 0) {
                showPopup("PAMIETNIK", "BRAK WPISOW...");
                inSubMenu = false; lcdMgr.showMenu(menuCursorIndex, menuScrollOffset);
            } else {
                LogEvent ev; logger.getEvent(subMenuCursor, ev); lcdMgr.showEventLog(ev, subMenuCursor, logger.getCount());
            }
            break;
        case 4: 
            if ((int)accessMgr.getLevel() < 3) {
                showPopup("BRAK UPRAWNIEN", "WYMAGANY POZ. P3");
                inSubMenu = false; accessMgr.startLoginSequence(); lcdMgr.showLoginScreen("");
            } else {
                inSubMenu = true; subMenuCursor = 0; lcdMgr.showResetConfirm(subMenuCursor);
            }
            break;
        case 5: // ZEGAR (DOSTĘPNY OD P2)
            if ((int)accessMgr.getLevel() < 2) { // Zmiana z 3 na 2
                showPopup("BRAK UPRAWNIEN", "WYMAGANY POZ. P2");
                inSubMenu = false; accessMgr.startLoginSequence(); lcdMgr.showLoginScreen("");
            } else {
                struct tm timeinfo;
                if(!getLocalTime(&timeinfo, 10)) { editHour = 12; editMinute = 0; } 
                else { editHour = timeinfo.tm_hour; editMinute = timeinfo.tm_min; }
                clockPartIndex = 0; isEditingConfig = false; subMenuCursor = 0;
                lcdMgr.showZegarMenu(subMenuCursor, ntpEnabled, editHour, editMinute);
            }
            break;
        case 6: lcdMgr.showInfoNetwork(subMenuCursor, WiFi.localIP().toString(), WiFi.RSSI(), WiFi.status() == WL_CONNECTED, netMgr.isMqttConnected()); break;
    }
}