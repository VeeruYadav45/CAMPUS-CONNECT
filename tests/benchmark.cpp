// =============================================================
//  Campus Connect - performance benchmark (fills Table IX)
//  Build + run (from the project root):  make bench
//    -> writes tests/results.txt
//
//  Columns:
//    topk_heap_ms / full_sort_ms         : Jaccard scoring + ranking (end to end)
//    rank_heap_ms / rank_sort_ms         : ranking only, scores precomputed,
//                                          so the heap vs sort difference is
//                                          not hidden by the cost of Jaccard
//  The table is written to stderr, because DataStore prints a line
//  for every student it adds (stdout).
// =============================================================
#include "DataStore.h"
#include "JaccardMatching.h"
#include "TopKRanker.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>
using namespace std;
using Clock = chrono::steady_clock;

// Strategy that returns a precomputed score: isolates the ranking cost
struct PrecomputedScore : public MatchingStrategy {
    const vector<double> *scoreById;  // index = student ID
    double calculateSimilarity(const Student &, const Student &s) const override {
        return (*scoreById)[s.getId()];
    }
};

static double msSince(Clock::time_point t) {
    return chrono::duration<double, milli>(Clock::now() - t).count();
}

int main() {
    const int SIZES[] = {1000, 10000, 100000};
    const int VOCAB = 60;          // number of different skills/interests
    const int RUNS = 5;            // repeat each ranking measurement
    const int LOOKUPS = 2000;      // ID lookups per measurement
    mt19937 rng(42);               // fixed seed -> same data every run

    vector<string> words;
    for (int i = 0; i < VOCAB; i++) words.push_back("tag" + to_string(i));

    fprintf(stderr, "n,hash_lookup_us,linear_scan_us,topk_heap_ms,full_sort_ms,rank_heap_ms,rank_sort_ms\n");
    for (int n : SIZES) {
        // ---- build n synthetic students (4-10 tags each) ----
        DataStore store;
        uniform_int_distribution<int> countDist(4, 10), wordDist(0, VOCAB - 1);
        for (int id = 1; id <= n; id++) {
            vector<string> skills, interests;
            int c = countDist(rng);
            for (int t = 0; t < c; t++) (t % 2 ? interests : skills).push_back(words[wordDist(rng)]);
            store.addStudent(Student(id, "Student " + to_string(id), skills, interests, "CSE", 1 + id % 4));
        }
        vector<Student> all = store.getAllStudents();
        uniform_int_distribution<int> idDist(1, n);
        vector<int> queries;
        for (int i = 0; i < LOOKUPS; i++) queries.push_back(idDist(rng));

        // ---- 1. hash lookup by ID ----
        volatile long sink = 0;
        auto t = Clock::now();
        for (int q : queries) sink += store.findStudentById(q)->getYear();
        double hashUs = msSince(t) * 1000.0 / LOOKUPS;

        // ---- 2. linear scan by ID ----
        t = Clock::now();
        for (int q : queries)
            for (const Student &s : all)
                if (s.getId() == q) { sink += s.getYear(); break; }
        double scanUs = msSince(t) * 1000.0 / LOOKUPS;

        // ---- 3. top-10 with the bounded min-heap (TopKRanker) ----
        JaccardMatching J;
        const Student &target = all[0];
        t = Clock::now();
        for (int r = 0; r < RUNS; r++) sink += TopKRanker::getTopK(target, all, J, 10).size();
        double heapMs = msSince(t) / RUNS;

        // ---- 4. top-10 by scoring everyone and sorting the full list ----
        t = Clock::now();
        for (int r = 0; r < RUNS; r++) {
            vector<pair<double, int>> scored;
            for (const Student &s : all) {
                if (s.getId() == target.getId()) continue;
                double sc = J.calculateSimilarity(target, s);
                if (sc > 0) scored.push_back({sc, s.getId()});
            }
            sort(scored.begin(), scored.end(), [](const pair<double, int> &a, const pair<double, int> &b) {
                return a.first > b.first;
            });
            sink += min<size_t>(10, scored.size());
        }
        double sortMs = msSince(t) / RUNS;

        // ---- 5/6. ranking only: same scores, heap vs full sort ----
        vector<double> scores(n + 1, 0.0);
        for (const Student &s : all) scores[s.getId()] = J.calculateSimilarity(target, s);
        PrecomputedScore P;
        P.scoreById = &scores;

        t = Clock::now();
        for (int r = 0; r < RUNS; r++) sink += TopKRanker::getTopK(target, all, P, 10).size();
        double rankHeapMs = msSince(t) / RUNS;

        t = Clock::now();
        for (int r = 0; r < RUNS; r++) {
            vector<pair<double, int>> scored;
            for (const Student &s : all) {
                if (s.getId() == target.getId()) continue;
                double sc = P.calculateSimilarity(target, s);
                if (sc > 0) scored.push_back({sc, s.getId()});
            }
            sort(scored.begin(), scored.end(), [](const pair<double, int> &a, const pair<double, int> &b) {
                return a.first > b.first;
            });
            sink += min<size_t>(10, scored.size());
        }
        double rankSortMs = msSince(t) / RUNS;

        fprintf(stderr, "%d,%.3f,%.3f,%.2f,%.2f,%.3f,%.3f\n", n, hashUs, scanUs, heapMs, sortMs,
                rankHeapMs, rankSortMs);
    }
    return 0;
}
