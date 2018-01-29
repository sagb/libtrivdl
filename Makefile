
all: srclib examples-all tg

srclib:
	cd src && make all

examples-all: srclib
	cd examples && make all

clean:
	cd src && make clean
	cd examples && make clean

tg:
	ctags -R src examples

u: examples-all
	#cd examples/echo/msp430 && make upload
	cd examples/stream/msp430 && make upload


