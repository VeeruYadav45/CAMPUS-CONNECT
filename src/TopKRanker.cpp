#include "../include/TopKRanker.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
using namespace std;

// -------------------------------------------------------
// getTopK: Bounded MIN-HEAP of size k.
//
// The heap holds the best k results seen so far, with the
// WORST of them on top. For each new candidate:
//   - heap has < k items        -> push           O(log k)
//   - candidate beats the top   -> pop + push     O(log k)
//   - otherwise                 -> skip           O(1)
//
// Time:  O(N log k) instead of O(N log N) for pushing all N
//        into a max-heap (or sorting everything).
// Space: O(k) instead of O(N).
// -------------------------------------------------------
vector<RankedResult> TopKRanker::getTopK(
    const Student& target,
    const vector<Student>& candidates,
    const MatchingStrategy& strategy,
    int k
) {
    vector<RankedResult> topResults;
    if (k <= 0) return topResults;

    priority_queue<RankedResult, vector<RankedResult>, WorstOnTop> minHeap;

    for (const Student& candidate : candidates) {
        // Skip if candidate is the same as target
        if (candidate.getId() == target.getId()) continue;

        // Use Aditya's MatchingStrategy interface (polymorphism)
        double score = strategy.calculateSimilarity(target, candidate);

        // Only rank candidates with some similarity
        if (score <= 0.0) continue;

        RankedResult result;
        result.studentId = candidate.getId();
        result.score = score;

        bool heapFull = ((int)minHeap.size() == k);
        if (heapFull && !rankedBetter(result, minHeap.top())) continue;  // not in top k

        result.studentName = candidate.getName();  // copy the name only for kept results
        if (heapFull) minHeap.pop();               // evict the current worst
        minHeap.push(result);
    }

    // Pop worst -> best, then reverse so the best comes first. O(k log k)
    while (!minHeap.empty()) {
        topResults.push_back(minHeap.top());
        minHeap.pop();
    }
    reverse(topResults.begin(), topResults.end());
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
