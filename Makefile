
all:
	make -C examples/basic basic
	make -C examples/broadcast broadcast

clean:
	make -C src clean
	make -C examples/basic clean
	make -C examples/broadcast clean