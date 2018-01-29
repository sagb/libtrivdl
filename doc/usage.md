
Files
-----
```
src/libtrivdl.c          the library for both MCU and PC
src/libtrivdl.h          API header
examples/                examples, see below
```

API
---

You would want to make yourself familiar with shortcuts 
and other definitions in [`libtrivdl.h`](../src/libtrivdl.h),
because they are used extensively throughout the code.

The 'line' (`struct t_line`) represents a single peer-to-peer connection.
It contains two 'frames', one is RX buffer, another is TX buffer.
Along with frames, `line` has a file descriptor associated with serial port in POSIX code,
flags for signalling asynchronous machine (`lflags`) and `void` pointer to
arbitrary user data associated with the connection.

Each frame (`struct t_frame`) represents a single 
datalink frame (see [protocol](protocol.md)), also serving
as RX or TX buffer.
It has the data (all bytes from 0xBA to the end), 
index of next RX/TX byte 
and transmission synchronization flags resembling modem's CTS/RTX.

In the core sits 'asynchronous machine', which transmits and receives
next byte of frame buffer when physical layer becomes ready.
In between these events, when both TX and RX are busy, it calls `cb_idle()`.
Each character is handled by `incoming_char()` or `outgoing_char()`.
When they finish receiving/transmitting the entire frame, `cb_frame_tx_done()`
and `cb_frame_rx_done()` are called.
The main synchronization mean for all these functions is READY flag
in `struct t_frame`, see [`libtrivdl.h`](../src/libtrivdl.h) for its description.

```
            library code           |    user code (callbacks)
                                   |
                                   |
                                   |   cb_frame_tx_done()
                                    /
                                   /
                   incoming_char() |
                 /                 |
  async_machine()  ------------------  cb_idle()
  or MCU ISRs                      |
  user code      \                 |
                   outgoing_char() |
                                   \
                                   |\
                                   |  cb_frame_rx_done()
```

Users must define three callback functions in their code: `cb_frame_tx_done`, 
`cb_frame_rx_done` and `cb_idle`. See [`libtrivdl.h`](../src/libtrivdl.h) for 
their prototypes.
Also, asyncronous machine itself is fully implemented for POSIX
but expected to be implemented by user as interrupt service routines (ISR)
for their MCU, see [stream](../examples/stream/msp430/stream.c) example 
for MSP430 implementation.

Along with core procedures, the following helper functions are provided:

* `init_frame`
* `init_line`
* `compute_checksum`
* `add_hdr_and_checksum`
* `build_frame`
* `strfr` (return frame as a string; only in POSIX version)
* `strfrret` (return callbacks' `status` argument as a string; only in POSIX version)

See [`libtrivdl.h`](../src/libtrivdl.h) for details.

Build
-----

libtrivdl provides code for both MCU and PC (aka 'POSIX' aka 'libc').
Moreover, core functions for both versions share the same C code.
Source for MCU must be compiled with `MCU` defined:
```
mspgcc -DMCU ...
or:
#define MCU
```
while POSIX version must be compiled without `MCU` defined.

Provided makefiles build the library and examples for both Linux PC and 
[TI MSP-EXP430G2 Launchpad](http://www.ti.com/tool/MSP-EXP430G2)
with MSP430G2553 MCU on board, hopefully as simple as:
```
apt-get install gcc-msp430 binutils-msp430 msp430-libc
cd libtrivdl
make
# upload 'echo' example to MCU
cd examples/echo/msp430 && make upload
# initiate example transmission
../posix/echo
```

For debug, enable `-DDEBUG` in makefiles
and additional callback functions in MCU code: `mcudebug1` and `mcudebug2`
([prototypes](../src/libtrivdl.h), [example](../examples/stream/msp430/stream.c)).
POSIX version will output debug information to stderr, 
while TI Launchpad will blink green (RX) and red (TX) LED.


Examples
--------

## echo

The single synchronous ping-pong.
See how compact the [code](../examples/echo/posix/echo.c) using libtrivdl can be.

## stream

PC (master) commands the MCU to start flooding with programmable-size
frames,
and starts its own flood in the opposite direction,
fully two-way synchronous.

After transferring or receiving the given amount of data, master
commands MCU to stop and stops itself.
MCU sends statistics (rx/tx/errors) to master.

The state machine is described is [stream.h](../examples/stream/stream.h).

This procedure runs multiple times using different frame sizes.
At the end statistics table is printed.

You can also try to run `echo` master vs `stream` slave and vice versa.

