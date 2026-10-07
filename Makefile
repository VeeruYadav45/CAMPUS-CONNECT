CXX = g++
CXXFLAGS = -std=c++11 -Wall -I include
TARGET = campus_connect

SRCS = main.cpp src/student.cpp src/post.cpp src/HelpRequest.cpp src/HelpOffer.cpp src/Trie.cpp src/Group.cpp src/DataStore.cpp src/Graph.cpp src/JaccardMatching.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o src/*.o

run: all
	./$(TARGET)
