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
using namespace std;

// ── Helper: get current timestamp as string ──
string getCurrentTime() {
    time_t now = time(0);
    string dt = ctime(&now);
    dt.pop_back(); // remove trailing newline
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

// ── Helper: print menu ──
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
    
    cout << "\n--- Posts & Offers ---" << endl;
    cout << "11. Create Help Request" << endl;
    cout << "12. Create Help Offer" << endl;
    cout << "13. View All Posts" << endl;
    cout << "14. View Help Requests Only" << endl;
    cout << "15. View Help Offers Only" << endl;
    
    cout << "\n--- Keyword Search (Trie) ---" << endl;
    cout << "16. Search Posts by Keyword" << endl;
    cout << "17. Autocomplete Search" << endl;
    cout << "18. View All Indexed Keywords" << endl;
    
    cout << "\n--- Group Posts ---" << endl;
    cout << "19. Post in a Group" << endl;
    cout << "20. View Posts in a Group" << endl;
    
    cout << "\n0. Exit" << endl;
    cout << "================================" << endl;
    cout << "Enter choice: ";
}

int main() {
    DataStore store;
    int nextStudentId = 1;
    int nextGroupId = 1;
    int choice;

    cout << "\n========================================" << endl;
    cout << "   Welcome to CAMPUS-CONNECT!" << endl;
    cout << "   Peer Discovery & Collaboration" << endl;
    cout << "========================================" << endl;

    do {
        printMenu();
        choice = readInt();

        // ══════════════════════════════════════
        //          1. REGISTER STUDENT
        // ══════════════════════════════════════
        if (choice == 1) {
            string name, department;
            int year;
            int numSkills, numInterests;
            vector<string> skills, interests;

            cout << "Enter Name: ";
            getline(cin, name);
            cout << "Enter Department: ";
            getline(cin, department);
            cout << "Enter Year (1-4): ";
            year = readInt();

            cout << "How many skills? ";
            numSkills = readInt();
            for (int i = 0; i < numSkills; i++) {
                string sk;
                cout << "  Skill " << i + 1 << ": ";
                getline(cin, sk);
                skills.push_back(sk);
            }

            cout << "How many interests? ";
            numInterests = readInt();
            for (int i = 0; i < numInterests; i++) {
                string in;
                cout << "  Interest " << i + 1 << ": ";
                getline(cin, in);
                interests.push_back(in);
            }

            Student s(nextStudentId++, name, skills, interests, department, year);
            store.addStudent(s);

        // ══════════════════════════════════════
        //        2. VIEW ALL STUDENTS
        // ══════════════════════════════════════
        } else if (choice == 2) {
            store.displayAllStudents();

        // ══════════════════════════════════════
        //       3. VIEW STUDENT BY ID
        // ══════════════════════════════════════
        } else if (choice == 3) {
            int id;
            cout << "Enter Student ID: ";
            id = readInt();
            Student* s = store.findStudentById(id);
            if (s) s->display();
            else cout << "Student not found." << endl;

        // ══════════════════════════════════════
        //     4. SEARCH STUDENTS BY SKILL
        // ══════════════════════════════════════
        } else if (choice == 4) {
            string skill;
            cout << "Enter Skill to search: ";
            getline(cin, skill);
            vector<Student> result = store.matchBySkill(skill);
            if (result.empty()) cout << "No students found with skill: " << skill << endl;
            else for (Student& s : result) s.display();

        // ══════════════════════════════════════
        //    5. SEARCH STUDENTS BY INTEREST
        // ══════════════════════════════════════
        } else if (choice == 5) {
            string interest;
            cout << "Enter Interest to search: ";
            getline(cin, interest);
            vector<Student> result = store.matchByInterest(interest);
            if (result.empty()) cout << "No students found with interest: " << interest << endl;
            else for (Student& s : result) s.display();

        // ══════════════════════════════════════
        //   6. SEARCH STUDENTS BY DEPARTMENT
        // ══════════════════════════════════════
        } else if (choice == 6) {
            string dept;
            cout << "Enter Department to search: ";
            getline(cin, dept);
            vector<Student> result = store.matchByDepartment(dept);
            if (result.empty()) cout << "No students found in department: " << dept << endl;
            else for (Student& s : result) s.display();

        // ══════════════════════════════════════
        //          7. CREATE GROUP
        // ══════════════════════════════════════
        } else if (choice == 7) {
            string gName, gDesc;
            int adminId;
            cout << "Enter Group Name: ";
            getline(cin, gName);
            cout << "Enter Description: ";
            getline(cin, gDesc);
            cout << "Enter Your Student ID (Admin): ";
            adminId = readInt();

            Student* admin = store.findStudentById(adminId);
            if (!admin) {
                cout << "Student ID not found. Cannot create group." << endl;
            } else {
                Group g(nextGroupId++, gName, gDesc, adminId);
                store.addGroup(g);
            }

        // ══════════════════════════════════════
        //        8. VIEW ALL GROUPS
        // ══════════════════════════════════════
        } else if (choice == 8) {
            store.displayAllGroups();

        // ══════════════════════════════════════
        //          9. JOIN GROUP
        // ══════════════════════════════════════
        } else if (choice == 9) {
            int gId, sId;
            cout << "Enter Group ID: ";
            gId = readInt();
            cout << "Enter Your Student ID: ";
            sId = readInt();
            Group* g = store.findGroupById(gId);
            if (!g) cout << "Group not found." << endl;
            else g->addMember(sId);

        // ══════════════════════════════════════
        //         10. LEAVE GROUP
        // ══════════════════════════════════════
        } else if (choice == 10) {
            int gId, sId;
            cout << "Enter Group ID: ";
            gId = readInt();
            cout << "Enter Your Student ID: ";
            sId = readInt();
            Group* g = store.findGroupById(gId);
            if (!g) cout << "Group not found." << endl;
            else g->removeMember(sId);

        // ══════════════════════════════════════
        //  11. CREATE HELP REQUEST (Vivekanand)
        //  OOP: Inheritance — HelpRequest : Post
        // ══════════════════════════════════════
        } else if (choice == 11) {
            int sId;
            string content, skill, urgency;
            cout << "Enter Your Student ID: ";
            sId = readInt();

            Student* s = store.findStudentById(sId);
            if (!s) {
                cout << "Student not found." << endl;
            } else {
                cout << "What skill do you need help with? (e.g. C++, DSA): ";
                getline(cin, skill);

                cout << "Urgency (Low / Medium / High): ";
                getline(cin, urgency);

                cout << "Describe your problem: ";
                getline(cin, content);

                // Create HelpRequest on the heap (polymorphic pointer)
                HelpRequest* req = new HelpRequest(
                    store.getNextPostId(), sId, s->getName(),
                    content, getCurrentTime(),
                    skill, urgency
                );

                store.addHelpRequest(req);
                cout << "Help Request published!" << endl;
            }

        // ══════════════════════════════════════
        //  12. CREATE HELP OFFER (Vivekanand)
        //  OOP: Inheritance — HelpOffer : Post
        // ══════════════════════════════════════
        } else if (choice == 12) {
            int sId;
            string content, skill, availability;
            cout << "Enter Your Student ID: ";
            sId = readInt();

            Student* s = store.findStudentById(sId);
            if (!s) {
                cout << "Student not found." << endl;
            } else {
                cout << "What skill can you help with? (e.g. Python, ML): ";
                getline(cin, skill);

                cout << "Your availability (Weekdays / Weekends / Anytime): ";
                getline(cin, availability);

                cout << "Describe how you can help: ";
                getline(cin, content);

                // Create HelpOffer on the heap
                HelpOffer* offer = new HelpOffer(
                    store.getNextPostId(), sId, s->getName(),
                    content, getCurrentTime(),
                    skill, availability
                );

                store.addHelpOffer(offer);
                cout << "Help Offer published!" << endl;
            }

        // ══════════════════════════════════════
        //       13. VIEW ALL POSTS
        //  POLYMORPHISM: display() calls correct
        //  version based on actual object type
        // ══════════════════════════════════════
        } else if (choice == 13) {
            store.displayAllPosts();

        // ══════════════════════════════════════
        //    14. VIEW HELP REQUESTS ONLY
        // ══════════════════════════════════════
        } else if (choice == 14) {
            store.displayHelpRequests();

        // ══════════════════════════════════════
        //     15. VIEW HELP OFFERS ONLY
        // ══════════════════════════════════════
        } else if (choice == 15) {
            store.displayHelpOffers();

        // ══════════════════════════════════════
        //  16. SEARCH POSTS BY KEYWORD (Trie)
        //  DSA: Trie exact keyword search
        // ══════════════════════════════════════
        } else if (choice == 16) {
            string keyword;
            cout << "Enter keyword to search: ";
            getline(cin, keyword);
            store.searchByKeyword(keyword);

        // ══════════════════════════════════════
        //  17. AUTOCOMPLETE SEARCH (Trie)
        //  DSA: Trie prefix search
        //  User types partial keyword → get suggestions
        // ══════════════════════════════════════
        } else if (choice == 17) {
            string prefix;
            cout << "Start typing keyword (prefix): ";
            getline(cin, prefix);
            store.autocompleteSearch(prefix);

        // ══════════════════════════════════════
        //  18. VIEW ALL TRIE KEYWORDS
        //  Shows everything indexed in the Trie
        // ══════════════════════════════════════
        } else if (choice == 18) {
            store.displayTrieKeywords();

        // ══════════════════════════════════════
        //     19. POST IN A GROUP
        // ══════════════════════════════════════
        } else if (choice == 19) {
            int gId, sId;
            string content, skill;
            cout << "Enter Group ID: ";
            gId = readInt();
            cout << "Enter Your Student ID: ";
            sId = readInt();
            Group* g = store.findGroupById(gId);
            Student* s = store.findStudentById(sId);
            if (!g) cout << "Group not found." << endl;
            else if (!s) cout << "Student not found." << endl;
            else if (!g->isMember(sId)) cout << "You are not a member of this group." << endl;
            else {
                cout << "What type of post? (1 = Help Request, 2 = Help Offer): ";
                int postType;
                postType = readInt();

                if (postType == 1) {
                    cout << "Skill needed: ";
                    getline(cin, skill);
                    cout << "Describe your problem: ";
                    getline(cin, content);

                    HelpRequest req(store.getNextPostId(), sId, s->getName(),
                                    content, getCurrentTime(), skill, "Medium");
                    g->addPost(req);
                } else {
                    cout << "Skill you offer: ";
                    getline(cin, skill);
                    cout << "Describe how you can help: ";
                    getline(cin, content);

                    HelpOffer offer(store.getNextPostId(), sId, s->getName(),
                                    content, getCurrentTime(), skill, "Anytime");
                    g->addPost(offer);
                }
                cout << "Posted in group '" << g->getGroupName() << "'!" << endl;
            }

        // ══════════════════════════════════════
        //    20. VIEW POSTS IN A GROUP
        // ══════════════════════════════════════
        } else if (choice == 20) {
            int gId;
            cout << "Enter Group ID: ";
            gId = readInt();
            Group* g = store.findGroupById(gId);
            if (!g) cout << "Group not found." << endl;
            else g->displayPosts();

        } else if (choice != 0) {
            cout << "Invalid choice. Try again." << endl;
        }

    } while (choice != 0);

    cout << "\nGoodbye! See you on Campus!" << endl;
    return 0;
}
