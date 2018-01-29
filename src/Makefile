
#CFLAGS += -DDEBUG -g

all: libtrivdl-libc.o libtrivdl-msp430.o

libtrivdl-libc.o: libtrivdl.c libtrivdl.h
	${CC} ${CFLAGS} -c libtrivdl.c -o libtrivdl-libc.o

libtrivdl-msp430.o: libtrivdl.c libtrivdl.h
	msp430-gcc -mmcu=msp430g2553 -O2 -Wall ${CFLAGS} -DMCU -c -o libtrivdl-msp430.o libtrivdl.c
	#msp430-gcc -mmcu=msp430g2553 -O0 -Wall ${CFLAGS} -DMCU -c -o libtrivdl-msp430.o libtrivdl.c

clean:
	rm -f libtrivdl-libc.o libtrivdl-msp430.o

