#include "SystemController.h"
#include "Config.h"
#include "SystemState.h"
#include "DsoManager.h"
#include "VentManager.h"
#include "LcdManager.h"
#include "SensorManager.h"
#include "AccessManager.h" 

extern void changeState(SystemState newState, String name, String zone, String source, String user);
extern String nazwyLinii[]; 
extern int nagraniaLinii[]; 
extern LcdManager lcdMgr;     
extern AccessManager accessMgr;
extern String currentZone, currentSource, currentUser;

SystemController::SystemController() {
    timerStart = 0;
    lastBlink = 0;
    ledState = false;
    detectedLineId = -1;
}

void SystemController::begin() {
    pinMode(PIN_BUILTIN_LED, OUTPUT);
    digitalWrite(PIN_BUILTIN_LED, LOW);
    Serial.println("SystemController: Start logiki PPOZ.");
}

void SystemController::loop() {
    extern SystemState currentState;
    unsigned long now = millis();
    
    // T1 - Alarm Wstępny
    if (currentState == STATE_PRE_ALARM) {
        if (now - lastBlink > 500) {
            lastBlink = now;
            ledState = !ledState;
            digitalWrite(PIN_BUILTIN_LED, ledState ? HIGH : LOW);
        }
        if (now - timerStart > TIME_T1_PRE_ALARM) {
            activateFireAlarm();
        }
    }
    // T2 - Weryfikacja
    else if (currentState == STATE_VERIFICATION) {
        digitalWrite(PIN_BUILTIN_LED, HIGH);
        if (now - timerStart > TIME_T2_VERIFY) {
            activateFireAlarm();
        }
    }
    else if (currentState == STATE_IDLE) {
        digitalWrite(PIN_BUILTIN_LED, LOW);
    }
}

// --- OBSŁUGA WEJŚCIA ---
void SystemController::handleInput(char key) {
    extern SystemState currentState;

    // 1. TRYB LOGOWANIA (Wpisywanie hasła)
    if (accessMgr.isLoginInProgress()) {
        
        if (key >= '0' && key <= '9') {
            accessMgr.addDigit(key);
            lcdMgr.showLoginScreen(accessMgr.getBufferMasked());
        } 
        else if (key == 'E') { // S12 (ENTER) - Zatwierdź hasło
            bool ok = accessMgr.tryLogin();
            if (ok) {
                lcdMgr.showInfo("ZALOGOWANO", "POZIOM P" + String((int)accessMgr.getLevel()));
                delay(1500);
                // Odśwież ekran główny
                changeState(currentState, currentZone, currentZone, currentSource, currentUser);
            } else {
                lcdMgr.showInfo("BLEDNE HASLO", "SPROBUJ PONOWNIE");
                delay(1500);
                accessMgr.startLoginSequence();
                lcdMgr.showLoginScreen("");
            }
        }
        else if (key == 'C') { // S16 (ANULUJ) - Wyjście z logowania
             Serial.println("SysCtrl: Anulowano logowanie.");
             accessMgr.stopLoginSequence();
             changeState(currentState, currentZone, currentZone, currentSource, currentUser);
        }
        return; // Blokujemy inne klawisze podczas logowania
    }

    // 2. TRYB NORMALNY (Klawisze funkcyjne)
    
    // --- ACK (S4) ---
    if (key == 'A') { 
        if (accessMgr.canAck()) {
            onAckPressed();
        }
    }
    
    // --- RESET (S8) ---
    else if (key == 'R') { 
        if (accessMgr.canReset()) {
            onResetPressed();
        } else {
            // Brak uprawnień -> Wymuś logowanie
            Serial.println("Wymagane logowanie do Resetu.");
            accessMgr.startLoginSequence();
            lcdMgr.showLoginScreen(""); 
        }
    }
    
    // --- ENTER (S12) - Info ---
    else if (key == 'E') {
        lcdMgr.showInfo("POZIOM DOSTEPU", "AKTUALNY: P" + String((int)accessMgr.getLevel()));
        delay(1500);
        changeState(currentState, currentZone, currentZone, currentSource, currentUser);
    }

    // --- ANULUJ (S16) - Wyloguj (Opcjonalnie) ---
    else if (key == 'C') {
        // Możemy tu zrobić ręczne wylogowanie
        accessMgr.logout();
        lcdMgr.showInfo("WYLOGOWANO", "POZIOM P1");
        delay(1500);
        changeState(currentState, currentZone, currentZone, currentSource, currentUser);
    }
}

void SystemController::onSensorSignal(int lineId) {
    extern SystemState currentState;
    if (currentState == STATE_IDLE) {
        detectedLineId = lineId;
        timerStart = millis();
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
    changeState(STATE_IDLE, "CZUWANIE", "CALY_OBIEKT", "SYSTEM", "AUTO");
}

int SystemController::getRemainingTime() {
    extern SystemState currentState;
    unsigned long duration = 0;
    if (currentState == STATE_PRE_ALARM) duration = TIME_T1_PRE_ALARM;
    else if (currentState == STATE_VERIFICATION) duration = TIME_T2_VERIFY;
    else return 0; 
    unsigned long passed = millis() - timerStart;
    if (passed >= duration) return 0;
    return (duration - passed) / 1000; 
}