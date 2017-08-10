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
  Wire.onRequest(i2c_request);
  Wire.onReceive(i2c_receive);
}

long last_rx = 0;
uint8_t last_buf[50];
uint8_t last_buflen = 0;

// Current message
// FIXME: May need to store a stack of recent messages
char cur_msg[100];

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
        return 0;
      }
      
      // Copy the buffer data to a character
      memcpy(data, buf, buflen);
      data[buflen] = "\0";

      // FIXME: Get a realtime clock
      unsigned long msg_time;
      msg_time = millis();
      
      sprintf(cur_msg,"%d,%lu,%s", msgcnt++, msg_time, data);
      Serial.println(cur_msg);

      // Copy the last buffer received to skip re-trys
      memcpy(last_buf, buf, buflen);
      last_buflen = buflen;
    }
}

bool compare_buffers(uint8_t buff1[50], uint8_t buff2[50], uint8_t buflen) {
  // Compare two buffer arrays 
  for (int i; i<buflen; i++)
  {
    if (buff1[i] != buff2[i])
    {
      return false;
    }
  }
  return true;
}

void i2c_request() {
  Wire.write(cur_msg);
}

void i2c_receive() {
  Serial.write("Got I2C Receive.");
}

