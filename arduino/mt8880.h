#ifndef MT8880_H
#define MT8880_H

#define TONE_LENGTH 125
#define TONE_GAP 50
#define MAX_PATIENCE TONE_LENGTH // Number if milliseconds we're willing to wait for channels to catch up

#define LEFT 1
#define RIGHT 2

/**
 * Reset both MT8880 chips
 */
void setupMT8880();

void writeData(unsigned char d);

#endif

