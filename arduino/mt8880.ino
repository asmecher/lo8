#include "pins.h"
#include "mt8880.h"

#include <PinChangeInt.h>

#define WHICH_INDEX(x) ((x)-1)
#define OTHER_WHICH(x) ((x)==LEFT?RIGHT:LEFT)
#define OTHER_WHICH_INDEX(x) (WHICH_INDEX(OTHER_WHICH(x)))
#define ASSEMBLE_NUMBER(d3,d2,d1,d0) (((d3)*8)+((d2)*4)+((d1)*2)+(d0))

#define RESERVE_BASE 248
#define INVERT_LEFT 2
#define INVERT_RIGHT 1

//#define DEBUG

volatile unsigned long channelTimes[] = {0, 0};
volatile unsigned char channelData[2];
boolean ignoreInterrupts = false;

/**
 * Write data to the MT8880 chip(s)
 * @param d3 boolean
 * @param d2 boolean
 * @param d1 boolean
 * @param d0 boolean
 * @param rs0 boolean
 * @param which int Which chip(s) to send to (LEFT|RIGHT)
 */
void dataWrite(boolean d3, boolean d2, boolean d1, boolean d0, boolean rs0, unsigned char which) {
  digitalWrite(RW_PIN, LOW);
  digitalWrite(RS0_PIN, rs0);
  pinMode(D3_PIN, OUTPUT);
  pinMode(D2_PIN, OUTPUT);
  pinMode(D1_PIN, OUTPUT);
  pinMode(D0_PIN, OUTPUT);
  digitalWrite(D0_PIN, d0);
  digitalWrite(D1_PIN, d1);
  digitalWrite(D2_PIN, d2);
  digitalWrite(D3_PIN, d3);
  if (which & LEFT) digitalWrite(LEFT_CS_PIN, LOW);
  if (which & RIGHT) digitalWrite(RIGHT_CS_PIN, LOW);
  delay(5);
  digitalWrite(O2_PIN, HIGH);
  delay(5);
  digitalWrite(O2_PIN, LOW);
  delay(10);
  if (which & LEFT) digitalWrite(LEFT_CS_PIN, HIGH);
  if (which & RIGHT) digitalWrite(RIGHT_CS_PIN, HIGH);
  delay(3);
}

/**
 * Read data from an MT8880 chip
 * @param d3 pointer to boolean
 * @param d2 pointer to boolean
 * @param d1 pointer to boolean
 * @param d0 pointer to boolean
 * @param rs0 boolean
 * @param which int Which chip(s) to send to (LEFT|RIGHT)
 */
void dataRead(boolean *d3, boolean *d2, boolean *d1, boolean *d0, boolean rs0, unsigned char which) {
  pinMode(D3_PIN, INPUT);
  pinMode(D2_PIN, INPUT);
  pinMode(D1_PIN, INPUT);
  pinMode(D0_PIN, INPUT);
  if (which & LEFT) digitalWrite(LEFT_CS_PIN, LOW);
  if (which & RIGHT) digitalWrite(RIGHT_CS_PIN, LOW);
  delay(5);
  digitalWrite(RW_PIN, HIGH);
  digitalWrite(RS0_PIN, rs0);
  digitalWrite(O2_PIN, HIGH);
  delay(5);
  *d0 = digitalRead(D0_PIN);
  *d1 = digitalRead(D1_PIN);
  *d2 = digitalRead(D2_PIN);
  *d3 = digitalRead(D3_PIN);
  if (which & LEFT) digitalWrite(LEFT_CS_PIN, HIGH);
  if (which & RIGHT) digitalWrite(RIGHT_CS_PIN, HIGH);
  digitalWrite(O2_PIN, LOW);
  digitalWrite(RW_PIN, LOW);
  digitalWrite(RS0_PIN, LOW);
  delay(5);
}

/**
 * Reset both MT8880 chips
 */
void setupMT8880() {
  // Pin modes
  pinMode(LEFT_IRQ_PIN, INPUT);
  pinMode(RIGHT_IRQ_PIN, INPUT);
  digitalWrite(LEFT_IRQ_PIN, HIGH); // Enable pullup
  digitalWrite(RIGHT_IRQ_PIN, HIGH); // Enable pullup
  pinMode(O2_PIN, OUTPUT);
  pinMode(RS0_PIN, OUTPUT);
  pinMode(LEFT_CS_PIN, OUTPUT);
  pinMode(RIGHT_CS_PIN, OUTPUT);
  pinMode(RW_PIN, OUTPUT);
  pinMode(D0_PIN, INPUT);
  pinMode(D1_PIN, INPUT);
  pinMode(D2_PIN, INPUT);
  pinMode(D3_PIN, INPUT);

  // MT8880 Initial state
  digitalWrite(LEFT_CS_PIN, HIGH); // Disable left
  digitalWrite(RIGHT_CS_PIN, HIGH); // Disable right
  digitalWrite(O2_PIN, LOW);
  digitalWrite(RS0_PIN, LOW);
  digitalWrite(RW_PIN, HIGH);

  delay(100); // Permit device to ready itself for reset
  
  // Reset process (see https://docs.isy.liu.se/twiki/pub/VanHeden/DataSheets/mt8880.pdf P12)
  boolean d0, d1, d2, d3;
  dataRead(&d3, &d2, &d1, &d0, 1, LEFT); // Read status register
  dataRead(&d3, &d2, &d1, &d0, 1, RIGHT); // Read status register
  dataWrite(0, 0, 0, 0, 1, LEFT | RIGHT);
  dataWrite(0, 0, 0, 0, 1, LEFT | RIGHT);
  dataWrite(1, 0, 0, 0, 1, LEFT | RIGHT);
  dataWrite(0, 0, 0, 0, 1, LEFT | RIGHT);
  dataRead(&d3, &d2, &d1, &d0, 1, LEFT);
  dataRead(&d3, &d2, &d1, &d0, 1, RIGHT);
  
  dataWrite(1, 1, 0, 0, 1, LEFT | RIGHT); // CRA: Enable TOUT, IRQ, next write CRB
  dataWrite(0, 0, 0, 1, 1, LEFT | RIGHT); // CRB: DTMF; disable test; disable tone burst

  PCintPort::attachInterrupt(LEFT_IRQ_PIN, &leftMtIrqFunc, FALLING);
  PCintPort::attachInterrupt(RIGHT_IRQ_PIN, &rightMtIrqFunc, FALLING);
}

void leftMtIrqFunc() {
  mtIrqFunc(LEFT);
}

void rightMtIrqFunc() {
  mtIrqFunc(RIGHT);
}

void setIgnoreInterrupts(boolean shouldIgnore) {
  ignoreInterrupts = shouldIgnore;
}

unsigned char invertFlags=0;

void mtIrqFunc(unsigned char which) {
  boolean d0, d1, d2, d3;
  unsigned long now=millis();
  dataRead(&d3, &d2, &d1, &d0, 1, which); // Clear IRQ
  
  if (ignoreInterrupts) return;
  
  dataRead(&d3, &d2, &d1, &d0, 0, which); // Get data
  
  #ifdef DEBUG
    sprintf((char *) bufs[curBuf++], "%s%c%c%c%c\n",
      which==RIGHT?"    ":"",
      d3?'1':'0',
      d2?'1':'0',
      d1?'1':'0',
      d0?'1':'0'
    );
  #endif
  
  if (channelTimes[WHICH_INDEX(which)]!=0) {
    #ifdef DEBUG
      sprintf((char *) bufs[curBuf++], "FAULT: Missed data on the %s channel.\n",
        which==LEFT?"right":"left"
      );
    #endif
  }
  channelTimes[WHICH_INDEX(which)]=now;
  channelData[WHICH_INDEX(which)]=ASSEMBLE_NUMBER(d3,d2,d1,d0);
  if (channelTimes[OTHER_WHICH_INDEX(which)]!=0) {
    // There is data on the other channel.
    if (channelTimes[OTHER_WHICH_INDEX(which)]+MAX_PATIENCE>=now) {
      // Fresh data on the other channel. Use it. */
      if (invertFlags) {
        // We received an escape earlier. Invert as per those.
        if (invertFlags & INVERT_LEFT) channelData[WHICH_INDEX(LEFT)] = 15 - channelData[WHICH_INDEX(LEFT)];
        if (invertFlags & INVERT_RIGHT) channelData[WHICH_INDEX(RIGHT)] = 15 - channelData[WHICH_INDEX(RIGHT)];
        
        sprintf((char *) bufs[curBuf++], "%c%s",
          (channelData[WHICH_INDEX(LEFT)] << 4) | channelData[WHICH_INDEX(RIGHT)],
          #ifdef DEBUG
            "\n"
          #else
            ""
          #endif
        );
        invertFlags = 0;
      } else {
        unsigned char c = (channelData[WHICH_INDEX(LEFT)] << 4) | channelData[WHICH_INDEX(RIGHT)];
        if (c>RESERVE_BASE && c<=(RESERVE_BASE|INVERT_LEFT|INVERT_RIGHT)) {
          // It's a reserved sequence; store the flags for the next byte.
          invertFlags = c & (INVERT_LEFT | INVERT_RIGHT);
        } else {
          // It's a normal character.
          sprintf((char *) bufs[curBuf++], "%c%s", c,
            #ifdef DEBUG
              "\n"
            #else
              ""
            #endif
          );
        }
      }
    } else {
      #ifdef DEBUG
      sprintf((char *) bufs[curBuf++], "FAULT: Old data chucked from the %s channel.",
        which==LEFT?"right":"left"
      );
      #endif
    }
    channelTimes[WHICH_INDEX(LEFT)] = channelTimes[WHICH_INDEX(RIGHT)] = 0;
  }
}

/**
 * Write a tone to one or both MT8880 chips
 * @param which Which chip(s) to read to (LEFT|RIGHT)
 * @param c Tone to write
 * @param wait True iff the function should delay TONE_DELAY after writing
 */
void setTone(unsigned char which, unsigned char c) {
  dataWrite(c&8, c&4, c&2, c&1, 0, which); // Set tone
}

void toneOn(unsigned char which) {
  dataWrite(0, 1, 0, 1, 1, which); // CRA: Enable TOUT, IRQ
}

void toneOff(unsigned char which) {
  dataWrite(0, 1, 0, 0, 1, which); // CRA: Disable TOUT, enable IRQ
}

void writeData(unsigned char d) {
  unsigned char leftHalf = d/16, rightHalf=d%16;
  unsigned char modifiers = 0;
  
  // If we've encountered problem characters or the escape character...
  if (leftHalf==0 || leftHalf==3 || leftHalf==6 || leftHalf==0xd || leftHalf==0xe) modifiers |= INVERT_LEFT;
  if (rightHalf==0 || rightHalf==3 || rightHalf==13 || rightHalf==14) modifiers |= INVERT_RIGHT;
  if (d==(RESERVE_BASE|INVERT_LEFT) || d==(RESERVE_BASE|INVERT_RIGHT) || d==(RESERVE_BASE|INVERT_LEFT|INVERT_RIGHT)) {
    modifiers = INVERT_LEFT | INVERT_RIGHT;
  }
  if (modifiers != 0) {
    #ifdef DEBUG
      Serial.print("Special character: ");
    #endif
    // Send the escape tone
    unsigned char c=RESERVE_BASE|modifiers;
    setTone(LEFT, c/16);
    setTone(RIGHT, c%16);
    toneOn(LEFT | RIGHT);
    delay(TONE_LENGTH);
    toneOff(LEFT | RIGHT);
    delay(TONE_GAP);

    #ifdef DEBUG
      Serial.print(c, BIN);
      Serial.print(' ');
    #endif
    
    // Figure out what to send for the second byte
    if (modifiers & INVERT_LEFT) leftHalf = 15-leftHalf;
    if (modifiers & INVERT_RIGHT) rightHalf = 15-rightHalf;
    #ifdef DEBUG
      Serial.println((leftHalf << 4) | rightHalf, BIN);
    #endif
  } else {
    #ifdef DEBUG
      Serial.print("Normal character: ");
      Serial.println(d, BIN);
    #endif
  }
  
  // It's a normal tone, or we've already processed it above and sent an escape.
  setTone(LEFT, leftHalf);
  setTone(RIGHT, rightHalf);
  toneOn(LEFT | RIGHT);
  delay(TONE_LENGTH);
  toneOff(LEFT | RIGHT);
  delay(TONE_GAP);
}

