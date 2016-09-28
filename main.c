/*
 *	adsbox - ADS-B decoder software
 *	(c) atty 2011-16 romulmsg@mail.ru
 */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h> 
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <syscall.h>
#include <netinet/tcp.h>
#include "misc.h"
#include "http.h"
#include "adsb.h"
#include "db.h"
#include "file.h"
#include "serial.h"
#include "tcp_avr.h"
#include "tcp_sbs3.h"
#include "tcp_beast.h"
#include "udp_avr.h"

#ifdef RTLSDR
#include "rtlsdr.h"
#endif

#ifdef MLAT
#include "mlat.h"
#endif

#define SBS_PORT 30003
#define HTTP_PORT 8080

pthread_mutex_t mux;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER, cond_var_queue =
		PTHREAD_COND_INITIALIZER;
char gdata[BUFLEN];
char grawdata[BUFLEN];
int dmode = 0;
_Bool record = 0;
_Bool seed_server = 0;
#ifdef MLAT
_Bool with_mlat = 0;
#endif
FILE * rd;
int delay = 0;
sqlite3 *db;

item * head;
pthread_mutex_t mux_queue;

void enqueue_data(adsb_data *d) {
	item *curr, *tail;

	pthread_mutex_lock(&mux_queue);

	if (head == NULL) {
		head = malloc(sizeof(item));
		memcpy(&head->list_data, d, sizeof(adsb_data));
		head->next = NULL;
	} else {
		curr = head;
		while (curr->next)
			curr = curr->next;
		tail = malloc(sizeof(item));
		curr->next = tail;
		memcpy(&tail->list_data, d, sizeof(adsb_data));
		tail->next = NULL;
	}
	pthread_cond_signal(&cond_var_queue);
	pthread_mutex_unlock(&mux_queue);
}

/* decoder thread */
void *decoder(void *arg) {
	unsigned char msg[128];
	struct tm *now;
	struct timespec ts;
	adsb_data *data;
	item * curr;
	clockid_t cid;

	pthread_getcpuclockid(pthread_self(), &cid);
	db_insert_thread(cid, "ADS-B decoder");

	bzero(msg, sizeof(msg));
	while (1) {

		pthread_mutex_lock(&mux_queue);
		if (head != NULL) {
			data = &head->list_data;
			curr = head;
			head = head->next;
			pthread_mutex_unlock(&mux_queue);
		} else {
			pthread_cond_wait(&cond_var_queue, &mux_queue);
			pthread_mutex_unlock(&mux_queue);
			continue;
		}

		if (data->data_len == 4) { // frequency measure packet
			pthread_mutex_lock(&mux);
			sprintf(grawdata, "%s\r\n", data->avr_data);
			pthread_cond_broadcast(&cond_var);
			pthread_mutex_unlock(&mux);
		}

		if ((data->data_len == 7 && decode_message56(data, (char *) msg))
				|| (data->data_len == 14 && decode_message(data, (char *) msg))) {
			pthread_mutex_lock(&mux);
			bzero(grawdata, BUFLEN);
			if (!data->timestamp)
				sprintf(grawdata, "%s\r\n", data->avr_data); // raw data to seed
			else
				sprintf(grawdata, "@%012X%s\r\n", data->timestamp,
						data->avr_data + 1);
			if (record) {
				clock_gettime(CLOCK_REALTIME, &ts);
				now = localtime(&ts.tv_sec);
				fprintf(rd, "%02d:%02d:%02d.%03lu %02d.%02d.%04d %s",
						now->tm_hour, now->tm_min, now->tm_sec,
						ts.tv_nsec / 1000000, now->tm_mday, now->tm_mon + 1,
						now->tm_year + 1900, grawdata);
				fflush(rd);
			}
			bzero(gdata, BUFLEN);
			memcpy(gdata, msg, strlen((char *) msg));
			pthread_cond_broadcast(&cond_var);
			pthread_mutex_unlock(&mux);
			bzero(msg, sizeof(msg));
			db_update_source_stat(data);
		}
		free(curr);
	}
	db_delete_thread(cid);
	DEBUG("decoder thread exit.\n")
	pthread_exit(0);
}

/* process client connection thread. MSG format data seed */
void *sbs1_server_client(void *arg) {
	char buf[2];
	char cdata[BUFLEN];
	struct timespec to;
	struct timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	int en = 1;

	intptr_t *s = (intptr_t*) arg;

	if (setsockopt(*s, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
			sizeof(timeout)) < 0)
		panic("sbs-1 setsockopt()");
	if (setsockopt(*s, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout,
			sizeof(timeout)) < 0)
		panic("sbs-1 setsockopt()");
	if (setsockopt(*s, SOL_SOCKET, SO_KEEPALIVE, &en, sizeof(en)) != 0)
		panic("sbs-1 setsockopt()");
	if (setsockopt(*s, IPPROTO_TCP, TCP_NODELAY, &en, sizeof(en)) != 0)
		panic("sbs-1 setsockopt()");

	while (recv(*s, buf, 1, MSG_DONTWAIT)) {
		pthread_mutex_lock(&mux);
		to.tv_sec = time(NULL) + 1;
		to.tv_nsec = 0;
		pthread_cond_timedwait(&cond_var, &mux, &to);
		if (strncmp(cdata, gdata, strlen(gdata))) {
			bzero(cdata, BUFLEN);
			memcpy(cdata, gdata, strlen(gdata));
			send(*s, cdata, strlen(cdata), 0);
		}
		pthread_mutex_unlock(&mux);
	}
	close(*s);
	free(s);
	DEBUG("\nsbs-1 thread exit");
	pthread_exit(0);
}

/* process client connection thread. AVR data seed */
void *avr_server_client(void *arg) {
	char buf[2];
	char cdata[BUFLEN];
	struct timespec to;
	struct timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	int en = 1;

	intptr_t *s = (intptr_t*) arg;

	if (setsockopt(*s, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
			sizeof(timeout)) < 0)
		panic("setsockopt()");
	if (setsockopt(*s, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout,
			sizeof(timeout)) < 0)
		panic("setsockopt()");
	if (setsockopt(*s, SOL_SOCKET, SO_KEEPALIVE, &en, sizeof(en)) != 0)
		panic("http setsockopt()");
	if (setsockopt(*s, IPPROTO_TCP, TCP_NODELAY, &en, sizeof(en)) != 0)
		panic("http setsockopt()");

	while (recv(*s, buf, 1, MSG_DONTWAIT)) {
		pthread_mutex_lock(&mux);
		to.tv_sec = time(NULL) + 1;
		to.tv_nsec = 0;
		pthread_cond_timedwait(&cond_var, &mux, &to);
		if (strncmp(cdata, grawdata, strlen(grawdata))) {
			bzero(cdata, BUFLEN);
			memcpy(cdata, grawdata, strlen(grawdata));
			send(*s, cdata, strlen(cdata), 0);
		}
		pthread_mutex_unlock(&mux);
	}
	close(*s);
	free(s);
	DEBUG("AVR seed thread exit\n");
	pthread_exit(0);
}

void usage(void) {
	printf("adsbox [options]\n\t-s, --serial input serial device\n");
	printf(
			"\t--baud serial device baudrate (valid rates are: 115200, 230400, 460800,\n");
	printf("\t\t500000, 576000, 921600, 1000000, 1152000, 1500000,\n");
	printf("\t\t2000000, 2500000, 3000000, 3500000, 4000000)\n");
	printf("\t--bits serial device bits (7 or 8)\n");
	printf("\t-d, --daemon be a daemon\n");
	printf("\t-l, --lat data source latitude\n\t-g, --lon data source longitude\n");
	printf("\t--name [name] set data source name\n");
	printf("\t-r, --record [file] record ADS-B data to file\n");
	printf("\t-p, --play [file] play recordered file\n");
	printf("\t-x, --rate [n] play n times faster\n");
	printf("\t-z, --snooze [n] snooze for n seconds before play\n");
	printf("\t--http-port [port] run http server on port (default 8080)\n");
	printf("\t--seed seeding AVR data on tcp port (default 30004)\n");
	printf("\t--seed-port [port] seed AVR data on tcp port\n");
	printf("\t--avr-server [IP] connect to AVR data server\n");
	printf("\t--avr-server-port [port] connect to AVR data server port (default 30004)\n");
	printf("\t--beast-server [IP] connect to binary beast data server\n");
	printf("\t--beast-server-port [port] connect to beast binary data server port (default 10005)\n");
#ifdef RTLSDR
	printf("\t--rtlsdr [=n] use RTLSDR device n (0 is default)\n");
	printf("\t--gain [auto|val] set RTLSDR device autogain or gain value (max available by default)\n");
	printf("\t--agc use RTLSDR device AGC\n");
	printf("\t--freq-correct [PPM] set RTLSDR device frequency correction to PPM\n");
#endif
	printf("\t--sbs3-server [IP] connect to SBS-3 data server\n");
	printf("\t--sbs3-server-port [port] connect to sbs3 data server port (default 10001)\n");
	printf("\t--udp-server-port [port] listen avr data on UDP port\n");
	printf("\t--udp-push-to [IP] push avrascii data to server\n");
	printf("\t--udp-push-port [port] push avrascii data to server port\n");
	printf("\t--dec-threads number of decoder threads (1 by default)\n");
#ifdef MLAT
	printf("\t--mlat perform MLAT (at least 3 data sources are required)\n");
#endif
	printf("\t--no-events do not log aircraft events (less CPU load)\n");
	printf("\t--no-indirect do not perform indirect calculations of speed, heading etc. (less CPU load)\n");
	printf("\t--hz timestamp counter frequency in Hz units\n");
	printf("\t-h, --help this help\n");
	exit(0);
}

void sigint_handler() {
	DEBUG("\nSIGINT\n");
	exit(0);
}

void short_timer() {
	static unsigned int count;
	db_stat_thread();
	count += 5;
	if (!(count%12)) {
		db_delete_expired();
		db_delete_expired_log();
		sqlite3_db_release_memory(db);
	}
	return;
}

int main(int ac, char *av[]) {
	extern char * optarg;
	static struct option long_options[] = {
			{"daemon", no_argument, NULL, 'd'},
			{"serial",required_argument, NULL, 's'},
			{"help", no_argument, NULL, 'h'},
			{"lat", required_argument, NULL, 'l'},
			{"lon",required_argument, NULL, 'g'},
			{"record", required_argument, NULL, 'r'},
			{"play", required_argument, NULL, 'p'},
			{"rate", required_argument, NULL, 'x'},
			{"snooze",required_argument, NULL, 'z'},
			{"baud", required_argument, NULL, 1},
			{"bits", required_argument, NULL, 2},
			{"seed", no_argument, NULL, 3},
			{"seed-port", required_argument, NULL, 4},
			{"avr-server", required_argument, NULL, 5},
			{"avr-server-port", required_argument, NULL, 6},
#ifdef RTLSDR
			{"rtlsdr", optional_argument, NULL, 7},
			{"gain", required_argument, NULL, 8},
			{"agc", no_argument, NULL, 9},
			{"freq-correct", required_argument, NULL, 10},
#endif
			{"sbs3-server", required_argument, NULL, 11},
			{"sbs3-server-port", required_argument, NULL, 12},
			{"dec-threads", required_argument, NULL, 13},
			{"udp-push-to", required_argument, NULL, 14},
			{"udp-push-port", required_argument, NULL, 15},
			{"udp-server-port", required_argument, NULL, 16},
			{"beast-server", required_argument, NULL, 17},
			{"beast-server-port", required_argument, NULL, 18},
			{"http-port", required_argument, NULL, 19},
#ifdef MLAT
			{"mlat", no_argument, NULL, 20},
#endif
			{"hz", required_argument, NULL, 21},
			{"no-events", no_argument, NULL, 22},
			{"no-indirect", no_argument, NULL, 23},
			{"name", required_argument, NULL, 24},
			{0, 0, 0, 0} };
	int option_index;
	struct sockaddr_in addr;
	int sd, sdd = 0, max_d, en = 1;
	int pid;
	double playrate = 1;
	long int baudrate = DEFAULT_SERIAL_BAUDRATE;
	long int bits = DEFAULT_SERIAL_BITS;
	int c;
	fd_set input;
	int rc;
	int seed_port = AVR_SERVER_PORT;
	pthread_t child;
	unsigned int id_source = 0;
	int dec_threads = 1;
	_Bool no_events = 0;
	_Bool no_indirect = 0;

	pthread_mutex_init(&mux, NULL);
	pthread_mutex_init(&mux_queue, NULL);
	head = NULL; // head of data queue
	udp_push_info *udp_i = NULL;
	http_info *hi = malloc(sizeof(http_info));
	hi->port = HTTP_PORT;

	DEBUG("ADSBox ADS-B decoder version %s.\n", VERSION);
	DEBUG("Use --help option for help.\n");
	/*
	 * Dealing with sqlite database.
	 */
	rc = sqlite3_open(":memory:", &db);
	if (rc) {
		DEBUG("Can't open database: %s\n", sqlite3_errmsg(db));
		exit(1);
	}

	if (db_init(db))
		exit(1);
	/*
	 * Done with sqlite.
	 */

	db_insert_thread(CLOCK_PROCESS_CPUTIME_ID, "ADSBox (total)");
	db_insert_thread(CLOCK_THREAD_CPUTIME_ID, "Main");

	while ((c = getopt_long(ac, av, "ds:hl:g:r:p:x:z:", long_options,
			&option_index)) != -1) {
		switch (c) {
		case 1:
			if (!strcmp(optarg, "115200"))
				baudrate = B115200;
			else if (!strcmp(optarg, "230400"))
				baudrate = B230400;
			else if (!strcmp(optarg, "460800"))
				baudrate = B460800;
			else if (!strcmp(optarg, "500000"))
				baudrate = B500000;
			else if (!strcmp(optarg, "576000"))
				baudrate = B576000;
			else if (!strcmp(optarg, "921600"))
				baudrate = B921600;
			else if (!strcmp(optarg, "1000000"))
				baudrate = B1000000;
			else if (!strcmp(optarg, "1152000"))
				baudrate = B1152000;
			else if (!strcmp(optarg, "1500000"))
				baudrate = B1500000;
			else if (!strcmp(optarg, "2000000"))
				baudrate = B2000000;
			else if (!strcmp(optarg, "2500000"))
				baudrate = B2500000;
			else if (!strcmp(optarg, "3000000"))
				baudrate = B3000000;
			else if (!strcmp(optarg, "3500000"))
				baudrate = B3500000;
			else if (!strcmp(optarg, "4000000"))
				baudrate = B4000000;
			else {
				DEBUG("%s - illegal baudrate.\n", optarg);
				exit(1);
			}
			db_update_source_serial_baud(&id_source, &baudrate);
			break;
		case 2:
			if (!strcmp(optarg, "7"))
				bits = CS7;
			else if (!strcmp(optarg, "8"))
				bits = CS8;
			else {
				DEBUG("%s - illegal bits option. Should be 7 or 8.\n", optarg);
				exit(1);
			}
			db_update_source_serial_bits(&id_source, &bits);
			break;
		case 3:
			seed_server = 1; // run seed server
			break;
		case 4:
			seed_port = atoi(optarg);
			break;
		case 5:
			id_source++;
			db_insert_source_tcp(&id_source, optarg);
			break;
		case 6:
			db_update_source_tcp_port(&id_source, atoi(optarg));
			break;
#ifdef RTLSDR
		case 7:
			id_source++;
			if (optarg != NULL)
				db_insert_source_rtlsdr(&id_source, atoi(optarg));
			else
				db_insert_source_rtlsdr(&id_source, 0);
			break;
		case 8:
			if (!strcmp(optarg, "auto"))
				db_update_source_rtlsdr_gain(&id_source, -2);
			else
				db_update_source_rtlsdr_gain(&id_source, atof(optarg));
			break;
		case 9:
			db_update_source_rtlsdr_agc(&id_source, 1);
			break;
		case 10:
			db_update_source_rtlsdr_freq_correct(&id_source, atoi(optarg));
			break;			
#endif
		case 11:
			id_source++;
			db_insert_source_tcp_sbs3(&id_source, optarg);
			break;
		case 12:
			db_update_source_tcp_sbs3_port(&id_source, atoi(optarg));
			break;
		case 13:
			dec_threads = atoi(optarg);
			break;
		case 14:
			udp_i = malloc(sizeof(udp_push_info));
			strncpy(udp_i->addr, optarg, sizeof(udp_i->addr));
			udp_i->port = 30005;
			break;
		case 15:
			udp_i->port = atoi(optarg);
			break;
		case 16:
			id_source++;
			db_insert_source_udp_avr(&id_source, atoi(optarg));
			break;
		case 17:
			id_source++;
			db_insert_source_tcp_beast(&id_source, optarg);
			break;
		case 18:
			db_update_source_tcp_beast_port(&id_source, atoi(optarg));
			break;
		case 19:
			hi->port = atoi(optarg);
			break;
#ifdef MLAT
		case 20:
			with_mlat = 1;
			break;
		case 21:
			db_update_hz(id_source, atoi(optarg));
			break;
#endif
		case 22:
			no_events = 1;
			break;
		case 23:
			no_indirect = 1;
			break;
		case 24:
			db_source_name(optarg, &id_source);
			break;	
		case 'd':
			dmode = 1;
			break;
		case 's':
			id_source++;
			db_insert_source_serial(&id_source, optarg);
			break;
		case 'l':
			if (db_source_lat(atof(optarg), &id_source) == -1) {
				DEBUG("Can't set latitude for source id %u\n", id_source);
				exit(1);
			}
			break;
		case 'g':
			if (db_source_lon(atof(optarg), &id_source) == -1) {
				DEBUG("Can't set longitude for source id %u\n", id_source);
				exit(1);
			}
			break;
		case 'r':
			record = 1;
			if ((rd = fopen(optarg, "w+")) == NULL) {
				DEBUG("Unable to open %s\n", optarg);
				exit(1);
			}
			DEBUG("Recording data to %s\n", optarg);
			break;
		case 'p':
			id_source++;
			db_insert_source_file(&id_source, optarg);
			break;
		case 'x':
			playrate = atof(optarg);
			if (db_update_source_file_playrate(&id_source, &playrate)) {
				DEBUG("Can't set playrate for source id %u\n", id_source);
				exit(1);
			}
			break;
		case 'z':
			delay = atoi(optarg);
			if (db_update_source_file_delay(&id_source, &delay)) {
				DEBUG("Can't set delay for source id %u\n", id_source);
				exit(1);
			}
			break;
		case 'h':
			usage();
			break;
		case '?':
		default:
			DEBUG("Invalid option. Try --help for more info.\n");
			exit(1);
		}
	}

	// log events
	if (!no_events)
		db_evt_trg();
	// perform some calculations
	if (!no_indirect)
		db_indirect_trg();

	if (db_attach("data.sqb", "EXTERNAL"))
			exit(1);

	signal(SIGPIPE, SIG_IGN); // ignoring SIGPIPE causing unexpected process kill
	signal(SIGINT, sigint_handler);

	if (dmode) { /* daemonize */
		printf("Entering daemon mode\n");
		pid = fork();
		if (pid < 0)
			panic("fork()");

		if (pid > 0)
			exit(0);

		umask(0);

		if (setsid() < 0)
			panic("setsid()");

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}

	/*
	 * Short timer
	 */
	struct sigaction sigact_s;
	struct sigevent sigev_s;
	timer_t t_id_s;
	struct itimerspec it_s;

	sigemptyset(&sigact_s.sa_mask);
	sigact_s.sa_flags = SA_SIGINFO;
	sigact_s.sa_sigaction = short_timer;

	if (sigaction(SIGUSR1, &sigact_s, 0) == -1)
		panic("short timer sigaction()\n");

	sigev_s.sigev_notify = SIGEV_SIGNAL;
	sigev_s.sigev_signo = SIGUSR1;
	sigev_s.sigev_value.sival_int = 1;

	if (timer_create(CLOCK_REALTIME, &sigev_s, &t_id_s) == 0) {
		it_s.it_value.tv_sec = 5; // 5 seconds Short timer period
		it_s.it_value.tv_nsec = 0;
		it_s.it_interval.tv_sec = it_s.it_value.tv_sec;
		it_s.it_interval.tv_nsec = it_s.it_value.tv_nsec;
		if (timer_settime(t_id_s, 0, &it_s, NULL) != 0)
			panic("short timer timer_settime()\n");
	} else
		panic("short timer timer_create()\n");
	/*
	 * End Short timer
	 */

	/*
	 * Start ADS-B decoder thread here
	 */
	while (dec_threads--) {
		pthread_create(&child, NULL, &decoder, NULL); /* start decoder thread */
		pthread_detach(child);
	}

	/* start UDP push thread */
	if (udp_i) {
		pthread_create(&child, NULL, &udp_avr_push, udp_i);
		pthread_detach(child);
	}

	if (init_radar_range()) {
		DEBUG("Can't init radar range\n");
		exit(1);
	}

	/* Run sources */
	run_source_file();
	run_source_tcp();
	run_source_serial();
#ifdef RTLSDR
	run_source_rtlsdr();
#endif
	run_source_tcp_sbs3();
	run_source_tcp_beast();
	run_source_udp_avr();
#ifdef MLAT
	if (with_mlat)
		run_mlat();
#endif
	/* Run servers */
	pthread_create(&child, NULL, &http_server, hi);
	pthread_detach(child);

	/*char msg[256];
	adsb_data data;
	data.data_len = 14;
	data.data[0] = 0x8D;
	data.data[1] = 0x15;
	data.data[2] = 0x77;
	data.data[3] = 0x0B;
	data.data[4] = 0x99;
	data.data[5] = 0x04;
	data.data[6] = 0x09;
	data.data[7] = 0x25;
	data.data[8] = 0x48;
	data.data[9] = 0xA0;
	data.data[10] = 0x01; //0x00
	data.data[11] = 0xA4;
	data.data[12] = 0x98;
	data.data[13] = 0xA6;

	data.data_len = 7;
	data.data[0] = 0x5D;
	data.data[1] = 0x40; // 0x42
	data.data[2] = 0x49;
	data.data[3] = 0xC6;
	data.data[4] = 0xF3;
	data.data[5] = 0xFF;
	data.data[6] = 0xDD;

	decode_message(&data, msg);
	exit(0); */

	/*--- create sbs-1 data socket ---*/
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd < 0)
		panic("sbs-1 socket()");
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(en)) != 0)
		panic("sbs-1 setsockopt()");

	/* non-blocking socket */
	if (ioctl(sd, FIONBIO, &en) < 0)
		panic("sbs-1 ioctl()");

	/*--- bind  ---*/
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SBS_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY); /* any interface */
	if (bind(sd, (struct sockaddr*) &addr, sizeof(addr)) != 0)
		panic("sbs-1 bind()");
	if (listen(sd, 10) != 0)
		panic("sbs-1 listen()")
	DEBUG("Run SBS-1 data server on port %i\n", SBS_PORT);

	/*--- create avr server socket ---*/
	if (seed_server == 1) {
		sdd = socket(AF_INET, SOCK_STREAM, 0);
		if (sdd < 0)
			panic("avr server socket()");
		if (setsockopt(sdd, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(en)) != 0)
			panic("avr server setsockopt()");

		/* non-blocking socket */
		if (ioctl(sdd, FIONBIO, &en) < 0)
			panic("avr server ioctl()");

		/*--- bind  ---*/
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(seed_port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY); /* any interface */
		if (bind(sdd, (struct sockaddr*) &addr, sizeof(addr)) != 0)
			panic("avr server bind()");
		if (listen(sdd, 10) != 0)
			panic("avr server listen()")
		DEBUG("Seeding AVR data on TCP port %i\n", seed_port);
	}

	max_d = sd;
	if ((seed_server == 1) && (sdd > max_d))
		max_d = sdd;
	max_d++;

	/*--- begin waiting for connections ---*/
	while (1) { /* process incoming clients */
		FD_ZERO(&input);
		FD_SET(sd, &input);
		if (seed_server == 1)
			FD_SET(sdd, &input);

		if (select(max_d, &input, NULL, NULL, NULL) > 0) {
			if (FD_ISSET(sd, &input)) {
				int *sd1 = (int*) malloc(sizeof(int));
				*sd1 = accept(sd, 0, 0); // accept connection
				if (pthread_create(&child, NULL, &sbs1_server_client, sd1)) {
					DEBUG("pthread_create() sbs1_server_client() error\n");
					free(sd1);
				}
				pthread_detach(child);
			}
			if (seed_server && FD_ISSET(sdd, &input)) {
				int *sdd1 = (int*) malloc(sizeof(int));
				*sdd1 = accept(sdd, 0, 0); // accept connection
				if (pthread_create(&child, NULL, &avr_server_client, sdd1)) {
					DEBUG("pthread_create() avr_server_client() error\n");
					free(sdd1);
				}
				pthread_detach(child);
			}
		}
	}
	exit(0);
}
