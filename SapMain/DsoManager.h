#ifndef DSO_MANAGER_H
#define DSO_MANAGER_H

#include <Arduino.h>

// Numery nagran na karcie SD: /MP3/0001.mp3, /MP3/0002.mp3 ...
#define DSO_TRACK_EWAKUACJA  1   // alarm II stopnia (ewakuacja)
#define DSO_TRACK_TEST       2   // TEST DSO z menu
#define DSO_TRACK_ODWOLANIE  3   // odwolanie alarmu (do wykorzystania w sesji DSO)

void initDso();                // Uruchomienie zadania DSO (inicjalizacja modulu w tle)
void playDsoMessage(int code); // Zlecenie odtworzenia /MP3/000N.mp3 (nie blokuje)
void stopDso();                // Zlecenie zatrzymania odtwarzania (nie blokuje)

// --- Diagnostyka (dla petli glownej / przyszlego stanu AWARIA) ---
bool isDsoOnline();            // modul odpowiada na zapytania (tylko diagnostyka)
bool isDsoInitDone();          // inicjalizacja zakonczona
bool isDsoStuck();             // polecenie do modulu trwa > 3 s (zawieszenie)

#endif
