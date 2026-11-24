#ifndef GRAPH_H
#define GRAPH_H

#include <unordered_map>
#include <vector>
#include <queue>
#include <unordered_set>
#include <iostream>

using namespace std;

// One "people you may know" suggestion: a student 2 steps away,
// plus the mutual friends (distance-1 students) that link to them.
struct FriendSuggestion {
    int studentId;
    vector<int> mutualIds;   // sorted ascending
};

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

    // Same students as peopleYouMayKnow, but each one comes with the
    // mutual friends that connect you, ranked by most mutual friends
    // first (ties: smaller ID first). Common-neighbours link prediction.
    vector<FriendSuggestion> suggestFriends(int studentId) const;

    void display() const;
};

#endif
