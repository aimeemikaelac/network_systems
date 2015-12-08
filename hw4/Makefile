###
# Makefile
#
# Author: Michael Coughlin
# ECEN 5023: Network Systems Hw 1
# Sept. 20, 2015
###
TARGET		:= transparentproxy
SRCS		:= TransparentProxy.cpp ConcurrentQueue.cpp
CXXFLAGS	:= -std=c++11
LDLIBS		:= -lpthread
OBJS		:= $(SRCS:.cpp=.o)

all: $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $(TARGET) $(LDLIBS)
clean:
	$(RM) *.o
	$(RM) $(TARGET)

