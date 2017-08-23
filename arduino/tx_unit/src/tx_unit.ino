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

#define READSSIZE 10 // Number of readings to store

DHT dht(DHTPIN, DHTTYPE);
RH_ASK driver(BPS);

uint8_t uid; // Unique ID stored in EEPROM
int min_delay = 5000; // Min tx delay
int max_delay = 6000; // Max tx delay

// Structure for holding the sensor values
char data_flds[5] = {'U','T','H','V','C'}; //Field sensor/value codes
typedef struct {
  int tempf = -999;
  int humid = -999;
  unsigned int batt_volts = 0;
  unsigned int seq;
  unsigned long time;
} vals;

vals reads_stack[READSSIZE];
vals *cur_read;

void setup()
{
    pinMode(TXLED, OUTPUT);
    dht.begin();

    // Load the board UID that was previously put
    EEPROM.get(0, uid);

    reads_stack[0].seq = 0;
    
    Serial.begin(9600);   // Debugging only
    if (!driver.init())
         Serial.println("init failed");
}

void loop()
{
  //NOTE: Float values are rounded and cast to integers for transmission
  
  // Get pointer to the current reading element
  cur_read = &reads_stack[0];

  // Read the temperature and humidity
  cur_read->humid = round(dht.readHumidity()*10);
  cur_read->tempf = round(dht.readTemperature(true)*10); //Farenheit
  // Compute the heat index
  //float heatidx = dht.computeHeatIndex(tempf, humid);

  // Read the battery voltage
  cur_read->batt_volts = round(analogRead(VMPIN) * 5.0/1023.0 * 2 * 10); // FIXME: Adjust *2 for measured resistor values

  // Format the message string to transmit
  char msg[30];
  sprintf(msg, "U:%04d;T:%04d;H:%03d;V:%03d;C:%d"
      , uid, cur_read->tempf, cur_read->humid
      , cur_read->batt_volts, cur_read->seq);
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
//  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
//  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
//  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);

  // Small random delay to minimize collisions
  // FIXME: Probably not necessary with long sleep
  //delay(random(500, 1000));

  // Testing
  delay(random(5000, 6000));

  // Shift the values array
  for (int j=READSSIZE; j-->1;){
	reads_stack[j] = reads_stack[j-1];
  }

  // Increment the counter
  cur_read->seq++;
  if (cur_read->seq>=5000)
  {
    cur_read->seq = 0;
  }
  cur_read->tempf = 9999;
  cur_read->humid = 9999;
  cur_read->batt_volts = 9999;

}
