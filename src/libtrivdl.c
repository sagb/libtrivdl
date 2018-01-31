/*
 * libtrivdl library implementation.
 *
 * Copyright 2018 Alexander Kulak.
 * This file is licensed under the MIT license.
 * See the LICENSE file in the project root for more information.
 */

#include "libtrivdl.h"

void init_frame (t_frame* fr)
{
    fr->next=0;
    fr->flags=0;
}


int init_line (t_line* line, char* portname, void* userdata)
{
#ifndef MCU
    line->fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (line->fd < 0) {
        err("error opening %s: %s\n", portname, strerror(errno));
        return 0;
    }
#endif
    line->lflags = 0;
    line->userdata = userdata;
    init_frame (&(line->rfr));
    init_frame (&(line->wfr));
    return 1;
}


uc compute_checksum (t_frame* fr)
{
    volatile uc pc, cs;
    cs=0;
    for (pc=LASTNDX; pc<FRLAST; pc++) {
        cs+=DATA[pc];
    }
    return cs;
}


void add_hdr_and_checksum (t_frame* fr)
{
    DATA[SIGNATURE] = FRAMEDELIMITER;
    DATA[FRLAST] = compute_checksum (fr);
}


t_frame* build_frame (t_frame* fr, uc* src, uc size)
{
    uc c, srci, dsti, cs;
    init_frame (fr);
    (fr->data)[SIGNATURE] = FRAMEDELIMITER;
    cs = 0;
    dsti = MESSAGE;
    for (srci=0; srci<size; srci++) {
        c = *(src+srci);
        if (dsti > (MAXFRAMESIZE-2))
            return NULL; // overflow
        (fr->data)[dsti] = c;
        cs += c;
        dsti++;
    }
    (fr->data)[LASTNDX] = dsti;
    cs += dsti;
    (fr->data)[dsti] = cs;
    return fr;
}


#ifdef MCU
void incoming_char (uc c)
#else
void incoming_char (t_line* line, uc c)
#endif
{
    // TODO: first test for delimiter/double delimiter, to allow 0xBA as opcode
    // TODO: separate header and footer from frame.data
    // TODO: two-byte delimiter, as in SLIP.
    //
    t_frame* rfr = &(line->rfr); // TODO: get rid of this
    uc checksum;
    if (RNEXT == SIGNATURE) {
        if (c == FRAMEDELIMITER) {
            RDATA[RNEXT] = c;
            RNEXT++;
            //wrn("perhaps new frame\n");
            return;
        } else {
            wrn("garbage: 0x%hhx\n", c);
            return;
        }
    }

    else if (RNEXT == LASTNDX) {
       if (c == FRAMEDELIMITER) {
            // TODO!
            wrn("bug: frame size == frame delimiter, can't cope with this, assuming garbage\n");
            RNEXT=SIGNATURE;
            return;
        } else {
            //wrn("checksum at position 0x%hhx\n", c);
            if (c >= MAXFRAMESIZE) {
                err("invalid checksum position, frame skipped\n");
                rfr->flags |= READY;
                X_DONE(cb_frame_rx_done, FRBADFMT);
                RNEXT=SIGNATURE;
                return;
            } else {
                RDATA[RNEXT] = c;
                RNEXT++;
                return;
            }
        }
    } // checksum position

    else if (RNEXT == MESSAGE) {
        RDATA[RNEXT] = c;
        RNEXT++;
        return;
    } // message[0]

    else if (RNEXT > MESSAGE) {
        if (RNEXT >= MAXFRAMESIZE) {
            err("incoming frame buffer overrun\n");
            rfr->flags |= READY;
            X_DONE(cb_frame_rx_done, FRTOOLONG);
            RNEXT = SIGNATURE;
            return;
        }
        else if (RNEXT == RFRLAST) {
            // last char, the checksum
            RDATA[RNEXT] = c;
            RNEXT++;
            checksum = compute_checksum (rfr);
            if (checksum != c) {
                err("checksum in frame (0x%hhx) doesn't match calculated (0x%hhx), frame skipped\n", c, checksum);
                rfr->flags |= READY;
                X_DONE(cb_frame_rx_done, FRBADSUM);
                RNEXT = SIGNATURE;
                return;
            } else {
                // transfer frame ownership to user code
                rfr->flags |= READY;
                X_DONE(cb_frame_rx_done, FROK);
                RNEXT = SIGNATURE;
                return;
            }
        } else {
            // msg[1+], not last
            if (c == FRAMEDELIMITER) {
                // BA!
                if (rfr->flags & HFDFL) {
                    //wrn("skipping extra 0x%hhX\n", FRAMEDELIMITER);
                    rfr->flags &= ~HFDFL;
                    return;
                } else {
                    RDATA[RNEXT] = c;
                    RNEXT++;
                    rfr->flags |= HFDFL;
                    return;
                }
            } else {
                if (rfr->flags & HFDFL) {
                    wrn("single 0x%hhX in the middle of frame, resetting frame\n", FRAMEDELIMITER);
                    rfr->flags |= READY;
                    X_DONE(cb_frame_rx_done, FRBADFMT);
                    RDATA[SIGNATURE] = FRAMEDELIMITER;
                    RDATA[SIGNATURE+1] = c;
                    RNEXT = SIGNATURE+2;
                    RFLAGS &= ~HFDFL;
                    return;
                } else {
                    // plain msg[1+]
                    RDATA[RNEXT] = c;
                    RNEXT++;
                    return;
                }
            }
        } // msg[1+]
    } // msg[1+] or checksum

} // incoming_char


#ifdef MCU
uc outgoing_char ()
#else
uc outgoing_char (t_line* line)
#endif
{
    if (LWNEXT != SIGNATURE && LWDATA[LWNEXT] == FRAMEDELIMITER) {
        if (LWFLAGS & HFDFL) {
            // half delimiter already sent, reset flag
            LWFLAGS &= ~HFDFL;
        } else {
            // sending half delimiter
            LWFLAGS |= HFDFL;
            return FRAMEDELIMITER;
        }
    }
    return LWDATA[LWNEXT++];
} // outgoing_char


#ifndef MCU
int async_machine (t_line* line)
{
    uc c;
    fd_set rfds, wfds;
    struct timeval tv;
    int selret;
    int rdlen, wrlen;
    bool exitrq;
    // before first cb_idle(), select() will return 
    // immediately if no IO available
    float timeout = 0;
    DEFINE_FRAME_VIA_LINE

    do {
        FD_ZERO (&rfds);
        FD_ZERO (&wfds);
        if (! (RFLAGS & READY)) {
            FD_SET (LFD, &rfds);
        }
        if (WFLAGS & READY) {
            FD_SET (LFD, &wfds);
        }
        tv.tv_sec = (int)timeout;
        tv.tv_usec = (timeout-tv.tv_sec)*1e6;
        selret = select (
                LFD+1, 
                (RFLAGS & READY) ? NULL : &rfds, 
                (WFLAGS & READY) ? &wfds : NULL, 
                NULL, &tv);
        //wrn("select ret %d\n", selret);

        if (selret == -1) {
            perror("select()");
            return errno;
        }

        else if (selret) {
            //wrn("select: some fd available, rxset=%d txset=%d\n", FD_ISSET(fd,&rfds), FD_ISSET(fd,&wfds));

            if ((!(RFLAGS & READY)) && FD_ISSET (LFD, &rfds)) {
                //wrn("select: rx\n");
                rdlen = read (LFD, &c, 1);
                if (rdlen > 0) {
                    incoming_char (line, c);  // generally, drop c in RDATA[NEXT++]
                } else if (rdlen < 0) {
                    perror("should not happen - select() mistake? read()");
                    return errno;
                }
            }

            if ((WFLAGS & READY) && (WNEXT <= WFRLAST) && FD_ISSET (LFD, &wfds)) {
                //wrn("select: tx pos %d (fr ptr %p, 0x%hhx)\n", WNEXT, wfr, WDATA[WNEXT]);
                c = outgoing_char (line);  // generally, WDATA[WNEXT++]
                //wrn("write 0x%hhx\n", c);
                wrlen = write (LFD, &c, 1);
                tcdrain (LFD);   // delay for output
                if (wrlen != 1) {
                    perror("should not happen - select() mistake? write()");
                    return errno;
                }
                if (WNEXT > WFRLAST) {
                    // frame transmitted
                    //WNEXT = SIGNATURE; // let sender rewind
                    WFLAGS &= ~READY;
                    X_DONE(cb_frame_tx_done, FROK);
                }
            }

        }

        else {
            //wrn("select: no data within timeout\n");
            timeout = cb_idle (line); // to use in next select()
        }

        exitrq = (line->lflags) & EXIT_A_M;
        line->lflags &= ~EXIT_A_M;
    } while (!exitrq);

    return 0;
}


/*
int write_frame (int fd, t_frame* fr)
{
    int wlen = write (fd, DATA, FRSIZE);
    tcdrain(fd);    // delay for output 
    if (wlen != FRSIZE) {
        err("error from write: %d, %s\n", wlen, strerror(errno));
        return 1;
    }
    wrn("%d chars written\n", FRSIZE);
    return 0;
}
*/


char res[5*MAXFRAMESIZE+1];

char* strfr (t_frame* fr)
{
    char inc[7];
    int p;
    res[0]='\0';
    for (p=0; p<=FRLAST; p++) {
        snprintf (inc, 6, " 0x%hhx", DATA[p]);
        inc[5]='\0';
        if ((strlen(res)+strlen(inc)) < (5*MAXFRAMESIZE+1))
            strcat (res, inc);
        else {
            err("overflow in frame_as_string()");
            return NULL;
        }
    }
    return res;
}


char* strfrret (uc status){

switch (status) {
    case FROK: return "OK";
    case FRBADFMT: return "malformed";
    case FRBADSUM: return "checksum mismatch";
    case FRTOOLONG: return "too long frame";
}
return NULL;
}


// version for libc
#endif

