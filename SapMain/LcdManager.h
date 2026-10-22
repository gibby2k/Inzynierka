#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <LiquidCrystal_I2C.h>
#include "Config.h"
#include "SystemState.h"
#include "EventLogger.h"

class LcdManager {
  private:
    LiquidCrystal_I2C lcd;

  public:
    LcdManager();
    void begin();
    void showStartup();
    
    void updateStatus(SystemState state, int level);
    
    void showLoginScreen(String maskedPass);
    void showInfo(String line1, String line2);

    void showMenu(int cursorIndex, int scrollOffset);
    void showBlokowanieMenu(int cursor, bool isBlocked);
    void showTestowanieMenu(int cursor);
    void showTestOnOffMenu(String title, int cursor);
    void showInfoNetwork(int page, String ip, long rssi, bool wifiOk, bool mqttOk);

    void showResetConfirm(int cursor);

    void showZegarMenu(int cursor, bool ntpOk, int h, int m);
    void showClockEdit(int cursor, int h, int m);

    void showEventLog(LogEvent event, int currentIndex, int totalCount);

    void showConfigMenu(int cursor, uint16_t t1, uint16_t t2);
    void showConfigEdit(int cursor, uint16_t value);

};
#endif