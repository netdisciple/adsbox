# ADSBox Makefile

# Use for cross compile
#CC = arm-linux-gcc
CC = gcc
INCLUDE =
LIBS = -lpthread -lm -lrt -ldl -lz
CFLAGS = -Wall -g
SQLITEFLAGS = -D SQLITE_THREADSAFE=1 -D SQLITE_DEFAULT_AUTOVACUUM=1
WITH_RTLSDR = no
WITH_MLAT = no
RTLSDR_INC = -I../../rtlsdr/rtl-sdr/include
RTLSDR_LIBS = -L../../rtlsdr/rtl-sdr/src/.libs
#RTLSDR_INC = -I/usr/local/include
#RTLSDR_LIBS = -L/usr/local/lib

ifeq ($(WITH_RTLSDR),yes)
RTLSDR_O_FILE = rtlsdr.o
LIBS +=  $(RTLSDR_LIBS) -lrtlsdr
DRTLSDR = -DRTLSDR
endif

ifeq ($(WITH_MLAT),yes)
INCLUDE += -I../mlat
MLATLIB = ../mlat/mlat.a
DMLAT = -DMLAT
endif

all: clean db http misc file serial tcp_avr tcp_beast \
 udp_avr tcp_sbs3 adsb main rtlsdr mlat sqlite build

adsb: adsb.c
	$(CC) $(INCLUDE) $(DRTLSDR) $(DMLAT) -c adsb.c $(CFLAGS)

http: http.c
	$(CC) -c http.c $(CFLAGS)

misc: misc.c
	$(CC) $(INCLUDE) $(DMLAT) -c misc.c $(CFLAGS)

file: file.c
	$(CC) -c file.c $(CFLAGS)

serial: serial.c
	$(CC) -c serial.c $(CFLAGS)

tcp_avr: tcp_avr.c
	$(CC) -c tcp_avr.c $(CFLAGS)

tcp_beast: tcp_beast.c
	$(CC) -c tcp_beast.c $(CFLAGS)

udp_avr: udp_avr.c
	$(CC) -c udp_avr.c $(CFLAGS)

rtlsdr: rtlsdr.c
ifeq ($(WITH_RTLSDR),yes)
	$(CC) $(RTLSDR_INC) -c rtlsdr.c $(CFLAGS)
else
	@printf "librtlsdr support is disabled. Enable with WITH_RTLSDR=yes option in Makefile.\n"
endif

tcp_sbs3: tcp_sbs3.c
	$(CC) -c tcp_sbs3.c $(CFLAGS)

sqlite: ../sqlite3/sqlite3.c
	$(CC) $(SQLITEFLAGS) -c ../sqlite3/sqlite3.c $(CFLAGS)

db:	db.c
	$(CC) -c db.c $(CFLAGS)

main:	main.c
	$(CC) $(INCLUDE) $(DRTLSDR) $(DMLAT) -c main.c $(CFLAGS)

mlat:
ifeq ($(WITH_MLAT),yes)
	cd ../mlat; make
endif

build: 
	$(CC) adsb.o http.o misc.o file.o serial.o tcp_avr.o tcp_beast.o udp_avr.o \
	tcp_sbs3.o sqlite3.o db.o main.o \
	$(RTLSDR_O_FILE) $(MLATLIB) $(LIBS) -o adsbox
	@printf "Done.\n"

clean:
	rm -f ./*.o ./adsbox
