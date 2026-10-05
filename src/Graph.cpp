#include "Graph.h"

void Graph::addStudent(int studentId) {
    adjacencyList[studentId];
}

void Graph::addConnection(int studentId1, int studentId2) {
    addStudent(studentId1);
    addStudent(studentId2);

    adjacencyList[studentId1].push_back(studentId2);
    adjacencyList[studentId2].push_back(studentId1);
}

vector<int> Graph::bfs(int startStudentId) const {
    vector<int> result;

    if (adjacencyList.find(startStudentId) == adjacencyList.end()) {
        return result;
    }

    queue<int> q;
    unordered_set<int> visited;

    q.push(startStudentId);
    visited.insert(startStudentId);

    while (!q.empty()) {
        int current = q.front();
        q.pop();

        result.push_back(current);

        auto it = adjacencyList.find(current);

        if (it != adjacencyList.end()) {
            for (int neighbour : it->second) {
                if (visited.find(neighbour) == visited.end()) {
                    visited.insert(neighbour);
                    q.push(neighbour);
                }
            }
        }
    }

    return result;
}

void Graph::display() const {
    for (const auto& pair : adjacencyList) {
        cout << pair.first << " -> ";

        for (int neighbour : pair.second) {
            cout << neighbour << " ";
        }

        cout << endl;
    }
}
