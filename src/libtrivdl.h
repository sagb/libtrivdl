/*
 * libtrivdl library API header.
 *
 * Copyright 2018 Alexander Kulak.
 * This file is licensed under the MIT license.
 * See the LICENSE file in the project root for more information.
 */

#ifndef LIBTRIVDL_H
#define LIBTRIVDL_H

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#ifndef MCU
// tcdrain
#include <termios.h>
#include <unistd.h>
// strerror
#include <string.h>
// select
#include <sys/select.h>
// open
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#ifdef MCU
#define msg(...) do {} while(0)
#define wrn(...) do {} while(0)
#define err(...) do {} while(0)
#else
#define msg(...) fprintf (stdout, __VA_ARGS__); fflush (stdout);
#define err(...) fprintf (stderr, __VA_ARGS__); fflush (stderr);
#ifdef DEBUG
#define wrn(...) fprintf (stderr, __VA_ARGS__); fflush (stderr);
#else
#define wrn(...) do {} while(0)
#endif
#endif

// maximum frame size. think about:
//   rx/tx error rate,
//   MCU RAM limit,
//   uchar limit 255,
//   race conditions in my bad MCU code
#define MAXFRAMESIZE    64
#define OVERHEAD        3    // header + footer
#define MINFRAMESIZE    4    // OVERHEAD + 1 char

// special data values
#define FRAMEDELIMITER  0xBA

// t_frame character index
#define SIGNATURE   0    // single FRAMEDELIMITER
#define LASTNDX     1    // index of last character, where checksum resides
#define MESSAGE     2    // first char of message

// frame flags
#define READY       1   // tx: data ready to send, rx: read complete/data ready
// tx:
// ~READY (0) = user code owns frame;  READY (1) = txmachine owns frame
// rx:
// ~READY (0) = rxmachine owns frame;  READY (1) = user code owns frame
// 
#define HFDFL       2   // tx: half delimiter sent, rx: half delimiter encountered

// line flags
#define EXIT_A_M    4   // request to exit async machine

// frame return status, see cb_frame_ callbacks and strfrret
#define FROK        0
#define FRBADFMT    1   // malformed
#define FRBADSUM    2   // checksum mismatch
#define FRTOOLONG   3   // frame too long

// shortcuts
#define DATA        (fr->data)
#define WDATA       (wfr->data)
#define RDATA       (rfr->data)
#define WNEXT       (wfr->next)
#define RNEXT       (rfr->next)
#define RFLAGS      (rfr->flags)
#define WFLAGS      (wfr->flags)
#define FRLAST      (DATA[LASTNDX])
#define WFRLAST     (WDATA[LASTNDX])
#define RFRLAST     (RDATA[LASTNDX])
#define LFD         (line->fd)
#define LFLAGS      (line->lflags)
#define LUSERDATA   (line->userdata)
#define LRMSG       (line->rfr).data[MESSAGE]
#define LWMSG       (line->wfr).data[MESSAGE]
#define LRLAST      (line->rfr).data[LASTNDX]
#define LWLAST      (line->wfr).data[LASTNDX]
#define LRFLAGS     (line->rfr).flags
#define LWFLAGS     (line->wfr).flags
#define LRFR        &(line->rfr)
#define LWFR        &(line->wfr)
#define LRDATA      (line->rfr).data
#define LWDATA      (line->wfr).data
#define LRNEXT      (line->rfr).next
#define LWNEXT      (line->wfr).next
// TODO: get rid of this
#define DEFINE_FRAME_VIA_LINE  \
    t_frame* wfr = &(line->wfr); \
    t_frame* rfr = &(line->rfr);


// PUBLIC

typedef unsigned char uc;
typedef struct {
    uc data[MAXFRAMESIZE];
    uc next;
    uc flags;
} t_frame;

typedef struct {
#ifndef MCU
    int fd;
#endif
    t_frame wfr;
    t_frame rfr;
    uc lflags;
    void* userdata;
} t_line;


void init_frame (t_frame* fr);
int init_line (t_line* line, char* portname, void* userdata);
uc compute_checksum (t_frame* fr);
void add_hdr_and_checksum (t_frame* fr);
t_frame* build_frame (t_frame* fr, uc* src, uc size); // fr must be allocated

#ifdef MCU
// to save MCU stack, assume SINGLE line (UART)
t_line *line; // allocate it!
void incoming_char (uc c); // call from RX ISR
uc outgoing_char ();
#else
int async_machine (t_line* line);
void incoming_char (t_line* line, uc c);
uc outgoing_char (t_line* line);
char* strfr (t_frame* fr);
char* strfrret (uc status);
#endif

// callbacks for async machine.
// define them all in your source:

#ifdef MCU
// in MCU, keep them short because they are called from ISR
void cb_frame_rx_done (uc status);
//void cb_frame_tx_done (uc status);  for now, take care of TX yourself. TODO
#define X_DONE(a,b)   a(b)
#else
void cb_frame_rx_done (uc status, t_line* line);
void cb_frame_tx_done (uc status, t_line* line);
#define X_DONE(a,b)   a(b, line)
#endif
#ifndef MCU
// Called by async machine when there is nothing to send or receive.
// must return time in seconds before its next call.
// This time may be relatively large (1 sec or more), because 
// it doesn't introduce any delay in rx/tx.
float cb_idle (t_line* line);
#endif

// debug means

#ifdef MCU
#ifdef DEBUG
#define DEBUG1  mcudebug1();
#define DEBUG2  mcudebug2();
// define them as short LED flashes or beeps
void DEBUG1
void DEBUG2
#else
#define DEBUG1
#define DEBUG2
#endif
#endif
#ifndef MCU
#ifndef DEBUG
#define DEBUG1
#define DEBUG2
#endif
#endif

#endif
