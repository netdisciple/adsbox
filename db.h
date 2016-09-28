#ifndef __DB_H__
#define __DB_H__

#include "../sqlite3/sqlite3.h"

typedef struct {
	int lat0;
	int lon0;
	int lat1;
	int lon1;
	double lat;
	double lon;
	char parity;
	int id_source;
} cpr_info;

int db_exec(char **, sqlite3 *);
int db_exec_sql(char *, ...);
int db_init (sqlite3 *);
int db_init_history(char *);
int db_attach(char *, char *);
int db_delete_expired();
int db_delete_expired_log();
int db_update_icao(unsigned int *, adsb_data *);
int db_update_position(unsigned int*, double*, double*, adsb_data *);
int db_update_alt(unsigned int *, int *, int *, adsb_data *, unsigned char *);
int db_update_speed_heading(unsigned int*, float*, float*, int *, adsb_data *, unsigned char *);
int db_update_squawk (unsigned int *, unsigned int *, int *, adsb_data *, unsigned char *);
int db_update_callsign(unsigned int *, char *, char *, adsb_data *, unsigned char *);
int db_source_name(char *, unsigned int *);
int db_source_lat(double, unsigned int *);
int db_source_lon(double, unsigned int *);
int db_update_source_stat(adsb_data *);
int db_update_status(unsigned int *, unsigned char *, uint16_t *,
	float *, float *, unsigned char *, unsigned char *,
	unsigned char *, unsigned char *, unsigned char *, adsb_data *, unsigned char *);
cpr_info db_update_cpr(unsigned int *, unsigned char *, int *, int *, int *);
int db_update_hz(int, unsigned int);
int db_insert_thread(unsigned int, char *);
int db_delete_thread(unsigned int);
int db_stat_thread();
int init_radar_range();
int db_evt_trg();
int db_indirect_trg();
int DEBUG_DB(char *, ...);

#endif
