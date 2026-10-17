CXX = g++
CXXFLAGS = -std=c++11 -Wall -I include
TARGET = campus_connect

SRCS = main.cpp src/student.cpp src/post.cpp src/HelpRequest.cpp src/HelpOffer.cpp src/Trie.cpp src/Group.cpp src/DataStore.cpp src/Graph.cpp src/JaccardMatching.cpp src/ProjectPost.cpp src/ProjectGroup.cpp src/TopKRanker.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

SERVER_TARGET = server
SERVER_SRCS = server.cpp src/student.cpp src/post.cpp src/HelpRequest.cpp src/HelpOffer.cpp src/Trie.cpp src/Group.cpp src/DataStore.cpp src/Graph.cpp src/JaccardMatching.cpp src/ProjectPost.cpp src/ProjectGroup.cpp src/TopKRanker.cpp
SERVER_OBJS = $(SERVER_SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $(SERVER_TARGET) $(SERVER_OBJS) -lws2_32 -lpthread

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	del /f /q $(TARGET).exe $(SERVER_TARGET).exe 2>nul || rm -f $(TARGET) $(SERVER_TARGET)
	del /f /q *.o src\*.o 2>nul || rm -f *.o src/*.o

run: all
	./$(TARGET)

run-server: $(SERVER_TARGET)
	./$(SERVER_TARGET)
