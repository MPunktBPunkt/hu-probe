# Host-only SessionMetrics / Timeline tests (no ESP32).
#
#   make -C tools/session_metrics test

CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O0 -g
ROOT := ../..
INCLUDES := -I$(ROOT)/firmware/src

.PHONY: all test clean

all: test

test_session_metrics: test_session_metrics.cpp \
		$(ROOT)/firmware/src/session/SessionMetrics.h \
		$(ROOT)/firmware/src/session/UsbSessionTimeline.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ test_session_metrics.cpp

test: test_session_metrics
	./test_session_metrics

clean:
	rm -f test_session_metrics
