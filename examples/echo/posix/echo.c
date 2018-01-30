/*
 * libtrivdl example: echo, POSIX version.
 *
 * Copyright 2018 Alexander Kulak.
 * This file is licensed under the MIT license.
 * See the LICENSE file in the project root for more information.
 */

#include "serial.h"
#include "libtrivdl.h"

typedef struct {
    char sent;
} t_userdata;

#define SENT    ((t_userdata*)(line->userdata))->sent

void cb_frame_rx_done (uc status, t_line* line)
{
    msg ("frame received (%s, msg 0x%hhx)\n", 
            strfrret(status), LRMSG);
    LFLAGS |= EXIT_A_M; // stop async machine
}

void cb_frame_tx_done (uc status, t_line* line)
{
    msg ("frame transmitted (%s, msg 0x%hhx)\n", 
            strfrret(status), LWMSG);
}

float cb_idle (t_line* line)
{
    if (SENT) {
        err ("no reply within timeout\n");
        LFLAGS |= EXIT_A_M; // stop async machine
    } else {
        build_frame (LWFR, "\x10payload", 9); // \x10=ping
        //wrn("sending: %s\n", strfr(LWFR));
        LWFLAGS |= READY; // async machine starts transmitting
        SENT = 1;
    }
    return 0.5;  // recall in 0.5 sec if there is no I/O
}

int main()
{
    t_line line;
    t_userdata userdata;

    init_line (&line, "/dev/ttyUSB0", &userdata);
    set_interface_attribs (line.fd, B9600); // 9600 bps 8N1
    userdata.sent = 0;
    return async_machine (&line);
}

