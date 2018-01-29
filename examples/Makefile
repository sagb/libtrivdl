
all: example-echo example-stream

example-echo: serial.c serial.h
	cd echo && make all

example-stream: serial.c serial.h
	cd stream && make all

clean:
	cd echo && make clean
	cd stream && make clean

