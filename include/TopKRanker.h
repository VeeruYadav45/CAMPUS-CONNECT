#ifndef TOPKRANKER_H
#define TOPKRANKER_H

#include "DataStore.h"
#include "Graph.h"
#include "MatchingStrategy.h"
#include "student.h"
#include <queue>
#include <utility>
#include <vector>
using namespace std;

// -------------------------------------------------------
// RankedResult: Holds a student and their compatibility score.
// Used as the element type inside the priority queue.
// -------------------------------------------------------
struct RankedResult {
  int studentId;
  string studentName;
  double score; // Jaccard similarity score (0.0 to 1.0)
};

// -------------------------------------------------------
// "Better" ordering: higher score first; on a tie, lower student
// ID first, so results are deterministic.
// -------------------------------------------------------
inline bool rankedBetter(const RankedResult &a, const RankedResult &b) {
  if (a.score != b.score) return a.score > b.score;
  return a.studentId < b.studentId;
}

// -------------------------------------------------------
// Comparator for the bounded MIN-HEAP used by getTopK.
// std::priority_queue keeps the element that compares "largest"
// on top. Treating "better" as "smaller" puts the WORST of the
// current top-k on top, ready to be evicted in O(log k).
// -------------------------------------------------------
struct WorstOnTop {
  bool operator()(const RankedResult &a, const RankedResult &b) const {
    return rankedBetter(a, b);
  }
};

// -------------------------------------------------------
// TopKRanker: Uses a size-k MIN-HEAP (std::priority_queue) to rank
// students by compatibility score and return the Top-K.
//
// How it connects to the other modules:
// 1. Uses Aditya's MatchingStrategy interface to compute scores
//    (via calculateSimilarity — polymorphism, could be Jaccard
//    or any future strategy).
// 2. Uses Aditya's Graph + BFS to find connected/nearby students
//    as candidates (instead of brute-forcing all students).
// 3. Reads student data from Veeru's DataStore (no duplication).
//
// DSA: Priority Queue (bounded min-heap of size k) — Tanvay's assigned DSA.
// -------------------------------------------------------
class TopKRanker {
public:
  // -------------------------------------------------------
  // getTopK: Given a target student, find the K most compatible
  // students from the candidate list.
  //
  // Parameters:
  //   target      - the student looking for teammates
  //   candidates  - list of candidate students to compare against
  //   strategy    - the matching algorithm to use (Jaccard etc.)
  //   k           - how many top results to return
  //
  // Returns: vector of RankedResult, sorted highest score first
  // -------------------------------------------------------
  static vector<RankedResult> getTopK(const Student &target,
                                      const vector<Student> &candidates,
                                      const MatchingStrategy &strategy, int k);

  // -------------------------------------------------------
  // getTopKUsingBFS: Same as getTopK, but first uses BFS on
  // the graph to find reachable students from the target,
  // then scores only those candidates.
  //
  // This shows the integration of:
  //   Graph (Aditya) -> BFS candidates
  //   DataStore (Veeru) -> look up Student objects
  //   MatchingStrategy (Aditya) -> compute scores
  //   Priority Queue (Tanvay) -> rank results
  // -------------------------------------------------------
  static vector<RankedResult>
  getTopKUsingBFS(const Student &target, const Graph &graph, DataStore &store,
                  const MatchingStrategy &strategy, int k);

  // Helper: print the ranked results to the terminal
  static void displayResults(const vector<RankedResult> &results);
};

#endif
