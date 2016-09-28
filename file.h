#ifndef _FILE_H_
#define _FILE_H_

typedef struct {
	int id;
	char name[256];
	int delay;
	float playrate;
	double lat;
	double lon;
} file_info;

void *player_thread (void *);
int db_insert_source_file(unsigned int*, char*);
int db_update_source_file_playrate(unsigned int*, double*);
int db_update_source_file_delay(unsigned int*, int*);
int run_source_file();
#endif
