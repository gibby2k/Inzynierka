#include "LcdManager.h"
#include "SystemState.h"
#include <Arduino.h>

// Deklaracje zmiennych zewnętrznych (z SapMain), aby LCD wiedział co pisać
extern String currentZone; 

LcdManager::LcdManager() : lcd(LCD_ADDR, LCD_COLS, LCD_ROWS) {}

void LcdManager::begin() {
  lcd.init();
  lcd.backlight();
}

void LcdManager::showStartup() {
  lcd.setCursor(0, 0);
  lcd.print(" CENTRALU SAP ");
  lcd.setCursor(0, 1);
  lcd.print(" INICJALIZACJA ");
  delay(1500);
  lcd.clear();
}

void LcdManager::updateStatus(SystemState state) {
  lcd.clear();
  
  // Linia 0: Nagłówek stanu
  lcd.setCursor(0, 0);
  switch (state) {
    case STATE_IDLE:       lcd.print("> STAN: CZUWANIE"); break;
    case STATE_FIRE_ALARM: lcd.print("! ALARM POZAR !"); break;
    case STATE_FAULT:      lcd.print("? AWARIA LINII "); break;
    default:               lcd.print("  SYSTEM PRACY  "); break;
  }

  // Linia 1: Szczegóły (Lokalizacja)
  lcd.setCursor(0, 1);
  if (state == STATE_FIRE_ALARM) {
    lcd.print(currentZone); // Wyświetli np. "L1 - GREEN"
  } else if (state == STATE_IDLE) {
    lcd.print("OBIEKT BEZPIECZNY");
  } else {
    lcd.print("SPRAWDZ PANEL");
  }
}