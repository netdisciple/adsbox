#ifndef _TCP_AVR_H_
#define _TCP_AVR_H_

#include <sys/socket.h>
#include <netdb.h>

#define AVR_SERVER_PORT 30004

typedef struct {
	int id;
	char addr[64];
	int port;
	double lat;
	double lon;
} tcp_info;

void *tcp_client(void *);
int db_insert_source_tcp(unsigned int *, char *);
int db_update_source_tcp_port(unsigned int*, unsigned int);
int run_source_tcp();

#endif
