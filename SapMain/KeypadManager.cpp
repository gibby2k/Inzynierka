#include "KeypadManager.h"
#include "SystemController.h"
#include <Wire.h> // Zmiana 1: Używamy czystego Wire.h zamiast PCF8575.h

extern SystemController sysCtrl;

const int I2C_ADDR = 0x20;

unsigned long licznikWcisniecia = 0;
const int CZAS_STABILIZACJI = 80; 
bool akcjaWykonana = false;
char ostatniZnak = 0;

void initKeypad() {
    // Wymuszamy stan wysoki (1) na wszystkich 16 pinach ekspandera (pull-up dla wejść)
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(0xFF); // Port 0 (P0-P7)
    Wire.write(0xFF); // Port 1 (P10-P17)
    Wire.endTransmission();
    Serial.println("KeypadManager: Matryca I2C zainicjalizowana.");
}

void checkKeypad() {
    char wcisnietyTeraz = 0;
    
    // MASKI WIERSZY: Ściągamy tylko jeden wiersz do zera (LOW), reszta zostaje na HIGH (1)
    // R1 (P3) = 1111 0111 -> 0xF7
    // R2 (P2) = 1111 1011 -> 0xFB
    // R3 (P1) = 1111 1101 -> 0xFD
    // R4 (P0) = 1111 1110 -> 0xFE
    uint8_t rowMasks[4] = {0xF7, 0xFB, 0xFD, 0xFE};
    
    for (int r = 0; r < 4; r++) {
        // Ustawienie maski dla aktualnego wiersza
        Wire.beginTransmission(I2C_ADDR);
        Wire.write(rowMasks[r]); 
        Wire.write(0xFF); // Górny port bez zmian
        Wire.endTransmission();
        
        delayMicroseconds(100); // Krótki czas na ustabilizowanie napięcia w matrycy
        
        // Odczyt stanu portów
        Wire.requestFrom(I2C_ADDR, 2);
        if (Wire.available() == 2) {
            uint8_t lowerPort = Wire.read();
            uint8_t upperPort = Wire.read(); // Czytamy by opróżnić bufor
            
            // Sprawdzamy kolumny - jeśli dany bit zjechał na 0, przycisk jest wciśnięty
            if ((lowerPort & (1 << 4)) == 0) { // C1 (P4)
                if (r == 0) wcisnietyTeraz = '1';
                if (r == 1) wcisnietyTeraz = '4';
                if (r == 2) wcisnietyTeraz = '7';
                if (r == 3) wcisnietyTeraz = '*';
            }
            else if ((lowerPort & (1 << 5)) == 0) { // C2 (P5)
                if (r == 0) wcisnietyTeraz = '2';
                if (r == 1) wcisnietyTeraz = '5';
                if (r == 2) wcisnietyTeraz = '8';
                if (r == 3) wcisnietyTeraz = '0';
            }
            else if ((lowerPort & (1 << 6)) == 0) { // C3 (P6)
                if (r == 0) wcisnietyTeraz = '3';
                if (r == 1) wcisnietyTeraz = '6';
                if (r == 2) wcisnietyTeraz = '9';
                if (r == 3) wcisnietyTeraz = '#';
            }
            else if ((lowerPort & (1 << 7)) == 0) { // C4 (P7)
                if (r == 0) wcisnietyTeraz = 'A';
                if (r == 1) wcisnietyTeraz = 'B';
                if (r == 2) wcisnietyTeraz = 'C';
                if (r == 3) wcisnietyTeraz = 'D';
            }
        }
        
        // Przywracamy wiersz na HIGH przed skanowaniem następnego
        Wire.beginTransmission(I2C_ADDR);
        Wire.write(0xFF);
        Wire.write(0xFF);
        Wire.endTransmission();
        
        if (wcisnietyTeraz != 0) break; // Przycisk znaleziony, nie szukamy dalej
    }

    // --- LOGIKA STABILIZACJI I DEBOUNCINGU ---
    if (wcisnietyTeraz != 0) {
        if (licznikWcisniecia == 0) {
            licznikWcisniecia = millis(); 
            ostatniZnak = wcisnietyTeraz; 
        }

        if (wcisnietyTeraz == ostatniZnak && !akcjaWykonana) {
            if (millis() - licznikWcisniecia > CZAS_STABILIZACJI) {
                
                // Wypisywanie
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