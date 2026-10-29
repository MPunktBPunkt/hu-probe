# Host-only C1a BCL framer tests (no ESP32 / TinyUSB / Arduino).
#
# Usage:
#   make -C tools/bcl_c1a test
#   make -C tools/bcl_c1a scenario

CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O0 -g
ROOT := ../..
INCLUDES := -I$(ROOT)/firmware/src
FRAMER_SRC := $(ROOT)/firmware/src/connected/bcl/BclFramer.cpp

.PHONY: all test scenario clean

all: test scenario

test_bcl_framer: test_bcl_framer.cpp $(FRAMER_SRC) \
		$(ROOT)/firmware/src/connected/bcl/BclFramer.h \
		$(ROOT)/firmware/src/connected/bcl/BclTypes.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ test_bcl_framer.cpp $(FRAMER_SRC)

scenario_ca_c1_lab_bcl: scenario_ca_c1_lab_bcl.cpp $(FRAMER_SRC) \
		$(ROOT)/firmware/src/connected/bcl/BclFramer.h \
		$(ROOT)/firmware/src/connected/bcl/BclTypes.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ scenario_ca_c1_lab_bcl.cpp $(FRAMER_SRC)

test: test_bcl_framer
	./test_bcl_framer

scenario: scenario_ca_c1_lab_bcl
	./scenario_ca_c1_lab_bcl

clean:
	rm -f test_bcl_framer scenario_ca_c1_lab_bcl
