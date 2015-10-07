/* Mode1090, a Mode S messages decoder for RTLSDR devices.
 *
 * Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
 *
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h> 
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include "rtl-sdr.h"
#include "anet.h"

#define MODES_DEFAULT_RATE         2000000 //разрешение 0.5мкс
#define MODES_DEFAULT_FREQ         740000000
#define MODES_DEFAULT_WIDTH        1000
#define MODES_DEFAULT_HEIGHT       700
#define MODES_ASYNC_BUF_NUMBER     12
#define MODES_DATA_LEN             (16*16384)   /* 256k */
#define MODES_AUTO_GAIN            -100         /* Use automatic gain. */
#define MODES_MAX_GAIN             999999       /* Use max available gain. */

#define MODES_PREAMBLE_US 8       /* microseconds */
#define MODES_LONG_MSG_BITS 112
#define MODES_SHORT_MSG_BITS 56


#define UVD_KOORD_KODE_LEN      45    //45 периодов по 0,5 мкс - длина анализа координатного кода
#define UVD_KEY_KODE_LEN        48    //48 периодов по 0,5 мкс - длина анализа ключевого кода 
#define UVD_INFO_KODE_LEN       640   //64*5=320 *2=640(с повтором) периодов по 0,5 мкс - длина анализа информационной части 

#define UVD_OK1_START           28    //14mks бортовой номер
#define UVD_OK2_START           22    //11mks высота топливо
#define UVD_OK3_START           36    //18mks скорость

#define UVD_OK1_DELAY           17    //8.5mks бортовой номер
#define UVD_OK2_DELAY           28    //14mks высота топливо
#define UVD_OK3_DELAY           20    //10mks скорость

#define UVD_OK1_OFFS            UVD_OK1_START+UVD_OK1_DELAY //28+17=45    //8.5mks бортовой номер
#define UVD_OK2_OFFS            UVD_OK2_START+UVD_OK2_DELAY //22+28=50    //14mks высота топливо
#define UVD_OK3_OFFS            UVD_OK3_START+UVD_OK3_DELAY //36+20=56    //10mks скорость

#define UVD_DECADE_LEN          64    //64 periods by 0.5mks

#define UVD_MAX_LEN             UVD_KOORD_KODE_LEN+UVD_KEY_KODE_LEN+UVD_OK2_DELAY+UVD_INFO_KODE_LEN

//выделение кода из сигнала происходит в процедуре detectUVD 

#define MODES_FULL_LEN (MODES_PREAMBLE_US+MODES_LONG_MSG_BITS)
#define MODES_LONG_MSG_BYTES (112/8)
#define MODES_SHORT_MSG_BYTES (56/8)

#define MODES_ICAO_CACHE_LEN 1024 /* Power of two required. */
#define MODES_ICAO_CACHE_TTL 60   /* Time to live of cached addresses. */
#define MODES_UNIT_FEET 0
#define MODES_UNIT_METERS 1

#define MODES_DEBUG_DEMOD (1<<0)
#define MODES_DEBUG_DEMODERR (1<<1)
#define MODES_DEBUG_BADCRC (1<<2)
#define MODES_DEBUG_GOODCRC (1<<3)
#define MODES_DEBUG_NOPREAMBLE (1<<4)
#define MODES_DEBUG_NET (1<<5)
#define MODES_DEBUG_JS (1<<6)

/* When debug is set to MODES_DEBUG_NOPREAMBLE, the first sample must be
 * at least greater than a given level for us to dump the signal. */
#define MODES_DEBUG_NOPREAMBLE_LEVEL 25

#define MODES_INTERACTIVE_REFRESH_TIME 250      /* Milliseconds */
#define MODES_INTERACTIVE_ROWS 15               /* Rows on screen */
#define MODES_INTERACTIVE_TTL 60                /* TTL before being removed */

#define MODES_NET_MAX_FD 1024
#define MODES_NET_OUTPUT_SBS_PORT 30003
#define MODES_NET_OUTPUT_RAW_PORT 30002
#define MODES_NET_INPUT_RAW_PORT 30001
#define MODES_NET_HTTP_PORT 8080
#define MODES_CLIENT_BUF_SIZE 1024
#define MODES_NET_SNDBUF_SIZE (1024*64)

#define MODES_NOTUSED(V) ((void) V)

/* Structure used to describe a networking client. */
struct client {
    int fd;         /* File descriptor. */
    int service;    /* TCP port the client is connected to. */
    char buf[MODES_CLIENT_BUF_SIZE+1];    /* Read buffer. */
    int buflen;                         /* Amount of data on buffer. */
};

/* Structure used to describe an aircraft in iteractive mode. */
struct aircraft {
    uint32_t addr;      /* ICAO address */
    char hexaddr[7];    /* Printable ICAO address */
    char flight[9];     /* Flight number */
    int altitude;       /* Altitude */
    int speed;          /* Velocity computed from EW and NS components. */
    int track;          /* Angle of flight. */
    time_t seen;        /* Time at which the last packet was received. */
    long messages;      /* Number of Mode S messages received. */
    /* Encoded latitude and longitude as extracted by odd and even
     * CPR encoded messages. */
    int odd_cprlat;
    int odd_cprlon;
    int even_cprlat;
    int even_cprlon;
    double lat, lon;    /* Coordinated obtained from CPR encoded data. */
    long long odd_cprtime, even_cprtime;
    struct aircraft *next; /* Next aircraft in our linked list. */
};

/* Program global state. */
struct {
    /* Internal state */
    pthread_t reader_thread;
    pthread_mutex_t data_mutex;     /* Mutex to synchronize buffer access. */
    pthread_cond_t data_cond;       /* Conditional variable associated. */
    unsigned char *data;            /* Raw IQ samples buffer */
    uint16_t *magnitude;            /* Magnitude vector */
    uint32_t data_len;              /* Buffer length. */
    int fd;                         /* --ifile option file descriptor. */
    int data_ready;                 /* Data ready to be processed. */
    uint32_t *icao_cache;           /* Recently seen ICAO addresses cache. */
    uint16_t *maglut;               /* I/Q -> Magnitude lookup table. */
    int exit;                       /* Exit from the main loop when true. */

    /* RTLSDR */
    int dev_index;
    int gain;
    int enable_agc;
    rtlsdr_dev_t *dev;
    int freq;

    /* Networking */
    char aneterr[ANET_ERR_LEN];
    struct client *clients[MODES_NET_MAX_FD]; /* Our clients. */
    int maxfd;                      /* Greatest fd currently active. */
    int sbsos;                      /* SBS output listening socket. */
    int ros;                        /* Raw output listening socket. */
    int ris;                        /* Raw input listening socket. */
    int https;                      /* HTTP listening socket. */

    /* Configuration */
    char *filename;                 /* Input form file, --ifile option. */
    int fix_errors;                 /* Single bit error correction if true. */
    int check_crc;                  /* Only display messages with good CRC. */
    int raw;                        /* Raw output format. */
    int debug;                      /* Debugging mode. */
    int net;                        /* Enable networking. */
    int net_only;                   /* Enable just networking. */
    int interactive;                /* Interactive mode */
    int interactive_rows;           /* Interactive mode: max number of rows. */
    int interactive_ttl;            /* Interactive mode: TTL before deletion. */
    int stats;                      /* Print stats at exit in --ifile mode. */
    int onlyaddr;                   /* Print only ICAO addresses. */
    int metric;                     /* Use metric units. */
    int aggressive;                 /* Aggressive detection algorithm. */

    /* Interactive mode */
    struct aircraft *aircrafts;
    long long interactive_last_update;  /* Last screen update in milliseconds */

    /* Statistics */
    long long stat_valid_preamble;
    long long stat_demodulated;
    long long stat_goodcrc;
    long long stat_badcrc;
    long long stat_fixed;
    long long stat_single_bit_fix;
    long long stat_two_bits_fix;
    long long stat_http_requests;
    long long stat_sbs_connections;
    long long stat_out_of_phase;
} Modes;

/* The struct we use to store information about a decoded message. */
struct modesMessage {
    /* Generic fields */
    unsigned char msg[MODES_LONG_MSG_BYTES]; /* Binary message. */
    int msgbits;                /* Number of bits in message */
    int msgtype;                /* Downlink format # */
    int crcok;                  /* True if CRC was valid */
    uint32_t crc;               /* Message CRC */
    int errorbit;               /* Bit corrected. -1 if no bit corrected. */
    int aa1, aa2, aa3;          /* ICAO Address bytes 1 2 and 3 */
    int phase_corrected;        /* True if phase correction was applied. */

    /* DF 11 */
    int ca;                     /* Responder capabilities. */

    /* DF 17 */
    int metype;                 /* Extended squitter message type. */
    int mesub;                  /* Extended squitter message subtype. */
    int heading_is_valid;
    int heading;
    int aircraft_type;
    int fflag;                  /* 1 = Odd, 0 = Even CPR message. */
    int tflag;                  /* UTC synchronized? */
    int raw_latitude;           /* Non decoded latitude */
    int raw_longitude;          /* Non decoded longitude */
    char flight[9];             /* 8 chars flight number. */
    int ew_dir;                 /* 0 = East, 1 = West. */
    int ew_velocity;            /* E/W velocity. */
    int ns_dir;                 /* 0 = North, 1 = South. */
    int ns_velocity;            /* N/S velocity. */
    int vert_rate_source;       /* Vertical rate source. */
    int vert_rate_sign;         /* Vertical rate sign. */
    int vert_rate;              /* Vertical rate. */
    int velocity;               /* Computed from EW and NS velocity. */

    /* DF4, DF5, DF20, DF21 */
    int fs;                     /* Flight status for DF4,5,20,21 */
    int dr;                     /* Request extraction of downlink request. */
    int um;                     /* Request extraction of downlink request. */
    int identity;               /* 13 bits identity (Squawk). */

    /* Fields used by multiple message types. */
    int altitude, unit;
};

void interactiveShowData(void);
struct aircraft* interactiveReceiveData(struct modesMessage *mm);
void modesSendRawOutput(struct modesMessage *mm);
void modesSendSBSOutput(struct modesMessage *mm, struct aircraft *a);
void useModesMessage(struct modesMessage *mm);
int fixSingleBitErrors(unsigned char *msg, int bits);
int fixTwoBitsErrors(unsigned char *msg, int bits);
int modesMessageLenByType(int type);
void sigWinchCallback();
int getTermRows();

/* ============================= Utility functions ========================== */

static long long mstime(void) {
    struct timeval tv;
    long long mst;

    gettimeofday(&tv, NULL);
    mst = ((long long)tv.tv_sec)*1000;
    mst += tv.tv_usec/1000;
    return mst;
}

/* =============================== Initialization =========================== */

void modesInitConfig(void) {
    Modes.gain = MODES_MAX_GAIN;
    Modes.dev_index = 0;
    Modes.enable_agc = 0;
    Modes.freq = MODES_DEFAULT_FREQ;
    Modes.filename = NULL;
    Modes.fix_errors = 1;
    Modes.check_crc = 1;
    Modes.raw = 0;
    Modes.net = 0;
    Modes.net_only = 0;
    Modes.onlyaddr = 0;
    Modes.debug = 0;
    Modes.interactive = 0;
    Modes.interactive_rows = MODES_INTERACTIVE_ROWS;
    Modes.interactive_ttl = MODES_INTERACTIVE_TTL;
    Modes.aggressive = 0;
    Modes.interactive_rows = getTermRows();
}

void modesInit(void) {
    int i, q;

    pthread_mutex_init(&Modes.data_mutex,NULL);
    pthread_cond_init(&Modes.data_cond,NULL);
    /* We add a full message minus a final bit to the length, so that we
     * can carry the remaining part of the buffer that we can't process
     * in the message detection loop, back at the start of the next data
     * to process. This way we are able to also detect messages crossing
     * two reads. */
    Modes.data_len = MODES_DATA_LEN + (MODES_FULL_LEN-1)*4;
    Modes.data_ready = 0;
    /* Allocate the ICAO address cache. We use two uint32_t for every
     * entry because it's a addr / timestamp pair for every entry. */
    Modes.icao_cache = malloc(sizeof(uint32_t)*MODES_ICAO_CACHE_LEN*2);
    memset(Modes.icao_cache,0,sizeof(uint32_t)*MODES_ICAO_CACHE_LEN*2);
    Modes.aircrafts = NULL;
    Modes.interactive_last_update = 0;
    if ((Modes.data = malloc(Modes.data_len)) == NULL ||
        (Modes.magnitude = malloc(Modes.data_len*2)) == NULL) {
        fprintf(stderr, "Out of memory allocating data buffer.\n");
        exit(1);
    }
    memset(Modes.data,127,Modes.data_len);

    /* Populate the I/Q -> Magnitude lookup table. It is used because
     * sqrt or round may be expensive and may vary a lot depending on
     * the libc used.
     *
     * We scale to 0-255 range multiplying by 1.4 in order to ensure that
     * every different I/Q pair will result in a different magnitude value,
     * not losing any resolution. */
    Modes.maglut = malloc(129*129*2);
    for (i = 0; i <= 128; i++) {
        for (q = 0; q <= 128; q++) {
            Modes.maglut[i*129+q] = round(sqrt(i*i+q*q)*360);
        }
    }

    /* Statistics */
    Modes.stat_valid_preamble = 0;
    Modes.stat_demodulated = 0;
    Modes.stat_goodcrc = 0;
    Modes.stat_badcrc = 0;
    Modes.stat_fixed = 0;
    Modes.stat_single_bit_fix = 0;
    Modes.stat_two_bits_fix = 0;
    Modes.stat_http_requests = 0;
    Modes.stat_sbs_connections = 0;
    Modes.stat_out_of_phase = 0;
    Modes.exit = 0;
}

/* =============================== RTLSDR handling ========================== */

void modesInitRTLSDR(void) {
    int j;
    int device_count;
    int ppm_error = 0;
    char vendor[256], product[256], serial[256];

    device_count = rtlsdr_get_device_count();
    if (!device_count) {
        fprintf(stderr, "RTLSDR devices NOT FOUND!\n");
        exit(1);
    }

    fprintf(stderr, "Found %d device(s):\n", device_count);
    for (j = 0; j < device_count; j++) {
        rtlsdr_get_device_usb_strings(j, vendor, product, serial);
        fprintf(stderr, "%d: %s, %s, SN: %s %s\n", j, vendor, product, serial,
            (j == Modes.dev_index) ? "(currently selected)" : "");
    }

    if (rtlsdr_open(&Modes.dev, Modes.dev_index) < 0) {
        fprintf(stderr, "Error opening the RTLSDR device: %s\n",
            strerror(errno));
        exit(1);
    }

    /* Set gain, frequency, sample rate, and reset the device. */
    rtlsdr_set_tuner_gain_mode(Modes.dev,
        (Modes.gain == MODES_AUTO_GAIN) ? 0 : 1);
    if (Modes.gain != MODES_AUTO_GAIN) {
        if (Modes.gain == MODES_MAX_GAIN) {
            /* Find the maximum gain available. */
            int numgains;
            int gains[100];

            numgains = rtlsdr_get_tuner_gains(Modes.dev, gains);
            
            fprintf(stderr, "Available GAIN values: ");
            for(j=0; j<numgains;j++) fprintf(stderr, "%.2f ", gains[j]/10.0);

            // fprintf(stderr, "\nMax available gain is: %.2f\n", gains[numgains-1]/10.0);

            //установим не самую большую чувствительность
            Modes.gain = gains[numgains-6];
        }
        rtlsdr_set_tuner_gain(Modes.dev, Modes.gain);
        fprintf(stderr, "\nSET TUNER GAIN to: %.2f (rtlsdr_set_tuner_gain)\n", Modes.gain/10.0);
    } else {
        fprintf(stderr, "Using automatic gain control.\n");
    }
    rtlsdr_set_freq_correction(Modes.dev, ppm_error);
    if (Modes.enable_agc) rtlsdr_set_agc_mode(Modes.dev, 1);
    rtlsdr_set_center_freq(Modes.dev, Modes.freq);
    rtlsdr_set_sample_rate(Modes.dev, MODES_DEFAULT_RATE);
    rtlsdr_reset_buffer(Modes.dev);
    fprintf(stderr, "Current GAIN: %.2f\n",
        rtlsdr_get_tuner_gain(Modes.dev)/10.0);
}

/* We use a thread reading data in background, while the main thread
 * handles decoding and visualization of data to the user.
 *
 * The reading thread calls the RTLSDR API to read data asynchronously, and
 * uses a callback to populate the data buffer.
 * A Mutex is used to avoid races with the decoding thread. */
void rtlsdrCallback(unsigned char *buf, uint32_t len, void *ctx) {
    MODES_NOTUSED(ctx);

    pthread_mutex_lock(&Modes.data_mutex);
    if (len > MODES_DATA_LEN) len = MODES_DATA_LEN;
    /* Move the last part of the previous buffer, that was not processed,
     * on the start of the new buffer. */
    memcpy(Modes.data, Modes.data+MODES_DATA_LEN, (MODES_FULL_LEN-1)*4);
    /* Read the new data. */
    memcpy(Modes.data+(MODES_FULL_LEN-1)*4, buf, len);
    Modes.data_ready = 1;
    /* Signal to the other thread that new data is ready */
    pthread_cond_signal(&Modes.data_cond);
    pthread_mutex_unlock(&Modes.data_mutex);
}

/* This is used when --ifile is specified in order to read data from file
 * instead of using an RTLSDR device. */
void readDataFromFile(void) {
    pthread_mutex_lock(&Modes.data_mutex);
    while(1) {
        ssize_t nread, toread;
        unsigned char *p;

        if (Modes.data_ready) {
            pthread_cond_wait(&Modes.data_cond,&Modes.data_mutex);
            continue;
        }

        if (Modes.interactive) {
            /* When --ifile and --interactive are used together, slow down
             * playing at the natural rate of the RTLSDR received. */
            pthread_mutex_unlock(&Modes.data_mutex);
            usleep(5000);
            pthread_mutex_lock(&Modes.data_mutex);
        }

        /* Move the last part of the previous buffer, that was not processed,
         * on the start of the new buffer. */
        memcpy(Modes.data, Modes.data+MODES_DATA_LEN, (MODES_FULL_LEN-1)*4);
        toread = MODES_DATA_LEN;
        p = Modes.data+(MODES_FULL_LEN-1)*4;
        while(toread) {
            nread = read(Modes.fd, p, toread);
            if (nread <= 0) {
                Modes.exit = 1; /* Signal the other thread to exit. */
                break;
            }
            p += nread;
            toread -= nread;
        }
        if (toread) {
            /* Not enough data on file to fill the buffer? Pad with
             * no signal. */
            memset(p,127,toread);
        }
        Modes.data_ready = 1;
        /* Signal to the other thread that new data is ready */
        pthread_cond_signal(&Modes.data_cond);
    }
}

/* We read data using a thread, so the main thread only handles decoding
 * without caring about data acquisition. */
void *readerThreadEntryPoint(void *arg) {
    MODES_NOTUSED(arg);

    if (Modes.filename == NULL) {
        rtlsdr_read_async(Modes.dev, rtlsdrCallback, NULL,
                              MODES_ASYNC_BUF_NUMBER,
                              MODES_DATA_LEN);
    } else {
        readDataFromFile();
    }
    return NULL;
}

/* ============================== Debugging ================================= */

/* Helper function for dumpMagnitudeVector().
 * It prints a single bar used to display raw signals.
 *
 * Since every magnitude sample is between 0-255, the function uses
 * up to 63 characters for every bar. Every character represents
 * a length of 4, 3, 2, 1, specifically:
 *
 * "O" is 4
 * "o" is 3
 * "-" is 2
 * "." is 1
 */
void dumpMagnitudeBar(int index, int magnitude) {
    char *set = " .-o";
    char buf[256];
    int div = magnitude / 256 / 4;
    int rem = magnitude / 256 % 4;

    memset(buf,'O',div);
    buf[div] = set[rem];
    buf[div+1] = '\0';

    if (index >= 0)
        printf("[%.3d] |%-66s %d\n", index, buf, magnitude);
    else
        printf("[%.2d] |%-66s %d\n", index, buf, magnitude);
}

/* Display an ASCII-art alike graphical representation of the undecoded
 * message as a magnitude signal.
 *
 * The message starts at the specified offset in the "m" buffer.
 * The function will display enough data to cover a short 56 bit message.
 *
 * If possible a few samples before the start of the messsage are included
 * for context. */

void dumpMagnitudeVector(uint16_t *m, uint32_t offset) {
    uint32_t padding = 5; /* Show a few samples before the actual start. */
    uint32_t start = (offset < padding) ? 0 : offset-padding;
    uint32_t end = offset + (MODES_PREAMBLE_US*2)+(MODES_SHORT_MSG_BITS*2) - 1;
    uint32_t j;

    for (j = start; j <= end; j++) {
        dumpMagnitudeBar(j-offset, m[j]);
    }
}

/* Produce a raw representation of the message as a Javascript file
 * loadable by debug.html. */
void dumpRawMessageJS(char *descr, unsigned char *msg,
                      uint16_t *m, uint32_t offset, int fixable)
{
    int padding = 5; /* Show a few samples before the actual start. */
    int start = offset - padding;
    int end = offset + (MODES_PREAMBLE_US*2)+(MODES_LONG_MSG_BITS*2) - 1;
    FILE *fp;
    int j, fix1 = -1, fix2 = -1;

    if (fixable != -1) {
        fix1 = fixable & 0xff;
        if (fixable > 255) fix2 = fixable >> 8;
    }

    if ((fp = fopen("frames.js","a")) == NULL) {
        fprintf(stderr, "Error opening frames.js: %s\n", strerror(errno));
        exit(1);
    }

    fprintf(fp,"frames.push({\"descr\": \"%s\", \"mag\": [", descr);
    for (j = start; j <= end; j++) {
        fprintf(fp,"%d", j < 0 ? 0 : m[j]);
        if (j != end) fprintf(fp,",");
    }
    fprintf(fp,"], \"fix1\": %d, \"fix2\": %d, \"bits\": %d, \"hex\": \"",
        fix1, fix2, modesMessageLenByType(msg[0]>>3));
    for (j = 0; j < MODES_LONG_MSG_BYTES; j++)
        fprintf(fp,"\\x%02x",msg[j]);
    fprintf(fp,"\"});\n");
    fclose(fp);
}

/* This is a wrapper for dumpMagnitudeVector() that also show the message
 * in hex format with an additional description.
 *
 * descr  is the additional message to show to describe the dump.
 * msg    points to the decoded message
 * m      is the original magnitude vector
 * offset is the offset where the message starts
 *
 * The function also produces the Javascript file used by debug.html to
 * display packets in a graphical format if the Javascript output was
 * enabled.
 */
void dumpRawMessage(char *descr, unsigned char *msg,
                    uint16_t *m, uint32_t offset)
{
    int j;
    int msgtype = msg[0]>>3;
    int fixable = -1;

    if (msgtype == 11 || msgtype == 17) {
        int msgbits = (msgtype == 11) ? MODES_SHORT_MSG_BITS :
                                        MODES_LONG_MSG_BITS;
        fixable = fixSingleBitErrors(msg,msgbits);
        if (fixable == -1)
            fixable = fixTwoBitsErrors(msg,msgbits);
    }

    if (Modes.debug & MODES_DEBUG_JS) {
        dumpRawMessageJS(descr, msg, m, offset, fixable);
        return;
    }

    printf("\n--- %s\n    ", descr);
    for (j = 0; j < MODES_LONG_MSG_BYTES; j++) {
        printf("%02x",msg[j]);
        if (j == MODES_SHORT_MSG_BYTES-1) printf(" ... ");
    }
    printf(" (DF %d, Fixable: %d)\n", msgtype, fixable);
    dumpMagnitudeVector(m,offset);
    printf("---\n\n");
}

/* ===================== Mode S detection and decoding  ===================== */

/* Parity table for MODE S Messages.
 * The table contains 112 elements, every element corresponds to a bit set
 * in the message, starting from the first bit of actual data after the
 * preamble.
 *
 * For messages of 112 bit, the whole table is used.
 * For messages of 56 bits only the last 56 elements are used.
 *
 * The algorithm is as simple as xoring all the elements in this table
 * for which the corresponding bit on the message is set to 1.
 *
 * The latest 24 elements in this table are set to 0 as the checksum at the
 * end of the message should not affect the computation.
 *
 * Note: this function can be used with DF11 and DF17, other modes have
 * the CRC xored with the sender address as they are reply to interrogations,
 * but a casual listener can't split the address from the checksum.
 */
uint32_t modes_checksum_table[112] = {
0x3935ea, 0x1c9af5, 0xf1b77e, 0x78dbbf, 0xc397db, 0x9e31e9, 0xb0e2f0, 0x587178,
0x2c38bc, 0x161c5e, 0x0b0e2f, 0xfa7d13, 0x82c48d, 0xbe9842, 0x5f4c21, 0xd05c14,
0x682e0a, 0x341705, 0xe5f186, 0x72f8c3, 0xc68665, 0x9cb936, 0x4e5c9b, 0xd8d449,
0x939020, 0x49c810, 0x24e408, 0x127204, 0x093902, 0x049c81, 0xfdb444, 0x7eda22,
0x3f6d11, 0xe04c8c, 0x702646, 0x381323, 0xe3f395, 0x8e03ce, 0x4701e7, 0xdc7af7,
0x91c77f, 0xb719bb, 0xa476d9, 0xadc168, 0x56e0b4, 0x2b705a, 0x15b82d, 0xf52612,
0x7a9309, 0xc2b380, 0x6159c0, 0x30ace0, 0x185670, 0x0c2b38, 0x06159c, 0x030ace,
0x018567, 0xff38b7, 0x80665f, 0xbfc92b, 0xa01e91, 0xaff54c, 0x57faa6, 0x2bfd53,
0xea04ad, 0x8af852, 0x457c29, 0xdd4410, 0x6ea208, 0x375104, 0x1ba882, 0x0dd441,
0xf91024, 0x7c8812, 0x3e4409, 0xe0d800, 0x706c00, 0x383600, 0x1c1b00, 0x0e0d80,
0x0706c0, 0x038360, 0x01c1b0, 0x00e0d8, 0x00706c, 0x003836, 0x001c1b, 0xfff409,
0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000
};

uint32_t modesChecksum(unsigned char *msg, int bits) {
    uint32_t crc = 0;
    int offset = (bits == 112) ? 0 : (112-56);
    int j;

    for(j = 0; j < bits; j++) {
        int byte = j/8;
        int bit = j%8;
        int bitmask = 1 << (7-bit);

        /* If bit is set, xor with corresponding table entry. */
        if (msg[byte] & bitmask)
            crc ^= modes_checksum_table[j+offset];
    }
    return crc; /* 24 bit checksum. */
}

/* Given the Downlink Format (DF) of the message, return the message length
 * in bits. */
int modesMessageLenByType(int type) {
    if (type == 16 || type == 17 ||
        type == 19 || type == 20 ||
        type == 21)
        return MODES_LONG_MSG_BITS;
    else
        return MODES_SHORT_MSG_BITS;
}

/* Try to fix single bit errors using the checksum. On success modifies
 * the original buffer with the fixed version, and returns the position
 * of the error bit. Otherwise if fixing failed -1 is returned. */
int fixSingleBitErrors(unsigned char *msg, int bits) {
    int j;
    unsigned char aux[MODES_LONG_MSG_BITS/8];

    for (j = 0; j < bits; j++) {
        int byte = j/8;
        int bitmask = 1 << (7-(j%8));
        uint32_t crc1, crc2;

        memcpy(aux,msg,bits/8);
        aux[byte] ^= bitmask; /* Flip j-th bit. */

        crc1 = ((uint32_t)aux[(bits/8)-3] << 16) |
               ((uint32_t)aux[(bits/8)-2] << 8) |
                (uint32_t)aux[(bits/8)-1];
        crc2 = modesChecksum(aux,bits);

        if (crc1 == crc2) {
            /* The error is fixed. Overwrite the original buffer with
             * the corrected sequence, and returns the error bit
             * position. */
            memcpy(msg,aux,bits/8);
            return j;
        }
    }
    return -1;
}

/* Similar to fixSingleBitErrors() but try every possible two bit combination.
 * This is very slow and should be tried only against DF17 messages that
 * don't pass the checksum, and only in Aggressive Mode. */
int fixTwoBitsErrors(unsigned char *msg, int bits) {
    int j, i;
    unsigned char aux[MODES_LONG_MSG_BITS/8];

    for (j = 0; j < bits; j++) {
        int byte1 = j/8;
        int bitmask1 = 1 << (7-(j%8));

        /* Don't check the same pairs multiple times, so i starts from j+1 */
        for (i = j+1; i < bits; i++) {
            int byte2 = i/8;
            int bitmask2 = 1 << (7-(i%8));
            uint32_t crc1, crc2;

            memcpy(aux,msg,bits/8);

            aux[byte1] ^= bitmask1; /* Flip j-th bit. */
            aux[byte2] ^= bitmask2; /* Flip i-th bit. */

            crc1 = ((uint32_t)aux[(bits/8)-3] << 16) |
                   ((uint32_t)aux[(bits/8)-2] << 8) |
                    (uint32_t)aux[(bits/8)-1];
            crc2 = modesChecksum(aux,bits);

            if (crc1 == crc2) {
                /* The error is fixed. Overwrite the original buffer with
                 * the corrected sequence, and returns the error bit
                 * position. */
                memcpy(msg,aux,bits/8);
                /* We return the two bits as a 16 bit integer by shifting
                 * 'i' on the left. This is possible since 'i' will always
                 * be non-zero because i starts from j+1. */
                return j | (i<<8);
            }
        }
    }
    return -1;
}

/* Hash the ICAO address to index our cache of MODES_ICAO_CACHE_LEN
 * elements, that is assumed to be a power of two. */
uint32_t ICAOCacheHashAddress(uint32_t a) {
    /* The following three rounds wil make sure that every bit affects
     * every output bit with ~ 50% of probability. */
    a = ((a >> 16) ^ a) * 0x45d9f3b;
    a = ((a >> 16) ^ a) * 0x45d9f3b;
    a = ((a >> 16) ^ a);
    return a & (MODES_ICAO_CACHE_LEN-1);
}

/* Add the specified entry to the cache of recently seen ICAO addresses.
 * Note that we also add a timestamp so that we can make sure that the
 * entry is only valid for MODES_ICAO_CACHE_TTL seconds. */
void addRecentlySeenICAOAddr(uint32_t addr) {
    uint32_t h = ICAOCacheHashAddress(addr);
    Modes.icao_cache[h*2] = addr;
    Modes.icao_cache[h*2+1] = (uint32_t) time(NULL);
}

/* Returns 1 if the specified ICAO address was seen in a DF format with
 * proper checksum (not xored with address) no more than * MODES_ICAO_CACHE_TTL
 * seconds ago. Otherwise returns 0. */
int ICAOAddressWasRecentlySeen(uint32_t addr) {
    uint32_t h = ICAOCacheHashAddress(addr);
    uint32_t a = Modes.icao_cache[h*2];
    uint32_t t = Modes.icao_cache[h*2+1];

    return a && a == addr && time(NULL)-t <= MODES_ICAO_CACHE_TTL;
}

/* If the message type has the checksum xored with the ICAO address, try to
 * brute force it using a list of recently seen ICAO addresses.
 *
 * Do this in a brute-force fashion by xoring the predicted CRC with
 * the address XOR checksum field in the message. This will recover the
 * address: if we found it in our cache, we can assume the message is ok.
 *
 * This function expects mm->msgtype and mm->msgbits to be correctly
 * populated by the caller.
 *
 * On success the correct ICAO address is stored in the modesMessage
 * structure in the aa3, aa2, and aa1 fiedls.
 *
 * If the function successfully recovers a message with a correct checksum
 * it returns 1. Otherwise 0 is returned. */
int bruteForceAP(unsigned char *msg, struct modesMessage *mm) {
    unsigned char aux[MODES_LONG_MSG_BYTES];
    int msgtype = mm->msgtype;
    int msgbits = mm->msgbits;

    if (msgtype == 0 ||         /* Short air surveillance */
        msgtype == 4 ||         /* Surveillance, altitude reply */
        msgtype == 5 ||         /* Surveillance, identity reply */
        msgtype == 16 ||        /* Long Air-Air survillance */
        msgtype == 20 ||        /* Comm-A, altitude request */
        msgtype == 21 ||        /* Comm-A, identity request */
        msgtype == 24)          /* Comm-C ELM */
    {
        uint32_t addr;
        uint32_t crc;
        int lastbyte = (msgbits/8)-1;

        /* Work on a copy. */
        memcpy(aux,msg,msgbits/8);

        /* Compute the CRC of the message and XOR it with the AP field
         * so that we recover the address, because:
         *
         * (ADDR xor CRC) xor CRC = ADDR. */
        crc = modesChecksum(aux,msgbits);
        aux[lastbyte] ^= crc & 0xff;
        aux[lastbyte-1] ^= (crc >> 8) & 0xff;
        aux[lastbyte-2] ^= (crc >> 16) & 0xff;
        
        /* If the obtained address exists in our cache we consider
         * the message valid. */
        addr = aux[lastbyte] | (aux[lastbyte-1] << 8) | (aux[lastbyte-2] << 16);
        if (ICAOAddressWasRecentlySeen(addr)) {
            mm->aa1 = aux[lastbyte-2];
            mm->aa2 = aux[lastbyte-1];
            mm->aa3 = aux[lastbyte];
            return 1;
        }
    }
    return 0;
}

/* Decode the 13 bit AC altitude field (in DF 20 and others).
 * Returns the altitude, and set 'unit' to either MODES_UNIT_METERS
 * or MDOES_UNIT_FEETS. */
int decodeAC13Field(unsigned char *msg, int *unit) {
    int m_bit = msg[3] & (1<<6);
    int q_bit = msg[3] & (1<<4);

    if (!m_bit) {
        *unit = MODES_UNIT_FEET;
        if (q_bit) {
            /* N is the 11 bit integer resulting from the removal of bit
             * Q and M */
            int n = ((msg[2]&31)<<6) |
                    ((msg[3]&0x80)>>2) |
                    ((msg[3]&0x20)>>1) |
                     (msg[3]&15);
            /* The final altitude is due to the resulting number multiplied
             * by 25, minus 1000. */
            return n*25-1000;
        } else {
            /* TODO: Implement altitude where Q=0 and M=0 */
        }
    } else {
        *unit = MODES_UNIT_METERS;
        /* TODO: Implement altitude when meter unit is selected. */
    }
    return 0;
}

/* Decode the 12 bit AC altitude field (in DF 17 and others).
 * Returns the altitude or 0 if it can't be decoded. */
int decodeAC12Field(unsigned char *msg, int *unit) {
    int q_bit = msg[5] & 1;

    if (q_bit) {
        /* N is the 11 bit integer resulting from the removal of bit
         * Q */
        *unit = MODES_UNIT_FEET;
        int n = ((msg[5]>>1)<<4) | ((msg[6]&0xF0) >> 4);
        /* The final altitude is due to the resulting number multiplied
         * by 25, minus 1000. */
        return n*25-1000;
    } else {
        return 0;
    }
}

/* Capability table. */
char *ca_str[8] = {
    /* 0 */ "Level 1 (Survillance Only)",
    /* 1 */ "Level 2 (DF0,4,5,11)",
    /* 2 */ "Level 3 (DF0,4,5,11,20,21)",
    /* 3 */ "Level 4 (DF0,4,5,11,20,21,24)",
    /* 4 */ "Level 2+3+4 (DF0,4,5,11,20,21,24,code7 - is on ground)",
    /* 5 */ "Level 2+3+4 (DF0,4,5,11,20,21,24,code7 - is on airborne)",
    /* 6 */ "Level 2+3+4 (DF0,4,5,11,20,21,24,code7)",
    /* 7 */ "Level 7 ???"
};

/* Flight status table. */
char *fs_str[8] = {
    /* 0 */ "Normal, Airborne",
    /* 1 */ "Normal, On the ground",
    /* 2 */ "ALERT,  Airborne",
    /* 3 */ "ALERT,  On the ground",
    /* 4 */ "ALERT & Special Position Identification. Airborne or Ground",
    /* 5 */ "Special Position Identification. Airborne or Ground",
    /* 6 */ "Value 6 is not assigned",
    /* 7 */ "Value 7 is not assigned"
};

/* ME message type to description table. */
char *me_str[] = {
};

char *getMEDescription(int metype, int mesub) {
    char *mename = "Unknown";

    if (metype >= 1 && metype <= 4)
        mename = "Aircraft Identification and Category";
    else if (metype >= 5 && metype <= 8)
        mename = "Surface Position";
    else if (metype >= 9 && metype <= 18)
        mename = "Airborne Position (Baro Altitude)";
    else if (metype == 19 && mesub >=1 && mesub <= 4)
        mename = "Airborne Velocity";
    else if (metype >= 20 && metype <= 22)
        mename = "Airborne Position (GNSS Height)";
    else if (metype == 23 && mesub == 0)
        mename = "Test Message";
    else if (metype == 24 && mesub == 1)
        mename = "Surface System Status";
    else if (metype == 28 && mesub == 1)
        mename = "Extended Squitter Aircraft Status (Emergency)";
    else if (metype == 28 && mesub == 2)
        mename = "Extended Squitter Aircraft Status (1090ES TCAS RA)";
    else if (metype == 29 && (mesub == 0 || mesub == 1))
        mename = "Target State and Status Message";
    else if (metype == 31 && (mesub == 0 || mesub == 1))
        mename = "Aircraft Operational Status Message";
    return mename;
}

/* Decode a raw Mode S message demodulated as a stream of bytes by
 * detectUVD(), and split it into fields populating a modesMessage
 * structure. */
void decodeModesMessage(struct modesMessage *mm, unsigned char *msg) {
    uint32_t crc2;   /* Computed CRC, used to verify the message CRC. */
    char *ais_charset = "?ABCDEFGHIJKLMNOPQRSTUVWXYZ????? ???????????????0123456789??????";

    /* Work on our local copy */
    memcpy(mm->msg,msg,MODES_LONG_MSG_BYTES);
    msg = mm->msg;

    /* Get the message type ASAP as other operations depend on this */
    mm->msgtype = msg[0]>>3;    /* Downlink Format */
    mm->msgbits = modesMessageLenByType(mm->msgtype);

    /* CRC is always the last three bytes. */
    mm->crc = ((uint32_t)msg[(mm->msgbits/8)-3] << 16) |
              ((uint32_t)msg[(mm->msgbits/8)-2] << 8) |
               (uint32_t)msg[(mm->msgbits/8)-1];
    crc2 = modesChecksum(msg,mm->msgbits);

    /* Check CRC and fix single bit errors using the CRC when
     * possible (DF 11 and 17). */
    mm->errorbit = -1;  /* No error */
    mm->crcok = (mm->crc == crc2);

    if (!mm->crcok && Modes.fix_errors &&
        (mm->msgtype == 11 || mm->msgtype == 17))
    {
        if ((mm->errorbit = fixSingleBitErrors(msg,mm->msgbits)) != -1) {
            mm->crc = modesChecksum(msg,mm->msgbits);
            mm->crcok = 1;
        } else if (Modes.aggressive && mm->msgtype == 17 &&
                   (mm->errorbit = fixTwoBitsErrors(msg,mm->msgbits)) != -1)
        {
            mm->crc = modesChecksum(msg,mm->msgbits);
            mm->crcok = 1;
        }
    }

    /* Note that most of the other computation happens *after* we fix
     * the single bit errors, otherwise we would need to recompute the
     * fields again. */
    mm->ca = msg[0] & 7;        /* Responder capabilities. */

    /* ICAO address */
    mm->aa1 = msg[1];
    mm->aa2 = msg[2];
    mm->aa3 = msg[3];

    /* DF 17 type (assuming this is a DF17, otherwise not used) */
    mm->metype = msg[4] >> 3;   /* Extended squitter message type. */
    mm->mesub = msg[4] & 7;     /* Extended squitter message subtype. */

    /* Fields for DF4,5,20,21 */
    mm->fs = msg[0] & 7;        /* Flight status for DF4,5,20,21 */
    mm->dr = msg[1] >> 3 & 31;  /* Request extraction of downlink request. */
    mm->um = ((msg[1] & 7)<<3)| /* Request extraction of downlink request. */
              msg[2]>>5;

    /* In the squawk (identity) field bits are interleaved like that
     * (message bit 20 to bit 32):
     *
     * C1-A1-C2-A2-C4-A4-ZERO-B1-D1-B2-D2-B4-D4
     *
     * So every group of three bits A, B, C, D represent an integer
     * from 0 to 7.
     *
     * The actual meaning is just 4 octal numbers, but we convert it
     * into a base ten number tha happens to represent the four
     * octal numbers.
     *
     * For more info: http://en.wikipedia.org/wiki/Gillham_code */
    {
        int a,b,c,d;

        a = ((msg[3] & 0x80) >> 5) |
            ((msg[2] & 0x02) >> 0) |
            ((msg[2] & 0x08) >> 3);
        b = ((msg[3] & 0x02) << 1) |
            ((msg[3] & 0x08) >> 2) |
            ((msg[3] & 0x20) >> 5);
        c = ((msg[2] & 0x01) << 2) |
            ((msg[2] & 0x04) >> 1) |
            ((msg[2] & 0x10) >> 4);
        d = ((msg[3] & 0x01) << 2) |
            ((msg[3] & 0x04) >> 1) |
            ((msg[3] & 0x10) >> 4);
        mm->identity = a*1000 + b*100 + c*10 + d;
    }

    /* DF 11 & 17: try to populate our ICAO addresses whitelist.
     * DFs with an AP field (xored addr and crc), try to decode it. */
    if (mm->msgtype != 11 && mm->msgtype != 17) {
        /* Check if we can check the checksum for the Downlink Formats where
         * the checksum is xored with the aircraft ICAO address. We try to
         * brute force it using a list of recently seen aircraft addresses. */
        if (bruteForceAP(msg,mm)) {
            /* We recovered the message, mark the checksum as valid. */
            mm->crcok = 1;
        } else {
            mm->crcok = 0;
        }
    } else {
        /* If this is DF 11 or DF 17 and the checksum was ok,
         * we can add this address to the list of recently seen
         * addresses. */
        if (mm->crcok && mm->errorbit == -1) {
            uint32_t addr = (mm->aa1 << 16) | (mm->aa2 << 8) | mm->aa3;
            addRecentlySeenICAOAddr(addr);
        }
    }

    /* Decode 13 bit altitude for DF0, DF4, DF16, DF20 */
    if (mm->msgtype == 0 || mm->msgtype == 4 ||
        mm->msgtype == 16 || mm->msgtype == 20) {
        mm->altitude = decodeAC13Field(msg, &mm->unit);
    }

    /* Decode extended squitter specific stuff. */
    if (mm->msgtype == 17) {
        /* Decode the extended squitter message. */

        if (mm->metype >= 1 && mm->metype <= 4) {
            /* Aircraft Identification and Category */
            mm->aircraft_type = mm->metype-1;
            mm->flight[0] = ais_charset[msg[5]>>2];
            mm->flight[1] = ais_charset[((msg[5]&3)<<4)|(msg[6]>>4)];
            mm->flight[2] = ais_charset[((msg[6]&15)<<2)|(msg[7]>>6)];
            mm->flight[3] = ais_charset[msg[7]&63];
            mm->flight[4] = ais_charset[msg[8]>>2];
            mm->flight[5] = ais_charset[((msg[8]&3)<<4)|(msg[9]>>4)];
            mm->flight[6] = ais_charset[((msg[9]&15)<<2)|(msg[10]>>6)];
            mm->flight[7] = ais_charset[msg[10]&63];
            mm->flight[8] = '\0';
        } else if (mm->metype >= 9 && mm->metype <= 18) {
            /* Airborne position Message */
            mm->fflag = msg[6] & (1<<2);
            mm->tflag = msg[6] & (1<<3);
            mm->altitude = decodeAC12Field(msg,&mm->unit);
            mm->raw_latitude = ((msg[6] & 3) << 15) |
                                (msg[7] << 7) |
                                (msg[8] >> 1);
            mm->raw_longitude = ((msg[8]&1) << 16) |
                                 (msg[9] << 8) |
                                 msg[10];
        } else if (mm->metype == 19 && mm->mesub >= 1 && mm->mesub <= 4) {
            /* Airborne Velocity Message */
            if (mm->mesub == 1 || mm->mesub == 2) {
                mm->ew_dir = (msg[5]&4) >> 2;
                mm->ew_velocity = ((msg[5]&3) << 8) | msg[6];
                mm->ns_dir = (msg[7]&0x80) >> 7;
                mm->ns_velocity = ((msg[7]&0x7f) << 3) | ((msg[8]&0xe0) >> 5);
                mm->vert_rate_source = (msg[8]&0x10) >> 4;
                mm->vert_rate_sign = (msg[8]&0x8) >> 3;
                mm->vert_rate = ((msg[8]&7) << 6) | ((msg[9]&0xfc) >> 2);
                /* Compute velocity and angle from the two speed
                 * components. */
                mm->velocity = sqrt(mm->ns_velocity*mm->ns_velocity+
                                    mm->ew_velocity*mm->ew_velocity);
                if (mm->velocity) {
                    int ewv = mm->ew_velocity;
                    int nsv = mm->ns_velocity;
                    double heading;

                    if (mm->ew_dir) ewv *= -1;
                    if (mm->ns_dir) nsv *= -1;
                    heading = atan2(ewv,nsv);

                    /* Convert to degrees. */
                    mm->heading = heading * 360 / (M_PI*2);
                    /* We don't want negative values but a 0-360 scale. */
                    if (mm->heading < 0) mm->heading += 360;
                } else {
                    mm->heading = 0;
                }
            } else if (mm->mesub == 3 || mm->mesub == 4) {
                mm->heading_is_valid = msg[5] & (1<<2);
                mm->heading = (360.0/128) * (((msg[5] & 3) << 5) |
                                              (msg[6] >> 3));
            }
        }
    }
    mm->phase_corrected = 0; /* Set to 1 by the caller if needed. */
}

/* This function gets a decoded Mode S Message and prints it on the screen
 * in a human readable format. */
void displayModesMessage(struct modesMessage *mm) {
    int j;

    /* Handle only addresses mode first. */
    if (Modes.onlyaddr) {
        printf("%02x%02x%02x\n", mm->aa1, mm->aa2, mm->aa3);
        return;
    }

    /* Show the raw message. */
    printf("*");
    for (j = 0; j < mm->msgbits/8; j++) printf("%02x", mm->msg[j]);
    printf(";\n");

    if (Modes.raw) {
        fflush(stdout); /* Provide data to the reader ASAP. */
        return; /* Enough for --raw mode */
    }

    printf("CRC: %06x (%s)\n", (int)mm->crc, mm->crcok ? "ok" : "wrong");
    if (mm->errorbit != -1)
        printf("Single bit error fixed, bit %d\n", mm->errorbit);

    if (mm->msgtype == 0) {
        /* DF 0 */
        printf("DF 0: Short Air-Air Surveillance.\n");
        printf("  Altitude       : %d %s\n", mm->altitude,
            (mm->unit == MODES_UNIT_METERS) ? "meters" : "feet");
        printf("  ICAO Address   : %02x%02x%02x\n", mm->aa1, mm->aa2, mm->aa3);
    } else if (mm->msgtype == 4 || mm->msgtype == 20) {
        printf("DF %d: %s, Altitude Reply.\n", mm->msgtype,
            (mm->msgtype == 4) ? "Surveillance" : "Comm-B");
        printf("  Flight Status  : %s\n", fs_str[mm->fs]);
        printf("  DR             : %d\n", mm->dr);
        printf("  UM             : %d\n", mm->um);
        printf("  Altitude       : %d %s\n", mm->altitude,
            (mm->unit == MODES_UNIT_METERS) ? "meters" : "feet");
        printf("  ICAO Address   : %02x%02x%02x\n", mm->aa1, mm->aa2, mm->aa3);

        if (mm->msgtype == 20) {
            /* TODO: 56 bits DF20 MB additional field. */
        }
    } else if (mm->msgtype == 5 || mm->msgtype == 21) {
        printf("DF %d: %s, Identity Reply.\n", mm->msgtype,
            (mm->msgtype == 5) ? "Surveillance" : "Comm-B");
        printf("  Flight Status  : %s\n", fs_str[mm->fs]);
        printf("  DR             : %d\n", mm->dr);
        printf("  UM             : %d\n", mm->um);
        printf("  Squawk         : %d\n", mm->identity);
        printf("  ICAO Address   : %02x%02x%02x\n", mm->aa1, mm->aa2, mm->aa3);

        if (mm->msgtype == 21) {
            /* TODO: 56 bits DF21 MB additional field. */
        }
    } else if (mm->msgtype == 11) {
        /* DF 11 */
        printf("DF 11: All Call Reply.\n");
        printf("  Capability  : %s\n", ca_str[mm->ca]);
        printf("  ICAO Address: %02x%02x%02x\n", mm->aa1, mm->aa2, mm->aa3);
    } else if (mm->msgtype == 17) {
        /* DF 17 */
        printf("DF 17: ADS-B message.\n");
        printf("  Capability     : %d (%s)\n", mm->ca, ca_str[mm->ca]);
        printf("  ICAO Address   : %02x%02x%02x\n", mm->aa1, mm->aa2, mm->aa3);
        printf("  Extended Squitter  Type: %d\n", mm->metype);
        printf("  Extended Squitter  Sub : %d\n", mm->mesub);
        printf("  Extended Squitter  Name: %s\n",
            getMEDescription(mm->metype,mm->mesub));

        /* Decode the extended squitter message. */
        if (mm->metype >= 1 && mm->metype <= 4) {
            /* Aircraft identification. */
            char *ac_type_str[4] = {
                "Aircraft Type D",
                "Aircraft Type C",
                "Aircraft Type B",
                "Aircraft Type A"
            };

            printf("    Aircraft Type  : %s\n", ac_type_str[mm->aircraft_type]);
            printf("    Identification : %s\n", mm->flight);
        } else if (mm->metype >= 9 && mm->metype <= 18) {
            printf("    F flag   : %s\n", mm->fflag ? "odd" : "even");
            printf("    T flag   : %s\n", mm->tflag ? "UTC" : "non-UTC");
            printf("    Altitude : %d feet\n", mm->altitude);
            printf("    Latitude : %d (not decoded)\n", mm->raw_latitude);
            printf("    Longitude: %d (not decoded)\n", mm->raw_longitude);
        } else if (mm->metype == 19 && mm->mesub >= 1 && mm->mesub <= 4) {
            if (mm->mesub == 1 || mm->mesub == 2) {
                /* Velocity */
                printf("    EW direction      : %d\n", mm->ew_dir);
                printf("    EW velocity       : %d\n", mm->ew_velocity);
                printf("    NS direction      : %d\n", mm->ns_dir);
                printf("    NS velocity       : %d\n", mm->ns_velocity);
                printf("    Vertical rate src : %d\n", mm->vert_rate_source);
                printf("    Vertical rate sign: %d\n", mm->vert_rate_sign);
                printf("    Vertical rate     : %d\n", mm->vert_rate);
            } else if (mm->mesub == 3 || mm->mesub == 4) {
                printf("    Heading status: %d", mm->heading_is_valid);
                printf("    Heading: %d", mm->heading);
            }
        } else {
            printf("    Unrecognized ME type: %d subtype: %d\n", 
                mm->metype, mm->mesub);
        }
    } else {
        if (Modes.check_crc)
            printf("DF %d with good CRC received "
                   "(decoding still not implemented).\n",
                mm->msgtype);
    }
}

/* Turn I/Q samples pointed by Modes.data into the magnitude vector
 * pointed by Modes.magnitude. */
void computeMagnitudeVector(void) {
    uint16_t *m = Modes.magnitude;
    unsigned char *p = Modes.data;
    uint32_t j;

    /* Compute the magnitudo vector. It's just SQRT(I^2 + Q^2), but
     * we rescale to the 0-255 range to exploit the full resolution. */
    for (j = 0; j < Modes.data_len; j += 2) {
        int i = p[j]-127;
        int q = p[j+1]-127;

        if (i < 0) i = -i;
        if (q < 0) q = -q;
        m[j/2] = Modes.maglut[i*129+q];
    }
}

/* Return -1 if the message is out of fase left-side
 * Return  1 if the message is out of fase right-size
 * Return  0 if the message is not particularly out of phase.
 *
 * Note: this function will access m[-1], so the caller should make sure to
 * call it only if we are not at the start of the current buffer. */
int detectOutOfPhase(uint16_t *m) {
    if (m[3] > m[2]/3) return 1;
    if (m[10] > m[9]/3) return 1;
    if (m[6] > m[7]/3) return -1;
    if (m[-1] > m[1]/3) return -1;
    return 0;
}

/* This function does not really correct the phase of the message, it just
 * applies a transformation to the first sample representing a given bit:
 *
 * If the previous bit was one, we amplify it a bit.
 * If the previous bit was zero, we decrease it a bit.
 *
 * This simple transformation makes the message a bit more likely to be
 * correctly decoded for out of phase messages:
 *
 * When messages are out of phase there is more uncertainty in
 * sequences of the same bit multiple times, since 11111 will be
 * transmitted as continuously altering magnitude (high, low, high, low...)
 * 
 * However because the message is out of phase some part of the high
 * is mixed in the low part, so that it is hard to distinguish if it is
 * a zero or a one.
 *
 * However when the message is out of phase passing from 0 to 1 or from
 * 1 to 0 happens in a very recognizable way, for instance in the 0 -> 1
 * transition, magnitude goes low, high, high, low, and one of of the
 * two middle samples the high will be *very* high as part of the previous
 * or next high signal will be mixed there.
 *
 * Applying our simple transformation we make more likely if the current
 * bit is a zero, to detect another zero. Symmetrically if it is a one
 * it will be more likely to detect a one because of the transformation.
 * In this way similar levels will be interpreted more likely in the
 * correct way. */
void applyPhaseCorrection(uint16_t *m) {
    int j;

    m += 16; /* Skip preamble. */
    for (j = 0; j < (MODES_LONG_MSG_BITS-1)*2; j += 2) {
        if (m[j] > m[j+1]) {
            /* One */
            m[j+2] = (m[j+2] * 5) / 4;
        } else {
            /* Zero */
            m[j+2] = (m[j+2] * 4) / 5;
        }
    }
}

/*
12:11:09.991 - OK1 OK RKK=110 REGN=46008 OK1VAL(6)=6 //ok1-12-11-09-991.data
12:11:11.210 - OK1 OK RKK=110 REGN=46008 OK1VAL(6)=6
12:11:11.284 - OK2 OK RKK=000 [2605<10287 - 2520<8935 - 1589<9664] 1298>1000 MED=667 OK2VAL(0)=0
12:11:23.345 - OK2 OK RKK=000 [4281<20339 - 3538<21294 - 3559<22631] 1440>1104 MED=736 OK2VAL(0)=0
12:11:25.642 - OK2 OK RKK=000 [3114<19472 - 2754<20288 - 3114<20997] 2621>1800 MED=1200 OK2VAL(0)=0
12:11:25.708 - OK1 OK RKK=110 REGN=46008 OK1VAL(6)=6
12:11:25.917 - OK2 OK RKK=000 [3474<17277 - 3029<18616 - 3412<18666] 1298>1072 MED=715 OK2VAL(0)=0
12:11:25.980 - OK1 OK RKK=110 REGN=46008 OK1VAL(6)=6
12:11:26.350 - OK2 OK RKK=000 [2965<17440 - 2309<16474 - 3412<17557] 2546>1543 MED=1029 OK2VAL(0)=0
12:11:29.153 - OK1 OK RKK=110 REGN=46008 OK1VAL(6)=6
12:11:30.503 - OK2 OK RKK=000 [3532<37667 - 2394<36262 - 3905<36564] 4694>3034 MED=2023 OK2VAL(0)=0
12:11:43.626 - OK2 OK RKK=000 [2396<17005 - 3745<15686 - 3749<15650] 2277>1689 MED=1126 OK2VAL(0)=0
12:11:44.545 - OK2 OK RKK=000 [3836<36384 - 4921<36642 - 3073<36475] 5192>2988 MED=1992 OK2VAL(0)=0
12:11:44.758 - OK1 OK RKK=110 REGN=46008 OK1VAL(6)=6
12:11:48.359 - OK2 OK RKK=000 [3687<63406 - 3623<63221 - 3098<62530] 8707>5047 MED=3365 OK2VAL(0)=0
12:11:48.576 - OK1 OK RKK=110 REGN=46008 OK1VAL(6)=6
12:11:49.200 - OK1 OK RKK=110 REGN=46008 OK1VAL(6)=6
12:11:49.274 - OK2 OK RKK=000 [5123<58572 - 5403<57933 - 4676<57534] 7953>4701 MED=3134 OK2VAL(0)=0
12:11:52.374 - OK1 OK RKK=110 REGN=46008 OK1VAL(6)=6
12:11:54.041 - OK2 OK RKK=000 [10633<50138 - 9960<50059 - 10558<51716] 5805>2787 MED=1858 OK2VAL(0)=0
12:11:55.157 - OK2 OK RKK=000 [11231<60139 - 10710<59906 - 10979<58834] 6638>3282 MED=2188 OK2VAL(0)=0
12:11:59.130 - OK2 OK RKK=000 [7304<57475 - 6378<56610 - 6779<54683] 3671>2547 MED=1698 OK2VAL(0)=0
12:11:59.218 - OK1 OK RKK=110 REGN=46008 OK1VAL(6)=6
12:11:59.597 - OK2 OK RKK=000 [11131<40791 - 11128<39836 - 10490<41147] 3097>2164 MED=1443 OK2VAL(0)=0
12:12:00.152 - OK2 OK RKK=000 [7196<45358 - 7002<45191 - 6699<45350] 2902>2313 MED=1542 OK2VAL(0)=0
12:12:03.545 - OK2 OK RKK=000 [4112<26146 - 4041<26514 - 5148<26968] 1527>1470 MED=980 OK2VAL(0)=0
12:12:03.844 - OK2 OK RKK=000 [6232<22906 - 5997<22931 - 5625<23477] 1800>1614 MED=1076 OK2VAL(0)=0
12:12:04.652 - OK2 OK RKK=000 [7965<23333 - 4554<22486 - 5482<23362] 1298>1234 MED=823 OK2VAL(0)=0
12:12:04.821 - OK2 OK RKK=000 [3263<20020 - 4710<19891 - 2967<19635] 2969>2512 MED=1675 OK2VAL(0)=0
12:12:05.059 - OK1 OK RKK=110 REGN=46008 OK1VAL(6)=6
12:12:05.301 - OK2 OK RKK=000 [3476<22004 - 3114<21642 - 3474<21645] 3396>2574 MED=1716 OK2VAL(0)=0
12:12:05.375 - OK1 OK RKK=110 REGN=46008 OK1VAL(6)=6
12:12:08.253 - OK1 OK RKK=110 REGN=02008 OK1VAL(6)=6
12:12:13.588 - OK2 OK RKK=000 [2098<8295 - 1738<7996 - 3476<8301] 2415>1062 MED=708 OK2VAL(0)=0
13:48:43.633 - OK2 OK RKK=000 [869<3412 - 1229<3474 - 720<3623] 509>436 MED=291 OK2VAL(0)=0
14:25:49.619 - OK2 OK RKK=000 [1440<3474 - 720<3836 - 509<3389] 509>462 MED=308 OK2VAL(0)=0
*/


uint decodePOS(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {

return (uint32_t) (m[pkkoffs]+m[pkkoffs+1]+m[pkkoffs+2]+m[pkkoffs+3]+m[pkkoffs+4]+m[pkkoffs+5]+m[pkkoffs+6]+m[pkkoffs+7])/8 > pkkpulselevel ? 1 : 0; 

} //end decodePOS

// uint decodePOSPrint(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {

// // printf("%d%d%d%d%d%d%d%d",m[pkkoffs],m[pkkoffs+1],m[pkkoffs+2],m[pkkoffs+3],m[pkkoffs+4],m[pkkoffs+5],m[pkkoffs+6],m[pkkoffs+7]);
// printf("%d", (uint32_t) (m[pkkoffs]+m[pkkoffs+1]+m[pkkoffs+2]+m[pkkoffs+3]+m[pkkoffs+4]+m[pkkoffs+5]+m[pkkoffs+6]+m[pkkoffs+7])/8 > pkkpulselevel ? 1 : 0);
// return (uint32_t) (m[pkkoffs]+m[pkkoffs+1]+m[pkkoffs+2]+m[pkkoffs+3]+m[pkkoffs+4]+m[pkkoffs+5]+m[pkkoffs+6]+m[pkkoffs+7])/8 > pkkpulselevel ? 1 : 0; 

// } //end decodePOS

// 6 - OK1, 0 - OK2, 5 - OK3
uint decodeKEY(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {
// uint32_t pkkmediana, pkkpulselevel, i, pkkend;    
uint result, p1, p2, p3, p4, p5, p6;

//определение среднего значения в ряде периодов для выделения посылки над помехами
// pkkmediana = 0;
// pkkend = pkkoffs + UVD_KEY_KODE_LEN;
// for (i = pkkoffs; i < pkkend; i++) { 
//     pkkmediana+=m[i]; //SUMM(ALL)
// }    
// pkkmediana = pkkmediana / UVD_KEY_KODE_LEN;     //48 периодов 0,5мкс в коде
// pkkpulselevel = pkkmediana / 2 + pkkmediana;

p1 = decodePOS(m, pkkoffs,    pkkpulselevel);
p2 = decodePOS(m, pkkoffs+8,  pkkpulselevel);
p3 = decodePOS(m, pkkoffs+16, pkkpulselevel);
p4 = decodePOS(m, pkkoffs+24, pkkpulselevel);
p5 = decodePOS(m, pkkoffs+32, pkkpulselevel);
p6 = decodePOS(m, pkkoffs+40, pkkpulselevel);

result = 0;
result = ((p1 < p2) ? 0 : 4) | ((p3 < p4) ? 0 : 2) | ((p5 < p6) ? 0 : 1);

return result; 

} //end decodeKEY


uint decodeDECADE(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {
// uint32_t pkkmediana, pkkpulselevel, i, pkkend;
uint result, b1, b2, b3, b4, b5, b6, b7, b8;

//определение среднего значения в ряде периодов для выделения посылки над помехами
// pkkmediana = 0;
// pkkend = pkkoffs + UVD_DECADE_LEN;
// for (i = pkkoffs; i < pkkend; i++) { 
//     pkkmediana+=m[i]; //SUMM(ALL)
// }    
// pkkmediana = pkkmediana / UVD_DECADE_LEN;     
// pkkpulselevel = pkkmediana / 2 + pkkmediana;

// printf("START DECADE ");
b1 = decodePOS(m, pkkoffs,    pkkpulselevel);
b2 = decodePOS(m, pkkoffs+8,  pkkpulselevel);
// printf(" ");
b3 = decodePOS(m, pkkoffs+16, pkkpulselevel);
b4 = decodePOS(m, pkkoffs+24, pkkpulselevel);
// printf(" ");
b5 = decodePOS(m, pkkoffs+32, pkkpulselevel);
b6 = decodePOS(m, pkkoffs+40, pkkpulselevel);
// printf(" ");
b7 = decodePOS(m, pkkoffs+48, pkkpulselevel);
b8 = decodePOS(m, pkkoffs+56, pkkpulselevel);
// printf("  END DECADE\n");

result = 0;
result = ((b1 < b2) ? 0 : 1) | ((b3 < b4) ? 0 : 2) | ((b5 < b6) ? 0 : 4) | ((b7 < b8) ? 0 : 8);

return result; 
} //end decodeDECADE


uint testFUNC(uint16_t *m, uint32_t j) {
    return m[j];
}


/* Detect a UVD responses inside the magnitude buffer pointed by 'm' and of
 * size 'mlen' bytes. 
 * Every detected UVD message is convert it into a
 * stream of bits and passed to the function to display it. */
void detectUVD(uint16_t *m, uint32_t mlen) {

/*    
    Всего стандартом предусмотрено шесть координатных кодов: ОК1-ОК6.
    Они образуются двумя периодами: РК1 и РК3.
    Кодовый интервал между ними определяет содержание ответа и может быть: 14, 11, 18, 22, 9 и 6мкс.
    На привычные нам запросы ЗК1-ЗК4 высылаются ответы с первыми четырьмя значениями интервала.
    Между РК1 и РК3 может посылаться импульс РК2, который кодирует два дополнительных сообщения.
    Когда РК2 опережает РК3 на 6мкс - передается сигнал "Бедствие".
    Если РК2 отстает от РК1 на 6мкс - передается сигнал "Шасси выпущено".
    Длительность периода неважна - определяется только период времени между началами импульсов.
    
    ЕСЛИ СЭМПЛИРОВАНИЕ СИГНАЛА 0,5мкс
    ТО ВРЕМЕННаЯ РАСКЛАДКА БУДЕТ ТАКОЙ ([0,1,..] – номер значения в массиве данных)
    
    [0]   начало 1-го периода т=0 мкс        ------------------ PK1 начало
    [1]   начало 2-го периода т=0,5 мкс
    [2]   начало 3-го периода т=1 мкс
    [3]   начало 4-го периода т=1,5 мкс     
    [4]   начало 5-го периода т=2 мкс  
    [5]   начало 6-го периода т=2,5 мкс  
    [6]   начало 7-го периода т=3 мкс          
    [7]   начало 8-го периода т=3,5 мкс
    [8]   начало 9-го периода т=4 мкс     
    [9]   начало 10-го периода т=4,5 мкс  
    [10]  начало 11-го периода т=5 мкс         ------------------------ РК2 для ОК2 - ОК6 (бедствие)        
    [11]  начало 12-го периода т=5,5 мкс       
    [12]  начало 13-го периода т=6 мкс         ------------ РК2 начало после РК1 - ОК5 (шасси выпущено)
    [13]  начало 14-го периода т=6,5 мкс       
    [14]  начало 15-го периода т=7 мкс         
    [15]  начало 16-го периода т=7,5 мкс       
    [16]  начало 17-го периода т=8 мкс         ------------------------ РК2 для ОК1 - ОК6 (бедствие) 
    [17]  начало 18-го периода т=8,5 мкс       
    [18]  начало 19-го периода т=9 мкс          
    [19]  начало 20-го периода т=9,5 мкс       
    [20]  начало 21-го периода т=10 мкс                   
    [21]  начало 22-го периода т=10,5 мкс      
    [22]  начало 23-го периода т=11 мкс        ------------------ **** PK3 начало - ОК2 (для ЗК2) 
    [23]  начало 24-го периода т=11,5 мкс      
    [24]  начало 25-го периода т=12 мкс        ------------------------ РК2 для ОК3 - ОК6 (бедствие)     
    [25]  начало 26-го периода т=12,5 мкс      
    [26]  начало 27-го периода т=13 мкс        
    [27]  начало 28-го периода т=13,5 мкс      
    [28]  начало 29-го периода т=14 мкс        ------------------ **** PK3 начало - ОК1 (для ЗК1) 
    [29]  начало 30-го периода т=14,5 мкс  
    [30]  начало 31-го периода т=15 мкс 
    [31]  начало 32-го периода т=15,5 мкс
    [32]  начало 33-го периода т=16 мкс        ------------------------ РК2 для ОК4 - ОК6 (бедствие) 
    [33]  начало 34-го периода т=16,5 мкс     
    [34]  начало 35-го периода т=17 мкс  
    [35]  начало 36-го периода т=17,5 мкс  
    [36]  начало 37-го периода т=18 мкс        ------------------ **** PK3 начало - ОК3 (для ЗК3) 
    [37]  начало 38-го периода т=18,5 мкс
    [38]  начало 39-го периода т=19 мкс     
    [39]  начало 40-го периода т=19,5 мкс  
    [40]  начало 41-го периода т=20 мкс 
    [41]  начало 42-го периода т=20,5 мкс
    [42]  начало 43-го периода т=21 мкс
    [43]  начало 44-го периода т=21,5 мкс     
    [44]  начало 45-го периода т=22 мкс        ------------------ **** PK3 начало - ОК4 (-)

   

    Наиболее эффективный фильтр получается по принципу "n за t" - n-штук сообщений за контрольный интервал времени t.
    Если выполнилось двойное условие - считаем (hex или К1) достоверным.
    Нет - сбрасываем счетчик и начинаем цикл заново.
    В УВД, из-за отсутствия CRC и других средств - n требуется больше, чем в ModeS.
    Зато и по стандарту их до 150 шт. в секунду может излучаться против 20-30 шт. (каждого подтипа) в adsb.    
*/
//UVD_KOORD_KODE_LEN

uint32_t mediana, pulselevel, pkkmediana, pkkpulselevel;
// uint32_t b1, b2, b3, b4, b5, b6, b7, b8;
uint32_t p1, p2, p3, p4, p5, p6;
uint dec1, dec2, dec3, dec4, dec5;
uint32_t i, j, pkkoffs, pkkend;
uint okval;
// char regnumber[6];
// time_t t;
// uint32_t hashval;
// time_t now = time(NULL);
    uint16_t marrwrite[UVD_MAX_LEN];

    printf("Try to read %d elements with size %d bytes, all length %d bytes\n", sizeof(marrwrite), sizeof(uint16_t), sizeof(marrwrite)*sizeof(uint16_t));
    // printf("Read ok1-12-11-09-991.data = RA-26001\n\n");
    // FILE *ifp = fopen("ok1-12-11-09-991.data", "rb");
    printf("Read ok2.data for RA-26001\n\n");
    FILE *ifp = fopen("ok2-12-11-11-284.data", "rb");    
    //fwrite(marrwrite, sizeof(uint16_t), sizeof(marrwrite), f);
    // fread(clientdata, sizeof(char), sizeof(clientdata), ifp);
    fread(marrwrite, sizeof(uint16_t), sizeof(marrwrite), ifp);
    fclose(ifp);

    for(i=0;i<UVD_MAX_LEN;i++) {
    m[i]=marrwrite[i];
    }

//сканирование буффера длиной mlen
for (j = 0; j < mlen-UVD_MAX_LEN; j++) {



    // printf("VAL=%d FUNC=%d\n", m[j], testFUNC(m,j));

    //определение среднего значения в ряде периодов для выделения посылки над помехами
    mediana = 0;
    for (i = 0; i < UVD_KOORD_KODE_LEN; i++) { 
        mediana+=m[j+i]; //SUMM(ALL)
    }
    
    mediana = mediana / UVD_KOORD_KODE_LEN;

    pulselevel = mediana / 2 + mediana;

    // printf("MED=%d ", mediana);
    // printf("PULSE=%d ", pulselevel);

    //если начало не идет с импульса (PK1) то сдвигаем окно на следующий период
    if(m[j]<pulselevel) continue;

    /*
    Для передачи сообщения используется натуральный двоично-десятичный четырехразрядный код с активной паузой,
    т. е. импульс передается как на символ 1, так и на 0, но сдвинутым на 4 мкс. 
    Таким образом, каждому разряду отводится 8 мкс. 
    */

    // time(&t);
    

    // struct timeb tmb;
    // ftime(&tmb);
    // printf("%ld.%d\n", tmb.time, tmb.millitm);



    char timestr[20], filestr[20];
    struct timeval tp;
    gettimeofday(&tp, 0);
    time_t curtime = tp.tv_sec;
    struct tm *t = localtime(&curtime);
    sprintf(timestr, "%02d:%02d:%02d.%03d", (int) t->tm_hour, (int) t->tm_min, (int) t->tm_sec, (int) tp.tv_usec/1000);
    // regnumber[0] = (char) (int) '0'; //+1);
    // printf("regnumber=%c",regnumber[0]);

    //******************* OK2 **********************

    if((uint32_t) ((m[j+21]+m[j+22]+m[j+23])/3)>pulselevel) {
    //OK2
    //t=14mks 000
    /*
    [23]  т=0,5 мкс       
    [24]  т=1 мкс       
    [25]  т=1,5 мкс               
    [26]  т=2 мкс       
    [27]  т=2,5 мкс       
    [28]  т=3 мкс   
    [29]  т=3,5 мкс       
    [30]  т=4 мкс       
    [31]  т=4,5 мкс
    [32]  т=5 мкс       
    [33]  т=5.5 мкс       
    [34]  т=6 мкс               
    [35]  т=6.5 мкс       
    [36]  т=7 мкс       
    [37]  т=7.5 мкс   
    [38]  т=8 мкс       
    [39]  т=8.5 мкс       
    [40]  т=9 мкс          
    [41]  т=9.5 мкс               
    [42]  т=10 мкс       
    [43]  т=10.5 мкс       
    [44]  т=11 мкс   
    [45]  т=11.5 мкс       
    [46]  т=12 мкс       
    [47]  т=12.5 мкс     
    [48]  т=13 мкс               
    [49]  т=13.5 мкс   

    ---РКИ1 =0
    -----POS1 =0 SUMM(50-57)<SUMM(58-65)
    [50]  т=14 мкс     t=0 mks
    [51]  т=14.5 мкс   t=0.5 mks       
    [52]  т=15 мкс     t=1 mks  
    [53]  т=15,5 мкс   t=1.5 mks  
    [54]  т=16 мкс     t=2 mks
    [55]  т=16,5 мкс   t=2.5 mks
    [56]  т=17 мкс     t=3 mks
    [57]  т=17,5 мкс   t=3.5 mks
    -----POS2 =1   
    [58]  т=18 мкс     t=4mks   t=0 mks
    [59]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [60]  т=19 мкс     t=5mks   t=1 mks  
    [61]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [62]  т=20 мкс     t=6mks   t=2 mks
    [63]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [64]  т=21 мкс     t=7mks   t=3 mks
    [65]  т=21,5 мкс   t=7.5mks t=3.5 mks

    ---РКИ2 =0   
    -----POS1 =0 SUMM(66-73)<SUMM(74-81)
    [66]  т=14 мкс     t=0 mks
    [67]  т=14.5 мкс   t=0.5 mks       
    [68]  т=15 мкс     t=1 mks  
    [69]  т=15,5 мкс   t=1.5 mks  
    [70]  т=16 мкс     t=2 mks
    [71]  т=16,5 мкс   t=2.5 mks
    [72]  т=17 мкс     t=3 mks
    [73]  т=17,5 мкс   t=3.5 mks
    -----POS2 =1   
    [74]  т=18 мкс     t=4mks   t=0 mks
    [75]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [76]  т=19 мкс     t=5mks   t=1 mks  
    [77]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [78]  т=20 мкс     t=6mks   t=2 mks
    [79]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [80]  т=21 мкс     t=7mks   t=3 mks
    [81]  т=21,5 мкс   t=7.5mks t=3.5 mks

    ---РКИ3 =0   
    -----POS1 =0 SUMM(82-89)<SUMM(90-97)
    [82]  т=14 мкс     t=0 mks
    [83]  т=14.5 мкс   t=0.5 mks       
    [84]  т=15 мкс     t=1 mks  
    [85]  т=15,5 мкс   t=1.5 mks  
    [86]  т=16 мкс     t=2 mks
    [87]  т=16,5 мкс   t=2.5 mks
    [88]  т=17 мкс     t=3 mks
    [89]  т=17,5 мкс   t=3.5 mks
    -----POS2 =1   
    [90]  т=18 мкс     t=4mks   t=0 mks
    [91]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [92]  т=19 мкс     t=5mks   t=1 mks  
    [93]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [94]  т=20 мкс     t=6mks   t=2 mks
    [95]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [96]  т=21 мкс     t=7mks   t=3 mks
    [97]  т=21,5 мкс   t=7.5mks t=3.5 mks    

    ---РКИ1 =0
    -----POS1 =0 SUMM(50-57)<SUMM(58-65)  
    ---РКИ2 =0   
    -----POS1 =0 SUMM(66-73)<SUMM(74-81)
    ---РКИ3 =0   
    -----POS1 =0 SUMM(82-89)<SUMM(90-97)                
    */      



        //определение среднего значения в ряде периодов для выделения посылки над помехами
        // pkkmediana = 0;
        pkkoffs = UVD_OK2_OFFS; //50
        // pkkend = pkkoffs + UVD_KEY_KODE_LEN; //+48=97
        // for (i = pkkoffs; i < pkkend; i++) { 
        //     pkkmediana+=m[j+i]; //SUMM(ALL)
        // }    
        // pkkmediana = pkkmediana / UVD_KEY_KODE_LEN;     //48 периодов 0,5мкс в коде
        // pkkpulselevel = pkkmediana / 2 + pkkmediana;

        pkkpulselevel = pulselevel;
        //декодирование ключевого кода
        okval = decodeKEY(m, j+pkkoffs, pkkpulselevel); // 6 - OK1, 0 - OK2, 5 - OK3

        if(okval == 0)
        {

        pkkoffs = pkkoffs + UVD_KEY_KODE_LEN;
        pkkpulselevel = pulselevel;

        dec1 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        dec1+=(int) '0';

        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec2 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        dec2+=(int) '0';

        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec3 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        dec3+=(int) '0';

        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec4 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        dec4+=(int) '0';

        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec5 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        dec5+=(int) '0';

        // следущее - это повторение
        // pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        //Print result
            // printf("%s - OK2 OK RKK=000 [%d<%d - %d<%d - %d<%d] %d>%d MED=%d OK2VAL(0)=%d\n",
            //     timestr,
            //     m[j+pkkoffs]+m[j+pkkoffs+1]+m[j+pkkoffs+2]+m[j+pkkoffs+3]+m[j+pkkoffs+4]+m[j+pkkoffs+5]+m[j+pkkoffs+6]+m[j+pkkoffs+7],
            //     m[j+pkkoffs+8]+m[j+pkkoffs+9]+m[j+pkkoffs+10]+m[j+pkkoffs+11]+m[j+pkkoffs+12]+m[j+pkkoffs+13]+m[j+pkkoffs+14]+m[j+pkkoffs+15],
            //     m[j+pkkoffs+16]+m[j+pkkoffs+17]+m[j+pkkoffs+18]+m[j+pkkoffs+19]+m[j+pkkoffs+20]+m[j+pkkoffs+21]+m[j+pkkoffs+22]+m[j+pkkoffs+23],
            //     m[j+pkkoffs+24]+m[j+pkkoffs+25]+m[j+pkkoffs+26]+m[j+pkkoffs+27]+m[j+pkkoffs+28]+m[j+pkkoffs+29]+m[j+pkkoffs+30]+m[j+pkkoffs+31],
            //     m[j+pkkoffs+32]+m[j+pkkoffs+33]+m[j+pkkoffs+34]+m[j+pkkoffs+35]+m[j+pkkoffs+36]+m[j+pkkoffs+37]+m[j+pkkoffs+38]+m[j+pkkoffs+39],
            //     m[j+pkkoffs+40]+m[j+pkkoffs+41]+m[j+pkkoffs+42]+m[j+pkkoffs+43]+m[j+pkkoffs+44]+m[j+pkkoffs+45]+m[j+pkkoffs+46]+m[j+pkkoffs+47],
            //     m[j],
            //     pulselevel,
            //     mediana,
            //     // 6 - OK1, 0 - OK2, 5 - OK3
            //     okval
            //     );

        printf("%s - OK2 OK RKK=000 0 ALT=%c%c%c%cm  FUEL=%c\n", timestr, (char) (0b1100 & dec4)>>2, (char) dec3, (char) dec2, (char) dec1, (char) dec5);            

            // for(i=0;i<UVD_MAX_LEN;i++) {
            // marrwrite[i] = m[j+i];
            // }

            // sprintf(filestr, "ok2-%02d-%02d-%02d-%03d.data", (int) t->tm_hour, (int) t->tm_min, (int) t->tm_sec, (int) tp.tv_usec/1000);
            // FILE *f = fopen(filestr, "wb");
            // fwrite(marrwrite, sizeof(uint16_t), sizeof(marrwrite), f);
            // fclose(f);


            j+=UVD_KOORD_KODE_LEN+UVD_OK2_DELAY+UVD_KEY_KODE_LEN+UVD_INFO_KODE_LEN;
            continue;
        } 
            // else printf("%s - OK2 BAD RKK\n", timestr);
    } //end OK2
      

    // continue;




    //******************* OK1 **********************

    if((uint32_t) ((m[j+27]+m[j+28]+m[j+29])/3)>pulselevel) {        
    //OK1
    //t=8.5mks 110
    /*
    [29]  т=0,5 мкс       
    [30]  т=1 мкс       
    [31]  т=1,5 мкс               
    [32]  т=2 мкс       
    [33]  т=2,5 мкс       
    [34]  т=3 мкс   
    [35]  т=3,5 мкс       
    [36]  т=4 мкс       
    [37]  т=4,5 мкс
    [38]  т=5 мкс       
    [39]  т=5.5 мкс       
    [40]  т=6 мкс               
    [41]  т=6.5 мкс       
    [42]  т=7 мкс       
    [43]  т=7.5 мкс   
    [44]  т=8 мкс       

    ---РКИ1 =1
    -----POS1 =1 SUMM(45-52)>SUMM(53-60)
    [45]  т=14 мкс     t=0 mks
    [46]  т=14.5 мкс   t=0.5 mks       
    [47]  т=15 мкс     t=1 mks  
    [48]  т=15,5 мкс   t=1.5 mks  
    [49]  т=16 мкс     t=2 mks
    [50]  т=16,5 мкс   t=2.5 mks
    [51]  т=17 мкс     t=3 mks
    [52]  т=17,5 мкс   t=3.5 mks
    -----POS2 =0   
    [53]  т=18 мкс     t=4mks   t=0 mks
    [54]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [55]  т=19 мкс     t=5mks   t=1 mks  
    [56]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [57]  т=20 мкс     t=6mks   t=2 mks
    [58]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [59]  т=21 мкс     t=7mks   t=3 mks
    [60]  т=21,5 мкс   t=7.5mks t=3.5 mks

    ---РКИ2 =1   
    -----POS1 =1 SUMM(61-68)>SUMM(69-76)
    [61]  т=14 мкс     t=0 mks
    [62]  т=14.5 мкс   t=0.5 mks       
    [63]  т=15 мкс     t=1 mks  
    [64]  т=15,5 мкс   t=1.5 mks  
    [65]  т=16 мкс     t=2 mks
    [66]  т=16,5 мкс   t=2.5 mks
    [67]  т=17 мкс     t=3 mks
    [68]  т=17,5 мкс   t=3.5 mks
    -----POS2 =0   
    [69]  т=18 мкс     t=4mks   t=0 mks
    [70]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [71]  т=19 мкс     t=5mks   t=1 mks  
    [72]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [73]  т=20 мкс     t=6mks   t=2 mks
    [74]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [75]  т=21 мкс     t=7mks   t=3 mks
    [76]  т=21,5 мкс   t=7.5mks t=3.5 mks

    ---РКИ3 =0   
    -----POS1 =0 SUMM(77-84)<SUMM(85-92)
    [77]  т=14 мкс     t=0 mks
    [78]  т=14.5 мкс   t=0.5 mks       
    [79]  т=15 мкс     t=1 mks  
    [80]  т=15,5 мкс   t=1.5 mks  
    [81]  т=16 мкс     t=2 mks
    [82]  т=16,5 мкс   t=2.5 mks
    [83]  т=17 мкс     t=3 mks
    [84]  т=17,5 мкс   t=3.5 mks
    -----POS2 =1   
    [85]  т=18 мкс     t=4mks   t=0 mks
    [86]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [87]  т=19 мкс     t=5mks   t=1 mks  
    [88]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [89]  т=20 мкс     t=6mks   t=2 mks
    [90]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [91]  т=21 мкс     t=7mks   t=3 mks
    [92]  т=21,5 мкс   t=7.5mks t=3.5 mks    

    ---РКИ1 =1
    -----POS1 =1 SUMM(45-52)>SUMM(53-60)
    ---РКИ2 =1   
    -----POS1 =1 SUMM(61-68)>SUMM(69-76)
    ---РКИ3 =0   
    -----POS1 =0 SUMM(77-84)<SUMM(85-92)

    */     
        //определение среднего значения в ряде периодов для выделения посылки над помехами
        // pkkmediana = 0;
        pkkoffs = UVD_OK1_OFFS; //45
        // pkkend = pkkoffs + UVD_KEY_KODE_LEN; //+48=93
        // for (i = pkkoffs; i < pkkend; i++) { 
        //     pkkmediana+=m[j+i]; //SUMM(ALL)
        // }    
        // pkkmediana = pkkmediana / UVD_KEY_KODE_LEN;     //48 периодов 0,5мкс в коде
        // pkkpulselevel = pkkmediana / 2 + pkkmediana;

        pkkpulselevel = pulselevel;
        //декодирование ключевого кода
        okval = decodeKEY(m, j+pkkoffs, pkkpulselevel); // 6 - OK1, 0 - OK2, 5 - OK3

        if(okval==6)
        {
        //DECODE INFO CODE - BORT NUMBER  RF- D5 D4 D3 D2 D1

        //START DECADE 1 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_KEY_KODE_LEN;
        //определение среднего значения в ряде периодов для выделения посылки над помехами
        // pkkmediana = 0;
        // pkkend = pkkoffs + UVD_DECADE_LEN;
        // for (i = pkkoffs; i < pkkend; i++) { 
        //     pkkmediana+=m[j+i]; //SUMM(ALL)
        // }    
        // pkkmediana = pkkmediana / UVD_DECADE_LEN;     
        // pkkpulselevel = pkkmediana / 2 + pkkmediana;

        dec1 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        dec1+=(int) '0';
        //END DECADE 1

        //START DECADE 2 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks
        //определение среднего значения в ряде периодов для выделения посылки над помехами
        // pkkmediana = 0;
        // pkkend = pkkoffs + UVD_DECADE_LEN;
        // for (i = pkkoffs; i < pkkend; i++) { 
        //     pkkmediana+=m[j+i]; //SUMM(ALL)
        // }    
        // pkkmediana = pkkmediana / UVD_DECADE_LEN;     
        // pkkpulselevel = pkkmediana / 2 + pkkmediana;
        dec2 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        dec2+=(int) '0';
        //END DECADE 2

        //START DECADE 3 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks
        //определение среднего значения в ряде периодов для выделения посылки над помехами
        // pkkmediana = 0;
        // pkkend = pkkoffs + UVD_DECADE_LEN;
        // for (i = pkkoffs; i < pkkend; i++) { 
        //     pkkmediana+=m[j+i]; //SUMM(ALL)
        // }    
        // pkkmediana = pkkmediana / UVD_DECADE_LEN;    
        // pkkpulselevel = pkkmediana / 2 + pkkmediana;

        dec3 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        dec3+=(int) '0';
        //END DECADE 3

        //START DECADE 4 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks
        // //определение среднего значения в ряде периодов для выделения посылки над помехами
        // pkkmediana = 0;
        // pkkend = pkkoffs + UVD_DECADE_LEN;
        // for (i = pkkoffs; i < pkkend; i++) { 
        //     pkkmediana+=m[j+i]; //SUMM(ALL)
        // }    
        // pkkmediana = pkkmediana / UVD_DECADE_LEN;     
        // pkkpulselevel = pkkmediana / 2 + pkkmediana;

        dec4 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        dec4+=(int) '0';
        //END DECADE 4

        //START DECADE 5 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks
        //определение среднего значения в ряде периодов для выделения посылки над помехами
        // pkkmediana = 0;
        // pkkend = pkkoffs + UVD_DECADE_LEN;
        // for (i = pkkoffs; i < pkkend; i++) { 
        //     pkkmediana+=m[j+i]; //SUMM(ALL)
        // }    
        // pkkmediana = pkkmediana / UVD_DECADE_LEN;     
        // pkkpulselevel = pkkmediana / 2 + pkkmediana;

        dec5 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        dec5+=(int) '0';
        //END DECADE 5

        //DECODE INFO CODE - BORT NUMBER  RF- D5 D4 D3 D2 D1

            // printf("%s - OK1 OK RKK=110 [%d>%d - %d>%d - %d<%d] %d>%d MED=%d\n",
            //     timestr,
            //     m[j+pkkoffs]+m[j+pkkoffs+1]+m[j+pkkoffs+2]+m[j+pkkoffs+3]+m[j+pkkoffs+4]+m[j+pkkoffs+5]+m[j+pkkoffs+6]+m[j+pkkoffs+7],
            //     m[j+pkkoffs+8]+m[j+pkkoffs+9]+m[j+pkkoffs+10]+m[j+pkkoffs+11]+m[j+pkkoffs+12]+m[j+pkkoffs+13]+m[j+pkkoffs+14]+m[j+pkkoffs+15],
            //     m[j+pkkoffs+16]+m[j+pkkoffs+17]+m[j+pkkoffs+18]+m[j+pkkoffs+19]+m[j+pkkoffs+20]+m[j+pkkoffs+21]+m[j+pkkoffs+22]+m[j+pkkoffs+23],
            //     m[j+pkkoffs+24]+m[j+pkkoffs+25]+m[j+pkkoffs+26]+m[j+pkkoffs+27]+m[j+pkkoffs+28]+m[j+pkkoffs+29]+m[j+pkkoffs+30]+m[j+pkkoffs+31],
            //     m[j+pkkoffs+32]+m[j+pkkoffs+33]+m[j+pkkoffs+34]+m[j+pkkoffs+35]+m[j+pkkoffs+36]+m[j+pkkoffs+37]+m[j+pkkoffs+38]+m[j+pkkoffs+39],
            //     m[j+pkkoffs+40]+m[j+pkkoffs+41]+m[j+pkkoffs+42]+m[j+pkkoffs+43]+m[j+pkkoffs+44]+m[j+pkkoffs+45]+m[j+pkkoffs+46]+m[j+pkkoffs+47],
            //     m[j],
            //     pulselevel,
            //     mediana
            //     );
            printf("%s - OK1 OK RKK=110 6 REGN=%c%c%c%c%c\n", timestr, (char) dec5, (char) dec4, (char) dec3, (char) dec2, (char) dec1);


            // for(i=0;i<UVD_MAX_LEN;i++) {
            // marrwrite[i] = m[j+i];
            // }

            // sprintf(filestr, "ok1-%02d-%02d-%02d-%03d.data", (int) t->tm_hour, (int) t->tm_min, (int) t->tm_sec, (int) tp.tv_usec/1000);
            // FILE *f = fopen(filestr, "wb");
            // fwrite(marrwrite, sizeof(uint16_t), sizeof(marrwrite), f);
            // fclose(f);


            j+=UVD_KOORD_KODE_LEN+UVD_OK1_DELAY+UVD_KEY_KODE_LEN+UVD_INFO_KODE_LEN;
            continue;
        } 
            // else printf("OK1 BAD RKK\n");
    } //end OK1


    //******************* OK3 **********************

    if((uint32_t) ((m[j+35]+m[j+36]+m[j+37])/3)>pulselevel) {         
    //OK3
    //t=10mks 101
    /*
    [37]  т=0,5 мкс       
    [38]  т=1 мкс       
    [39]  т=1,5 мкс               
    [40]  т=2 мкс       
    [41]  т=2,5 мкс       
    [42]  т=3 мкс   
    [43]  т=3,5 мкс       
    [44]  т=4 мкс       
    [45]  т=4,5 мкс
    [46]  т=5 мкс       
    [47]  т=5.5 мкс       
    [48]  т=6 мкс               
    [49]  т=6.5 мкс       
    [50]  т=7 мкс       
    [51]  т=7.5 мкс   
    [52]  т=8 мкс       
    [53]  т=8.5 мкс       
    [54]  т=9 мкс          
    [55]  т=9.5 мкс               


    ---РКИ1 =1
    -----POS1 =1 SUMM(56-63)>SUMM(64-71)
    [56]  т=14 мкс     t=0 mks
    [57]  т=14.5 мкс   t=0.5 mks       
    [58]  т=15 мкс     t=1 mks  
    [59]  т=15,5 мкс   t=1.5 mks  
    [60]  т=16 мкс     t=2 mks
    [61]  т=16,5 мкс   t=2.5 mks
    [62]  т=17 мкс     t=3 mks
    [63]  т=17,5 мкс   t=3.5 mks
    -----POS2 =0   
    [64]  т=18 мкс     t=4mks   t=0 mks
    [65]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [66]  т=19 мкс     t=5mks   t=1 mks  
    [67]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [68]  т=20 мкс     t=6mks   t=2 mks
    [69]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [70]  т=21 мкс     t=7mks   t=3 mks
    [71]  т=21,5 мкс   t=7.5mks t=3.5 mks

    ---РКИ2 =0   
    -----POS1 =0 SUMM(72-79)<SUMM(80-87)
    [72]  т=14 мкс     t=0 mks
    [73]  т=14.5 мкс   t=0.5 mks       
    [74]  т=15 мкс     t=1 mks  
    [75]  т=15,5 мкс   t=1.5 mks  
    [76]  т=16 мкс     t=2 mks
    [77]  т=16,5 мкс   t=2.5 mks
    [78]  т=17 мкс     t=3 mks
    [79]  т=17,5 мкс   t=3.5 mks
    -----POS2 =1   
    [80]  т=18 мкс     t=4mks   t=0 mks
    [81]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [82]  т=19 мкс     t=5mks   t=1 mks  
    [83]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [84]  т=20 мкс     t=6mks   t=2 mks
    [85]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [86]  т=21 мкс     t=7mks   t=3 mks
    [87]  т=21,5 мкс   t=7.5mks t=3.5 mks

    ---РКИ3 =1   
    -----POS1 =1 SUMM(88-95)>SUMM(96-103)   
    [88]  т=14 мкс     t=0 mks
    [89]  т=14.5 мкс   t=0.5 mks       
    [90]  т=15 мкс     t=1 mks  
    [91]  т=15,5 мкс   t=1.5 mks  
    [92]  т=16 мкс     t=2 mks
    [93]  т=16,5 мкс   t=2.5 mks
    [94]  т=17 мкс     t=3 mks
    [95]  т=17,5 мкс   t=3.5 mks
    -----POS2 =0
    [96]  т=18 мкс     t=4mks   t=0 mks
    [97]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [98]  т=19 мкс     t=5mks   t=1 mks  
    [99]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    100]  т=20 мкс     t=6mks   t=2 mks
    101]  т=20,5 мкс   t=6.5mks t=2.5 mks
    102]  т=21 мкс     t=7mks   t=3 mks
    103]  т=21,5 мкс   t=7.5mks t=3.5 mks    

    ---РКИ1 =1
    -----POS1 =1 SUMM(56-63)>SUMM(64-71)
    ---РКИ2 =0   
    -----POS1 =0 SUMM(72-79)<SUMM(80-87)
    ---РКИ3 =1   
    -----POS1 =1 SUMM(88-95)>SUMM(96-103)                      
    */
                    //определение среднего значения в ряде периодов для выделения посылки над помехами
        pkkmediana = 0;
        pkkoffs = UVD_OK3_OFFS; //56
        pkkend = pkkoffs + UVD_KEY_KODE_LEN; //+48=104
        for (i = pkkoffs; i < pkkend; i++) { 
            pkkmediana+=m[j+i]; //SUMM(ALL)
        }    
        pkkmediana = pkkmediana / UVD_KEY_KODE_LEN;     //48 периодов 0,5мкс в коде
        pkkpulselevel = pkkmediana / 2 + pkkmediana;

        p1 = (uint32_t) (m[j+pkkoffs]+m[j+pkkoffs+1]+m[j+pkkoffs+2]+m[j+pkkoffs+3]+m[j+pkkoffs+4]+m[j+pkkoffs+5]+m[j+pkkoffs+6]+m[j+pkkoffs+7])/8           > pkkpulselevel ? 1 : 0;
        p2 = (uint32_t) (m[j+pkkoffs+8]+m[j+pkkoffs+9]+m[j+pkkoffs+10]+m[j+pkkoffs+11]+m[j+pkkoffs+12]+m[j+pkkoffs+13]+m[j+pkkoffs+14]+m[j+pkkoffs+15])/8   > pkkpulselevel ? 1 : 0;
        p3 = (uint32_t) (m[j+pkkoffs+16]+m[j+pkkoffs+17]+m[j+pkkoffs+18]+m[j+pkkoffs+19]+m[j+pkkoffs+20]+m[j+pkkoffs+21]+m[j+pkkoffs+22]+m[j+pkkoffs+23])/8 > pkkpulselevel ? 1 : 0;
        p4 = (uint32_t) (m[j+pkkoffs+24]+m[j+pkkoffs+25]+m[j+pkkoffs+26]+m[j+pkkoffs+27]+m[j+pkkoffs+28]+m[j+pkkoffs+29]+m[j+pkkoffs+30]+m[j+pkkoffs+31])/8 > pkkpulselevel ? 1 : 0;
        p5 = (uint32_t) (m[j+pkkoffs+32]+m[j+pkkoffs+33]+m[j+pkkoffs+34]+m[j+pkkoffs+35]+m[j+pkkoffs+36]+m[j+pkkoffs+37]+m[j+pkkoffs+38]+m[j+pkkoffs+39])/8 > pkkpulselevel ? 1 : 0;
        p6 = (uint32_t) (m[j+pkkoffs+40]+m[j+pkkoffs+41]+m[j+pkkoffs+42]+m[j+pkkoffs+43]+m[j+pkkoffs+44]+m[j+pkkoffs+45]+m[j+pkkoffs+46]+m[j+pkkoffs+47])/8 > pkkpulselevel ? 1 : 0;

        if( //101
            p1>p2 &&    //10
            p3<p4 &&    //01
            p5>p6       //10
          ) 
        {
        
        okval = decodeKEY(m, j+pkkoffs, pkkpulselevel); // 6 - OK1, 0 - OK2, 5 - OK3


            for(i=0;i<UVD_MAX_LEN;i++) {
            marrwrite[i] = m[j+i];
            }

            sprintf(filestr, "ok3-%02d-%02d-%02d-%03d.data", (int) t->tm_hour, (int) t->tm_min, (int) t->tm_sec, (int) tp.tv_usec/1000);
            FILE *f = fopen(filestr, "wb");
            fwrite(marrwrite, sizeof(uint16_t), sizeof(marrwrite), f);
            fclose(f);


            printf("%s - OK3 OK RKK=101 [%d>%d - %d<%d - %d>%d] %d>%d OK3VAL(5)=%d MED=%d\n",
                timestr,
                m[j+pkkoffs]+m[j+pkkoffs+1]+m[j+pkkoffs+2]+m[j+pkkoffs+3]+m[j+pkkoffs+4]+m[j+pkkoffs+5]+m[j+pkkoffs+6]+m[j+pkkoffs+7],
                m[j+pkkoffs+8]+m[j+pkkoffs+9]+m[j+pkkoffs+10]+m[j+pkkoffs+11]+m[j+pkkoffs+12]+m[j+pkkoffs+13]+m[j+pkkoffs+14]+m[j+pkkoffs+15],
                m[j+pkkoffs+16]+m[j+pkkoffs+17]+m[j+pkkoffs+18]+m[j+pkkoffs+19]+m[j+pkkoffs+20]+m[j+pkkoffs+21]+m[j+pkkoffs+22]+m[j+pkkoffs+23],
                m[j+pkkoffs+24]+m[j+pkkoffs+25]+m[j+pkkoffs+26]+m[j+pkkoffs+27]+m[j+pkkoffs+28]+m[j+pkkoffs+29]+m[j+pkkoffs+30]+m[j+pkkoffs+31],
                m[j+pkkoffs+32]+m[j+pkkoffs+33]+m[j+pkkoffs+34]+m[j+pkkoffs+35]+m[j+pkkoffs+36]+m[j+pkkoffs+37]+m[j+pkkoffs+38]+m[j+pkkoffs+39],
                m[j+pkkoffs+40]+m[j+pkkoffs+41]+m[j+pkkoffs+42]+m[j+pkkoffs+43]+m[j+pkkoffs+44]+m[j+pkkoffs+45]+m[j+pkkoffs+46]+m[j+pkkoffs+47],
                m[j],
                pulselevel,
                okval,
                mediana
                );

            j+=UVD_KOORD_KODE_LEN+UVD_OK3_DELAY+UVD_KEY_KODE_LEN+UVD_INFO_KODE_LEN;
            continue;
        }
            // else printf("OK3 BAD RKK\n");          

    } //end OK3 

} //end for j

} //end proc

/* When a new message is available, because it was decoded from the
 * RTL device, file, or received in the TCP input port, or any other
 * way we can receive a decoded message, we call this function in order
 * to use the message.
 *
 * Basically this function passes a raw message to the upper layers for
 * further processing and visualization. */
void useModesMessage(struct modesMessage *mm) {
    if (!Modes.stats && (Modes.check_crc == 0 || mm->crcok)) {
        /* Track aircrafts in interactive mode or if the HTTP
         * interface is enabled. */
        if (Modes.interactive || Modes.stat_http_requests > 0 || Modes.stat_sbs_connections > 0) {
            struct aircraft *a = interactiveReceiveData(mm);
            if (a && Modes.stat_sbs_connections > 0) modesSendSBSOutput(mm, a);  /* Feed SBS output clients. */
        }
        /* In non-interactive way, display messages on standard output. */
        if (!Modes.interactive) {
            displayModesMessage(mm);
            if (!Modes.raw && !Modes.onlyaddr) printf("\n");
        }
        /* Send data to connected clients. */
        if (Modes.net) {
            modesSendRawOutput(mm);  /* Feed raw output clients. */
        }
    }
}

/* ========================= Interactive mode =============================== */

/* Return a new aircraft structure for the interactive mode linked list
 * of aircrafts. */
struct aircraft *interactiveCreateAircraft(uint32_t addr) {
    struct aircraft *a = malloc(sizeof(*a));

    a->addr = addr;
    snprintf(a->hexaddr,sizeof(a->hexaddr),"%06x",(int)addr);
    a->flight[0] = '\0';
    a->altitude = 0;
    a->speed = 0;
    a->track = 0;
    a->odd_cprlat = 0;
    a->odd_cprlon = 0;
    a->odd_cprtime = 0;
    a->even_cprlat = 0;
    a->even_cprlon = 0;
    a->even_cprtime = 0;
    a->lat = 0;
    a->lon = 0;
    a->seen = time(NULL);
    a->messages = 0;
    a->next = NULL;
    return a;
}

/* Return the aircraft with the specified address, or NULL if no aircraft
 * exists with this address. */
struct aircraft *interactiveFindAircraft(uint32_t addr) {
    struct aircraft *a = Modes.aircrafts;

    while(a) {
        if (a->addr == addr) return a;
        a = a->next;
    }
    return NULL;
}

/* Always positive MOD operation, used for CPR decoding. */
int cprModFunction(int a, int b) {
    int res = a % b;
    if (res < 0) res += b;
    return res;
}

/* The NL function uses the precomputed table from 1090-WP-9-14 */
int cprNLFunction(double lat) {
    if (lat < 0) lat = -lat; /* Table is simmetric about the equator. */
    if (lat < 10.47047130) return 59;
    if (lat < 14.82817437) return 58;
    if (lat < 18.18626357) return 57;
    if (lat < 21.02939493) return 56;
    if (lat < 23.54504487) return 55;
    if (lat < 25.82924707) return 54;
    if (lat < 27.93898710) return 53;
    if (lat < 29.91135686) return 52;
    if (lat < 31.77209708) return 51;
    if (lat < 33.53993436) return 50;
    if (lat < 35.22899598) return 49;
    if (lat < 36.85025108) return 48;
    if (lat < 38.41241892) return 47;
    if (lat < 39.92256684) return 46;
    if (lat < 41.38651832) return 45;
    if (lat < 42.80914012) return 44;
    if (lat < 44.19454951) return 43;
    if (lat < 45.54626723) return 42;
    if (lat < 46.86733252) return 41;
    if (lat < 48.16039128) return 40;
    if (lat < 49.42776439) return 39;
    if (lat < 50.67150166) return 38;
    if (lat < 51.89342469) return 37;
    if (lat < 53.09516153) return 36;
    if (lat < 54.27817472) return 35;
    if (lat < 55.44378444) return 34;
    if (lat < 56.59318756) return 33;
    if (lat < 57.72747354) return 32;
    if (lat < 58.84763776) return 31;
    if (lat < 59.95459277) return 30;
    if (lat < 61.04917774) return 29;
    if (lat < 62.13216659) return 28;
    if (lat < 63.20427479) return 27;
    if (lat < 64.26616523) return 26;
    if (lat < 65.31845310) return 25;
    if (lat < 66.36171008) return 24;
    if (lat < 67.39646774) return 23;
    if (lat < 68.42322022) return 22;
    if (lat < 69.44242631) return 21;
    if (lat < 70.45451075) return 20;
    if (lat < 71.45986473) return 19;
    if (lat < 72.45884545) return 18;
    if (lat < 73.45177442) return 17;
    if (lat < 74.43893416) return 16;
    if (lat < 75.42056257) return 15;
    if (lat < 76.39684391) return 14;
    if (lat < 77.36789461) return 13;
    if (lat < 78.33374083) return 12;
    if (lat < 79.29428225) return 11;
    if (lat < 80.24923213) return 10;
    if (lat < 81.19801349) return 9;
    if (lat < 82.13956981) return 8;
    if (lat < 83.07199445) return 7;
    if (lat < 83.99173563) return 6;
    if (lat < 84.89166191) return 5;
    if (lat < 85.75541621) return 4;
    if (lat < 86.53536998) return 3;
    if (lat < 87.00000000) return 2;
    else return 1;
}

int cprNFunction(double lat, int isodd) {
    int nl = cprNLFunction(lat) - isodd;
    if (nl < 1) nl = 1;
    return nl;
}

double cprDlonFunction(double lat, int isodd) {
    return 360.0 / cprNFunction(lat, isodd);
}

/* This algorithm comes from:
 * http://www.lll.lu/~edward/edward/adsb/DecodingADSBposition.html.
 *
 *
 * A few remarks:
 * 1) 131072 is 2^17 since CPR latitude and longitude are encoded in 17 bits.
 * 2) We assume that we always received the odd packet as last packet for
 *    simplicity. This may provide a position that is less fresh of a few
 *    seconds.
 */
void decodeCPR(struct aircraft *a) {
    const double AirDlat0 = 360.0 / 60;
    const double AirDlat1 = 360.0 / 59;
    double lat0 = a->even_cprlat;
    double lat1 = a->odd_cprlat;
    double lon0 = a->even_cprlon;
    double lon1 = a->odd_cprlon;

    /* Compute the Latitude Index "j" */
    int j = floor(((59*lat0 - 60*lat1) / 131072) + 0.5);
    double rlat0 = AirDlat0 * (cprModFunction(j,60) + lat0 / 131072);
    double rlat1 = AirDlat1 * (cprModFunction(j,59) + lat1 / 131072);

    if (rlat0 >= 270) rlat0 -= 360;
    if (rlat1 >= 270) rlat1 -= 360;

    /* Check that both are in the same latitude zone, or abort. */
    if (cprNLFunction(rlat0) != cprNLFunction(rlat1)) return;

    /* Compute ni and the longitude index m */
    if (a->even_cprtime > a->odd_cprtime) {
        /* Use even packet. */
        int ni = cprNFunction(rlat0,0);
        int m = floor((((lon0 * (cprNLFunction(rlat0)-1)) -
                        (lon1 * cprNLFunction(rlat0))) / 131072) + 0.5);
        a->lon = cprDlonFunction(rlat0,0) * (cprModFunction(m,ni)+lon0/131072);
        a->lat = rlat0;
    } else {
        /* Use odd packet. */
        int ni = cprNFunction(rlat1,1);
        int m = floor((((lon0 * (cprNLFunction(rlat1)-1)) -
                        (lon1 * cprNLFunction(rlat1))) / 131072.0) + 0.5);
        a->lon = cprDlonFunction(rlat1,1) * (cprModFunction(m,ni)+lon1/131072);
        a->lat = rlat1;
    }
    if (a->lon > 180) a->lon -= 360;
}

/* Receive new messages and populate the interactive mode with more info. */
struct aircraft *interactiveReceiveData(struct modesMessage *mm) {
    uint32_t addr;
    struct aircraft *a, *aux;

    if (Modes.check_crc && mm->crcok == 0) return NULL;
    addr = (mm->aa1 << 16) | (mm->aa2 << 8) | mm->aa3;

    /* Loookup our aircraft or create a new one. */
    a = interactiveFindAircraft(addr);
    if (!a) {
        a = interactiveCreateAircraft(addr);
        a->next = Modes.aircrafts;
        Modes.aircrafts = a;
    } else {
        /* If it is an already known aircraft, move it on head
         * so we keep aircrafts ordered by received message time.
         *
         * However move it on head only if at least one second elapsed
         * since the aircraft that is currently on head sent a message,
         * othewise with multiple aircrafts at the same time we have an
         * useless shuffle of positions on the screen. */
        if (0 && Modes.aircrafts != a && (time(NULL) - a->seen) >= 1) {
            aux = Modes.aircrafts;
            while(aux->next != a) aux = aux->next;
            /* Now we are a node before the aircraft to remove. */
            aux->next = aux->next->next; /* removed. */
            /* Add on head */
            a->next = Modes.aircrafts;
            Modes.aircrafts = a;
        }
    }

    a->seen = time(NULL);
    a->messages++;

    if (mm->msgtype == 0 || mm->msgtype == 4 || mm->msgtype == 20) {
        a->altitude = mm->altitude;
    } else if (mm->msgtype == 17) {
        if (mm->metype >= 1 && mm->metype <= 4) {
            memcpy(a->flight, mm->flight, sizeof(a->flight));
        } else if (mm->metype >= 9 && mm->metype <= 18) {
            a->altitude = mm->altitude;
            if (mm->fflag) {
                a->odd_cprlat = mm->raw_latitude;
                a->odd_cprlon = mm->raw_longitude;
                a->odd_cprtime = mstime();
            } else {
                a->even_cprlat = mm->raw_latitude;
                a->even_cprlon = mm->raw_longitude;
                a->even_cprtime = mstime();
            }
            /* If the two data is less than 10 seconds apart, compute
             * the position. */
            if (abs(a->even_cprtime - a->odd_cprtime) <= 10000) {
                decodeCPR(a);
            }
        } else if (mm->metype == 19) {
            if (mm->mesub == 1 || mm->mesub == 2) {
                a->speed = mm->velocity;
                a->track = mm->heading;
            }
        }
    }
    return a;
}

/* Show the currently captured interactive data on screen. */
void interactiveShowData(void) {
    struct aircraft *a = Modes.aircrafts;
    time_t now = time(NULL);
    char progress[4];
    int count = 0;

    memset(progress,' ',3);
    progress[time(NULL)%3] = '.';
    progress[3] = '\0';

    printf("\x1b[H\x1b[2J");    /* Clear the screen */
    printf(
"Hex    Flight   Altitude  Speed   Lat       Lon       Track  Messages Seen %s\n"
"--------------------------------------------------------------------------------\n",
        progress);

    while(a && count < Modes.interactive_rows) {
        int altitude = a->altitude, speed = a->speed;

        /* Convert units to metric if --metric was specified. */
        if (Modes.metric) {
            altitude /= 3.2828;
            speed *= 1.852;
        }

        printf("%-6s %-8s %-9d %-7d %-7.03f   %-7.03f   %-3d   %-9ld %d sec\n",
            a->hexaddr, a->flight, altitude, speed,
            a->lat, a->lon, a->track, a->messages,
            (int)(now - a->seen));
        a = a->next;
        count++;
    }
}

/* When in interactive mode If we don't receive new nessages within
 * MODES_INTERACTIVE_TTL seconds we remove the aircraft from the list. */
void interactiveRemoveStaleAircrafts(void) {
    struct aircraft *a = Modes.aircrafts;
    struct aircraft *prev = NULL;
    time_t now = time(NULL);

    while(a) {
        if ((now - a->seen) > Modes.interactive_ttl) {
            struct aircraft *next = a->next;
            /* Remove the element from the linked list, with care
             * if we are removing the first element. */
            free(a);
            if (!prev)
                Modes.aircrafts = next;
            else
                prev->next = next;
            a = next;
        } else {
            prev = a;
            a = a->next;
        }
    }
}

/* ============================== Snip mode ================================= */

/* Get raw IQ samples and filter everything is < than the specified level
 * for more than 256 samples in order to reduce example file size. */
void snipMode(int level) {
    int i, q;
    long long c = 0;

    while ((i = getchar()) != EOF && (q = getchar()) != EOF) {
        if (abs(i-127) < level && abs(q-127) < level) {
            c++;
            if (c > MODES_PREAMBLE_US*4) continue;
        } else {
            c = 0;
        }
        putchar(i);
        putchar(q);
    }
}

/* ============================= Networking =================================
 * Note: here we risregard any kind of good coding practice in favor of
 * extreme simplicity, that is:
 *
 * 1) We only rely on the kernel buffers for our I/O without any kind of
 *    user space buffering.
 * 2) We don't register any kind of event handler, from time to time a
 *    function gets called and we accept new connections. All the rest is
 *    handled via non-blocking I/O and manually pulling clients to see if
 *    they have something new to share with us when reading is needed.
 */

#define MODES_NET_SERVICE_RAWO 0
#define MODES_NET_SERVICE_RAWI 1
#define MODES_NET_SERVICE_HTTP 2
#define MODES_NET_SERVICE_SBS 3
#define MODES_NET_SERVICES_NUM 4
struct {
    char *descr;
    int *socket;
    int port;
} modesNetServices[MODES_NET_SERVICES_NUM] = {
    {"Raw TCP output", &Modes.ros, MODES_NET_OUTPUT_RAW_PORT},
    {"Raw TCP input", &Modes.ris, MODES_NET_INPUT_RAW_PORT},
    {"HTTP server", &Modes.https, MODES_NET_HTTP_PORT},
    {"Basestation TCP output", &Modes.sbsos, MODES_NET_OUTPUT_SBS_PORT}
};

/* Networking "stack" initialization. */
void modesInitNet(void) {
    // int j;

    // memset(Modes.clients,0,sizeof(Modes.clients));
    // Modes.maxfd = -1;

    // for (j = 0; j < MODES_NET_SERVICES_NUM; j++) {
    //     int s = anetTcpServer(Modes.aneterr, modesNetServices[j].port, NULL);
    //     if (s == -1) {
    //         fprintf(stderr, "Error opening the listening port %d (%s): %s\n",
    //             modesNetServices[j].port,
    //             modesNetServices[j].descr,
    //             strerror(errno));
    //         exit(1);
    //     }
    //     anetNonBlock(Modes.aneterr, s);
    //     *modesNetServices[j].socket = s;
    // }

    // signal(SIGPIPE, SIG_IGN);
}

/* This function gets called from time to time when the decoding thread is
 * awakened by new data arriving. This usually happens a few times every
 * second. */
void modesAcceptClients(void) {
    // int fd, port;
    // unsigned int j;
    // struct client *c;

    // for (j = 0; j < MODES_NET_SERVICES_NUM; j++) {
    //     fd = anetTcpAccept(Modes.aneterr, *modesNetServices[j].socket,
    //                        NULL, &port);
    //     if (fd == -1) {
    //         if (Modes.debug & MODES_DEBUG_NET && errno != EAGAIN)
    //             printf("Accept %d: %s\n", *modesNetServices[j].socket,
    //                    strerror(errno));
    //         continue;
    //     }

    //     if (fd >= MODES_NET_MAX_FD) {
    //         close(fd);
    //         return; /* Max number of clients reached. */
    //     }

    //     anetNonBlock(Modes.aneterr, fd);
    //     c = malloc(sizeof(*c));
    //     c->service = *modesNetServices[j].socket;
    //     c->fd = fd;
    //     c->buflen = 0;
    //     Modes.clients[fd] = c;
    //     anetSetSendBuffer(Modes.aneterr,fd,MODES_NET_SNDBUF_SIZE);

    //     if (Modes.maxfd < fd) Modes.maxfd = fd;
    //     if (*modesNetServices[j].socket == Modes.sbsos)
    //         Modes.stat_sbs_connections++;

    //     j--; /* Try again with the same listening port. */

    //     if (Modes.debug & MODES_DEBUG_NET)
    //         printf("Created new client %d\n", fd);
    // }
}

/* On error free the client, collect the structure, adjust maxfd if needed. */
void modesFreeClient(int fd) {
    close(fd);
    free(Modes.clients[fd]);
    Modes.clients[fd] = NULL;

    if (Modes.debug & MODES_DEBUG_NET)
        printf("Closing client %d\n", fd);

    /* If this was our maxfd, scan the clients array to find the new max.
     * Note that we are sure there is no active fd greater than the closed
     * fd, so we scan from fd-1 to 0. */
    if (Modes.maxfd == fd) {
        int j;

        Modes.maxfd = -1;
        for (j = fd-1; j >= 0; j--) {
            if (Modes.clients[j]) {
                Modes.maxfd = j;
                break;
            }
        }
    }
}

/* Send the specified message to all clients listening for a given service. */
void modesSendAllClients(int service, void *msg, int len) {
    int j;
    struct client *c;

    for (j = 0; j <= Modes.maxfd; j++) {
        c = Modes.clients[j];
        if (c && c->service == service) {
            int nwritten = write(j, msg, len);
            if (nwritten != len) {
                modesFreeClient(j);
            }
        }
    }
}

/* Write raw output to TCP clients. */
void modesSendRawOutput(struct modesMessage *mm) {
    char msg[128], *p = msg;
    int j;

    *p++ = '*';
    for (j = 0; j < mm->msgbits/8; j++) {
        sprintf(p, "%02X", mm->msg[j]);
        p += 2;
    }
    *p++ = ';';
    *p++ = '\n';
    modesSendAllClients(Modes.ros, msg, p-msg);
}


/* Write SBS output to TCP clients. */
void modesSendSBSOutput(struct modesMessage *mm, struct aircraft *a) {
    char msg[256], *p = msg;
    int emergency = 0, ground = 0, alert = 0, spi = 0;

    if (mm->msgtype == 4 || mm->msgtype == 5 || mm->msgtype == 21) {
        /* Node: identity is calculated/kept in base10 but is actually
         * octal (07500 is represented as 7500) */
        if (mm->identity == 7500 || mm->identity == 7600 ||
            mm->identity == 7700) emergency = -1;
        if (mm->fs == 1 || mm->fs == 3) ground = -1;
        if (mm->fs == 2 || mm->fs == 3 || mm->fs == 4) alert = -1;
        if (mm->fs == 4 || mm->fs == 5) spi = -1;
    }

    if (mm->msgtype == 0) {
        p += sprintf(p, "MSG,5,,,%02X%02X%02X,,,,,,,%d,,,,,,,,,,",
        mm->aa1, mm->aa2, mm->aa3, mm->altitude);
    } else if (mm->msgtype == 4) {
        p += sprintf(p, "MSG,5,,,%02X%02X%02X,,,,,,,%d,,,,,,,%d,%d,%d,%d",
        mm->aa1, mm->aa2, mm->aa3, mm->altitude, alert, emergency, spi, ground);
    } else if (mm->msgtype == 5) {
        p += sprintf(p, "MSG,6,,,%02X%02X%02X,,,,,,,,,,,,,%d,%d,%d,%d,%d",
        mm->aa1, mm->aa2, mm->aa3, mm->identity, alert, emergency, spi, ground);
    } else if (mm->msgtype == 11) {
        p += sprintf(p, "MSG,8,,,%02X%02X%02X,,,,,,,,,,,,,,,,,",
        mm->aa1, mm->aa2, mm->aa3);
    } else if (mm->msgtype == 17 && mm->metype == 4) {
        p += sprintf(p, "MSG,1,,,%02X%02X%02X,,,,,,%s,,,,,,,,0,0,0,0",
        mm->aa1, mm->aa2, mm->aa3, mm->flight);
    } else if (mm->msgtype == 17 && mm->metype >= 9 && mm->metype <= 18) {
        if (a->lat == 0 && a->lon == 0)
            p += sprintf(p, "MSG,3,,,%02X%02X%02X,,,,,,,%d,,,,,,,0,0,0,0",
            mm->aa1, mm->aa2, mm->aa3, mm->altitude);
        else
            p += sprintf(p, "MSG,3,,,%02X%02X%02X,,,,,,,%d,,,%1.5f,%1.5f,,,"
                            "0,0,0,0",
            mm->aa1, mm->aa2, mm->aa3, mm->altitude, a->lat, a->lon);
    } else if (mm->msgtype == 17 && mm->metype == 19 && mm->mesub == 1) {
        int vr = (mm->vert_rate_sign==0?1:-1) * (mm->vert_rate-1) * 64;

        p += sprintf(p, "MSG,4,,,%02X%02X%02X,,,,,,,,%d,%d,,,%i,,0,0,0,0",
        mm->aa1, mm->aa2, mm->aa3, a->speed, a->track, vr);
    } else if (mm->msgtype == 21) {
        p += sprintf(p, "MSG,6,,,%02X%02X%02X,,,,,,,,,,,,,%d,%d,%d,%d,%d",
        mm->aa1, mm->aa2, mm->aa3, mm->identity, alert, emergency, spi, ground);
    } else {
        return;
    }

    *p++ = '\n';
    modesSendAllClients(Modes.sbsos, msg, p-msg);
}

/* Turn an hex digit into its 4 bit decimal value.
 * Returns -1 if the digit is not in the 0-F range. */
int hexDigitVal(int c) {
    c = tolower(c);
    if (c >= '0' && c <= '9') return c-'0';
    else if (c >= 'a' && c <= 'f') return c-'a'+10;
    else return -1;
}

/* This function decodes a string representing a Mode S message in
 * raw hex format like: *8D4B969699155600E87406F5B69F;
 * The string is supposed to be at the start of the client buffer
 * and null-terminated.
 * 
 * The message is passed to the higher level layers, so it feeds
 * the selected screen output, the network output and so forth.
 * 
 * If the message looks invalid is silently discarded.
 *
 * The function always returns 0 (success) to the caller as there is
 * no case where we want broken messages here to close the client
 * connection. */
int decodeHexMessage(struct client *c) {
    char *hex = c->buf;
    int l = strlen(hex), j;
    unsigned char msg[MODES_LONG_MSG_BYTES];
    struct modesMessage mm;

    /* Remove spaces on the left and on the right. */
    while(l && isspace(hex[l-1])) {
        hex[l-1] = '\0';
        l--;
    }
    while(isspace(*hex)) {
        hex++;
        l--;
    }

    /* Turn the message into binary. */
    if (l < 2 || hex[0] != '*' || hex[l-1] != ';') return 0;
    hex++; l-=2; /* Skip * and ; */
    if (l > MODES_LONG_MSG_BYTES*2) return 0; /* Too long message... broken. */
    for (j = 0; j < l; j += 2) {
        int high = hexDigitVal(hex[j]);
        int low = hexDigitVal(hex[j+1]);

        if (high == -1 || low == -1) return 0;
        msg[j/2] = (high<<4) | low;
    }
    decodeModesMessage(&mm,msg);
    useModesMessage(&mm);
    return 0;
}

/* Return a description of planes in json. */
char *aircraftsToJson(int *len) {
    struct aircraft *a = Modes.aircrafts;
    int buflen = 1024; /* The initial buffer is incremented as needed. */
    char *buf = malloc(buflen), *p = buf;
    int l;

    l = snprintf(p,buflen,"[\n");
    p += l; buflen -= l;
    while(a) {
        int altitude = a->altitude, speed = a->speed;

        /* Convert units to metric if --metric was specified. */
        if (Modes.metric) {
            altitude /= 3.2828;
            speed *= 1.852;
        }

        if (a->lat != 0 && a->lon != 0) {
            l = snprintf(p,buflen,
                "{\"hex\":\"%s\", \"flight\":\"%s\", \"lat\":%f, "
                "\"lon\":%f, \"altitude\":%d, \"track\":%d, "
                "\"speed\":%d},\n",
                a->hexaddr, a->flight, a->lat, a->lon, a->altitude, a->track,
                a->speed);
            p += l; buflen -= l;
            /* Resize if needed. */
            if (buflen < 256) {
                int used = p-buf;
                buflen += 1024; /* Our increment. */
                buf = realloc(buf,used+buflen);
                p = buf+used;
            }
        }
        a = a->next;
    }
    /* Remove the final comma if any, and closes the json array. */
    if (*(p-2) == ',') {
        *(p-2) = '\n';
        p--;
        buflen++;
    }
    l = snprintf(p,buflen,"]\n");
    p += l; buflen -= l;

    *len = p-buf;
    return buf;
}

#define MODES_CONTENT_TYPE_HTML "text/html;charset=utf-8"
#define MODES_CONTENT_TYPE_JSON "application/json;charset=utf-8"

/* Get an HTTP request header and write the response to the client.
 * Again here we assume that the socket buffer is enough without doing
 * any kind of userspace buffering.
 *
 * Returns 1 on error to signal the caller the client connection should
 * be closed. */
int handleHTTPRequest(struct client *c) {
    char hdr[512];
    int clen, hdrlen;
    int httpver, keepalive;
    char *p, *url, *content;
    char *ctype;

    if (Modes.debug & MODES_DEBUG_NET)
        printf("\nHTTP request: %s\n", c->buf);

    /* Minimally parse the request. */
    httpver = (strstr(c->buf, "HTTP/1.1") != NULL) ? 11 : 10;
    if (httpver == 10) {
        /* HTTP 1.0 defaults to close, unless otherwise specified. */
        keepalive = strstr(c->buf, "Connection: keep-alive") != NULL;
    } else if (httpver == 11) {
        /* HTTP 1.1 defaults to keep-alive, unless close is specified. */
        keepalive = strstr(c->buf, "Connection: close") == NULL;
    }

    /* Identify he URL. */
    p = strchr(c->buf,' ');
    if (!p) return 1; /* There should be the method and a space... */
    url = ++p; /* Now this should point to the requested URL. */
    p = strchr(p, ' ');
    if (!p) return 1; /* There should be a space before HTTP/... */
    *p = '\0';

    if (Modes.debug & MODES_DEBUG_NET) {
        printf("\nHTTP keep alive: %d\n", keepalive);
        printf("HTTP requested URL: %s\n\n", url);
    }

    /* Select the content to send, we have just two so far:
     * "/" -> Our google map application.
     * "/data.json" -> Our ajax request to update planes. */
    if (strstr(url, "/data.json")) {
        content = aircraftsToJson(&clen);
        ctype = MODES_CONTENT_TYPE_JSON;
    } else {
        struct stat sbuf;
        int fd = -1;

        if (stat("gmap.html",&sbuf) != -1 &&
            (fd = open("gmap.html",O_RDONLY)) != -1)
        {
            content = malloc(sbuf.st_size);
            if (read(fd,content,sbuf.st_size) == -1) {
                snprintf(content,sbuf.st_size,"Error reading from file: %s",
                    strerror(errno));
            }
            clen = sbuf.st_size;
        } else {
            char buf[128];

            clen = snprintf(buf,sizeof(buf),"Error opening HTML file: %s",
                strerror(errno));
            content = strdup(buf);
        }
        if (fd != -1) close(fd);
        ctype = MODES_CONTENT_TYPE_HTML;
    }

    /* Create the header and send the reply. */
    hdrlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Server: Dump1090\r\n"
        "Content-Type: %s\r\n"
        "Connection: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        ctype,
        keepalive ? "keep-alive" : "close",
        clen);

    if (Modes.debug & MODES_DEBUG_NET)
        printf("HTTP Reply header:\n%s", hdr);

    /* Send header and content. */
    if (write(c->fd, hdr, hdrlen) != hdrlen ||
        write(c->fd, content, clen) != clen)
    {
        free(content);
        return 1;
    }
    free(content);
    Modes.stat_http_requests++;
    return !keepalive;
}

/* This function polls the clients using read() in order to receive new
 * messages from the net.
 *
 * The message is supposed to be separated by the next message by the
 * separator 'sep', that is a null-terminated C string.
 *
 * Every full message received is decoded and passed to the higher layers
 * calling the function 'handler'.
 *
 * The handelr returns 0 on success, or 1 to signal this function we
 * should close the connection with the client in case of non-recoverable
 * errors. */
void modesReadFromClient(struct client *c, char *sep,
                         int(*handler)(struct client *))
{
    while(1) {
        int left = MODES_CLIENT_BUF_SIZE - c->buflen;
        int nread = read(c->fd, c->buf+c->buflen, left);
        int fullmsg = 0;
        int i;
        char *p;

        if (nread <= 0) {
            if (nread == 0 || errno != EAGAIN) {
                /* Error, or end of file. */
                modesFreeClient(c->fd);
            }
            break; /* Serve next client. */
        }
        c->buflen += nread;

        /* Always null-term so we are free to use strstr() */
        c->buf[c->buflen] = '\0';

        /* If there is a complete message there must be the separator 'sep'
         * in the buffer, note that we full-scan the buffer at every read
         * for simplicity. */
        while ((p = strstr(c->buf, sep)) != NULL) {
            i = p - c->buf; /* Turn it as an index inside the buffer. */
            c->buf[i] = '\0'; /* Te handler expects null terminated strings. */
            /* Call the function to process the message. It returns 1
             * on error to signal we should close the client connection. */
            if (handler(c)) {
                modesFreeClient(c->fd);
                return;
            }
            /* Move what's left at the start of the buffer. */
            i += strlen(sep); /* The separator is part of the previous msg. */
            memmove(c->buf,c->buf+i,c->buflen-i);
            c->buflen -= i;
            c->buf[c->buflen] = '\0';
            /* Maybe there are more messages inside the buffer.
             * Start looping from the start again. */
            fullmsg = 1;
        }
        /* If our buffer is full discard it, this is some badly
         * formatted shit. */
        if (c->buflen == MODES_CLIENT_BUF_SIZE) {
            c->buflen = 0;
            /* If there is garbage, read more to discard it ASAP. */
            continue;
        }
        /* If no message was decoded process the next client, otherwise
         * read more data from the same client. */
        if (!fullmsg) break;
    }
}

/* Read data from clients. This function actually delegates a lower-level
 * function that depends on the kind of service (raw, http, ...). */
void modesReadFromClients(void) {
    int j;
    struct client *c;

    for (j = 0; j <= Modes.maxfd; j++) {
        if ((c = Modes.clients[j]) == NULL) continue;
        if (c->service == Modes.ris)
            modesReadFromClient(c,"\n",decodeHexMessage);
        else if (c->service == Modes.https)
            modesReadFromClient(c,"\r\n\r\n",handleHTTPRequest);
    }
}

/* This function is used when "net only" mode is enabled to know when there
 * is at least a new client to serve. Note that the dump1090 networking model
 * is extremely trivial and a function takes care of handling all the clients
 * that have something to serve, without a proper event library, so the
 * function here returns as long as there is a single client ready, or
 * when the specified timeout in milliesconds elapsed, without specifying to
 * the caller what client requires to be served. */
void modesWaitReadableClients(int timeout_ms) {
    struct timeval tv;
    fd_set fds;
    int j, maxfd = Modes.maxfd;

    FD_ZERO(&fds);

    /* Set client FDs */
    for (j = 0; j <= Modes.maxfd; j++) {
        if (Modes.clients[j]) FD_SET(j,&fds);
    }

    /* Set listening sockets to accept new clients ASAP. */
    for (j = 0; j < MODES_NET_SERVICES_NUM; j++) {
        int s = *modesNetServices[j].socket;
        FD_SET(s,&fds);
        if (s > maxfd) maxfd = s;
    }

    tv.tv_sec = timeout_ms/1000;
    tv.tv_usec = (timeout_ms%1000)*1000;
    /* We don't care why select returned here, timeout, error, or
     * FDs ready are all conditions for which we just return. */
    select(maxfd+1,&fds,NULL,NULL,&tv);
}

/* ============================ Terminal handling  ========================== */

/* Handle resizing terminal. */
void sigWinchCallback() {
    signal(SIGWINCH, SIG_IGN);
    Modes.interactive_rows = getTermRows();
    interactiveShowData();
    signal(SIGWINCH, sigWinchCallback);
}

/* Get the number of rows after the terminal changes size. */
int getTermRows() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row;
}

/* ================================ Main ==================================== */

void showHelp(void) {
    printf(
"--device-index <index>   Select RTL device (default: 0).\n"
"--gain <db>              Set gain (default: max gain. Use -100 for auto-gain).\n"
"--enable-agc             Enable the Automatic Gain Control (default: off).\n"
"--freq <hz>              Set frequency (default: 1090 Mhz).\n"
"--ifile <filename>       Read data from file (use '-' for stdin).\n"
"--interactive            Interactive mode refreshing data on screen.\n"
"--interactive-rows <num> Max number of rows in interactive mode (default: 15).\n"
"--interactive-ttl <sec>  Remove from list if idle for <sec> (default: 60).\n"
"--raw                    Show only messages hex values.\n"
"--net                    Enable networking.\n"
"--net-only               Enable just networking, no RTL device or file used.\n"
"--net-ro-port <port>     TCP listening port for raw output (default: 30002).\n"
"--net-ri-port <port>     TCP listening port for raw input (default: 30001).\n"
"--net-http-port <port>   HTTP server port (default: 8080).\n"
"--net-sbs-port <port>    TCP listening port for BaseStation format output (default: 30003).\n"
"--no-fix                 Disable single-bits error correction using CRC.\n"
"--no-crc-check           Disable messages with broken CRC (discouraged).\n"
"--aggressive             More CPU for more messages (two bits fixes, ...).\n"
"--stats                  With --ifile print stats at exit. No other output.\n"
"--onlyaddr               Show only ICAO addresses (testing purposes).\n"
"--metric                 Use metric units (meters, km/h, ...).\n"
"--snip <level>           Strip IQ file removing samples < level.\n"
"--debug <flags>          Debug mode (verbose), see README for details.\n"
"--help                   Show this help.\n"
"\n"
"Debug mode flags: d = Log frames decoded with errors\n"
"                  D = Log frames decoded with zero errors\n"
"                  c = Log frames with bad CRC\n"
"                  C = Log frames with good CRC\n"
"                  p = Log frames with bad preamble\n"
"                  n = Log network debugging info\n"
"                  j = Log frames to frames.js, loadable by debug.html.\n"
    );
}

/* This function is called a few times every second by main in order to
 * perform tasks we need to do continuously, like accepting new clients
 * from the net, refreshing the screen in interactive mode, and so forth. */
void backgroundTasks(void) {
    if (Modes.net) {
        modesAcceptClients();
        modesReadFromClients();
        interactiveRemoveStaleAircrafts();
    }

    /* Refresh screen when in interactive mode. */
    if (Modes.interactive &&
        (mstime() - Modes.interactive_last_update) >
        MODES_INTERACTIVE_REFRESH_TIME)
    {
        interactiveRemoveStaleAircrafts();
        interactiveShowData();
        Modes.interactive_last_update = mstime();
    }
}

int main(int argc, char **argv) {
    int j;

    /* Set sane defaults. */
    modesInitConfig();

    /* Parse the command line options */
    for (j = 1; j < argc; j++) {
        int more = j+1 < argc; /* There are more arguments. */

        if (!strcmp(argv[j],"--device-index") && more) {
            Modes.dev_index = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--gain") && more) {
            Modes.gain = atof(argv[++j])*10; /* Gain is in tens of DBs */
        } else if (!strcmp(argv[j],"--enable-agc")) {
            Modes.enable_agc++;
        } else if (!strcmp(argv[j],"--freq") && more) {
            Modes.freq = strtoll(argv[++j],NULL,10);
        } else if (!strcmp(argv[j],"--ifile") && more) {
            Modes.filename = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--no-fix")) {
            Modes.fix_errors = 0;
        } else if (!strcmp(argv[j],"--no-crc-check")) {
            Modes.check_crc = 0;
        } else if (!strcmp(argv[j],"--raw")) {
            Modes.raw = 1;
        } else if (!strcmp(argv[j],"--net")) {
            Modes.net = 1;
        } else if (!strcmp(argv[j],"--net-only")) {
            Modes.net = 1;
            Modes.net_only = 1;
        } else if (!strcmp(argv[j],"--net-ro-port") && more) {
            modesNetServices[MODES_NET_SERVICE_RAWO].port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-ri-port") && more) {
            modesNetServices[MODES_NET_SERVICE_RAWI].port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-http-port") && more) {
            modesNetServices[MODES_NET_SERVICE_HTTP].port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-sbs-port") && more) {
            modesNetServices[MODES_NET_SERVICE_SBS].port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--onlyaddr")) {
            Modes.onlyaddr = 1;
        } else if (!strcmp(argv[j],"--metric")) {
            Modes.metric = 1;
        } else if (!strcmp(argv[j],"--aggressive")) {
            Modes.aggressive++;
        } else if (!strcmp(argv[j],"--interactive")) {
            Modes.interactive = 1;
        } else if (!strcmp(argv[j],"--interactive-rows")) {
            Modes.interactive_rows = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--interactive-ttl")) {
            Modes.interactive_ttl = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--debug") && more) {
            char *f = argv[++j];
            while(*f) {
                switch(*f) {
                case 'D': Modes.debug |= MODES_DEBUG_DEMOD; break;
                case 'd': Modes.debug |= MODES_DEBUG_DEMODERR; break;
                case 'C': Modes.debug |= MODES_DEBUG_GOODCRC; break;
                case 'c': Modes.debug |= MODES_DEBUG_BADCRC; break;
                case 'p': Modes.debug |= MODES_DEBUG_NOPREAMBLE; break;
                case 'n': Modes.debug |= MODES_DEBUG_NET; break;
                case 'j': Modes.debug |= MODES_DEBUG_JS; break;
                default:
                    fprintf(stderr, "Unknown debugging flag: %c\n", *f);
                    exit(1);
                    break;
                }
                f++;
            }
        } else if (!strcmp(argv[j],"--stats")) {
            Modes.stats = 1;
        } else if (!strcmp(argv[j],"--snip") && more) {
            snipMode(atoi(argv[++j]));
            exit(0);
        } else if (!strcmp(argv[j],"--help")) {
            showHelp();
            exit(0);
        } else {
            fprintf(stderr,
                "Unknown or not enough arguments for option '%s'.\n\n",
                argv[j]);
            showHelp();
            exit(1);
        }
    }

    /* Setup for SIGWINCH for handling lines */
    if (Modes.interactive == 1) signal(SIGWINCH, sigWinchCallback);

    /* Initialization */
    modesInit();
    if (Modes.net_only) {
        fprintf(stderr,"Net-only mode, no RTL device or file open.\n");
    } else if (Modes.filename == NULL) {
        modesInitRTLSDR();
    } else {
        if (Modes.filename[0] == '-' && Modes.filename[1] == '\0') {
            Modes.fd = STDIN_FILENO;
        } else if ((Modes.fd = open(Modes.filename,O_RDONLY)) == -1) {
            perror("Opening data file");
            exit(1);
        }
    }
    if (Modes.net) modesInitNet();

    /* If the user specifies --net-only, just run in order to serve network
     * clients without reading data from the RTL device. */
    while (Modes.net_only) {
        backgroundTasks();
        modesWaitReadableClients(100);
    }

    /* Create the thread that will read the data from the device. */
    pthread_create(&Modes.reader_thread, NULL, readerThreadEntryPoint, NULL);

    pthread_mutex_lock(&Modes.data_mutex);
    while(1) {
        if (!Modes.data_ready) {
            pthread_cond_wait(&Modes.data_cond,&Modes.data_mutex);
            continue;
        }
        computeMagnitudeVector();

        /* Signal to the other thread that we processed the available data
         * and we want more (useful for --ifile). */
        Modes.data_ready = 0;
        pthread_cond_signal(&Modes.data_cond);

        /* Process data after releasing the lock, so that the capturing
         * thread can read data while we perform computationally expensive
         * stuff * at the same time. (This should only be useful with very
         * slow processors). */
        pthread_mutex_unlock(&Modes.data_mutex);
        detectUVD(Modes.magnitude, Modes.data_len/2);
        backgroundTasks();
        pthread_mutex_lock(&Modes.data_mutex);
        if (Modes.exit) break;
    }

    /* If --ifile and --stats were given, print statistics. */
    if (Modes.stats && Modes.filename) {
        printf("%lld valid preambles\n", Modes.stat_valid_preamble);
        printf("%lld demodulated again after phase correction\n",
            Modes.stat_out_of_phase);
        printf("%lld demodulated with zero errors\n",
            Modes.stat_demodulated);
        printf("%lld with good crc\n", Modes.stat_goodcrc);
        printf("%lld with bad crc\n", Modes.stat_badcrc);
        printf("%lld errors corrected\n", Modes.stat_fixed);
        printf("%lld single bit errors\n", Modes.stat_single_bit_fix);
        printf("%lld two bits errors\n", Modes.stat_two_bits_fix);
        printf("%lld total usable messages\n",
            Modes.stat_goodcrc + Modes.stat_fixed);
    }

    rtlsdr_close(Modes.dev);
    return 0;
}


