mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(dir $(mkfile_path))

CXXFLAGS += -I$(mkfile_dir) -std=c++17
LDLIBS += -lstdc++ -lpthread -lfmt -lssl -lcrypto

CXXFLAGS += -g3 -O0 -fno-omit-frame-pointer

LIBSRC := $(wildcard $(mkfile_dir)/*.cc)
FILTEROUT := $(wildcard $(mkfile_dir)/*test.cc)
LIBSRC := $(filter-out $(FILTEROUT), $(LIBSRC))
LIBOBJS := $(patsubst %.cc, %.o, $(LIBSRC))

CXXFLAGS += -DHPL_ENABLE_PING_PONG=10 -DSMALL_MEMORY=1024

# CC=clang
# CXX=clang++
all: $(LIBOBJS)

clean:
	rm -f basic *.o ../src/*.o