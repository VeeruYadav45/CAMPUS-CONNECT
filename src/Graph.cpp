#include "Graph.h"
#include <algorithm>

void Graph::addStudent(int studentId) {
    adjacencyList[studentId];
}

void Graph::addConnection(int studentId1, int studentId2) {
    // A student cannot connect to themselves
    if (studentId1 == studentId2) {
        return;
    }

    addStudent(studentId1);
    addStudent(studentId2);

    // If they are already connected, do not add the connection again
    vector<int>& list1 = adjacencyList[studentId1];
    if (find(list1.begin(), list1.end(), studentId2) != list1.end()) {
        return;
    }

    // Undirected graph: add the connection in both directions
    list1.push_back(studentId2);
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

vector<int> Graph::getConnections(int studentId) const {
    auto it = adjacencyList.find(studentId);
    if (it != adjacencyList.end()) {
        return it->second;
    }
    return vector<int>();
}

// -------------------------------------------------------
// peopleYouMayKnow: BFS that also remembers each student's
// distance (number of steps) from the start student.
//   distance 0 = the student themselves
//   distance 1 = direct connections (already friends)
//   distance 2 = friends of friends  -> these are suggested
// We stop exploring after distance 2, because we never need
// students who are further away.
// Time: O(V + E) in the worst case.
// -------------------------------------------------------
vector<int> Graph::peopleYouMayKnow(int studentId) const {
    vector<int> suggestions;

    if (adjacencyList.find(studentId) == adjacencyList.end()) {
        return suggestions;   // unknown student -> no suggestions
    }

    unordered_map<int, int> distance;   // student ID -> steps from start
    queue<int> q;

    distance[studentId] = 0;
    q.push(studentId);

    while (!q.empty()) {
        int current = q.front();
        q.pop();

        if (distance[current] == 2) {
            suggestions.push_back(current);
            continue;   // don't go further than 2 steps
        }

        auto it = adjacencyList.find(current);
        if (it == adjacencyList.end()) {
            continue;
        }

        for (int neighbour : it->second) {
            if (distance.find(neighbour) == distance.end()) {
                distance[neighbour] = distance[current] + 1;
                q.push(neighbour);
            }
        }
    }

    return suggestions;
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
