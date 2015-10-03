CFLAGS?=-O2 -g -Wall -W $(shell pkg-config --cflags librtlsdr)
LDLIBS+=$(shell pkg-config --libs librtlsdr) -lpthread -lm
CC?=gcc
PROGNAME=dump740

all: dump740

%.o: %.c
	$(CC) $(CFLAGS) -c $<

dump1090: dump740.o anet.o
	$(CC) -g -o dump740 dump740.o anet.o $(LDFLAGS) $(LDLIBS)

clean:
	rm -f *.o dump740
