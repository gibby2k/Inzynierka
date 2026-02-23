#ifndef KEYPAD_MANAGER_H
#define KEYPAD_MANAGER_H

#include <Arduino.h>

// Deklaracje funkcji, aby SapMain je widział
void initKeypad();
void checkKeypad();

// Jeśli używasz starej klasy KeypadManager, możesz ją zostawić poniżej
// ale dla naszych nowych funkcji najważniejsze są te dwie linie powyżej.

class KeypadManager {
public:
    char readKey(); // stara funkcja, może zostać dla kompatybilności
};

#endif