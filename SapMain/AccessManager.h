#ifndef ACCESS_MANAGER_H
#define ACCESS_MANAGER_H

#include <Arduino.h>

// Definicja poziomów
enum AccessLevel {
    LEVEL_GUEST = 1, // P1
    LEVEL_USER  = 2, // P2
    LEVEL_ADMIN = 3  // P3
};

class AccessManager {
private:
    AccessLevel currentLevel;
    String inputBuffer;       
    unsigned long lastActivityTime; 
    const unsigned long LOGOUT_TIMEOUT = 120000; // 2 minuty
    bool isLoggingIn; 

public:
    AccessManager();
    void begin();

    // Obsługa wprowadzania
    void addDigit(char digit);
    void clearBuffer();
    bool tryLogin(); 
    void logout();   
    void removeLastDigit();
    bool loop(); // <-- Zmień z void na bool
    void resetTimer();


    // Uprawnienia
    bool canReset(); 
    bool canAck(); 


    
    // Gettery
    AccessLevel getLevel();
    String getBufferMasked(); 
    bool isLoginInProgress();
    void startLoginSequence(); 
    void stopLoginSequence();
};

#endif