#include <LowPower.h>
#include <DHT.h>
#include <RH_ASK.h>
#include <SPI.h> // Not used but needed to compile
#include <math.h>
#include <EEPROM.h>

#define TXTRYS 3
#define BPS 4800 // TX baud rate
#define VMPIN 5 // Voltage meter pin
#define TXLED LED_BUILTIN // Pin to light during TX
#define DHTPIN 2 // Digital pin the DHT is connected to
#define DHTTYPE DHT11 // DHT## Sensor model

DHT dht(DHTPIN, DHTTYPE);
RH_ASK driver(BPS);

int uid;
int min_delay = 5000; // Min tx delay
int max_delay = 6000; // Max tx delay

void setup()
{
    pinMode(TXLED, OUTPUT);
    dht.begin();

    // Load the board UID that was previously put
    EEPROM.get(0, uid);
    
    Serial.begin(9600);   // Debugging only
    if (!driver.init())
         Serial.println("init failed");
}

unsigned int i=0;

void loop()
{
  //NOTE: Float values are rounded and cast to integers for transmission
  
  // Read the temperature and humidity
  int humid = round(dht.readHumidity()*10);
  int tempf = round(dht.readTemperature(true)*10); //Farenheit
  // Compute the heat index
  float heatidx = dht.computeHeatIndex(tempf, humid);

  // Read the battery voltage
  int batt_volts = round(analogRead(VMPIN) * 5.0/1023.0 * 2 * 10); // FIXME: Adjust *2 for measured resistor values

  // Format the message string to transmit
  char msg[30];
  sprintf(msg, "U:%04d;T:%04d;H:%03d;V:%03d;C:%d", uid, tempf, humid, batt_volts, i);
  msg[strlen(msg)] = '\0';

  for (int j=0; j<TXTRYS; j++)
  {
    digitalWrite(TXLED, HIGH);
    driver.send((uint8_t *)msg, strlen(msg)+1);
    driver.waitPacketSent();
    digitalWrite(TXLED, LOW);
  }

//  // Randomize delay to minimize collisions with other units
//  delay(random(min_delay, max_delay));

  // Enter power down state for 8 s with ADC and BOD module disabled
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
//  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);

  // Small random delay to minimize collisions
  // FIXME: Probably not necessary with long sleep
  delay(random(500, 1000));

  // Increment the counter
  i++;
  if (i>=5000)
  {
    i = 0;
  }
  
}
