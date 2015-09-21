###
# Makefile
#
# Author: Michael Coughlin
# ECEN 5023: Network Systems Hw 1
# Sept. 20, 2015
###
TARGET	:= webserver
SRCS	:= webserver.cpp webserver_util.cpp ConcurrentQueue.cpp
LDLIBS	:= -lpthread
OBJS	:= $(SRCS:.cpp=.o)

all: $(OBJS)
	$(CXX) $^ -o $(TARGET) $(LDLIBS)
clean:
	$(RM) *.o
	$(RM) $(TARGET)

