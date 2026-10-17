CXX = g++
CXXFLAGS = -std=c++11 -Wall -I include
TARGET = campus_connect
SERVER_TARGET = server

# Detect OS for libraries and cleanup commands
ifeq ($(OS),Windows_NT)
    SOCKET_LIBS = -lws2_32
    RM_TARGETS = del /f /q $(TARGET).exe $(SERVER_TARGET).exe 2>nul || rm -f $(TARGET) $(SERVER_TARGET)
    RM_OBJS = del /f /q *.o src\*.o 2>nul || rm -f *.o src/*.o
else
    SOCKET_LIBS = -lpthread
    RM_TARGETS = rm -f $(TARGET) $(SERVER_TARGET)
    RM_OBJS = rm -f *.o src/*.o
endif

COMMON_SRCS = src/student.cpp src/post.cpp src/HelpRequest.cpp src/HelpOffer.cpp src/Trie.cpp src/Group.cpp src/DataStore.cpp src/Graph.cpp src/JaccardMatching.cpp src/ProjectPost.cpp src/ProjectGroup.cpp src/TopKRanker.cpp
COMMON_OBJS = $(COMMON_SRCS:.cpp=.o)

SRCS = main.cpp $(COMMON_SRCS)
OBJS = main.o $(COMMON_OBJS)

SERVER_SRCS = server.cpp $(COMMON_SRCS)
SERVER_OBJS = server.o $(COMMON_OBJS)

.PHONY: all clean run run-server

all: $(TARGET) $(SERVER_TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $(SERVER_TARGET) $(SERVER_OBJS) $(SOCKET_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM_TARGETS)
	$(RM_OBJS)

run: $(TARGET)
	./$(TARGET)

run-server: $(SERVER_TARGET)
	./$(SERVER_TARGET)
