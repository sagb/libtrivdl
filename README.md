[![chat on freenode](https://img.shields.io/badge/chat-on%20freenode-brightgreen.svg)](https://webchat.freenode.net/?channels=%23libtrivdl)

# Trivial Data Link Layer

This is library for simple data link, first OSI layer above physical.
It's designed for synchronous and asynchronous serial communication between PC
and microcontroller hardware UART (tested with MSP430).
Data is transmitted in [sized frames with CRC](doc/protocol.md),
and [API](doc/usage.md) features callbacks for asynchronous machine.

## Documentation

([1](doc/protocol.md)) Protocol description

([2](doc/usage.md)) API description, usage hints

([3](examples/)) Examples of synchronous and asynchronous data exchange 
(see [usage](doc/usage.md) for details)  
   
   
Feel free to [contact](https://webchat.freenode.net/?channels=%23libtrivdl) me in IRC.  
   
   
libtrivdl (C) Alexander Kulak &lt;sa-dev AT rainbow POINT by&gt; 2018

