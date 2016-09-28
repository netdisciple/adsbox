/*
 *	adsbox - ADS-B decoder software
 *	(c) atty 2011-15 romulmsg@mail.ru
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "misc.h"
#include "serial.h"
#include "db.h"

extern sqlite3 *db;
extern char grawdata[BUFLEN];

int db_insert_source_serial(unsigned int *id, char *device) {
    
    if (db_exec_sql("INSERT INTO SOURCE_SERIAL (ID, DEVICE) VALUES (%i, \"%s\")", *id, device))
	return 1;
   
    return 0;
}

int db_update_source_serial_baud(unsigned int *id, long int *baud) {

    if (db_exec_sql("UPDATE SOURCE_SERIAL SET BAUD=%li WHERE ID=%i", *baud, *id))
	return 1;

    if (!sqlite3_changes(db)) {
        return 1;
    }
    return 0;
}

int db_update_source_serial_bits(unsigned int *id, long int *bits) {

    if (db_exec_sql("UPDATE SOURCE_SERIAL SET BITS=%li WHERE ID=%i", *bits, *id))
	return 1;

    if (!sqlite3_changes(db)) {
        return 1;
    }
    return 0;
}

int run_source_serial() {
	char ** results;
	int rows, cols, i;
	char *zErrMsg = 0;
	char sql[] = "SELECT ID, DEVICE, BAUD, BITS, LAT, LON FROM SOURCE_SERIAL ORDER BY ID";
	pthread_t child;

	if (sqlite3_get_table(db, sql, &results, &rows, &cols, &zErrMsg) != SQLITE_OK) {
		DEBUG("SQLite error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_free_table(results);
		return 1;
	}

	for (i = 1; i <= rows; i++) {
		serial_info *si = malloc(sizeof(serial_info));
		si->id = atoi(results[i * cols]);
		strcpy(si->device, results[i * cols + 1]);

		if (!strcmp(results[i * cols + 2], "0"))
			si->baudrate = DEFAULT_SERIAL_BAUDRATE;
		else
			si->baudrate = atoi(results[i * cols + 2]);

		if (!strcmp(results[i * cols + 3], "0"))
			si->bits = DEFAULT_SERIAL_BITS;
		else
			si->bits = atoi(results[i * cols + 3]);
		si->lat = atof(results[i * cols + 4]);
		si->lon = atof(results[i * cols + 5]);

		pthread_create(&child, NULL, &serial_thread, si);
		pthread_detach(child);
	}

	sqlite3_free_table(results);

	return 0;
}

void *serial_thread(void *arg) {
	serial_info *si = (serial_info*) arg;
	int fd;
	char rawbuf[BUFLEN];
	struct termios oldtio, newtio;
	char *p;
	adsb_data *data = malloc(sizeof(adsb_data));

	clockid_t cid;
	pthread_getcpuclockid(pthread_self(), &cid);

	data->source_id = si->id;
	data->type = TYPE_SERIAL_AVR; // type of source
	data->lat = si->lat;
	data->lon = si->lon;
	data->source_bytes = 0;

	DEBUG("Using %s as input serial device.\n", si->device);

	fd = open(si->device, O_RDONLY | O_NOCTTY);
	if (fd < 0) {
		DEBUG("Unable to open device %s\n", si->device);
		goto exit;
	}

	sprintf(rawbuf, "AVR serial data source %s", si->device);
	db_insert_thread(cid, rawbuf);

	tcgetattr(fd, &oldtio); // TODO restore old termio

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag &= ~PARENB;
	newtio.c_cflag &= ~CSTOPB;
	newtio.c_cflag &= ~CSIZE;
	newtio.c_cflag |= si->baudrate | si->bits | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL; // ignore parity error
	newtio.c_oflag = 0;

	newtio.c_lflag = ICANON;

	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VEOL] = 0;
	newtio.c_cc[VEOL2] = 0;

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	while (1) {
		bzero(rawbuf, BUFLEN);
		/*while (strstr((char *)rawbuf, ";") == NULL) {
			read(fd, d, 1);
			if (d[0] == '*' || d[0] == '@' || d[0] == '#')
				bzero(rawbuf, BUFLEN);
			strncat((char *)rawbuf, d, 1);
		}*/
		read(fd, rawbuf, sizeof(rawbuf));
		p = strstr(rawbuf, "\n");
		if (p) *p = '\0'; else continue;
		if (!avr_data(data, rawbuf))
			enqueue_data(data);
	}

	exit:
	free(si);
	free(data);
	db_delete_thread(cid);
	DEBUG("serial thread exit.");
	pthread_exit(0);
}
