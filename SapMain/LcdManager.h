#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <LiquidCrystal_I2C.h>
#include "Config.h"
#include "SystemState.h"

class LcdManager {
  private:
    LiquidCrystal_I2C lcd;

  public:
    LcdManager();
    void begin();
    void showStartup();
    
    // ZMIANA TUTAJ: dodajemy 'int level'
    void updateStatus(SystemState state, int level);
    
    void showLoginScreen(String maskedPass);
    void showInfo(String line1, String line2);
};
#endif