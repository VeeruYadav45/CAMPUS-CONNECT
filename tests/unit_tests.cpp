// =============================================================
//  Campus Connect - unit tests for the core C++ modules
//  Build (from the project root):
//    g++ -std=c++11 -I include -o unit_tests tests/unit_tests.cpp \
//        src/student.cpp src/post.cpp src/Group.cpp src/DataStore.cpp \
//        src/Graph.cpp src/JaccardMatching.cpp src/TopKRanker.cpp \
//        src/Message.cpp src/ProjectPost.cpp src/ProjectGroup.cpp
//  Run:  ./unit_tests      (prints PASS/FAIL for every check)
//  Uses the same 8-student demo data as server.cpp loadDemoData().
// =============================================================
#include "DataStore.h"
#include "Graph.h"
#include "Group.h"
#include "JaccardMatching.h"
#include "TopKRanker.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

static int passed = 0, failed = 0;
static void check(const string &id, const string &name, bool ok, const string &got) {
    cout << (ok ? "PASS " : "FAIL ") << id << "  " << name << "  -> " << got << endl;
    ok ? passed++ : failed++;
}
static string list(vector<int> v) {
    ostringstream s; s << "{";
    for (size_t i = 0; i < v.size(); i++) s << (i ? "," : "") << v[i];
    s << "}"; return s.str();
}
static string num(double x) { ostringstream s; s << x; return s.str(); }

int main() {
    // ---- demo data (same as server.cpp) ----
    vector<Student> S = {
        Student(1, "Tanvay Jain",  {"C++", "Python", "DSA"},        {"AI", "Web Dev"}, "CSE", 2),
        Student(2, "Ravi Kumar",   {"Python", "C++", "ML"},         {"AI", "Data Science"}, "CSE", 2),
        Student(3, "Priya Sharma", {"Java", "Python", "Web Dev"},   {"Web Dev", "Open Source"}, "CSE", 3),
        Student(4, "Ankit Singh",  {"C++", "Embedded", "IoT"},      {"Robotics", "IoT"}, "ECE", 2),
        Student(5, "Neha Gupta",   {"Python", "DSA", "ML"},         {"AI", "Competitive Programming"}, "CSE", 2),
        Student(6, "Arjun Mehta",  {"C++", "DSA", "Python"},        {"Competitive Programming", "AI"}, "CSE", 3),
        Student(7, "Sanya Verma",  {"Java", "Spring", "SQL"},       {"Backend Dev", "Cloud"}, "IT", 3),
        Student(8, "Karan Patel",  {"C++", "DSA", "System Design"}, {"AI", "Open Source"}, "CSE", 2)};
    DataStore store;
    Graph g;
    for (auto &s : S) { store.addStudent(s); g.addStudent(s.getId()); }
    int edges[][2] = {{1,2},{1,5},{2,5},{2,6},{3,7},{5,6},{5,8},{6,8},{4,1}};
    for (auto &e : edges) g.addConnection(e[0], e[1]);
    cout << "\n================ UNIT TESTS ================\n";

    // ---- Jaccard similarity ----
    JaccardMatching J;
    double j16 = J.calculateSimilarity(S[0], S[5]);
    check("U1", "Jaccard(Tanvay, Arjun) = 4/6", fabs(j16 - 4.0 / 6) < 1e-9, num(j16));
    double j11 = J.calculateSimilarity(S[0], S[0]);
    check("U2", "Jaccard of identical profiles = 1", fabs(j11 - 1.0) < 1e-9, num(j11));
    double j17 = J.calculateSimilarity(S[0], S[6]);
    check("U3", "Jaccard with nothing shared = 0", j17 == 0.0, num(j17));
    Student e1(90, "Empty A", {}, {}, "CSE", 1), e2(91, "Empty B", {}, {}, "CSE", 1);
    double jee = J.calculateSimilarity(e1, e2);
    check("U4", "Jaccard of two empty profiles = 0 (no division by zero)", jee == 0.0, num(jee));
    check("U5", "Jaccard is symmetric", J.calculateSimilarity(S[1], S[4]) == J.calculateSimilarity(S[4], S[1]), "J(a,b)=J(b,a)");

    // ---- Graph and BFS ----
    vector<int> order = g.bfs(1);
    check("U6", "BFS order from student 1", order == vector<int>({1, 2, 5, 4, 6, 8}), list(order));
    check("U7", "BFS never reaches the other component (3,7)",
          find(order.begin(), order.end(), 3) == order.end(), list(order));
    g.addConnection(1, 2);   // duplicate
    g.addConnection(1, 1);   // self
    vector<int> f1 = g.getConnections(1); sort(f1.begin(), f1.end());
    check("U8", "Duplicate and self connections are ignored", f1 == vector<int>({2, 4, 5}), list(f1));
    vector<int> p1 = g.peopleYouMayKnow(1); sort(p1.begin(), p1.end());
    check("U9", "People you may know for student 1 (distance 2)", p1 == vector<int>({6, 8}), list(p1));
    vector<int> p3 = g.peopleYouMayKnow(3);
    check("U10", "People you may know for student 3 is empty", p3.empty(), list(p3));

    // ---- Top-K ranking (max-heap) ----
    vector<Student> all = store.getAllStudents();
    vector<RankedResult> top = TopKRanker::getTopK(S[0], all, J, 3);
    check("U11", "Top-3 for student 1 starts with Arjun (0.67)",
          top.size() == 3 && top[0].studentId == 6, top.empty() ? "none" : top[0].studentName + " " + num(top[0].score));
    bool sorted = true;
    for (size_t i = 1; i < top.size(); i++) if (top[i].score > top[i - 1].score) sorted = false;
    check("U12", "Top-K results are in descending score order", sorted, "ok");
    vector<RankedResult> every = TopKRanker::getTopK(S[0], all, J, 100);
    bool noSelf = true, noZero = true;
    for (auto &r : every) { if (r.studentId == 1) noSelf = false; if (r.score <= 0) noZero = false; }
    check("U13", "Top-K skips the student themselves and zero scores", noSelf && noZero,
          to_string(every.size()) + " results");
    vector<RankedResult> viaBfs = TopKRanker::getTopKUsingBFS(S[0], g, store, J, 100);
    bool outside = false;
    for (auto &r : viaBfs) if (r.studentId == 3 || r.studentId == 7) outside = true;
    check("U14", "BFS-scoped Top-K excludes students outside the component", !outside,
          to_string(viaBfs.size()) + " results");

    // ---- DataStore inverted index ----
    vector<Student> py = store.matchBySkill("Python");
    check("U15", "Inverted index: students with skill Python", py.size() == 5, to_string(py.size()));
    check("U16", "Inverted index: unknown skill returns nothing", store.matchBySkill("Rust").empty(), "0");

    // ---- Group rules ----
    Group grp(1, "AI Study Group", "test", 1);
    grp.addMember(2);
    grp.addMember(2);   // second join should not duplicate
    vector<int> m = grp.getMemberIds();
    check("U17", "Joining twice does not duplicate a member", count(m.begin(), m.end(), 2) == 1, list(m));
    grp.removeMember(1);   // admin
    check("U18", "The group admin cannot be removed", grp.isMember(1), list(grp.getMemberIds()));

    cout << "============================================\n";
    cout << "Passed " << passed << " / " << (passed + failed) << endl;
    return failed == 0 ? 0 : 1;
}
