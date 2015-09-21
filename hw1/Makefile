TARGET	:= webserver
SRCS	:= webserver.cpp webserver_util.cpp ConcurrentQueue.cpp
LDLIBS	:= -lpthread
OBJS	:= $(SRCS:.cpp=.o)

all: $(OBJS)
	$(CXX) $^ -o $(TARGET) $(LDLIBS)
clean:
	$(RM) *.o
	$(RM) $(TARGET)

