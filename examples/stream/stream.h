
// common header for posix and MCU versions

#ifndef STREAM_H
#define STREAM_H

#include <stdint.h>

// non-used byte + network STATS packet (opcode + counters)
typedef union {
    uc c[10];
    uint16_t i[5];
} t_ctr;

typedef struct {
    // ----- DATA type frames
    uint16_t tx, rx, txerr, rxerr;
    // ----- data below are not transferred between peers
    uc state;
    uint16_t peer_tx, peer_rx, peer_txerr, peer_rxerr;
    int maxchars;
    uc framesize;
} t_userdata;

typedef struct {
    uc fsize;
    int brc, btc, rbrc, rbtc;
} t_baud;

#define UD         ((t_userdata*)(line->userdata))->
#define TXTOTAL    UD tx
#define RXTOTAL    UD rx
#define TXERRORS   UD txerr
#define RXERRORS   UD rxerr
#define PTX        UD peer_tx
#define PRX        UD peer_rx
#define PTXERR     UD peer_txerr
#define PRXERR     UD peer_rxerr
#define STATE      UD state
#define MAXCHARS   UD maxchars
#define FSIZE      UD framesize

// opcodes (first char of message) used in this example
#define OP_STREAM_START 0x12
#define OP_STREAM_STOP  0x13
#define OP_STREAM_STATS 0x14
#define OP_STREAM_DATA  0x15

// states                       // PC                           MCU
#define ST_silence      0x00    //                              waits for START
#define ST_start        0x10    // sent START, waits for DATA   rcv START, trigger first tx
#define ST_async_data   0x20    //    first DATA (or ECHO) received, and continues
#define ST_stop         0x30    // sent STOP, waits for STATS   rcvd STOP, finishes frame
#define ST_stopped      0x40    //                              frame finished, send STATS
#define ST_stats        0x50    // received STATS, finishes     not used: sent STATS, enters silence

#endif
