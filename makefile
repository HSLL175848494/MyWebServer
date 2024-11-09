CXX ?= gcc
LEVEL ?=2

ifeq ($(LEVEL), 0)
    CXXFLAGS += -g
else ifeq ($(LEVEL), 1)
    CXXFLAGS += -O1make
else ifeq ($(LEVEL), 2)
    CXXFLAGS += -O2
else ifeq ($(LEVEL), 3)
    CXXFLAGS += -O3
endif

target=server
src=$(wildcard Log/Log.cpp SqlPool/SqlPool.cpp HttpParse/Utils.cpp HttpParse/HttpParse.cpp WebServer.cpp main.cpp)

$(target): $(src)

	$(CXX) $(CXXFLAGS) $^ -o $@ -lpthread -lmysqlclient -std=c++20

.PHONY:clean

clean:
	rm  -r server