# Host-only field plan tests (no ESP32).
#
#   make -C tools/field_plan test

CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O0 -g
ROOT := ../..
INCLUDES := -I$(ROOT)/firmware/src

.PHONY: all test clean

all: test

test_field_plan: test_field_plan.cpp $(ROOT)/firmware/src/core/FieldPlanCore.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ test_field_plan.cpp

test: test_field_plan
	./test_field_plan

clean:
	rm -f test_field_plan
