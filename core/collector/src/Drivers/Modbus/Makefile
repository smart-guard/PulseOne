# =============================================================================
# PulseOne Modbus Driver Makefile
# =============================================================================

# 플랫폼 판별
UNAME_S := $(shell uname -s)
ifeq ($(CROSS_COMPILE_WINDOWS),1)
    TARGET_PLATFORM := Windows-Cross
else ifeq ($(OS),Windows_NT)
    TARGET_PLATFORM := Windows
else
    TARGET_PLATFORM := $(UNAME_S)
endif

# Default to Debug build for now
DEBUG ?= 1

# Project Paths
COLLECTOR_ROOT = ../../../
SHARED_ROOT = ../../../../shared
BIN_DIR = $(COLLECTOR_ROOT)bin
PLUGINS_DIR = $(BIN_DIR)/drivers

# Target
TARGET = $(PLUGINS_DIR)/libmodbus_driver.so

# Sources
SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

# Compiler & Flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -fPIC -fdiagnostics-color=always -DHAS_MODBUS=1
LDFLAGS = -shared -L$(SHARED_ROOT)/lib/$(TARGET_PLATFORM) -L/usr/local/lib

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

# Libraries
LIBS = -lmodbus -L$(SHARED_ROOT)/lib/$(TARGET_PLATFORM) -lpulseone-common -lpulseone-dblib -lpulseone-security

# Build Rules
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	@mkdir -p $(PLUGINS_DIR)
	@echo "🔨 Building Modbus Plugin: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)
