#include "DsoManager.h"
#include <DFRobotDFPlayerMini.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// =====================================================================
//  DSO (DFPlayer Mini) W OSOBNYM ZADANIU FreeRTOS
//
//  Logika pozarowa NIGDY nie rozmawia z odtwarzaczem bezposrednio - tylko
//  wklada polecenie do kolejki. Jesli modul DSO sie zawiesi (zasilanie,
//  zaklocenia na UART), zawiesi sie najwyzej zadanie DSO, a centrala dalej
//  wykrywa, alarmuje i steruje oddymianiem. Zawieszenie DSO jest wykrywane
//  i zglaszane jako AWARIA DSO w pamieci zdarzen.
// =====================================================================

HardwareSerial dsoSerial(2);
static DFRobotDFPlayerMini myDFPlayer;

enum DsoCmdType : uint8_t { DSO_CMD_PLAY = 0, DSO_CMD_STOP = 1 };
struct DsoCmd { DsoCmdType type; int track; };

static QueueHandle_t dsoQueue = nullptr;
static volatile bool dsoOnline = false;            // modul odpowiada na zapytania (diagnostyka)
static volatile bool dsoInitDone = false;          // inicjalizacja zakonczona
static volatile unsigned long dsoBusySince = 0;    // >0 = trwa wykonywanie polecenia
static const uint8_t DSO_VOLUME = 5;               // bezpieczny poziom glosnosci
static const unsigned long DSO_STUCK_MS = 3000;    // polecenie dluzej niz 3 s = zawieszenie
static const unsigned long DSO_BOOT_MS = 2000;     // czas na wlaczenie sie modulu i odczyt karty SD

// ---------------------------------------------------------------------
// WAZNE: wiele modulow DFPlayer (klony z ukladem MH2024K / GD3200B) NIE
// odsyla poprawnych odpowiedzi na UART, a mimo to wykonuje polecenia.
// Dlatego polecenia wysylamy ZAWSZE ("wyslij i zapomnij"), bez resetu
// modulu i bez czekania na potwierdzenia. Odpowiedz modulu sluzy tylko
// do diagnostyki w monitorze portu szeregowego.
// (Reset modulu tuz przed odtwarzaniem powodowal, ze polecenie PLAY
//  trafialo do modulu w trakcie jego restartu i bylo ignorowane.)
// ---------------------------------------------------------------------
static void dsoTask(void* arg) {
    // Modul potrzebuje chwili po wlaczeniu zasilania na odczyt karty SD
    vTaskDelay(pdMS_TO_TICKS(DSO_BOOT_MS));

    dsoBusySince = millis();
    myDFPlayer.begin(dsoSerial, /*isACK=*/false, /*doReset=*/false); // bez resetu, bez ACK
    myDFPlayer.setTimeOut(300);
    myDFPlayer.volume(DSO_VOLUME);
    vTaskDelay(pdMS_TO_TICKS(100));
    int vol = myDFPlayer.readVolume();            // tylko diagnostyka (max ~0,3 s)
    dsoBusySince = 0;

    dsoOnline = (vol >= 0);
    if (dsoOnline) Serial.printf("DSO: Modul odpowiada (glosnosc %d). Gotowy.\n", vol);
    else Serial.println(F("DSO: Modul nie odsyla odpowiedzi (czesto u klonow) - polecenia i tak beda wysylane."));
    dsoInitDone = true;

    DsoCmd cmd;
    for (;;) {
        if (xQueueReceive(dsoQueue, &cmd, portMAX_DELAY) != pdTRUE) continue;
        dsoBusySince = millis();
        if (cmd.type == DSO_CMD_PLAY) {
            myDFPlayer.volume(DSO_VOLUME);            // na wypadek samoczynnego restartu modulu
            vTaskDelay(pdMS_TO_TICKS(50));
            myDFPlayer.playMp3Folder(cmd.track);      // /MP3/000N.mp3
            Serial.printf("DSO: Wyslano polecenie PLAY /MP3/%04d.mp3\n", cmd.track);
        } else {
            myDFPlayer.stop();
        }
        dsoBusySince = 0;
    }
}

void initDso() {
    Serial.println(F("DSO: Start inicjalizacji (w tle)..."));
    dsoSerial.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17
    if (!dsoQueue) dsoQueue = xQueueCreate(8, sizeof(DsoCmd));
    // Rdzen 0 (rdzen 1 = logika centrali), niski priorytet
    xTaskCreatePinnedToCore(dsoTask, "DsoTask", 4096, NULL, 1, NULL, 0);
}

void playDsoMessage(int messageID) {
    Serial.print("DSO: Odtwarzanie komunikatu nr: ");
    Serial.println(messageID);
    if (messageID < 1 || messageID > 999) {
        Serial.println("DSO: Nieobslugiwany numer komunikatu.");
        return;
    }
    DsoCmd cmd = { DSO_CMD_PLAY, messageID };
    if (!dsoQueue || xQueueSend(dsoQueue, &cmd, 0) != pdTRUE) {
        Serial.println("DSO: Kolejka polecen pelna - polecenie pominiete!");
    }
}

void stopDso() {
    Serial.println("DSO: Wyciszenie komunikatu.");
    DsoCmd cmd = { DSO_CMD_STOP, 0 };
    if (!dsoQueue || xQueueSend(dsoQueue, &cmd, 0) != pdTRUE) {
        Serial.println("DSO: Kolejka polecen pelna - polecenie pominiete!");
    }
}

bool isDsoOnline() { return dsoOnline; }

bool isDsoInitDone() { return dsoInitDone; }

bool isDsoStuck() {
    unsigned long since = dsoBusySince;
    return since != 0 && (millis() - since) > DSO_STUCK_MS;
}
