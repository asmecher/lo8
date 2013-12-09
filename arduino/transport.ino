#include "pins.h"
#include "transport.h"

#include <PinChangeInt.h>

boolean motorState = false;
boolean ffState = false;
volatile boolean hitEOT=false;

void eotFunc() {
  hitEOT=true;
}

void setupTransport() {    
  // Miscellaneous pins  
  pinMode(TRACK_SENSOR_PIN, INPUT);
  pinMode(TRACK_SOLENOID_PIN, OUTPUT);
  pinMode(FF_RELAY_PIN, OUTPUT);
  pinMode(MOTOR_RELAY_PIN, OUTPUT);
  pinMode(EOT_PIN, INPUT);
  pinMode(TAPE_SWITCH_PIN, INPUT);
  
  digitalWrite(EOT_PIN, HIGH); // Enable pullup
  digitalWrite(TAPE_SWITCH_PIN, HIGH); // Enable pullup
  digitalWrite(TRACK_SOLENOID_PIN, LOW);
  digitalWrite(FF_RELAY_PIN, LOW);
  digitalWrite(MOTOR_RELAY_PIN, LOW);

  // We use an interrupt on this pin to make sure we don't miss an EOT during e.g. write delay
  PCintPort::attachInterrupt(EOT_PIN, &eotFunc, FALLING);
}

/**
 * Get the current track.
 * @return int 0-3
 */
unsigned char getTrack() {
  int reading = analogRead(TRACK_SENSOR_PIN);
  if (reading < 600) return 2;
  if (reading < 750) return 0;
  if (reading < 850) return 1;
  return 3;
}

/**
 * Go to the desired track.
 * @param track TRACK_x
 */
void gotoTrack(unsigned char track) {
  boolean prevMotorState = motorState;
  if (!prevMotorState) {
    startMotor();
    delay(200);
  }
  while (getTrack() != track) {
    digitalWrite(TRACK_SOLENOID_PIN, 1);
    delay(100);
    digitalWrite(TRACK_SOLENOID_PIN, 0);
    delay(200);
  }
  if (!prevMotorState) stopMotor();
}

/**
 * Return true iff the end-of-tape marker is present
 * @param reset True iff the EOT marker should be reset
 * @return boolean
 */
boolean getEOT(boolean reset) {
  boolean returner = hitEOT;
  if (hitEOT && reset) hitEOT = false;
  return returner;
}

/**
 * Return true iff a tape is inserted
 * @return boolean
 */
boolean getTapeIn() {
  return !digitalRead(TAPE_SWITCH_PIN);
}

/**
 * Start the motor
 * @return boolean True iff the motor had to be started
 */
boolean startMotor() {
  if (!motorState) {
    digitalWrite(MOTOR_RELAY_PIN, HIGH);
    motorState=true;
  }
  return false;
}

/**
 * Stop the motor
 * @return boolean True iff the motor had to be stopped
 */
boolean stopMotor() {
  if (motorState) {
    if (ffState) stopFF();
    digitalWrite(MOTOR_RELAY_PIN, LOW);
    motorState=false;
    return true;
  }
  return false;
}

/**
 * Start fast-forwarding
 * @return boolean True iff FF had to be started
 */
boolean startFF() {
  if (motorState && !ffState) {
    digitalWrite(FF_RELAY_PIN, HIGH);
    ffState=true;
    return true;
  }
  return false;
}

/**
 * Stop fast-forwarding
 * @return boolean True iff FF had to be stopped.
 */
boolean stopFF() {
  if (ffState) {
    ffState=false;
    digitalWrite(FF_RELAY_PIN, LOW);
    return true;
  }
  return false;
}

/**
 * Seek to the start of the tape.
 * @param track int Optional track to seek to in the process
 * @param $seekPast boolean True iff the tape should be positioned just past the EOT.
 * @return boolean True IFF the end of tape was encountered.
 */
boolean seekStart(int track, boolean seekPast) {
  boolean isEOT;
  boolean originalMotorState = motorState;
  startMotor();
  startFF();
  if (track != -1) {
    if (!originalMotorState) delay(200); // Give the motor time to start
    gotoTrack(track);
  }
  // Seek until we hit the EOT (or the tape is pulled)
  isEOT=false; // Force a reset before we check it
  while (!(isEOT = getEOT(false)) && getTapeIn());
  // If desired, seek past the EOT
  if (seekPast) while ((isEOT = getEOT(true)) && getTapeIn());
  stopFF();
  stopMotor();
  return isEOT;
}


