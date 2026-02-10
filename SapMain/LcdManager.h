#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <LiquidCrystal_I2C.h>
#include "Config.h"
#include "SystemState.h"

class LcdManager {
  private:
    LiquidCrystal_I2C lcd; // Obiekt biblioteki (ukryty)

  public:
    // Konstruktor
    LcdManager();

    // Metody (Funkcje dostępne dla głównego programu)
    void begin();
    void showStartup();
    void updateStatus(SystemState state);
};

#endif