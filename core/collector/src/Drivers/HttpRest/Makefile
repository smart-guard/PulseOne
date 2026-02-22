# =============================================================================
# PulseOne HttpRest Driver Makefile
# =============================================================================

DEBUG ?= 1

# Project Paths
COLLECTOR_ROOT = ../../../
SHARED_ROOT = ../../../../shared
BIN_DIR = $(COLLECTOR_ROOT)bin
PLUGINS_DIR = $(BIN_DIR)/drivers

TARGET = $(PLUGINS_DIR)/libhttp_rest_driver.so

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -fPIC -fdiagnostics-color=always
LDFLAGS = -shared -L$(SHARED_ROOT)/lib -L/usr/local/lib

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

LIBS = -lcurl -L$(SHARED_ROOT)/lib -lpulseone-common -lpulseone-dblib -lpulseone-security

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	@mkdir -p $(PLUGINS_DIR)
	@echo "🔨 Building HttpRest Plugin: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)
