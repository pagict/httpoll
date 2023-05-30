TESTLIB = -lgtest_main -lgtest -lpthread

CXXFLAGS = -std=c++17 -g3 -O0

LIBS = -lfmt

COMMON_FILES = http_method.cc http_version.cc

TESTFILES = http_header_parser_test.cc

test:  $(TESTFILES) http_header_parser.cc http_header_parser.h $(COMMON_FILES)
	$(CXX)  $^ $(CXXFLAGS) $(TESTLIB)  $(LIBS) -o $@


clean: 
	rm -f test *.o