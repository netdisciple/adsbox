
ADSBox - free ADS-B decoding software.

Compile with SQLite notes:
    - download SQLite source from http://sqlite.org/download.html.
    - place sqlite3 directory with SQLite amalgamation source in the
    same level as ADSBox source.
    - cd to ADSBox source directory and type make.

Google Earth notes:
    - install GE;
    - place adsbox.kml file (see ADSBox source dir) in preffered directory;
    - open adsbox.kml in editor and point ADSBox host IP 
    and port (8080 as default);
    - start ADSBox server then open adsbox.kml in GE.
    
Web server notes:
    - open ADSBox info page (e.g http://192.168.1.6:8080) in
    browser and see current status.
    
Librtlsdr compile notes:
    - set WITH_RTLSDR=yes in Makefile to enable USB SDR dongles support.
    You may need to point rtl-sdr.h and librtlsdr.so files location also.

Free for non profit use.

All the best,
(c)atty romulmsg@mail.ru
