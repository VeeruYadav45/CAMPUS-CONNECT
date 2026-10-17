#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include "include/student.h"
#include "include/post.h"
#include "include/HelpRequest.h"
#include "include/HelpOffer.h"
#include "include/Group.h"
#include "include/DataStore.h"
#include "include/Graph.h"
#include "include/JaccardMatching.h"
#include "include/ProjectPost.h"
#include "include/ProjectGroup.h"
#include "include/TopKRanker.h"

using namespace std;

// ── Helper: get current timestamp as string ──
string getCurrentTime() {
    time_t now = time(0);
    string dt = ctime(&now);
    if (!dt.empty() && dt.back() == '\n') {
        dt.pop_back();
    }
    return dt;
}

// ── Helper: safely read an integer from cin ──
int readInt() {
    int value;
    while (!(cin >> value)) {
        cin.clear();
        cin.ignore(10000, '\n');
        cout << "Invalid input. Please enter a number: ";
    }
    cin.ignore(10000, '\n');
    return value;
}

// ── Automated Full System Demo ──
void runFullDemo(DataStore &store, Graph &graph, int &nextStudentId, int &nextGroupId) {
    cout << "\n============================================" << endl;
    cout << "     CAMPUS-CONNECT: FULL SYSTEM DEMO       " << endl;
    cout << "============================================" << endl;

    // --- Step 1: Register 8 sample students ---
    cout << "\n--- Step 1: Registering Sample Students ---" << endl;

    Student s1(nextStudentId++, "Tanvay Jain", {"C++", "Python", "DSA"}, {"AI", "Web Dev"}, "CSE", 2);
    store.addStudent(s1);

    Student s2(nextStudentId++, "Ravi Kumar", {"Python", "C++", "ML"}, {"AI", "Data Science"}, "CSE", 2);
    store.addStudent(s2);

    Student s3(nextStudentId++, "Priya Sharma", {"Java", "Python", "Web Dev"}, {"Web Dev", "Open Source"}, "CSE", 3);
    store.addStudent(s3);

    Student s4(nextStudentId++, "Ankit Singh", {"C++", "Embedded", "IoT"}, {"Robotics", "IoT"}, "ECE", 2);
    store.addStudent(s4);

    Student s5(nextStudentId++, "Neha Gupta", {"Python", "DSA", "ML"}, {"AI", "Competitive Programming"}, "CSE", 2);
    store.addStudent(s5);

    Student s6(nextStudentId++, "Arjun Mehta", {"C++", "DSA", "Python"}, {"Competitive Programming", "AI"}, "CSE", 3);
    store.addStudent(s6);

    Student s7(nextStudentId++, "Sanya Verma", {"Java", "Spring", "SQL"}, {"Backend Dev", "Cloud"}, "IT", 3);
    store.addStudent(s7);

    Student s8(nextStudentId++, "Karan Patel", {"C++", "DSA", "System Design"}, {"AI", "Open Source"}, "CSE", 2);
    store.addStudent(s8);

    // --- Step 2: Create various posts (Polymorphic + Trie indexing) ---
    cout << "\n--- Step 2: Creating Posts (Polymorphic Posts & Trie Indexing) ---" << endl;

    Post regularPost(store.getNextPostId(), s1.getId(), s1.getName(),
                     "Hello everyone! Excited to connect on Campus-Connect!", getCurrentTime());
    store.addPost(regularPost);

    ProjectPost projPost(store.getNextPostId(), s1.getId(), s1.getName(),
                         "AI Study Group Project", {"Python", "ML", "DSA"}, 3, getCurrentTime());
    store.addPost(projPost);

    HelpRequest* req = new HelpRequest(store.getNextPostId(), s3.getId(), s3.getName(),
                                       "Need urgent help with binary search trees", getCurrentTime(),
                                       "DSA", "High");
    store.addHelpRequest(req);

    HelpOffer* offer = new HelpOffer(store.getNextPostId(), s6.getId(), s6.getName(),
                                     "Available to mentor anyone struggling with C++ templates", getCurrentTime(),
                                     "C++", "Weekends");
    store.addHelpOffer(offer);

    cout << "Created regular post, project post, help request, and help offer." << endl;

    // --- Step 3: Trie search and autocomplete demo ---
    cout << "\n--- Step 3: Trie Keyword & Autocomplete Search ---" << endl;
    cout << "Autocomplete search for prefix 'c':" << endl;
    store.autocompleteSearch("c");

    // --- Step 4: Social Graph & Connections ---
    cout << "\n--- Step 4: Building Social Graph ---" << endl;
    for (int i = s1.getId(); i <= s8.getId(); i++) {
        graph.addStudent(i);
    }
    graph.addConnection(s1.getId(), s2.getId());
    graph.addConnection(s1.getId(), s5.getId());
    graph.addConnection(s2.getId(), s5.getId());
    graph.addConnection(s2.getId(), s6.getId());
    graph.addConnection(s3.getId(), s7.getId());
    graph.addConnection(s5.getId(), s6.getId());
    graph.addConnection(s5.getId(), s8.getId());
    graph.addConnection(s6.getId(), s8.getId());
    graph.addConnection(s4.getId(), s1.getId());

    graph.display();

    // --- Step 5: Top-K Matching via Max-Heap ---
    cout << "\n--- Step 5: Top-K Ranking for " << s1.getName() << " (Max-Heap) ---" << endl;
    JaccardMatching matcher;
    vector<Student> allStudents = store.getAllStudents();
    int k = 5;
    vector<RankedResult> topAll = TopKRanker::getTopK(s1, allStudents, matcher, k);
    TopKRanker::displayResults(topAll);

    // --- Step 6: Top-K via Graph BFS + Jaccard ---
    cout << "\n--- Step 6: Top-K via BFS + Jaccard for " << s1.getName() << " ---" << endl;
    vector<RankedResult> topBFS = TopKRanker::getTopKUsingBFS(s1, graph, store, matcher, k);
    TopKRanker::displayResults(topBFS);

    // --- Step 7: Create Project Group ---
    cout << "\n--- Step 7: Creating Project Group with Top Matches ---" << endl;
    ProjectGroup pGroup(nextGroupId++, "AI Study Group",
                        "A team for the AI Study Group Project",
                        s1.getId(), projPost.getPostId());

    for (int i = 0; i < (int)topBFS.size() && i < 2; i++) {
        pGroup.addMember(topBFS[i].studentId);
    }
    pGroup.display();
    store.addGroup(pGroup);

    cout << "\nFull system demonstration complete!" << endl;
}

// ── Print Menu ──
void printMenu() {
    cout << "\n======== CAMPUS-CONNECT ========" << endl;
    cout << "--- Students & Groups ---" << endl;
    cout << "1. Register Student" << endl;
    cout << "2. View All Students" << endl;
    cout << "3. View Student by ID" << endl;
    cout << "4. Search Students by Skill" << endl;
    cout << "5. Search Students by Interest" << endl;
    cout << "6. Search Students by Department" << endl;
    cout << "7. Create Group" << endl;
    cout << "8. View All Groups" << endl;
    cout << "9. Join Group" << endl;
    cout << "10. Leave Group" << endl;

    cout << "\n--- Posts & Collaboration ---" << endl;
    cout << "11. Create Regular Post" << endl;
    cout << "12. Create Help Request (Urgency & Skill)" << endl;
    cout << "13. Create Help Offer (Skills Offered)" << endl;
    cout << "14. Create Project Post (Find Teammates)" << endl;
    cout << "15. View All Posts" << endl;
    cout << "16. View Help Requests Only" << endl;
    cout << "17. View Help Offers Only" << endl;
    cout << "18. Post in a Group" << endl;
    cout << "19. View Posts in a Group" << endl;

    cout << "\n--- Keyword Search & Autocomplete (Trie) ---" << endl;
    cout << "20. Search Posts by Keyword" << endl;
    cout << "21. Autocomplete Search (Prefix)" << endl;
    cout << "22. View All Indexed Keywords" << endl;

    cout << "\n--- Matching & Graph Discovery ---" << endl;
    cout << "23. Find Similar Students (Jaccard)" << endl;
    cout << "24. Top-K Ranked Matches (Max-Heap)" << endl;
    cout << "25. Top-K via BFS + Jaccard (Graph + Heap)" << endl;
    cout << "26. Create Project Group from Post" << endl;
    cout << "27. Connect Students in Social Graph" << endl;
    cout << "28. Display Social Graph" << endl;

    cout << "\n--- Automated Demo ---" << endl;
    cout << "29. Run Full System Demo (Auto)" << endl;

    cout << "\n0. Exit" << endl;
    cout << "================================" << endl;
    cout << "Enter choice: ";
}

int main() {
    DataStore store;
    Graph graph;
    int nextStudentId = 1;
    int nextGroupId = 1;
    int choice;

    cout << "\n========================================" << endl;
    cout << "   Welcome to CAMPUS-CONNECT!           " << endl;
    cout << "   Peer Discovery & Collaboration       " << endl;
    cout << "========================================" << endl;

    do {
        printMenu();
        choice = readInt();

        if (choice == 1) {
            string name, department, temp;
            int year, numSkills, numInterests;
            vector<string> skills, interests;

            cout << "Enter Name: ";
            getline(cin, name);
            cout << "Enter Department (e.g. CSE, IT, ECE): ";
            getline(cin, department);
            cout << "Enter Year (1-4): ";
            year = readInt();

            cout << "How many skills do you want to add? ";
            numSkills = readInt();
            for (int i = 0; i < numSkills; i++) {
                cout << "  Skill " << i + 1 << ": ";
                getline(cin, temp);
                skills.push_back(temp);
            }

            cout << "How many interests do you want to add? ";
            numInterests = readInt();
            for (int i = 0; i < numInterests; i++) {
                cout << "  Interest " << i + 1 << ": ";
                getline(cin, temp);
                interests.push_back(temp);
            }

            Student s(nextStudentId++, name, skills, interests, department, year);
            store.addStudent(s);
            graph.addStudent(s.getId());

        } else if (choice == 2) {
            store.displayAllStudents();

        } else if (choice == 3) {
            cout << "Enter Student ID: ";
            int id = readInt();
            Student* s = store.findStudentById(id);
            if (!s) cout << "Student not found." << endl;
            else s->display();

        } else if (choice == 4) {
            cout << "Enter skill to search: ";
            string skill;
            getline(cin, skill);
            vector<Student> matches = store.matchBySkill(skill);
            cout << "Found " << matches.size() << " student(s) with skill '" << skill << "':" << endl;
            for (auto& s : matches) s.display();

        } else if (choice == 5) {
            cout << "Enter interest to search: ";
            string interest;
            getline(cin, interest);
            vector<Student> matches = store.matchByInterest(interest);
            cout << "Found " << matches.size() << " student(s) interested in '" << interest << "':" << endl;
            for (auto& s : matches) s.display();

        } else if (choice == 6) {
            cout << "Enter department to search: ";
            string dept;
            getline(cin, dept);
            vector<Student> matches = store.matchByDepartment(dept);
            cout << "Found " << matches.size() << " student(s) in department '" << dept << "':" << endl;
            for (auto& s : matches) s.display();

        } else if (choice == 7) {
            string gName, desc;
            int adminId;
            cout << "Enter Group Name: ";
            getline(cin, gName);
            cout << "Enter Description: ";
            getline(cin, desc);
            cout << "Enter Admin Student ID: ";
            adminId = readInt();

            if (!store.findStudentById(adminId)) {
                cout << "Admin Student ID not found." << endl;
            } else {
                Group g(nextGroupId++, gName, desc, adminId);
                store.addGroup(g);
            }

        } else if (choice == 8) {
            store.displayAllGroups();

        } else if (choice == 9) {
            cout << "Enter Group ID: ";
            int gId = readInt();
            cout << "Enter Your Student ID: ";
            int sId = readInt();
            Group* g = store.findGroupById(gId);
            if (!g) cout << "Group not found." << endl;
            else g->addMember(sId);

        } else if (choice == 10) {
            cout << "Enter Group ID: ";
            int gId = readInt();
            cout << "Enter Your Student ID: ";
            int sId = readInt();
            Group* g = store.findGroupById(gId);
            if (!g) cout << "Group not found." << endl;
            else g->removeMember(sId);

        } else if (choice == 11) {
            // Create Regular Post
            cout << "Enter Your Student ID: ";
            int sId = readInt();
            Student* s = store.findStudentById(sId);
            if (!s) {
                cout << "Student not found." << endl;
            } else {
                cout << "Enter post content: ";
                string content;
                getline(cin, content);
                Post p(store.getNextPostId(), sId, s->getName(), content, getCurrentTime());
                store.addPost(p);
                cout << "Post created!" << endl;
            }

        } else if (choice == 12) {
            // Create Help Request
            cout << "Enter Your Student ID: ";
            int sId = readInt();
            Student* s = store.findStudentById(sId);
            if (!s) {
                cout << "Student not found." << endl;
            } else {
                string skill, urgency, content;
                cout << "What skill do you need help with? (e.g. C++, DSA): ";
                getline(cin, skill);
                cout << "Urgency (Low / Medium / High): ";
                getline(cin, urgency);
                cout << "Describe your problem: ";
                getline(cin, content);

                HelpRequest* req = new HelpRequest(store.getNextPostId(), sId, s->getName(),
                                                   content, getCurrentTime(), skill, urgency);
                store.addHelpRequest(req);
                cout << "Help Request posted!" << endl;
            }

        } else if (choice == 13) {
            // Create Help Offer
            cout << "Enter Your Student ID: ";
            int sId = readInt();
            Student* s = store.findStudentById(sId);
            if (!s) {
                cout << "Student not found." << endl;
            } else {
                string skill, avail, content;
                cout << "What skill can you help with? (e.g. Python, ML): ";
                getline(cin, skill);
                cout << "Availability (e.g. Weekends, Evenings): ";
                getline(cin, avail);
                cout << "Describe what you can offer: ";
                getline(cin, content);

                HelpOffer* offer = new HelpOffer(store.getNextPostId(), sId, s->getName(),
                                                 content, getCurrentTime(), skill, avail);
                store.addHelpOffer(offer);
                cout << "Help Offer posted!" << endl;
            }

        } else if (choice == 14) {
            // Create Project Post
            cout << "Enter Your Student ID: ";
            int sId = readInt();
            Student* s = store.findStudentById(sId);
            if (!s) {
                cout << "Student not found." << endl;
            } else {
                string title;
                int teamSize, numSkills;
                vector<string> reqSkills;
                cout << "Enter Project Title: ";
                getline(cin, title);
                cout << "How many team members needed? ";
                teamSize = readInt();
                cout << "How many required skills? ";
                numSkills = readInt();
                for (int i = 0; i < numSkills; i++) {
                    string sk;
                    cout << "  Skill " << i + 1 << ": ";
                    getline(cin, sk);
                    reqSkills.push_back(sk);
                }
                ProjectPost pp(store.getNextPostId(), sId, s->getName(), title,
                               reqSkills, teamSize, getCurrentTime());
                store.addPost(pp);
                cout << "Project Post published!" << endl;
                pp.display();
            }

        } else if (choice == 15) {
            store.displayAllPosts();

        } else if (choice == 16) {
            store.displayHelpRequests();

        } else if (choice == 17) {
            store.displayHelpOffers();

        } else if (choice == 18) {
            // Post in a group
            cout << "Enter Group ID: ";
            int gId = readInt();
            Group* g = store.findGroupById(gId);
            if (!g) {
                cout << "Group not found." << endl;
            } else {
                cout << "Enter Your Student ID: ";
                int sId = readInt();
                Student* s = store.findStudentById(sId);
                if (!s) {
                    cout << "Student not found." << endl;
                } else if (!g->isMember(sId)) {
                    cout << "You must join this group first!" << endl;
                } else {
                    cout << "Enter post content: ";
                    string content;
                    getline(cin, content);
                    Post p(store.getNextPostId(), sId, s->getName(), content, getCurrentTime());
                    g->addPost(p);
                    cout << "Posted in group '" << g->getGroupName() << "'!" << endl;
                }
            }

        } else if (choice == 19) {
            cout << "Enter Group ID: ";
            int gId = readInt();
            Group* g = store.findGroupById(gId);
            if (!g) cout << "Group not found." << endl;
            else g->displayPosts();

        } else if (choice == 20) {
            cout << "Enter keyword to search (exact match): ";
            string kw;
            getline(cin, kw);
            store.searchByKeyword(kw);

        } else if (choice == 21) {
            cout << "Enter prefix to search / autocomplete: ";
            string pfx;
            getline(cin, pfx);
            store.autocompleteSearch(pfx);

        } else if (choice == 22) {
            store.displayTrieKeywords();

        } else if (choice == 23) {
            // Find Similar Students using Jaccard Similarity
            cout << "Enter Your Student ID: ";
            int sId = readInt();
            Student* target = store.findStudentById(sId);
            if (!target) {
                cout << "Student not found." << endl;
            } else {
                JaccardMatching matcher;
                vector<Student> all = store.getAllStudents();
                cout << "\n===== SIMILAR STUDENTS (Jaccard) =====" << endl;
                cout << "Comparing with: " << target->getName() << endl;
                cout << "--------------------------------------" << endl;
                bool anyFound = false;
                for (Student &s : all) {
                    if (s.getId() == sId) continue;
                    double score = matcher.calculateSimilarity(*target, s);
                    if (score > 0) {
                        anyFound = true;
                        cout << "Student: " << s.getName() << " | ID: " << s.getId()
                             << " | Similarity: " << (int)(score * 100) << "%" << endl;
                    }
                }
                if (!anyFound) cout << "No similar students found." << endl;
            }

        } else if (choice == 24) {
            // Top-K Ranked Matches using Max-Heap
            cout << "Enter Your Student ID: ";
            int sId = readInt();
            cout << "Enter K (how many top matches): ";
            int k = readInt();

            Student* target = store.findStudentById(sId);
            if (!target) {
                cout << "Student not found." << endl;
            } else {
                JaccardMatching matcher;
                vector<Student> all = store.getAllStudents();
                cout << "\n===== TOP-" << k << " MATCHES (Max-Heap) =====" << endl;
                vector<RankedResult> results = TopKRanker::getTopK(*target, all, matcher, k);
                TopKRanker::displayResults(results);
            }

        } else if (choice == 25) {
            // Top-K via BFS + Jaccard (graph-scoped)
            cout << "Enter Your Student ID: ";
            int sId = readInt();
            cout << "Enter K (how many top matches): ";
            int k = readInt();

            Student* target = store.findStudentById(sId);
            if (!target) {
                cout << "Student not found." << endl;
            } else {
                JaccardMatching matcher;
                cout << "\n===== TOP-" << k << " MATCHES (BFS + Heap) =====" << endl;
                vector<RankedResult> results = TopKRanker::getTopKUsingBFS(*target, graph, store, matcher, k);
                TopKRanker::displayResults(results);
            }

        } else if (choice == 26) {
            // Create Project Group from top matches
            cout << "Enter Your Student ID (Leader): ";
            int sId = readInt();
            cout << "Enter ProjectPost ID to link: ";
            int postId = readInt();
            cout << "Enter Group Name: ";
            string gName;
            getline(cin, gName);
            cout << "How many top matches to auto-add? ";
            int teamSize = readInt();

            Student* leader = store.findStudentById(sId);
            if (!leader) {
                cout << "Student not found." << endl;
            } else {
                ProjectGroup pg(nextGroupId++, gName,
                                "Project team for post #" + to_string(postId), sId, postId);

                JaccardMatching matcher;
                vector<Student> all = store.getAllStudents();
                vector<RankedResult> results = TopKRanker::getTopK(*leader, all, matcher, teamSize);

                for (int i = 0; i < (int)results.size(); i++) {
                    pg.addMember(results[i].studentId);
                }

                pg.display();
                store.addGroup(pg);
            }

        } else if (choice == 27) {
            // Connect students in social graph
            cout << "Enter Student 1 ID: ";
            int s1 = readInt();
            cout << "Enter Student 2 ID: ";
            int s2 = readInt();

            if (!store.findStudentById(s1) || !store.findStudentById(s2)) {
                cout << "One or both students not found." << endl;
            } else {
                graph.addConnection(s1, s2);
                cout << "Connection added between Student " << s1 << " and Student " << s2 << "!" << endl;
            }

        } else if (choice == 28) {
            // Display social graph
            cout << "\n===== SOCIAL GRAPH =====" << endl;
            graph.display();

        } else if (choice == 29) {
            // Automated Demo
            runFullDemo(store, graph, nextStudentId, nextGroupId);

        } else if (choice != 0) {
            cout << "Invalid choice. Try again." << endl;
        }

    } while (choice != 0);

    cout << "\nGoodbye! See you on Campus!" << endl;
    return 0;
}
