#include "pins.h"
#include "transport.h"
#include "mt8880.h"

#define GET_STATUS  0
#define SET_TRACK   1
#define SEEK        2
#define START_MOTOR 3
#define STOP_MOTOR  4
#define WRITE       5
#define START_WRITE 6
#define STOP_WRITE  7
#define DATA        8
#define DATA_EOT    9
#define RESET_EOT   10

#define BUF_LENGTH 80
#define NUM_BUFS 10
unsigned char bufs[NUM_BUFS][BUF_LENGTH];
volatile int curBuf=0;

void setup() {
  for (int i=0; i<NUM_BUFS; i++) bufs[i][0] = 0;
  Serial.begin(9600);

  setupMT8880();
  setupTransport();
}

void loop() {
  // Dump the serial buffer
  noInterrupts();
  for (int i=0; i<NUM_BUFS; i++) {
    unsigned char *l = bufs[i];
    while (*l != 0) {
      Serial.write(getEOT()?DATA_EOT:DATA);
      Serial.write(*l);
      l++;
    }
    bufs[i][0] = 0;
  }
  curBuf=0;
  interrupts();
  
  if (Serial.available() >= 2) {
    unsigned char c=Serial.read(); // Command
    unsigned char d=Serial.read(); // Data
    switch (c) {
      // Driver-mode interactions
      case GET_STATUS:
        Serial.write((unsigned char) GET_STATUS);
        Serial.write((unsigned char) (getTrack() | (getEOT(d)<<2) | (getTapeIn()<<3)));
        break;
      case SET_TRACK:
        gotoTrack(d);
        Serial.write(SET_TRACK);
        Serial.write(d);
        break;
      case SEEK:
      case 'a':
        seekStart(-1, true);
        Serial.write((unsigned char) SEEK);
        Serial.write((unsigned char) 0);
        break;
      case START_MOTOR:
        startMotor();
        Serial.write((unsigned char) START_MOTOR);
        Serial.write((unsigned char) getEOT());
        break;
      case STOP_MOTOR:
        stopMotor();
        Serial.write((unsigned char) STOP_MOTOR);
        Serial.write((unsigned char) 0);
        break;
      case WRITE:
        writeData(d);
        Serial.write((unsigned char) WRITE);
        Serial.write((unsigned char) getEOT());
        break;
      case START_WRITE:
        setIgnoreInterrupts(true);
        startMotor();
        Serial.write((unsigned char) START_WRITE);
        Serial.write((unsigned char) 0);
        break;
      case STOP_WRITE:
        setIgnoreInterrupts(false);
        stopMotor();
        Serial.write((unsigned char) STOP_WRITE);
        Serial.write((unsigned char) 0);
        break;
      case RESET_EOT:
        Serial.write((unsigned char) RESET_EOT);
        Serial.write((unsigned char) getEOT(true));
        break;
    }
  }
}
