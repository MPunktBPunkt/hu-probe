# Host-only B1 usb_gadget / sticky precedence tests (no ESP32).
#
#   make -C tools/gadget_nvs test

CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O0 -g
ROOT := ../..
INCLUDES := -I$(ROOT)/firmware/src

.PHONY: all test clean

all: test

test_gadget_mode: test_gadget_mode.cpp $(ROOT)/firmware/src/core/GadgetMode.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ test_gadget_mode.cpp

test: test_gadget_mode
	./test_gadget_mode

clean:
	rm -f test_gadget_mode
