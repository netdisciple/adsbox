#ifndef __MISC_H__
#define __MISC_H__

#define VERSION "20160417"
#define DEBUG(...) if(!dmode) {fprintf(stdout,__VA_ARGS__);fflush(stdout);}
void panic(char *msg);
#define panic(m) {perror(m); abort();}

#define BUFLEN 256

#define TYPE_SERIAL_AVR 0
#define TYPE_FILE 1
#define TYPE_TCP_AVR 2
#define TYPE_RTLSDR 3
#define TYPE_TCP_SBS3 4
#define TYPE_TCP_BEAST 5
#define TYPE_UDP_AVR 6

#include <stdint.h>

typedef struct {
	int source_id;
	unsigned char type;
	unsigned int source_bytes;
	unsigned char data[16];
	char avr_data[44];
	unsigned int data_len;
	uint32_t timestamp;
	double lat;
	double lon;
} adsb_data;

struct list_item {
    adsb_data list_data;
    struct list_item * next;
};
typedef struct list_item item;

void enqueue_data(adsb_data *);
unsigned int avr_data(adsb_data *, char *);

extern int dmode;

#endif
