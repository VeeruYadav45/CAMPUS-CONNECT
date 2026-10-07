#include "../include/TopKRanker.h"
#include <iostream>
#include <iomanip>
using namespace std;

// -------------------------------------------------------
// getTopK: Push all candidate scores into a MAX-HEAP,
// then pop the top K elements.
//
// Time complexity: O(N log N) to push N candidates,
// O(K log N) to pop K elements. Total: O(N log N).
// This is more efficient than sorting when K << N.
// -------------------------------------------------------
vector<RankedResult> TopKRanker::getTopK(
    const Student& target,
    const vector<Student>& candidates,
    const MatchingStrategy& strategy,
    int k
) {
    // Max-heap: highest score is always on top
    priority_queue<RankedResult, vector<RankedResult>, CompareScore> maxHeap;

    // Step 1: Compute similarity for each candidate and push into heap
    for (int i = 0; i < (int)candidates.size(); i++) {
        // Skip if candidate is the same as target
        if (candidates[i].getId() == target.getId()) continue;

        // Use Aditya's MatchingStrategy interface (polymorphism)
        double score = strategy.calculateSimilarity(target, candidates[i]);

        // Only add to heap if there is some similarity
        if (score > 0.0) {
            RankedResult result;
            result.studentId = candidates[i].getId();
            result.studentName = candidates[i].getName();
            result.score = score;
            maxHeap.push(result);
        }
    }

    // Step 2: Pop top K results from the heap
    vector<RankedResult> topResults;
    int count = 0;
    while (!maxHeap.empty() && count < k) {
        topResults.push_back(maxHeap.top());
        maxHeap.pop();
        count++;
    }

    return topResults;
}

// -------------------------------------------------------
// getTopKUsingBFS: Uses BFS on Aditya's Graph to find
// reachable student IDs, looks them up in Veeru's DataStore,
// then feeds them into getTopK.
// -------------------------------------------------------
vector<RankedResult> TopKRanker::getTopKUsingBFS(
    const Student& target,
    const Graph& graph,
    DataStore& store,
    const MatchingStrategy& strategy,
    int k
) {
    // Step 1: BFS from target student to find connected students
    vector<int> reachableIds = graph.bfs(target.getId());

    // Step 2: Look up each reachable student in the DataStore
    vector<Student> candidates;
    for (int i = 0; i < (int)reachableIds.size(); i++) {
        Student* s = store.findStudentById(reachableIds[i]);
        if (s != nullptr) {
            candidates.push_back(*s);
        }
    }

    // Step 3: Use the regular getTopK with the BFS-scoped candidates
    return getTopK(target, candidates, strategy, k);
}

// -------------------------------------------------------
// displayResults: Nicely prints the ranked results.
// -------------------------------------------------------
void TopKRanker::displayResults(const vector<RankedResult>& results) {
    if (results.empty()) {
        cout << "No matching students found." << endl;
        return;
    }

    cout << "\n+------+-----+----------------------+------------+" << endl;
    cout << "| Rank | ID  | Name                 | Score      |" << endl;
    cout << "+------+-----+----------------------+------------+" << endl;

    for (int i = 0; i < (int)results.size(); i++) {
        cout << "| " << setw(4) << left << (i + 1)
             << " | " << setw(3) << left << results[i].studentId
             << " | " << setw(20) << left << results[i].studentName
             << " | " << setw(8) << left << fixed << setprecision(1)
             << (results[i].score * 100) << " %  |" << endl;
    }

    cout << "+------+-----+----------------------+------------+" << endl;
}
