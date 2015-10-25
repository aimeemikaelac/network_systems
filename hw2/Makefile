CLIENT		:= dfc
SERVER		:= dfs
CLIENT_SRC	:= DFSClient.cpp
SERVER_SRC	:= DFSServer.cpp
SHARED		:= util.cpp ConcurrentQueue.cpp
CXXFLAGS	:= -g
LDLIBS		:= -lpthread -lssl -lcrypto
SHARED_OBJS	:= $(SHARED:.cpp=.o)
CLIENT_OBJS	:= $(CLIENT_SRC:.cpp=.o)
SERVER_OBJS	:= $(SERVER_SRC:.cpp=.o)

all: client server

client: $(SHARED_OBJS) $(CLIENT_OBJS)
	$(CXX) $^ -o $(CLIENT) $(CXXFLAGS) $(LDLIBS)

server: $(SHARED_OBJS) $(SERVER_OBJS)
	$(CXX) $^ -o $(SERVER) $(CXXFLAGS) $(LDLIBS)

clean:
	$(RM) *.o
	$(RM) $(CLIENT)
	$(RM) $(SERVER)
