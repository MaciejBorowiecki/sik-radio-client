CXX      = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2
LDFLAGS  = -lssl -lcrypto

OBJS   = CircularBuffer.o common.o err.o SikClient.o sik-radio-client.o
TARGET = sikradio

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
