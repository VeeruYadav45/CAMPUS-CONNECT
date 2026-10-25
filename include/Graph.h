#ifndef GRAPH_H
#define GRAPH_H

#include <unordered_map>
#include <vector>
#include <queue>
#include <unordered_set>
#include <iostream>

using namespace std;

class Graph {
private:
    // Student ID -> connected student IDs
    unordered_map<int, vector<int>> adjacencyList;

public:
    void clearAll() { adjacencyList.clear(); }

    void addStudent(int studentId);

    void addConnection(int studentId1, int studentId2);

    vector<int> bfs(int startStudentId) const;

    vector<int> getConnections(int studentId) const;

    // "People you may know": students exactly 2 steps away
    // (friends of friends who are not already your friends)
    vector<int> peopleYouMayKnow(int studentId) const;

    void display() const;
};

#endif
