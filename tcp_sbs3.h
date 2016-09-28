#ifndef _TCP_SBS3_H_
#define _TCP_SBS3_H_

#include <sys/socket.h>
#include <netdb.h>

#define DLE 0x10
#define STX 0x02
#define ETX 0x03

#define STATE_WAIT          0
#define STATE_DLE           1
#define STATE_STX           2
#define STATE_ETX           3
#define STATE_COLLECT       4

#define MODE_UNKNOWN        0
#define MODE_SHORT          1
#define MODE_LONG           2

#define KAL_PKT_MODES_ADSB 0x01
#define KAL_PKT_MODES_LONG 0x05
#define KAL_PKT_MODES_SHORT 0x07

typedef struct {
	int id;
	char addr[64];
	int port;
	double lat;
	double lon;
} tcp_sbs3_info;

void *tcp_sbs3_client(void *);
int db_insert_source_tcp_sbs3(unsigned int *, char *);
int db_update_source_tcp_sbs3_port(unsigned int*, unsigned int);
int run_source_tcp_sbs3();

#endif
