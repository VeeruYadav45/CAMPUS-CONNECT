// =============================================================
//  Campus Connect - unit tests for the DSA core
//  Build + run (from the project root):  make test
//  Exit code 0 = all tests passed.
// =============================================================
#include "DataStore.h"
#include "Graph.h"
#include "JaccardMatching.h"
#include "ProjectGroup.h"
#include "ProjectPost.h"
#include "TopKRanker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            fprintf(stderr, "  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

static bool contains(const vector<int> &v, int x) {
    return find(v.begin(), v.end(), x) != v.end();
}

static Student makeStudent(int id, vector<string> skills, vector<string> interests = {},
                           string dept = "CSE") {
    return Student(id, "S" + to_string(id), skills, interests, dept, 2);
}

// ---------------------------------------------------------------- Graph
static void testGraph() {
    Graph g;
    g.addConnection(1, 2);
    g.addConnection(1, 2);  // duplicate
    g.addConnection(2, 1);  // duplicate (reverse direction)
    g.addConnection(3, 3);  // self connection
    CHECK(g.getConnections(1).size() == 1);
    CHECK(g.getConnections(2).size() == 1);
    CHECK(g.getConnections(3).empty());

    // 1 - 2 - 3 - 4, and 1 - 5
    g.addConnection(2, 3);
    g.addConnection(3, 4);
    g.addConnection(1, 5);

    vector<int> order = g.bfs(1);
    CHECK(order.size() == 5);
    CHECK(order[0] == 1);
    CHECK(g.bfs(99).empty());  // unknown student

    vector<int> pymk = g.peopleYouMayKnow(1);
    CHECK(pymk.size() == 1 && pymk[0] == 3);  // only friends-of-friends
    CHECK(!contains(pymk, 1));                // not yourself
    CHECK(!contains(pymk, 2) && !contains(pymk, 5));  // not existing friends
    CHECK(!contains(pymk, 4));                // 3 steps away
}

// ---------------------------------------------------------------- Graph: ranked suggestions
static void testSuggestFriends() {
    Graph g;
    CHECK(g.suggestFriends(1).empty());   // unknown student
    g.addStudent(1);
    CHECK(g.suggestFriends(1).empty());   // no friends yet

    // Demo network from the paper (Fig. 3): Tanvay = 1
    // 1-2, 1-5, 1-4, 2-5, 2-6, 5-6, 5-8, 6-8, and a separate 3-7
    Graph d;
    d.addConnection(1, 2); d.addConnection(1, 5); d.addConnection(1, 4);
    d.addConnection(2, 5); d.addConnection(2, 6); d.addConnection(5, 6);
    d.addConnection(5, 8); d.addConnection(6, 8); d.addConnection(3, 7);
    vector<FriendSuggestion> s = d.suggestFriends(1);
    CHECK(s.size() == 2);
    if (s.size() == 2) {
        CHECK(s[0].studentId == 6);                                   // Arjun first:
        CHECK(s[0].mutualIds == vector<int>({2, 5}));                 //   via Ravi and Neha
        CHECK(s[1].studentId == 8);                                   // then Karan:
        CHECK(s[1].mutualIds == vector<int>({5}));                    //   via Neha
    }
    CHECK(d.suggestFriends(3).empty());   // Priya: her only friend has no other friends

    // Equal mutual counts -> smaller ID first
    Graph t;
    t.addConnection(1, 2); t.addConnection(1, 3);
    t.addConnection(2, 10); t.addConnection(3, 9);
    vector<FriendSuggestion> ts = t.suggestFriends(1);
    CHECK(ts.size() == 2 && ts[0].studentId == 9 && ts[1].studentId == 10);

    // Random graphs: same students as peopleYouMayKnow, and each
    // mutual list equals the brute-force intersection of friend lists
    mt19937 rng(7);
    for (int round = 0; round < 200; round++) {
        Graph r;
        int n = 2 + (int)(rng() % 15);
        for (int i = 1; i <= n; i++) r.addStudent(i);
        int edges = (int)(rng() % (n * 2));
        for (int e = 0; e < edges; e++) {
            r.addConnection(1 + (int)(rng() % n), 1 + (int)(rng() % n));
        }
        int me = 1 + (int)(rng() % n);
        vector<FriendSuggestion> got = r.suggestFriends(me);
        vector<int> expected = r.peopleYouMayKnow(me);
        CHECK(got.size() == expected.size());

        vector<int> myFriends = r.getConnections(me);
        for (size_t i = 0; i < got.size(); i++) {
            CHECK(contains(expected, got[i].studentId));
            vector<int> brute;
            for (int f : r.getConnections(got[i].studentId)) {
                if (contains(myFriends, f)) brute.push_back(f);
            }
            sort(brute.begin(), brute.end());
            CHECK(got[i].mutualIds == brute);
            if (i > 0) {   // ranked: more mutual friends first, ties by ID
                size_t a = got[i - 1].mutualIds.size(), b = got[i].mutualIds.size();
                CHECK(a > b || (a == b && got[i - 1].studentId < got[i].studentId));
            }
        }
    }
}

// ---------------------------------------------------------------- DataStore index
static void testDataStoreIndex() {
    DataStore store;
    store.addStudent(makeStudent(1, {"Python", "Python"}, {"AI"}, "CSE"));
    CHECK(store.matchBySkill("Python").size() == 1);  // duplicate skill indexed once

    // Update student 1: Python -> Java, CSE -> ECE
    store.addStudent(makeStudent(1, {"Java"}, {"AI"}, "ECE"));
    CHECK(store.matchBySkill("Python").empty());       // stale entry removed
    CHECK(store.matchBySkill("Java").size() == 1);
    CHECK(store.matchByInterest("AI").size() == 1);    // kept skill not duplicated
    CHECK(store.matchByDepartment("CSE").empty());
    CHECK(store.matchByDepartment("ECE").size() == 1);
    CHECK(store.getAllStudents().size() == 1);

    store.addStudent(makeStudent(2, {"Java"}));
    CHECK(store.matchBySkill("Java").size() == 2);
    CHECK(store.findStudentById(2) != nullptr);
    CHECK(store.findStudentById(42) == nullptr);
}

// ---------------------------------------------------------------- Polymorphic storage
static void testNoSlicing() {
    DataStore store;
    ProjectPost pp(7, 1, "S1", "AI Project", {"Python", "ML"}, 3, "now");
    store.addPost(pp);
    store.addPost(Post(8, 1, "S1", "hello", "now"));

    ProjectPost *stored = dynamic_cast<ProjectPost *>(store.findPostById(7));
    CHECK(stored != nullptr);
    if (stored) {
        CHECK(stored->getProjectTitle() == "AI Project");
        CHECK(stored->getRequiredSkills().size() == 2);
        CHECK(stored->getTeamSizeNeeded() == 3);
    }
    CHECK(dynamic_cast<ProjectPost *>(store.findPostById(8)) == nullptr);

    ProjectGroup pg(3, "Team", "desc", 1, 7);
    store.addGroup(pg);
    ProjectGroup *g = dynamic_cast<ProjectGroup *>(store.findGroupById(3));
    CHECK(g != nullptr);
    if (g) CHECK(g->getLinkedPostId() == 7);

    // Virtual display: ProjectPost output must include the project title
    stringstream captured;
    streambuf *old = cout.rdbuf(captured.rdbuf());
    store.displayAllPosts();
    cout.rdbuf(old);
    CHECK(captured.str().find("AI Project") != string::npos);
}

// ---------------------------------------------------------------- Group
static void testGroup() {
    Group g(1, "Club", "desc", 10);
    g.addMember(11);
    g.addMember(11);  // duplicate
    CHECK(g.getMemberIds().size() == 2);
    g.removeMember(10);  // admin cannot be removed
    CHECK(g.isMember(10));
    g.removeMember(11);
    CHECK(!g.isMember(11));
}

// ---------------------------------------------------------------- Jaccard
static void testJaccard() {
    JaccardMatching J;
    Student a = makeStudent(1, {"A", "B"});
    Student b = makeStudent(2, {"B", "C"});
    Student empty = makeStudent(3, {});
    CHECK(fabs(J.calculateSimilarity(a, b) - 1.0 / 3.0) < 1e-9);
    CHECK(fabs(J.calculateSimilarity(a, a) - 1.0) < 1e-9);
    CHECK(J.calculateSimilarity(empty, empty) == 0.0);
    CHECK(J.calculateSimilarity(a, b) == J.calculateSimilarity(b, a));  // symmetric
}

// ---------------------------------------------------------------- Top-K
// Reference answer: score everyone, sort fully, take k
static vector<RankedResult> bruteForceTopK(const Student &target, const vector<Student> &all,
                                           const MatchingStrategy &s, int k) {
    vector<RankedResult> scored;
    for (const Student &c : all) {
        if (c.getId() == target.getId()) continue;
        double sc = s.calculateSimilarity(target, c);
        if (sc > 0) scored.push_back({c.getId(), c.getName(), sc});
    }
    sort(scored.begin(), scored.end(), rankedBetter);
    if ((int)scored.size() > k) scored.resize(k);
    return scored;
}

static void testTopK() {
    JaccardMatching J;
    mt19937 rng(7);
    uniform_int_distribution<int> tagDist(0, 7), countDist(0, 4);

    for (int trial = 0; trial < 300; trial++) {
        vector<Student> all;
        int n = 1 + trial % 40;
        for (int id = 1; id <= n; id++) {
            vector<string> skills;
            int c = countDist(rng);
            for (int t = 0; t < c; t++) skills.push_back("t" + to_string(tagDist(rng)));
            all.push_back(makeStudent(id, skills));
        }
        int k = trial % 12;  // includes k = 0 and k > n
        vector<RankedResult> got = TopKRanker::getTopK(all[0], all, J, k);
        vector<RankedResult> want = bruteForceTopK(all[0], all, J, k);

        bool same = got.size() == want.size();
        for (size_t i = 0; same && i < got.size(); i++)
            same = got[i].studentId == want[i].studentId && got[i].score == want[i].score &&
                   got[i].studentName == want[i].studentName;
        CHECK(same);
        for (const RankedResult &r : got) CHECK(r.studentId != all[0].getId() && r.score > 0);
    }
    CHECK(TopKRanker::getTopK(makeStudent(1, {"A"}), {}, J, 5).empty());
}

// ---------------------------------------------------------------- Messages
static void testMessages() {
    DataStore store;
    store.addMessage(Message(1, 1, 2, "S1", "hi", "t1"));
    store.addMessage(Message(2, 2, 1, "S2", "hey", "t2"));
    store.addMessage(Message(3, 1, 3, "S1", "yo", "t3"));
    CHECK(store.getConversation(1, 2).size() == 2);
    CHECK(store.getConversation(2, 1).size() == 2);
    vector<int> partners = store.getChatPartners(1);
    CHECK(partners.size() == 2 && contains(partners, 2) && contains(partners, 3));
}

int main() {
    // DataStore/Group print progress messages; hide them during tests
    stringstream sink;
    streambuf *old = cout.rdbuf(sink.rdbuf());

    testGraph();
    testSuggestFriends();
    testDataStoreIndex();
    testNoSlicing();
    testGroup();
    testJaccard();
    testTopK();
    testMessages();

    cout.rdbuf(old);
    if (failures == 0) {
        printf("All %d checks passed.\n", checks);
        return 0;
    }
    printf("%d of %d checks FAILED.\n", failures, checks);
    return 1;
}
