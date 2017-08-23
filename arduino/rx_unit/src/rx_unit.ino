#include <RH_ASK.h>
#include <SPI.h> // Not used but needed to compile
#include <Wire.h> // For I2C communication

#define BPS 4800 // TX baud rate
#define RXLED 13
#define I2C_ADDR 0x10

#define MAX_MSGS 5 // Size of the message stack
#define MSG_SIZE 100 // Size a message

RH_ASK rx_drvr(BPS);

uint8_t msgcnt = 0;
unsigned long last_rx = 0;
byte last_buf[50];
uint8_t last_buflen = 0;
int8_t send_msg_idx = 0;
uint8_t send_char_idx = 0;
uint8_t i2c_cmd = 0;

void setup()
{
  pinMode(RXLED, OUTPUT);

  Serial.begin(9600); // Debugging only
  if (!rx_drvr.init())
     Serial.println("init failed");

  Wire.begin(I2C_ADDR);
  Wire.onRequest(i2c_send);
  Wire.onReceive(i2c_receive);
}

// FIXME: Storing sensor data as a string is a waste of memory
typedef struct {
  char data[MSG_SIZE] = {0x0};
  uint8_t seq_num = 0; // Local sequential number
  unsigned long rcv_time = 0; // Time message was received locally
  uint8_t len = 0; // Data length
  uint8_t stat = 0; // Status 1-new; 2-transmitted
  unsigned int checksum = 0;
} data_message;

// Set up a FIFO queue for passing messages downstream
data_message messages[MAX_MSGS];

// Transient copy of the message being sent
data_message send_msg;

void loop()
{
    if ((millis() - last_rx) > 250)
    {
      digitalWrite(RXLED, LOW);
    }
    
    // TODO: Move the packet handling to a separate function
    uint8_t buf[50];
    uint8_t buflen = sizeof(buf);
    char data[50];
    if (rx_drvr.recv(buf, &buflen)) // Non-blocking
    {
      digitalWrite(RXLED, HIGH);
      last_rx = millis();
      
      // Skip this message if it is a retry
      // FIXME: Should this check the message number or compute a checksum?
      // See here for a simple checksum example, perhaps
      //   https://electronics.stackexchange.com/a/121712
      if (buflen==last_buflen && compare_buffers(buf, last_buf, buflen)){
        return;
      }
      
      // Copy the buffer data to a character
      memcpy(data, buf, buflen);
      data[buflen] = '\0';

      // FIXME: Get a realtime clock
      unsigned long msg_time;
      msg_time = millis();
      
      // Shift all the buffered messages
      for (uint8_t k=MAX_MSGS; k-->1; ){
//	Serial.println(k);
        messages[k] = messages[k-1];
      }

      // Load the current message buffer
      sprintf(messages[0].data,"%d,%lu,%s", msgcnt, msg_time, data);
      messages[0].len = strlen(messages[0].data);
      messages[0].stat = 1;
      messages[0].rcv_time = msg_time;
      messages[0].seq_num = msgcnt;

      messages[0].checksum = 0;
      for (uint8_t k; k<messages[0].len; k++){
	      messages[0].checksum += messages[0].data[k];
      }

      // Increment the message counter
      ++msgcnt;
      
      Serial.println(messages[0].data);
//      Serial.print(highByte(messages[0].checksum));
//      Serial.print(", ");
//      Serial.print(lowByte(messages[0].checksum));
//      Serial.print(", ");
      Serial.println(messages[0].checksum);

      // Copy the last buffer received to skip re-trys
      // FIXME: This could be grabbed from the queue
      memcpy(last_buf, buf, buflen);
      last_buflen = buflen;
    }
}

bool compare_buffers(uint8_t buff1[50], uint8_t buff2[50], uint8_t buflen) {
  // Compare two buffer arrays 
  // TODO: Add a checksum so the whole message does not need evaluation
  for (uint8_t i; i<buflen; i++)
  {
    if (buff1[i] != buff2[i])
    {
      return false;
    }
  }
  return true;
}

char m[60];
uint8_t ci = 0;
uint8_t cb = 0;

void i2c_send() {
//  sprintf(m, "I2C request: i2c_cmd=%d; idx=%d; len=%d", i2c_cmd, send_msg_idx, messages[send_msg_idx].len);

  switch (i2c_cmd){
    case 1:
      if (send_msg_idx >= 0){
        // Send number of bytes in the message
        Wire.write(send_msg.len);
        send_char_idx = 0;

      } else {
	// Nothing to send yet
	Wire.write((uint8_t)0);

      }

  //    Serial.println(m);
      break;

    case 2:
      // Send the next character
      if (send_char_idx >= send_msg.len+2) {
        Wire.write((uint8_t)0);
      
      } else {
	if (send_char_idx<send_msg.len){
          Wire.write(send_msg.data[send_char_idx]);
	} else {
	// Split the int checksum across the final 2 bytes
	// FIXME: Checksum isn't working
	  if (ci==0){
	    cb = highByte(send_msg.checksum);
            Wire.write(cb);
//	    Serial.print("High byte: ");
//	    Serial.println(cb);
	    ci = 1;
	  } else {
	    cb = lowByte(send_msg.checksum);
            Wire.write(cb);
//	    Serial.print("Low byte: ");
//	    Serial.println(cb);
	    ci = 0;
	  }
	}

      }

      ++send_char_idx;

//      if (send_char_idx==0) {
//        Serial.println(m);
//      }

     break;

    default:
      break;

  }
}

void i2c_receive(int nbytes) {
  // TODO: Implement a command protocol
  // Example - https://electronics.stackexchange.com/a/121712
  while (Wire.available()>0){
    i2c_cmd = (uint8_t)Wire.read();
  }

  switch (i2c_cmd){
    case 1:
      // Set the index to the oldest unread message
      send_msg_idx = -1; // Flag for no unread messages
      for (uint8_t i = MAX_MSGS; i-- > 0;){
        if (messages[i].stat==1){
	  send_msg_idx = i; 
          break;
	}
      }
      
      // If all packets have been sent upstream
      if (send_msg_idx<0){
        break;
      }

      // Copy the message while it's being sent
//      send_msg = messages[send_msg_idx];
//      memcpy(&send_msg, &messages[send_msg_idx], sizeof(data_message));
      strncpy(send_msg.data, messages[send_msg_idx].data, messages[send_msg_idx].len);
      send_msg.seq_num = messages[send_msg_idx].seq_num;
      send_msg.rcv_time = messages[send_msg_idx].rcv_time;
      send_msg.len = messages[send_msg_idx].len;
      send_msg.stat = messages[send_msg_idx].stat;
      send_msg.checksum = messages[send_msg_idx].checksum;
      
      send_char_idx = 0;
           
      Serial.print("Send Msg: "); 
      Serial.println(send_msg.data);

      break;

    case 2:
      // Send the message
      break;

    case 3:
      // Done with current message
      // Find it in the queue and set the status
      for (uint8_t i=0; i<MAX_MSGS; i++){
        if (messages[i].seq_num==send_msg.seq_num){
          messages[i].stat=2;
	  break;
	}
      }
      break;

    case 4:
      // Resend the current message
      send_msg.stat = 1;
      send_char_idx = 0;
      i2c_cmd = 2;
      break;
  }
 
//  sprintf(m, "I2C received: %d", i2c_cmd);
//  Serial.println(m);

}

