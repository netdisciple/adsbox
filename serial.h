#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <termios.h>

#define DEFAULT_SERIAL_BAUDRATE B115200
#define DEFAULT_SERIAL_BITS CS8
#define DEFAULT_SERIAL_DEVICE "/dev/ttyS0"

typedef struct {
	int id;
	char device[256];
	long int baudrate;
	long int bits;
	double lat;
	double lon;
} serial_info;

void *serial_thread(void *);
int db_insert_source_serial(unsigned int*, char*);
int db_update_source_serial_baud(unsigned int*, long int*);
int db_update_source_serial_bits(unsigned int*, long int*);
int run_source_serial();

#endif
