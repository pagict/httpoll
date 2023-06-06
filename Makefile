THIS_FILEPATH := $(abspath $(lastword $(MAKEFILE_LIST)))
export THIS_DIR := $(dir $(THIS_FILEPATH))
export LIBHTTPOLL := $(THIS_DIR)src/libhttpoll.a
export LDLIBS += -lasan -lstdc++ -lpthread -lfmt -lssl -lcrypto -lm
export CXXFLAGS += -I${THIS_DIR}src -g3 -O0 -fsanitize=address

all:
	make -C src
	make -C examples/

clean:
	make -C src clean
	make -C examples/ clean
