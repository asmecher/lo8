#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <Arduino.h>

/*#define TRACK_1 1
#define TRACK_2 2
#define TRACK_3 3
#define TRACK_4 4*/

/**
 * Set up the transport resources.
 */
void setupTransport();

/**
 * Get the current track.
 * @return int TRACK_x
 */
unsigned char getTrack();

/**
 * Go to the desired track.
 * @param track TRACK_x
 */
void gotoTrack(unsigned char track);

/**
 * Return true iff the end-of-tape marker is present
 * @param reset True iff the EOT marker should be reset
 * @return boolean
 */
boolean getEOT(boolean reset=false);

/**
 * Return true iff a tape is inserted
 * @return boolean
 */
boolean getTapeIn();

/**
 * Start the motor
 * @return boolean True iff the motor had to be started
 */
boolean startMotor();

/**
 * Stop the motor
 * @return boolean True iff the motor had to be stopped
 */
boolean stopMotor();

/**
 * Start fast-forwarding
 * @return boolean True iff FF had to be started
 */
boolean startFF();

/**
 * Stop fast-forwarding
 * @return boolean True iff FF had to be stopped.
 */
boolean stopFF();

/**
 * Seek to the start of the tape.
 * @param track int Optional track to seek to in the process
 * @param $seekPast boolean True iff the tape should be positioned just past the EOT.
 * @return boolean True IFF the end of tape was encountered.
 */
boolean seekStart(int track = -1, boolean seekPast = false);

#endif

