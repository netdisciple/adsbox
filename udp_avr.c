/*
 *	adsbox - ADS-B decoder software
 *	(c) atty 2011-14 romulmsg@mail.ru
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include "misc.h"
#include "udp_avr.h"
#include "db.h"

extern sqlite3 *db;

int db_insert_source_udp_avr(unsigned int *id, unsigned int port) {

	if (db_exec_sql("INSERT INTO SOURCE_UDP (ID, PORT) VALUES (%i, %i)", *id, port))
	    return 1;

	return 0;
}

int run_source_udp_avr() {
	char ** results;
	int rows, cols, i;
	char *zErrMsg = 0;
	char sql[] = "SELECT ID, PORT, LAT, LON FROM SOURCE_UDP ORDER BY ID";
	pthread_t child;

	if (sqlite3_get_table(db, sql, &results, &rows, &cols, &zErrMsg) != SQLITE_OK) {
		DEBUG("SQLite error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_free_table(results);
		return 1;
	}

	for (i = 1; i <= rows; i++) {
		udp_info *ui = malloc(sizeof(udp_info));
		ui->id = atoi(results[i * cols]);
		ui->port = atoi(results[i * cols + 1]);
		ui->lat = atof(results[i * cols + 2]);
		ui->lon = atof(results[i * cols + 3]);
		pthread_create(&child, NULL, &udp_server, ui); /* start udp server thread */
		pthread_detach(child);
	}

	sqlite3_free_table(results);
	return 0;
}

void *udp_server(void *arg) {
	udp_info * ui = (udp_info *) arg;
	int s;
	struct sockaddr_in serv_addr, client_addr;
	socklen_t len = sizeof(client_addr);
	char rawbuf[BUFLEN];
	adsb_data *data = malloc(sizeof(adsb_data));
	char * tok;

	data->source_id = ui->id;
	data->type = TYPE_UDP_AVR; // type of source
	data->lat = ui->lat;
	data->lon = ui->lon;
	data->source_bytes = 0;

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		panic("udp_server socket()");

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(ui->port);
	if (bind(s, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0)
		panic("udp_server bind()");

	DEBUG("Run UDP server on port %i\n", ui->port);

	while (1) {
		bzero(rawbuf, BUFLEN);
		recvfrom(s, rawbuf, BUFLEN, 0, (struct sockaddr *)&client_addr, &len);
		tok = strtok(rawbuf, "\r\n");
		while (tok != NULL) {
			if (!avr_data(data, tok))
						enqueue_data(data);
			tok = strtok(NULL, "\r\n");
		}
	}

	free(data);
	free(ui);
	DEBUG ("udp server thread exit\n");
}

void *udp_avr_push(void * arg) {
	struct sockaddr_in server;
	int len = sizeof(struct sockaddr_in);
	int s;
	udp_push_info *udp_i = (udp_push_info *) arg;
	char cdata[BUFLEN];

	bzero(cdata, BUFLEN);

	DEBUG("Push UDP avr data to %s:%i\n", udp_i->addr, udp_i->port);

	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		perror("udp_avr() socket()");
		exit(1);
	}

	memset((char *) &server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(udp_i->port);
	server.sin_addr.s_addr = inet_addr(udp_i->addr);

	while (1) {
		if (strncmp(cdata, grawdata, strlen(grawdata))) {
			bzero(cdata, BUFLEN);
			memcpy(cdata, grawdata, strlen(grawdata));
			if (sendto(s, cdata, strlen(cdata), 0, (struct sockaddr *) &server,
					len) == -1) {
				perror("udp_avr() sendto()");
				exit(1);
			}
		}
		usleep(1000);
	}

	return 0;
}
