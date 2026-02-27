# =============================================================================
# PulseOne OPCUA Driver Makefile
# =============================================================================

DEBUG ?= 1

# Project Paths
COLLECTOR_ROOT = ../../../
SHARED_ROOT = ../../../../shared
BIN_DIR = $(COLLECTOR_ROOT)bin
PLUGINS_DIR = $(BIN_DIR)/drivers

SHARED_INCLUDE_DIR = $(SHARED_DIR)/include
UNAME_S := $(shell uname -s)nifeq ($(CROSS_COMPILE_WINDOWS),1)n    TARGET_PLATFORM := Windows-Crossnelse ifeq ($(OS),Windows_NT)n    TARGET_PLATFORM := Windowsnelsen    TARGET_PLATFORM := $(UNAME_S)nendifnSHARED_LIB_DIR = $(SHARED_DIR)/lib

TARGET = $(PLUGINS_DIR)/libopcua_driver.so

SRCS_CPP = $(wildcard *.cpp)
SRCS_C = $(wildcard *.c)
SRCS = $(SRCS_CPP) $(SRCS_C)

CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -Wall -Wextra -fPIC -fdiagnostics-color=always
CFLAGS = -Wall -Wextra -fPIC -fdiagnostics-color=always
LDFLAGS = -shared -L$(SHARED_LIB_DIR) -L/usr/local/lib

ifeq ($(DEBUG),1)
    CXXFLAGS += -DDEBUG -g -O0
    CFLAGS += -DDEBUG -g -O0
else
    CXXFLAGS += -DNDEBUG -O3
    CFLAGS += -DNDEBUG -O3
endif

INCLUDES = -I$(COLLECTOR_ROOT)include \
           -I$(COLLECTOR_ROOT)src \
           -I$(SHARED_ROOT)/include \
           -I$(SHARED_ROOT)/modules/DbLib/include \
           -I$(SHARED_ROOT)/modules/LogLib/include \
           -I$(COLLECTOR_ROOT)include/Drivers/OPCUA \
           -I/usr/local/include

LIBS = -lopen62541 -L$(SHARED_ROOT)/lib -lpulseone-common -lpulseone-dblib -lpulseone-security -lpthread

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	@mkdir -p $(PLUGINS_DIR)
	@echo "🔨 Building OPCUA Plugin: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRCS_CPP) -c
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS_C) -c
	$(CXX) $(LDFLAGS) *.o -o $@ $(LIBS)
	rm -f *.o

clean:
	rm -f $(TARGET)
