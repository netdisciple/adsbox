/*
 *	adsbox - ADS-B decoder software
 *	(c) atty 2011-14 romulmsg@mail.ru
 *
 */
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "file.h"
#include "misc.h"
#include "db.h"

extern sqlite3 *db;
extern int msqkey;

float sec_between(struct timespec now, struct timespec then) {
	return now.tv_sec - then.tv_sec + (now.tv_nsec - then.tv_nsec)
			/ 1000000000.;
}

int db_insert_source_file(unsigned int *id, char *filename) {
	if (db_exec_sql("INSERT INTO SOURCE_FILE (ID, FILENAME) VALUES (%i, \"%s\")",
			*id, filename)) return 1;
	return 0;
}

int db_update_source_file_playrate(unsigned int *id, double *playrate) {
	if (db_exec_sql("UPDATE SOURCE_FILE SET PLAYRATE=%f WHERE ID=%i", *playrate,
			*id)) return 1;

	if (!sqlite3_changes(db)) {
		return 1;
	}
	return 0;
}

int db_update_source_file_delay(unsigned int *id, int *delay) {
	if (db_exec_sql("UPDATE SOURCE_FILE SET DELAY=%i WHERE ID=%i", *delay, *id))
	    return 1;
	if (!sqlite3_changes(db)) {
		return 1;
	}
	return 0;
}

int run_source_file() {
	char ** results;
	int rows, cols, i;
	char *zErrMsg = 0;
	char sql[] = 
	    "SELECT ID, FILENAME, PLAYRATE, DELAY, LAT, LON FROM SOURCE_FILE ORDER BY ID";
	pthread_t child;

	if (sqlite3_get_table(db, sql, &results, &rows, &cols, &zErrMsg) != SQLITE_OK) {
		DEBUG("SQLite error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_free_table(results);
		return 1;
	}

	for (i = 1; i <= rows; i++) {
		file_info *fi = malloc(sizeof(file_info));
		fi->id = atoi(results[i * cols]);
		strcpy(fi->name, results[i * cols + 1]);
		fi->playrate = atof(results[i * cols + 2]);
		fi->delay = atoi(results[i * cols + 3]);
		fi->lat = atof(results[i * cols + 4]);
		fi->lon = atof(results[i * cols + 5]);
		pthread_create(&child, NULL, &player_thread, fi); /* start file player thread */
		pthread_detach(child);
	}

	sqlite3_free_table(results);

	return 0;
}

void *player_thread(void *arg) {
	file_info *fi = (file_info *) arg;
	FILE * rd;
	char rawbuf[BUFLEN];
	struct tm tm;
	int r;
	struct timespec ts_now, ts_prev;
	struct timespec fts_now, fts_prev;
	adsb_data *data = malloc(sizeof(adsb_data));

	data->source_id = fi->id;
	data->type = TYPE_FILE; // type of source
	data->lat = fi->lat;
	data->lon = fi->lon;
	data->source_bytes = 0;

	if ((rd = fopen(fi->name, "r")) == NULL) {
		DEBUG("Unable to open %s\n", fi->name);
		free(fi);
		pthread_exit(0);
	} else
		DEBUG("Playing file %s\n", fi->name);

	if (fi->delay > 0) {
		DEBUG("Delay for %i sec before playing file.\n", fi->delay);
		for (r = 0; r < fi->delay; r++)
			sleep(1);
	}

	bzero(rawbuf, BUFLEN);
	r = fscanf(rd, "%u:%u:%u.%lu %u.%u.%u %s\n", &tm.tm_hour, &tm.tm_min,
			&tm.tm_sec, &fts_prev.tv_nsec, &tm.tm_mday, &tm.tm_mon,
			&tm.tm_year, (char *) &rawbuf);
	fts_prev.tv_sec = mktime(&tm);
	fts_prev.tv_nsec = fts_prev.tv_nsec * 1000000;

	while (r != EOF) {
		sprintf(data->avr_data, "%s", rawbuf);

		if (!avr_data(data, rawbuf))
			enqueue_data(data);

		bzero(rawbuf, BUFLEN);

		r = fscanf(rd, "%u:%u:%u.%lu %u.%u.%u %s\n", &tm.tm_hour, &tm.tm_min,
				&tm.tm_sec, &fts_now.tv_nsec, &tm.tm_mday, &tm.tm_mon,
				&tm.tm_year, (char *) &rawbuf);
		fts_now.tv_sec = mktime(&tm);
		fts_now.tv_nsec = fts_now.tv_nsec * 1000000;

		clock_gettime(CLOCK_REALTIME, &ts_prev);
		while (sec_between(ts_now, ts_prev) * (double) fi->playrate
				< sec_between(fts_now, fts_prev)) {
			usleep(1000);
			clock_gettime(CLOCK_REALTIME, &ts_now);
		}
		fts_prev = fts_now;
	}

	free(fi);
	free(data);
	DEBUG("\nEOF Ctrl-C to exit.\n");
	pthread_exit(0);
}
