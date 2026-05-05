#include "KeypadManager.h"
#include "SystemController.h"
#include <PCF8575.h> // Zmiana 1: Dodanie biblioteki ekspandera

extern SystemController sysCtrl;

// Zmiana 2: Inicjalizacja obiektu ekspandera pod znalezionym adresem 0x20
PCF8575 pcf8575(0x20);

// Zmiana 3: Przesiadka z pinów GPIO ESP32 na piny P0-P7 ekspandera
const uint8_t R_PINS[] = {P0, P1, P2, P3}; 
const uint8_t C_PINS[] = {P4, P5, P6, P7}; 

unsigned long licznikWcisniecia = 0;
const int CZAS_STABILIZACJI = 80; 
bool akcjaWykonana = false;
char ostatniZnak = 0;

void initKeypad() {
  // Zmiana 4: Uruchomienie komunikacji I2C dla ekspandera
  pcf8575.begin();

  for (int i = 0; i < 4; i++) {
    pcf8575.pinMode(R_PINS[i], OUTPUT);
    pcf8575.digitalWrite(R_PINS[i], HIGH);
  }
  for (int i = 0; i < 4; i++) {
    // W PCF8575 piny działają domyślnie z pull-upem, wystarczy INPUT
    pcf8575.pinMode(C_PINS[i], INPUT); 
  }
}

void checkKeypad() {
  char wcisnietyTeraz = 0;

  // SKANOWANIE MATRYCY
  for (int r = 0; r < 4; r++) {
    // Zmiana 5: Zastąpienie natywnego digitalWrite funkcją z biblioteki
    pcf8575.digitalWrite(R_PINS[r], LOW);
    delayMicroseconds(50); 

    for (int c = 0; c < 4; c++) {
      // Zmiana 6: Zastąpienie natywnego digitalRead funkcją z biblioteki
      if (pcf8575.digitalRead(C_PINS[c]) == LOW) {
        
        // --- WIERSZ 1 (S1-S4) ---
        if (r == 0 && c == 0) wcisnietyTeraz = '1';
        if (r == 0 && c == 1) wcisnietyTeraz = '2';
        if (r == 0 && c == 2) wcisnietyTeraz = '3';
        if (r == 0 && c == 3) wcisnietyTeraz = 'A'; // S4 = ACK

        // --- WIERSZ 2 (S5-S8) ---
        if (r == 1 && c == 0) wcisnietyTeraz = '4';
        if (r == 1 && c == 1) wcisnietyTeraz = '5';
        if (r == 1 && c == 2) wcisnietyTeraz = '6';
        if (r == 1 && c == 3) wcisnietyTeraz = 'B'; // (Kasowanie)

        // --- WIERSZ 3 (S9-S12) ---
        if (r == 2 && c == 0) wcisnietyTeraz = '7';
        if (r == 2 && c == 1) wcisnietyTeraz = '8';
        if (r == 2 && c == 2) wcisnietyTeraz = '9';
        if (r == 2 && c == 3) wcisnietyTeraz = 'C'; // (ESC/Wróć)

        // --- WIERSZ 4 (S13-S16) ---
        if (r == 3 && c == 0) wcisnietyTeraz = '*'; // S13
        if (r == 3 && c == 1) wcisnietyTeraz = '0'; // S14
        if (r == 3 && c == 2) wcisnietyTeraz = '#'; // S15
        if (r == 3 && c == 3) wcisnietyTeraz = 'D'; // S16
      }
    }
    pcf8575.digitalWrite(R_PINS[r], HIGH);
    if (wcisnietyTeraz != 0) break;
  }

  // --- LOGIKA STABILIZACJI POZOSTAJE BEZ ZMIAN ---
  if (wcisnietyTeraz != 0) {
    if (licznikWcisniecia == 0) {
        licznikWcisniecia = millis(); 
        ostatniZnak = wcisnietyTeraz; 
    }

    if (wcisnietyTeraz == ostatniZnak && !akcjaWykonana) {
        if (millis() - licznikWcisniecia > CZAS_STABILIZACJI) {
            
            // --- WYPISYWANIE KOMUNIKATÓW ---
            switch(ostatniZnak) {
                case '1': Serial.println("1"); break;
                case '2': Serial.println("2"); break;
                case '3': Serial.println("3"); break;
                case '4': Serial.println("4"); break;
                case '5': Serial.println("5"); break;
                case '6': Serial.println("6"); break;
                case '7': Serial.println("7"); break;
                case '8': Serial.println("8"); break;
                case '9': Serial.println("9"); break;
                case '0': Serial.println("0"); break;

                case 'A': Serial.println("(A) ACK"); break;
                case 'B': Serial.println("(B) RESET"); break;
                case 'C': Serial.println("(C) ESC / DELETE"); break;
                case 'D': Serial.println("(D) MENU"); break;
                case '#': Serial.println("(#) ENTER"); break;
                case '*': Serial.println("(*) WYLOGUJ"); break;
            }

            // Przekazujemy do systemu tylko aktywne klawisze
            if (ostatniZnak != 'x' && ostatniZnak != 'y') {
                sysCtrl.handleInput(ostatniZnak);
            }

            akcjaWykonana = true; 
        }
    }
  } else {
    licznikWcisniecia = 0;
    akcjaWykonana = false;
    ostatniZnak = 0;
  }
}