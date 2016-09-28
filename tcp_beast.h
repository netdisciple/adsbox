#ifndef _TCP_BEAST_H_
#define _TCP_BEAST_H_

typedef struct {
	int id;
	char addr[64];
	int port;
	double lat;
	double lon;
} tcp_beast_info;

#define ESC 0x1a

void *tcp_beast_client(void *);
int db_insert_source_tcp_beast(unsigned int *, char *);
int db_update_source_tcp_beast_port(unsigned int*, unsigned int);
int run_source_tcp_beast();
#endif
