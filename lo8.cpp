/**
 * lo8: An 8-track based tape drive control application.
 * This can be used to back up and restore content from a Linux host.
 * Copyright (c) 2013 by Alec Smecher
 * http://www.cassettepunk.com
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <getopt.h>
#include <malloc.h>
#include <stdlib.h>
#include <signal.h>

// Error return codes (e.g. for use with lo8 in a shell script)
#define LO8_SYNTAX_ERROR	-1
#define LO8_NO_TAPE		-2

const char *no_tape_error = "Error: Tape not inserted.\n";

// Command codes to interchange with the lo8 device
#define GET_STATUS	0
#define SET_TRACK	1
#define SEEK		2
#define START_MOTOR	3
#define STOP_MOTOR	4
#define WRITE		5
#define START_WRITE	6
#define STOP_WRITE	7
#define DATA		8
#define DATA_EOT	9
#define RESET_EOT	10

const char *default_device="/dev/ttyUSB1";
int default_baud_const=B9600;

/**
 * Read a command/byte sequence from the tape drive.
 * @param fd int File descriptor for device
 * @return int Two-byte sequence returned as a 16-bit int, -1 on error
 */
int readLo8(int fd) {
	unsigned char buf[2];
	int i=0;
	while (true) {
		int n=read(fd, &(buf[i]), 1);
		if (n<0) {
			perror("ERROR: Unable to read command.\n");
			return -1;
		}
		i+=n;
		if (i==2) break;
	}
	return *((int *) buf);
}

/**
 * Send a 2-byte command/data pair to the tape drive.
 * @param fd int File descriptor for device
 * @param cmd unsigned char Command (see constants defined at head of file)
 * @param data unsigned char Optional data byte to include with the command
 * @return int Data returned from drive, or -1 on error
 */
int sendLo8(int fd, unsigned char cmd, unsigned char data=0) {
	unsigned char buf[] = {cmd, data};
	if (write(fd, buf, 2)!=2) {
		perror("ERROR: Unable to send command.\n");
		return -1;
	}
	*((int *)buf) = readLo8(fd);
	if (buf[0] != cmd) {
		fprintf(stderr, "ERROR: The command was not echoed back. Got %i instead of %i.\n", buf[0], cmd);
		return -1;
	}
	return buf[1];
}

/**
 * Get the current track number
 * @param fd int File descriptor
 * @return int Track number (0-3)
 */
int getTrack(int fd) {
	return sendLo8(fd, GET_STATUS) & 0x03;
}

/**
 * Determine whether or not a tape is inserted.
 * @param fd int File descriptor
 * @return int 0 iff no tape is inserted
 */
int getTapeIn(int fd) {
	return sendLo8(fd, GET_STATUS) & 0x08;
}

/**
 * Determine whether or not EOT (end of tape) has been encountered.
 * @param fd int File descriptor
 * @return int 0 iff no EOT has been encountered
 */
int getEOT(int fd) {
	return sendLo8(fd, GET_STATUS, 1) & 0x04;
}

/**
 * Reset the EOT flag.
 * @param fd int File descriptor
 */
int resetEOT(int fd) {
	return sendLo8(fd, RESET_EOT);
}

/**
 * Go to the specified track.
 * @param fd int File descriptor
 * @param track int Track to skip to (0-3)
 */
void setTrack(int fd, int track) {
	sendLo8(fd, SET_TRACK, track);
}

/**
 * Seek to the beginning of the tape.
 * @param fd int File descriptor
 */
void doSeek(int fd) {
	sendLo8(fd, SEEK, 0);
}

/**
 * Start a write process.
 * @param fd int File descriptor
 */
void startWrite(int fd) {
	sendLo8(fd, START_WRITE);
}

/**
 * Stop a write process.
 * @param fd int File descriptor
 */
void stopWrite(int fd) {
	sendLo8(fd, STOP_WRITE);
}

/**
 * Write a byte.
 * @param fd int File descriptor
 * @param b unsigned char Data to write
 */
bool doWrite(int fd, unsigned char b) {
	return (sendLo8(fd, WRITE, b));
}

/**
 * Start a read process.
 * @param fd int File descriptor
 */
void startMotor(int fd) {
	sendLo8(fd, START_MOTOR);
}

/**
 * Stop a read process.
 * @param fd int File descriptor
 */
void stopMotor(int fd) {
	sendLo8(fd, STOP_MOTOR);
}

/**
 * Display usage help text
 */
void usage() {
	printf(	"Lo8 8-Track tape drive controller\n"
		"Copyright (c) 2013 by Alec Smecher (http://www.cassettepunk.com)\n"
		"Usage: lo8 [OPTIONS]\n"
		"Options:\n"
		"	-b <bps>	Set baud rate (default 9600)\n"
		"	-d <device>	Set device name (default %s)\n"
		"	-t <track>	Set track number before starting\n"
		"	-s 		Seek to beginning of track before starting\n"
		"	-r		Read data from the tape and dump to stdin\n"
		"	-w		Write data from stdin to the tape\n"
		"	-e		(Used with -w) Echo input to stdout\n"
		"	-i		Query and display status information\n"
		"\n"
		"Reading and writing may not be performed simultaneously. Tapes must\n"
		"be inserted with the record button pressed in order to record, and\n"
		"cannot be read in that mode.\n"
		"\n"
		"If the -i flag is specified, information will be queried after seek\n"
		"and track switching operations have been completed (if specified).\n"
		"\n",
		default_device
	);
}	

bool interrupt;
void int_handler(int signo) {
	if (signo==SIGINT) {
		interrupt=true;
	}
}

/**
 * Main function
 * @param argc int
 * @param argv char **
 * @return int
 */
int main(int argc, char **argv) {
	static struct option loptions[] = {
		{"help",	no_argument,		0,	'h'},
		{"device",	required_argument,	0,	'd'},
		{"baud",	required_argument,	0,	'b'},
		{"track",	required_argument,	0,	't'},
		{"seek",	no_argument,		0,	's'},
		{"read",	no_argument,		0,	'r'},
		{"write",	no_argument,		0,	'w'},
		{"info",	no_argument,		0,	'i'},
		{"echo",	no_argument,		0,	'e'},
		{NULL,		0,			0,	0}
	};

	// Defaults
	char *device = NULL;
	int baud_const=default_baud_const;
	int track = -1;
	bool seek=false,readRequested=false,writeRequested=false,info=false,echo=false;

	// Parse parameters
	while (1) {
		int oindex;
		int opt = getopt_long(argc, argv, "hb:d:t:srwie", loptions, &oindex);
		if (opt==-1) break; // Done parsing options
		switch (opt) {
			case 'h': // Help
				usage();
				return 0;
			case 'd': // Serial device
				device = strdup(optarg);
				break;
			case 'b': // Baud rate
				switch (strtol(optarg, NULL, 10)) {
					case 4800: baud_const=B4800; break;
					case 9600: baud_const=B9600; break;
					case 19200: baud_const=B19200; break;
					case 38400: baud_const=B38400; break;
					case 57600: baud_const=B57600; break;
					case 115200: baud_const=B115200; break;
					default: usage(); return -1; // Invalid
				}
				break;
			case 't': // Track
				switch(strtol(optarg, NULL, 10)) {
					case 1: track=0; break;
					case 2: track=1; break;
					case 3: track=2; break;
					case 4: track=3; break;
					default: usage(); return -1; // Invalid
				}
				break;
			case 's': seek=true; break; // Seek
			case 'i': info=true; break; // Info
			case 'r': readRequested=true; break; // Read
			case 'w': writeRequested=true; break; // Write
			case 'e': echo=true; break; // Echo
			default: // Unknown
				usage();
				return LO8_SYNTAX_ERROR;
		}
	}

	// If no device filename was specified, use the default.
	if (device==NULL) device=strdup(default_device);

	// Check validity of options
	if (
		(readRequested && writeRequested) || // Read and write cannot both be requested
		(!writeRequested && echo) // Echo cannot be requested without write
	) {
		usage();
		free(device);
		return LO8_SYNTAX_ERROR;
	}

	int fd;
	struct termios options;

	// Open the device
	fd = open(device, O_RDWR|O_NOCTTY);
	free(device); // Free allocated device name string
	if (fd == -1) {
		perror("open_port: Unable to open /dev/ttyUSB1");
		return -1;
	} else {
		fcntl(fd, F_SETFL, O_SYNC);
	}

	// Set the baud rate and options
	tcgetattr(fd, &options);
	cfsetispeed(&options, baud_const);
	cfsetospeed(&options, baud_const);
	tcsetattr(fd, TCSANOW, &options);

	// Perform early tasks on the tape drive
	if (track != -1) {
		if (!getTapeIn(fd)) {
			fputs(no_tape_error, stderr);
			close(fd);
			return LO8_NO_TAPE;
		}
		setTrack(fd, track);
	}
	if (seek) {
		if (!getTapeIn(fd)) {
			fputs(no_tape_error, stderr);
			close(fd);
			return LO8_NO_TAPE;
		}
		doSeek(fd);
	}
	if (info) {
		printf("Track: %i\n", getTrack(fd)+1); // 0-based
		printf("Tape: %s\n", getTapeIn(fd)?"Inserted":"Absent");
		printf("EOT: %s\n", getEOT(fd)?"Present":"Absent");
	} else resetEOT(fd);

	// Set up for read/write operations
	signal(SIGINT, int_handler);
	interrupt=false;

	// Handle read operations, if requested.
	if (readRequested) {
		if (!getTapeIn(fd)) {
			fputs(no_tape_error, stderr);
			close(fd);
			return LO8_NO_TAPE;
		}

		startMotor(fd);
		bool eot=false;
		while (!interrupt && !eot) {
			unsigned int d = readLo8(fd);
			unsigned char *buf = (unsigned char *) &d;
			switch (buf[0]) {
				case DATA_EOT:
					eot=true;
				case DATA:
					putc(buf[1], stdout);
					fflush(stdout);
					break;
				default:
					fprintf(stderr, "Unknown command: %i", d);
					break;
			}
			usleep(1000);
		}
		stopMotor(fd);
	}

	if (writeRequested) {
		if (!getTapeIn(fd)) {
			fputs(no_tape_error, stderr);
			close(fd);
			return LO8_NO_TAPE;
		}

		startWrite(fd);
		sleep(1); // Provide a quick break before starting
		bool eot=false;
		while (!interrupt && !eot && !feof(stdin)) {
			int i;
			if ((i=getc(stdin))>=0) {
				eot=doWrite(fd, i);
				if (echo) putc(i, stdout);
				fflush(stdout);
			}
			usleep(1000);
		}
		stopWrite(fd);
	}

	close(fd);
	return 0;
}
