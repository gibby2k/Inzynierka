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

// Definicja opcji w Menu
const int MENU_SIZE = 6;
String menuItems[MENU_SIZE] = {
    "1.KONFIGURACJA",
    "2.PAMIETNIK",
    "3.RESET PAM.ZD.",
    "4.TEST DSO",
    "5.ZEGAR",
    "6.INFO (Siec)"
    };

void LcdManager::showMenu(int cursorIndex, int scrollOffset) {
    // Rysujemy tylko 2 wiersze wyświetlacza (i = 0 oraz i = 1)
    for (int i = 0; i < 2; i++) {
        int itemIndex = scrollOffset + i;
        lcd.setCursor(0, i);
        
        if (itemIndex < MENU_SIZE) {
            // Rysowanie kursora
            if (itemIndex == cursorIndex) {
                lcd.print(">");
            } else {
                lcd.print(" ");
            }
            
            // Wypisanie tekstu i dopełnienie spacjami do 15 znaków (żeby uniknąć lcd.clear())
            String line = menuItems[itemIndex];
            while (line.length() < 15) {
                line += " "; 
            }
            lcd.print(line);
        } else {
            // Pusta linia, jeśli zjedziemy na sam dół, a opcji jest nieparzysta liczba
            lcd.print("                "); 
        }
    }
}

void LcdManager::showTestDsoMenu(int cursor) {
    // cursor: 0 = OFF, 1 = ON
    lcd.setCursor(0, 0);
    if (cursor == 0) lcd.print(">OFF            ");
    else             lcd.print(" OFF            ");

    lcd.setCursor(0, 1);
    if (cursor == 1) lcd.print(">ON             ");
    else             lcd.print(" ON             ");
}

void LcdManager::showInfoNetwork(int page, String ip, long rssi, bool wifiOk, bool mqttOk) {
    lcd.setCursor(0, 0);
    
    if (page == 0) {
        // STRONA 1: IP i RSSI
        String line1 = "IP:" + (wifiOk ? ip : "---.---.---.---");
        while(line1.length() < 16) line1 += " ";
        lcd.print(line1);

        lcd.setCursor(0, 1);
        String line2 = "RSSI: " + (wifiOk ? String(rssi) + " dBm" : "---");
        while(line2.length() < 16) line2 += " ";
        lcd.print(line2);
        
    } else {
        // STRONA 2: Status WiFi i MQTT
        String line1 = "WiFi: ";
        line1 += (wifiOk ? "OK" : "BRAK");
        while(line1.length() < 16) line1 += " ";
        lcd.print(line1);

        lcd.setCursor(0, 1);
        String line2 = "MQTT: ";
        line2 += (mqttOk ? "OK" : "BRAK");
        while(line2.length() < 16) line2 += " ";
        lcd.print(line2);
    }
}

void LcdManager::showZegarMenu(bool ntpOk, String timeStr) {
    lcd.setCursor(0, 0);
    String line1 = ntpOk ? "SYNC NTP: OK    " : "SYNC NTP: BRAK  ";
    lcd.print(line1);

    lcd.setCursor(0, 1);
    String line2 = "CZAS: " + timeStr;
    while(line2.length() < 16) line2 += " "; // czyszczenie reszty linii
    lcd.print(line2);
}

void LcdManager::showEventLog(LogEvent event, int currentIndex, int totalCount) {
    lcd.setCursor(0, 0);
    
    // Wyciągamy resztę z dzielenia przez 100 (np. 123 % 100 = 23)
    int displayId = event.id % 100; 
    
    // Budujemy wiersz 1: Np. "23.POZAR|L1-E1"
    char line1[17];
    snprintf(line1, sizeof(line1), "%02d.%s %s", displayId, event.state, event.zone);
    
    // Dopełniamy spacjami, by usunąć "śmieci" z ekranu
    String l1 = String(line1);
    while (l1.length() < 16) l1 += " ";
    lcd.print(l1);

    // Budujemy wiersz 2: "14:32 23.02.26 "
    lcd.setCursor(0, 1);
    String l2 = String(event.timestamp);
    while (l2.length() < 16) l2 += " ";
    lcd.print(l2);
}

void LcdManager::showConfigMenu(int cursor, uint16_t t1, uint16_t t2) {
    lcd.clear();
    char buf[17];
    
    // Rysowanie pierwszego wiersza: ">1.CZAS T1(30s)" lub " 1.CZAS T1(30s)"
    lcd.setCursor(0, 0);
    snprintf(buf, sizeof(buf), "%c1.CZAS T1(%ds)", cursor == 0 ? '>' : ' ', t1);
    lcd.print(buf);
    
    // Rysowanie drugiego wiersza: ">2.CZAS T2(60s)" lub " 2.CZAS T2(60s)"
    lcd.setCursor(0, 1);
    snprintf(buf, sizeof(buf), "%c2.CZAS T2(%ds)", cursor == 1 ? '>' : ' ', t2);
    lcd.print(buf);
}

void LcdManager::showConfigEdit(int cursor, uint16_t value) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(cursor == 0 ? "EDYCJA CZASU T1:" : "EDYCJA CZASU T2:");
    lcd.setCursor(0, 1);
    char buf[17];
    snprintf(buf, sizeof(buf), "Wartosc: %3d sek", value);
    lcd.print(buf);
}

void LcdManager::showClockEdit(int cursor, int h, int m, bool ntpStatus) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(ntpStatus ? "SYNC NTP: TAK" : "SYNC NTP: BRAK");
    
    lcd.setCursor(0, 1);
    char buf[17];
    // Wyświetlamy np. CZAS: >12:05:00 (strzałka wskazuje co edytujemy)
    if (cursor == 0) snprintf(buf, sizeof(buf), "CZAS: >%02d:%02d:00", h, m);
    else if (cursor == 1) snprintf(buf, sizeof(buf), "CZAS: %02d:>%02d:00", h, m);
    else snprintf(buf, sizeof(buf), "CZAS: %02d:%02d:00", h, m);
    
    lcd.print(buf);
}