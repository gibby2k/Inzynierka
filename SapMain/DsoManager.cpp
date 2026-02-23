#include "DsoManager.h"
#include <DFRobotDFPlayerMini.h>

HardwareSerial dsoSerial(2); 
DFRobotDFPlayerMini myDFPlayer;

void initDso() {
    dsoSerial.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17
    Serial.println(F("DSO: Start inicjalizacji..."));

    if (!myDFPlayer.begin(dsoSerial)) {
        Serial.println(F("DSO: Blad! Sprawdz SD, zasilanie i polaczenie GND."));
    } else {
        Serial.println(F("DSO: MP3 Player online."));
        myDFPlayer.volume(5); // Bezpieczny poziom glosnosci
    }
}

void playDsoMessage(int messageID) {
  Serial.print("DSO: Odtwarzanie komunikatu nr: ");
  Serial.println(messageID);

  // Sprawdzamy, czy player jest zainicjalizowany
  // myDFPlayer.play(messageID); // To jest najprostsza komenda

  switch (messageID) {
    case 1:
      myDFPlayer.playMp3Folder(1); // Odpala 0001.mp3 z folderu MP3
      break;
    case 2:
      myDFPlayer.playMp3Folder(2); // Odpala 0002.mp3 z folderu MP3
      break;
    case 3:
      myDFPlayer.playMp3Folder(3); // Odpala 0003.mp3 z folderu MP3
      break;
    case 4:
      myDFPlayer.playMp3Folder(4); // Odpala 0004.mp3 z folderu MP3 (Odwołanie)
      break;
    case 999:
      myDFPlayer.playMp3Folder(999); 
      break;
    default:
      Serial.print("DSO: Nieobslugiwany kod w switchu: ");
      Serial.println(messageID);
      break;
  }
}

void stopDso() {
  Serial.println("DSO: Wyciszenie komunikatu.");
  myDFPlayer.stop(); // Komenda zatrzymania odtwarzania
}