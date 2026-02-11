#include "AccessManager.h"

AccessManager::AccessManager() {
    currentLevel = LEVEL_GUEST;
    inputBuffer = "";
    lastActivityTime = 0;
    isLoggingIn = false;
}

void AccessManager::begin() {
    lastActivityTime = millis();
    Serial.println("AccessManager: Start (Poziom P1).");
}

void AccessManager::loop() {
    // Autowylogowanie po 2 minutach
    if (currentLevel > LEVEL_GUEST) {
        if (millis() - lastActivityTime > LOGOUT_TIMEOUT) {
            logout();
        }
    }
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

    if (inputBuffer == "111") {
        currentLevel = LEVEL_GUEST;
        success = true;
    } else if (inputBuffer == "222") {
        currentLevel = LEVEL_USER;
        success = true;
    } else if (inputBuffer == "333") {
        currentLevel = LEVEL_ADMIN;
        success = true;
    }

    if (success) {
        Serial.print("AccessManager: Zalogowano na P"); Serial.println((int)currentLevel);
        isLoggingIn = false;
        inputBuffer = "";
        return true;
    } else {
        Serial.println("AccessManager: Bledne haslo!");
        inputBuffer = "";
        return false;
    }
}

void AccessManager::logout() {
    currentLevel = LEVEL_GUEST;
    isLoggingIn = false;
    inputBuffer = "";
    Serial.println("AccessManager: AUTO-WYLOGOWANIE do P1.");
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