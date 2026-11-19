#include "AccessManager.h"
#include "EventLogger.h"
#include "Secrets.h"

extern EventLogger logger;

AccessManager::AccessManager() {
    currentLevel = LEVEL_GUEST;
    inputBuffer = "";
    lastActivityTime = 0;
    isLoggingIn = false;
    failedAttempts = 0;
}

void AccessManager::begin() {
    lastActivityTime = millis();
    Serial.println("AccessManager: Start (Poziom P1).");
}

void AccessManager::addDigit(char digit) {
    lastActivityTime = millis(); 
    if (isLoggingIn && inputBuffer.length() < 4) { 
        inputBuffer += digit;
    }
}

void AccessManager::clearBuffer() {
    inputBuffer = "";
}

bool AccessManager::tryLogin() {
    lastActivityTime = millis();

    bool success = false;
    // Kody pochodza z Secrets.h (poza repozytorium). Pusty bufor nigdy nie pasuje.
    if (inputBuffer.length() > 0) {
        if (inputBuffer == PIN_P1) {
            currentLevel = LEVEL_GUEST;
            success = true;
        } else if (inputBuffer == PIN_P2) {
            currentLevel = LEVEL_USER;
            success = true;
        } else if (inputBuffer == PIN_P3) {
            currentLevel = LEVEL_ADMIN;
            success = true;
        }
    }

    if (success) {
        Serial.print("AccessManager: Zalogowano na P"); Serial.println((int)currentLevel);
        failedAttempts = 0;
        isLoggingIn = false;
        inputBuffer = "";
        return true;
    }

    failedAttempts++;
    Serial.printf("AccessManager: Bledne haslo! (kolejne: %d)\n", failedAttempts);
    if (failedAttempts % FAILED_LOG_EVERY == 0) {
        logger.addEvent("BLEDNY", "PIN x" + String(failedAttempts));   // slad, bez blokady
    }
    inputBuffer = "";
    return false;
}

bool AccessManager::canReset() {
    return (currentLevel >= LEVEL_USER); // P2 i P3 mogą
}

bool AccessManager::canAck() {
    return (currentLevel >= LEVEL_GUEST); // Wszyscy mogą
}

AccessLevel AccessManager::getLevel() {
    return currentLevel;
}

String AccessManager::getBufferMasked() {
    String masked = "";
    for (unsigned int i = 0; i < inputBuffer.length(); i++) {
        masked += "*";
    }
    return masked;
}

bool AccessManager::isLoginInProgress() {
    return isLoggingIn;
}

void AccessManager::startLoginSequence() {
    isLoggingIn = true;
    inputBuffer = "";
    lastActivityTime = millis();
}

void AccessManager::stopLoginSequence() {
    isLoggingIn = false;
    inputBuffer = "";
}

void AccessManager::removeLastDigit() {
    lastActivityTime = millis();
    if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1); // Usuwa ostatni znak
    }
}

void AccessManager::resetTimer() {
    lastActivityTime = millis();
}

bool AccessManager::loop() {
    if (currentLevel > LEVEL_GUEST) {
        if (millis() - lastActivityTime > LOGOUT_TIMEOUT) {
            currentLevel = LEVEL_GUEST;
            isLoggingIn = false;
            inputBuffer = "";
            logger.addEvent("WYLOG.", "(AUTO)");
            Serial.println("AccessManager: AUTO-WYLOGOWANIE do P1.");
            return true; // Zwracamy informację, że system kogoś wylogował
        }
    }
    return false;
}

void AccessManager::logout() {
    currentLevel = LEVEL_GUEST;
    isLoggingIn = false;
    inputBuffer = "";
}
