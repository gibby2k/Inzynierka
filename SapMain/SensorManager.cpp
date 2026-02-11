#include "SensorManager.h"
#include "SystemState.h"
#include "DsoManager.h"
#include "LcdManager.h"
#include "VentManager.h"

extern LcdManager lcdMgr;
extern void changeState(SystemState newState, String name, String zone, String source, String user);

// --- KONFIGURACJA LINII ---
const int LICZBA_LINII = 3;
int pinyLinii[LICZBA_LINII] = {4, 5, 18}; 
bool stanAlarmu[LICZBA_LINII] = {false, false, false};
String nazwyLinii[LICZBA_LINII] = {"L1 - GREEN", "L2 - YELLOW", "L3 - BLUE"};
int nagraniaLinii[LICZBA_LINII] = {1, 2, 3}; 

// --- ZMIENNE POMOCNICZE ---
unsigned long czasWyciszenia = 0;
const int PRZERWA_PO_WYCISZENIU = 5000; 
bool audioWyciszone = false;

// Zmienna do odliczania czasu trwania komunikatu "Odwołanie alarmu"
unsigned long czasStartuOdwolania = 0;
const int CZAS_TRWANIA_ODWOLANIA = 8000; // 8 sekund na niebiesko

void initSensors() {
  for(int i=0; i < LICZBA_LINII; i++) {
    pinMode(pinyLinii[i], INPUT_PULLUP);
  }
}

void checkSensors() {
  bool czyJakikolwiekAlarm = false;
  int idAktywnejLinii = -1;

  // 1. SKANOWANIE LINII
  for(int i=0; i < LICZBA_LINII; i++) {
    if (digitalRead(pinyLinii[i]) == HIGH) { 
      czyJakikolwiekAlarm = true;
      idAktywnejLinii = i;
      
      if (!stanAlarmu[i]) {
        stanAlarmu[i] = true;
        audioWyciszone = false; 
        
        // Zmiana stanu zapala CZERWONĄ diodę (w changeState)
        changeState(STATE_FIRE_ALARM, "ALARM", nazwyLinii[i], "IN" + String(i+1), "FIZ");
        
        playDsoMessage(nagraniaLinii[i]);
      }
    } else {
      if (stanAlarmu[i]) {
        stanAlarmu[i] = false;
      }
    }
  }

  // 2. AUTOMATYKA WENTYLATORA
  static bool stanWentylatora = false;
  if (czyJakikolwiekAlarm && !stanWentylatora) {
      setVent(true);
      stanWentylatora = true;
  } else if (!czyJakikolwiekAlarm && stanWentylatora) {
      setVent(false); 
      stanWentylatora = false;
  }

  // 3. RESTART DŹWIĘKU PO WYCISZENIU
  if (czyJakikolwiekAlarm && audioWyciszone) {
    if (millis() - czasWyciszenia > PRZERWA_PO_WYCISZENIU) {
      audioWyciszone = false;
      playDsoMessage(nagraniaLinii[idAktywnejLinii]);
    }
  }

  // 4. ODWOŁANIE ALARMU
  static bool poprzedniStanGlobalny = false;
  
  if (!czyJakikolwiekAlarm && poprzedniStanGlobalny) {
    audioWyciszone = false;
    
    changeState(STATE_IDLE, "CZUWANIE", "CALY_OBIEKT", "SYSTEM", "AUTO");
    playDsoMessage(4); 
    
    // USUNIĘTO: setIndicatorColor(COLOR_BLUE);
    czasStartuOdwolania = millis(); // Timer zostawiamy, przyda się do TM1637!
  }

  // 5. KONIEC ODWOŁANIA
  if (czasStartuOdwolania > 0) {
    if (millis() - czasStartuOdwolania > CZAS_TRWANIA_ODWOLANIA) {
        // USUNIĘTO: setIndicatorColor(COLOR_GREEN);
        czasStartuOdwolania = 0; 
    }
  }

  poprzedniStanGlobalny = czyJakikolwiekAlarm;
}