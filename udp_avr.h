#ifndef _UDP_AVR_H_
#define _UDP_AVR_H_

typedef struct {
	int id;
	int port;
	double lat;
	double lon;
} udp_info;

typedef struct {
	char addr[64];
	int port;
} udp_push_info;

extern char grawdata[BUFLEN];

int db_insert_source_udp_avr(unsigned int *, unsigned int);
int run_source_udp_avr();
void *udp_server(void *);
void *udp_avr_push(void *);

#endif
