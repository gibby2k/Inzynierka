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
    void showTestDsoMenu(int cursor);
    void showInfoNetwork(int page, String ip, long rssi, bool wifiOk, bool mqttOk);

    void showZegarMenu(bool ntpOk, String timeStr);
    void showClockEdit(int cursor, int h, int m, bool ntpStatus);

    void showEventLog(LogEvent event, int currentIndex, int totalCount);

    void showConfigMenu(int cursor, uint16_t t1, uint16_t t2);
    void showConfigEdit(int cursor, uint16_t value);
};
#endif