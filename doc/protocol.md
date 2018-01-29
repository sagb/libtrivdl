Overview
--------

The protocol is peer-to-peer data link designed for serial lines,
like SLIP or AX.25, but somewhat simpler, suitable for cheap low-power MCU.

It features:

* full asynchronous operation
* frame synchronization (byte stuffing type)
* error detection with CRC
* frames of variable size

However, it does not provide:

* media access control, multiplexing (designed for physical layer like RS232)
* addressing, switching (peer-to-peer only)
* scheduling (single-frame buffer used for transmission)
* flow control (relies on physical layer)
* acknowledgment of frame reception/acceptance, retransmission, error correction


Frame structure
---------------
```
       0xBA data data ... 0xBA data data 0xBABA data ... 0xBA data ...
                          ^^^^           ^^^^^^
               frame delimiter           inline 0xBA
```
Protocol operates on 8-bit characters (bytes), represented by RS232 UART.
Bytes are grouped in frames, delimited by the 0xBA byte.
If the 0xBA byte occurs in the data to be sent,
the two byte sequence 0xBABA is sent instead.
I.e., wire transfer includes extra 0xBA (but note that frame 
in RAM, i.e. `struct t_frame`, does not).

*Caveat*: first byte of message (called 'opcode' in examples) can't be 0xBA.

Each frame begins with a delimiter, then a single byte, representing the index 
of last byte (i.e. frame size minus 1), then 'message' (arbitrary data,
but see caveat above), and ends with a single byte of checksum.
```
   |   0xBA    |  lastndx    | message[0] | message [1] ... |     CRC   |
   +-----------+-------------+------------+-------------   -+-----------+
   |     0     |      1      |      2     |       3     ...     lastndx |

                ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                                                   protected 
                                                   by CRC
```
CRC is unsigned 8-bit sum of all bytes excluding first and last one.


Operation
---------

Transmitter and receiver start and stop sending/listening frames asynchronously at arbitrary time.

No acknowledgements, retransmissions or error correction are provided. However, receiver is able to recover from invalid frames and continue to attempt frame synchronization.

