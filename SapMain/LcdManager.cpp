#include "LcdManager.h"
#include "SystemState.h"
#include <Arduino.h>

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

// ZMIENIONA FUNKCJA
void LcdManager::updateStatus(SystemState state, int level) {
  lcd.clear();
  
  // 1. Wyświetl Prefix "PX | " (5 znaków)
  lcd.setCursor(0, 0);
  lcd.print("P");
  lcd.print(level);
  lcd.print(" | ");
  
  // 2. Wyświetl Stan (Max 11 znaków!)
  switch (state) {
    case STATE_IDLE:       lcd.print("CZUWANIE   "); break;
    case STATE_FIRE_ALARM: lcd.print("POZAR !!!  "); break; // Skrócone z "! ALARM POZAR !"
    case STATE_PRE_ALARM:  lcd.print("PRE-ALARM (T1) "); break; // Skrócone z " ALARM WSTEPNY "
    case STATE_VERIFICATION: lcd.print("WERYFIKACJA (T2)"); break; // Idealnie 11 znaków
    case STATE_FAULT:      lcd.print("AWARIA     "); break;
    default:               lcd.print("SYSTEM     "); break;
  }

  // Linia 2: Szczegóły (Bez zmian)
  lcd.setCursor(0, 1);
  if (state == STATE_FIRE_ALARM || state == STATE_PRE_ALARM || state == STATE_VERIFICATION) {
    lcd.print(currentZone);
  } else if (state == STATE_IDLE) {
    lcd.print("OBIEKT BEZPIECZNY");
  } else {
    lcd.print("SPRAWDZ PANEL");
  }
}

void LcdManager::showLoginScreen(String maskedPass) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PODAJ HASLO:");
    lcd.setCursor(0, 1);
    lcd.print(maskedPass);
}

void LcdManager::showInfo(String line1, String line2) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);
}