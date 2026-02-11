#include "KeypadManager.h"
#include "DsoManager.h"

extern bool audioWyciszone;
extern unsigned long czasWyciszenia;

const int R_PINS[] = {13, 12, 14, 27}; 
const int C_PINS[] = {26, 25, 33, 32}; 

unsigned long ostatniCzasACK = 0;
const int CZAS_STABILIZACJI = 100; //ms - sygnał musi być ciagle LOW przez ten czas
unsigned long licznikWcisniecia = 0;
bool akcjaWykonana = false;

void initKeypad() {
  for (int i = 0; i < 4; i++) {
    pinMode(R_PINS[i], OUTPUT);
    digitalWrite(R_PINS[i], HIGH);
  }
  for (int i = 0; i < 4; i++) {
    pinMode(C_PINS[i], INPUT_PULLUP); // Wewnętrzny rezystor podciągający
  }
}

void checkKeypad() {
  digitalWrite(R_PINS[0], LOW); 
  // Krótka przerwa, aby stan elektryczny pinu się ustabilizował po zmianie na LOW
  delayMicroseconds(50); 

  bool s1 = (digitalRead(C_PINS[0]) == LOW);
  bool s4 = (digitalRead(C_PINS[3]) == LOW);

  if (s1 || s4) {
    if (licznikWcisniecia == 0) {
      licznikWcisniecia = millis(); // Zaczynamy mierzyć czas trzymania
    }

    // Warunek: Przycisk musi być trzymany stabilnie przez CZAS_STABILIZACJI
    if (!akcjaWykonana && (millis() - licznikWcisniecia > CZAS_STABILIZACJI)) {
      Serial.println(s1 ? "Klawiatura: ACK S1" : "Klawiatura: ACK S4");
      stopDso(); // [cite: 83, 88]
      
      audioWyciszone = true;    // Informujemy SensorManager o wyciszeniu
      czasWyciszenia = millis(); // Zaczynamy odliczać 5 sekund ciszy
      
      akcjaWykonana = true;
    }
  } else {
    // Reset wszystkiego, gdy nic nie jest wciśnięte
    licznikWcisniecia = 0;
    akcjaWykonana = false;
  }

  digitalWrite(R_PINS[0], HIGH); 
}