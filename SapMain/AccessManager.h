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
    const unsigned long LOGOUT_TIMEOUT = 600000; // Auto-wylogowanie po 10 minutach bezczynnosci
    bool isLoggingIn; 

    // --- Bledne kody: BEZ blokady (w stresie podczas alarmu trzeba moc probowac dalej),
    //     ale co 3 kolejne bledy zostaje slad w pamieci zdarzen ---
    const uint8_t FAILED_LOG_EVERY = 3;
    uint8_t failedAttempts;

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
