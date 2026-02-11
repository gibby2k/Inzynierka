#include "KeypadManager.h"
#include "SystemController.h"

extern SystemController sysCtrl;

const int R_PINS[] = {13, 12, 14, 27}; 
const int C_PINS[] = {26, 25, 33, 32}; 

unsigned long licznikWcisniecia = 0;
const int CZAS_STABILIZACJI = 80; 
bool akcjaWykonana = false;
char ostatniZnak = 0;

void initKeypad() {
  for (int i = 0; i < 4; i++) {
    pinMode(R_PINS[i], OUTPUT);
    digitalWrite(R_PINS[i], HIGH);
  }
  for (int i = 0; i < 4; i++) {
    pinMode(C_PINS[i], INPUT_PULLUP);
  }
}

void checkKeypad() {
  char wcisnietyTeraz = 0;

  // SKANOWANIE MATRYCY
  for (int r = 0; r < 4; r++) {
    digitalWrite(R_PINS[r], LOW);
    delayMicroseconds(50); 

    for (int c = 0; c < 4; c++) {
      if (digitalRead(C_PINS[c]) == LOW) {
        
        // --- WIERSZ 1 (S1-S4) ---
        if (r == 0 && c == 0) wcisnietyTeraz = '1';
        if (r == 0 && c == 1) wcisnietyTeraz = '2';
        if (r == 0 && c == 2) wcisnietyTeraz = '3';
        if (r == 0 && c == 3) wcisnietyTeraz = 'A'; // S4 = ACK

        // --- WIERSZ 2 (S5-S8) ---
        if (r == 1 && c == 0) wcisnietyTeraz = '4';
        if (r == 1 && c == 1) wcisnietyTeraz = '5';
        if (r == 1 && c == 2) wcisnietyTeraz = '6';
        if (r == 1 && c == 3) wcisnietyTeraz = 'R'; // S8 = RESET

        // --- WIERSZ 3 (S9-S12) ---
        if (r == 2 && c == 0) wcisnietyTeraz = '7';
        if (r == 2 && c == 1) wcisnietyTeraz = '8';
        if (r == 2 && c == 2) wcisnietyTeraz = '9';
        if (r == 2 && c == 3) wcisnietyTeraz = 'E'; // S12 = ENTER

        // --- WIERSZ 4 (S13-S16) ---
        if (r == 3 && c == 0) wcisnietyTeraz = 'x'; // S13 = pusty
        if (r == 3 && c == 1) wcisnietyTeraz = '0'; // S14 = 0
        if (r == 3 && c == 2) wcisnietyTeraz = 'y'; // S15 = pusty
        if (r == 3 && c == 3) wcisnietyTeraz = 'C'; // S16 = ANULUJ (ESC)
      }
    }
    digitalWrite(R_PINS[r], HIGH);
    if (wcisnietyTeraz != 0) break;
  }

  // LOGIKA STABILIZACJI
  if (wcisnietyTeraz != 0) {
    if (licznikWcisniecia == 0) {
        licznikWcisniecia = millis(); 
        ostatniZnak = wcisnietyTeraz; 
    }

    if (wcisnietyTeraz == ostatniZnak && !akcjaWykonana) {
        if (millis() - licznikWcisniecia > CZAS_STABILIZACJI) {
            
            // --- WYPISYWANIE KOMUNIKATÓW ---
            switch(ostatniZnak) {
                // Cyfry
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

                // Funkcyjne (Formatowanie zgodne z Twoim życzeniem)
                case 'A': Serial.println("(A) ACK"); break;          // S4
                case 'R': Serial.println("(B) RST"); break;          // S8
                case 'E': Serial.println("(C) ENTER [info]"); break; // S12
                case 'C': Serial.println("(D) - ESC"); break;        // S16

                // Puste przyciski
                case 'x': Serial.println("S13 = pusty"); break;
                case 'y': Serial.println("S15 = pusty"); break;
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