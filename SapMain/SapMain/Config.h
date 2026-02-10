#ifndef CONFIG_H
#define CONFIG_H

// --- Piny I2C (LCD) ---
#define LCD_ADDR 0x27  // Jeśli nie działa, sprawdź 0x3F
#define LCD_COLS 16
#define LCD_ROWS 2
#define PIN_LCD_SDA 21
#define PIN_LCD_SCL 22

// --- Piny Klawiatury (Keypad 4x4) ---
// Wiersze (R1-R4)
#define PIN_ROW_1 13
#define PIN_ROW_2 12
#define PIN_ROW_3 14
#define PIN_ROW_4 27

// Kolumny (C1-C4)
#define PIN_COL_1 26
#define PIN_COL_2 25
#define PIN_COL_3 33
#define PIN_COL_4 32

#endif