# =============================================================================
# PulseOne BACnet Driver Makefile
# =============================================================================

DEBUG ?= 1

# Project Paths
COLLECTOR_ROOT = ../../../
SHARED_ROOT = ../../../../shared
BIN_DIR = $(COLLECTOR_ROOT)bin
PLUGINS_DIR = $(BIN_DIR)/drivers

SHARED_INCLUDE_DIR = $(SHARED_DIR)/include
UNAME_S := $(shell uname -s)nifeq ($(CROSS_COMPILE_WINDOWS),1)n    TARGET_PLATFORM := Windows-Crossnelse ifeq ($(OS),Windows_NT)n    TARGET_PLATFORM := Windowsnelsen    TARGET_PLATFORM := $(UNAME_S)nendifnSHARED_LIB_DIR = $(SHARED_DIR)/lib

TARGET = $(PLUGINS_DIR)/libbacnet_driver.so

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -fPIC -fdiagnostics-color=always
# Fix for BACnet stack headers
CXXFLAGS += -DHAS_BACNET=1 -DHAS_BACNET_STACK=1
LDFLAGS = -shared -L$(SHARED_LIB_DIR) -L/usr/local/lib

ifeq ($(DEBUG),1)
    CXXFLAGS += -DDEBUG -g -O0
else
    CXXFLAGS += -DNDEBUG -O3
endif

INCLUDES = -I$(COLLECTOR_ROOT)include \
           -I$(COLLECTOR_ROOT)src \
           -I$(SHARED_ROOT)/include \
           -I$(SHARED_ROOT)/modules/DbLib/include \
           -I$(SHARED_ROOT)/modules/LogLib/include \
           -I/usr/local/include

LIBS = -lbacnet -L$(SHARED_ROOT)/lib -lpulseone-common -lpulseone-dblib -lpulseone-security

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	@mkdir -p $(PLUGINS_DIR)
	@echo "🔨 Building BACnet Plugin: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)
