CXX = g++
CXXFLAGS = -std=c++11 -Wall -I include

# Code shared by every program
CORE_SRCS = src/student.cpp src/post.cpp src/Group.cpp src/DataStore.cpp src/Graph.cpp \
            src/JaccardMatching.cpp src/ProjectPost.cpp src/ProjectGroup.cpp \
            src/TopKRanker.cpp src/Message.cpp src/trie1.cpp
CORE_OBJS = $(CORE_SRCS:.cpp=.o)

TARGET = campus_connect
SERVER_TARGET = server
TEST_TARGET = test_dsa
BENCH_TARGET = benchmark

# Platform differences: Windows (MinGW) needs Winsock for httplib,
# Linux/macOS need pthreads; cleanup command differs too.
ifeq ($(OS),Windows_NT)
    SERVER_LIBS = -lws2_32
    RM = del /f /q
    EXE = .exe
    fixpath = $(subst /,\,$1)
else
    SERVER_LIBS = -pthread
    RM = rm -f
    EXE =
    fixpath = $1
endif

.PHONY: all server test bench clean run run-server

all: $(TARGET)

$(TARGET): main.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

server: $(SERVER_TARGET)

$(SERVER_TARGET): server.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(SERVER_LIBS)

# Unit tests for the DSA core
test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): tests/test_dsa.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Performance benchmark (optimised build). Table goes to tests/results.txt
bench: $(BENCH_TARGET)
	./$(BENCH_TARGET) > $(if $(EXE),nul,/dev/null) 2> tests/results.txt
	@echo Results written to tests/results.txt

$(BENCH_TARGET): tests/benchmark.cpp $(CORE_SRCS)
	$(CXX) $(CXXFLAGS) -O2 -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	-$(RM) $(TARGET)$(EXE) $(SERVER_TARGET)$(EXE) $(TEST_TARGET)$(EXE) $(BENCH_TARGET)$(EXE)
	-$(RM) $(call fixpath,*.o src/*.o tests/*.o)

run: all
	./$(TARGET)

run-server: $(SERVER_TARGET)
	./$(SERVER_TARGET)
