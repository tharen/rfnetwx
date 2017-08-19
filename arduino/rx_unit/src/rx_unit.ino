#include <RH_ASK.h>
#include <SPI.h> // Not used but needed to compile
#include <Wire.h> // For I2C communication

#define BPS 4800 // TX baud rate
#define RXLED 13
#define I2C_ADDR 0x04

RH_ASK driver(BPS);

uint8_t msgcnt = 0;

void setup()
{
  pinMode(RXLED, OUTPUT);

  Serial.begin(9600); // Debugging only
  if (!driver.init())
     Serial.println("init failed");

  Wire.begin(I2C_ADDR);
  Wire.onRequest(i2c_send);
  Wire.onReceive(i2c_receive);
}

long last_rx = 0;
uint8_t last_buf[50];
uint8_t last_buflen = 0;

// Current message
// FIXME: May need to store a stack of recent messages
char cur_msg[100];
int cur_msg_len=0;

// The master node signals which data item is requested
byte requested_item = 0;

void loop()
{
    if ((millis() - last_rx) > 250)
    {
      digitalWrite(RXLED, LOW);
    }
    
    uint8_t buf[50];
    uint8_t buflen = sizeof(buf);
    char data[50];
    if (driver.recv(buf, &buflen)) // Non-blocking
    {
      digitalWrite(RXLED, HIGH);
      last_rx = millis();
      
      // Skip this message if it is a retry
      // FIXME: Should this check the message number or a possible compute a checksum?
      if (buflen==last_buflen && compare_buffers(buf, last_buf, buflen)){
        return;
      }
      
      // Copy the buffer data to a character
      memcpy(data, buf, buflen);
      data[buflen] = '\0';

      // FIXME: Get a realtime clock
      unsigned long msg_time;
      msg_time = millis();
      
      sprintf(cur_msg,"%d,%lu,%s", msgcnt++, msg_time, data);
      cur_msg_len = strlen(cur_msg);
      Serial.println(cur_msg);

      // Copy the last buffer received to skip re-trys
      memcpy(last_buf, buf, buflen);
      last_buflen = buflen;
    }
}

bool compare_buffers(uint8_t buff1[50], uint8_t buff2[50], uint8_t buflen) {
  // Compare two buffer arrays 
  // TODO: Add a checksum so the whole message does not need evaluation
  for (int i; i<buflen; i++)
  {
    if (buff1[i] != buff2[i])
    {
      return false;
    }
  }
  return true;
}

//char v[] = {'a','b','c','d','e'};
void i2c_send() {
  //Wire.write(v,5);
  uint8_t mbuff[11];
  uint8_t checksum=0;
  int i=0;
  uint8_t ok=0;
  int nretrys=0;

//  Serial.println("** I2C Send");

  for (int x=0; x<10; x++){
    checksum = 0;
    for (int j=0; j<10; j++){
      if (i>cur_msg_len){
        mbuff[j] = 0;
      } else {
        checksum += (uint8_t)cur_msg[i];
        mbuff[j] = cur_msg[i];
      }
      i += 1;
    }

    mbuff[11] = checksum;

//    char xmsg[20] = {""};
//    memcpy(xmsg,mbuff,10);
//    sprintf(xmsg, "%s: %d", xmsg, checksum);
    
    nretrys = 0;
    while (1){
//      Serial.print("Try sending: ");
//      Serial.println(xmsg);

      Wire.write(mbuff,11);
      
      delay(10);
      ok = Wire.read();
//      Serial.println(ok);

      if (ok==(uint8_t)8){
        break;

      } else {
        nretrys += 1;

	if (nretrys>5){
	  return false;
	}

	// Give the bus a chance to catchup
	delay(10);

      }
    }

  }
}

void i2c_receive(int value) {
  char msg[30];
  sprintf(msg, "IC2 Received (%d bytes).", value);
  Serial.println(msg);
}

