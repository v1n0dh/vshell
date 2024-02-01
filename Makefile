SRCDIR = src
HEADERS = include
BINDIR = bin

SRCS = $(wildcard $(SRCDIR)/*.cpp)
OBJS = $(patsubst $(SRCDIR)/%.cpp, $(BINDIR)/%.o, $(SRCS))

CXX = g++
CXXFLAGS = -Wall -Wextra -g -std=c++23

TARGET = $(BINDIR)/vsh

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BINDIR)/%.o: $(SRCDIR)/%.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -c $< -I $(HEADERS) -o $@

$(BINDIR):
	mkdir $(BINDIR)

clean:
	rm -rf $(BINDIR)

.PHONY: all clean
