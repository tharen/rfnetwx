#include <EEPROM.h>

void setup() {

  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  int uid = 2;  // Unique ID to store
  int eeAddress = 0; // EEPROM address

  EEPROM.put(eeAddress, uid);

}

void loop() {
  // put your main code here, to run repeatedly:

}
