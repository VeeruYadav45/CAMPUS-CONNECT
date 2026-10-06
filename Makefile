CXX = g++
CXXFLAGS = -std=c++11 -Wall -I include
TARGET = campus_connect

SRCS = main.cpp src/student.cpp src/post.cpp src/Group.cpp src/DataStore.cpp src/Graph.cpp src/JaccardMatching.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	del /f /q $(TARGET).exe 2>nul || rm -f $(TARGET)
	del /f /q *.o src\*.o 2>nul || rm -f *.o src/*.o

run: all
	./$(TARGET)
