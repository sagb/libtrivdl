/*
 * libtrivdl example: stream, POSIX version.
 *
 * Copyright 2018 Alexander Kulak.
 * This file is licensed under the MIT license.
 * See the LICENSE file in the project root for more information.
 */

#include "serial.h"
#include "libtrivdl.h"
#include "stream.h"
// rand
#include <stdlib.h>
// time
#include <time.h>

void create_and_send_data (t_line* line)
{
    uc pl[MAXFRAMESIZE]; // payload
    uc m;
    pl[0] = 0x10; // OP_ECHORQ
    for (m = 1; m < FSIZE-OVERHEAD; m++)
        pl[m] = (uc)rand();
    build_frame (LWFR, pl, FSIZE-OVERHEAD);
    //wrn("built: %s\n", strfr(LWFR));
    LWNEXT = 0;
    LWFLAGS |= READY; // async machine starts transmitting
}

void request_remote_stream (t_line* line)
{
    uc sf[2]; // start frame
    sf[0] = OP_STREAM_START;
    sf[1] = FSIZE;
    build_frame (LWFR, sf, 2);
    //wrn("built: %s\n", strfr(LWFR));
    LWNEXT = 0;
    LWFLAGS |= READY; // async machine starts transmitting
}

void request_remote_stop (t_line* line)
{
    build_frame (LWFR, "\x13", 1); // OP_STREAM_STOP
    //wrn("built: %s\n", strfr(LWFR));
    LWNEXT = 0;
    LWFLAGS |= READY; // async machine starts transmitting
}

void cb_frame_rx_done (uc status, t_line* line)
{
#ifdef DEBUG
    wrn ("frame received (%s, size %d, opcode 0x%hhx, state 0x%hhx)\n", 
            strfrret(status), LRLAST+1, STATE);
#else
    msg ("r%c", LRMSG+0x20);
#endif
    //wrn ("content: %s\n", strfr(LRFR));
    if (LRMSG == OP_STREAM_DATA || LRMSG == 0x11) {
        RXTOTAL++;
        if (status!=FROK) {
            RXERRORS++;
#ifndef DEBUG
            msg ("!");
#endif
        }
    }
    switch (LRMSG) {
        case OP_STREAM_DATA:
        case 0x11:
            STATE = ST_async_data;
            break;
        case OP_STREAM_STATS:
            LFLAGS |= EXIT_A_M; // stop async machine
            STATE = ST_stats;
            // recover slave stats
            t_ctr* ctr = (t_ctr*)(LRDATA+MESSAGE-1);
            PTX = ctr->i[1];
            PRX = ctr->i[2];
            PTXERR = ctr->i[3];
            PRXERR = ctr->i[4];
            break;
    }
    if ((RXTOTAL * FSIZE) >= MAXCHARS && (STATE < ST_stop)) {
        request_remote_stop;
        STATE = ST_stop;
    }
    // continue rx
    LRNEXT = 0;
    LRFLAGS &= ~READY;
}

void cb_frame_tx_done (uc status, t_line* line)
{
#ifdef DEBUG
    wrn ("frame transmitted (%s, size %d, opcode 0x%hhx, state 0x%hhx)\n", 
            strfrret(status), LWLAST+1, LWMSG, STATE);
#else
    msg ("t%c", LWMSG+0x20);
#endif
    if (LWMSG == OP_STREAM_DATA || LWMSG == 0x10) {
        TXTOTAL++;
        if (status!=FROK) {
            TXERRORS++;
#ifndef DEBUG
            msg ("!");
#endif
        }
    }
    if ((TXTOTAL * FSIZE) >= MAXCHARS)
        request_remote_stop (line);
    else
        create_and_send_data (line); // continuously send new frames
}

float cb_idle (t_line* line)
{
    // never called anyways
    return 0.0;
}

t_baud data_session (t_line* line, int maxchars, uc framesize)
{
    long int tstart, tstop, tdel;
    float rc, tc, rrc, rtc;
    t_baud res;

    time (&tstart);
    memset (line->userdata, 0, sizeof(t_userdata));
    FSIZE = framesize;
    MAXCHARS = maxchars;
    STATE = ST_silence;
    request_remote_stream (line); // tx from very start
    STATE = ST_start;
    async_machine (line);
    time (&tstop);
    tdel = tstop - tstart;
    if (tdel==0) tdel=1;
    rc = (RXTOTAL - RXERRORS) * FSIZE / tdel;
    tc = (TXTOTAL - TXERRORS) * FSIZE / tdel;
    rrc = (PRX - PRXERR) * FSIZE / tdel;
    rtc = (PTX - PTXERR) * FSIZE / tdel;
    // 8N1
    res.fsize = FSIZE;
    res.brc = rc*9;
    res.btc = tc*9;
    res.rbrc = rrc*9;
    res.rbtc = rtc*9;
    msg ("\nprobe with %hhu char frame took %ld sec", FSIZE, tdel);
    msg ("\nlocal DATA:\nrx: total %hu, errors %hu (useful %.1f cps, %d baud)\ntx: total %hu, errors %hu (useful %.1f cps, %d baud)\n", 
            RXTOTAL, RXERRORS, rc, res.brc,
            TXTOTAL, TXERRORS, tc, res.btc);
    msg ("remote reported DATA:\nrx: total %hu, errors %hu (useful %.1f cps, %d baud)\ntx: total %hu, errors %hu (useful %.1f cps, %d baud)\n", 
            PRX, PRXERR, rrc, res.rbrc,
            PTX, PTXERR, rtc, res.rbtc);
    return res;
}


#define PROBES   14

int main()
{
    t_line line;
    t_userdata ud;
    t_baud res[PROBES];
    int sess;
    uc fsize;

    init_line (&line, "/dev/ttyUSB0", &ud);
    set_interface_attribs (line.fd, B9600); // 9600 bps 8N1

    for (sess = 0; sess < PROBES; sess++) {
        fsize = (MAXFRAMESIZE - MINFRAMESIZE) * sess / (PROBES - 1) + MINFRAMESIZE;
        // expected cps around 300 for approx. 5 seconds
        res[sess] = data_session (&line, 300 * 5, fsize);
        sleep (5);
    }
    msg ("   frame   rx baud   tx baud  slave rx  slave tx\n");
    for (sess = 0; sess < PROBES; sess++) {
        msg ("%8hhu %9d %9d %9d %9d\n", 
                res[sess].fsize,
                res[sess].brc,
                res[sess].btc,
                res[sess].rbrc,
                res[sess].rbtc);
    }
    return 0;
}

