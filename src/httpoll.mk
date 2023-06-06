CXXFLAGS += -std=c++17

CXXFLAGS += -g3 -O0 -fno-omit-frame-pointer

LIBSRC := $(wildcard ./*.cc)
FILTEROUT := $(wildcard ./*test.cc)
LIBSRC := $(filter-out $(FILTEROUT), $(LIBSRC))
LIBOBJS := $(patsubst %.cc, %.o, $(LIBSRC))

CXXFLAGS += -DHPL_ENABLE_PING_PONG=10 -DSMALL_MEMORY=1024
