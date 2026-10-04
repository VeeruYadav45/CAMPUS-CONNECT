#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include "include/student.h"
#include "include/post.h"
#include "include/Group.h"
#include "include/DataStore.h"
using namespace std;

// Helper: get current timestamp as string
string getCurrentTime() {
    time_t now = time(0);
    string dt = ctime(&now);
    dt.pop_back(); // remove trailing newline
    return dt;
}

// Helper: print menu
void printMenu() {
    cout << "\n======== CAMPUS-CONNECT ========" << endl;
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
    cout << "11. Create Post (Global)" << endl;
    cout << "12. View All Posts" << endl;
    cout << "13. Post in a Group" << endl;
    cout << "14. View Posts in a Group" << endl;
    cout << "0. Exit" << endl;
    cout << "================================" << endl;
    cout << "Enter choice: ";
}

int main() {
    DataStore store;
    int nextStudentId = 1;
    int nextGroupId = 1;
    int choice;

    cout << "Welcome to CAMPUS-CONNECT!" << endl;

    do {
        printMenu();
        cin >> choice;
        cin.ignore();

        if (choice == 1) {
            // Register Student
            string name, department;
            int year;
            int numSkills, numInterests;
            vector<string> skills, interests;

            cout << "Enter Name: ";
            getline(cin, name);
            cout << "Enter Department: ";
            getline(cin, department);
            cout << "Enter Year (1-4): ";
            cin >> year;
            cin.ignore();

            cout << "How many skills? ";
            cin >> numSkills;
            cin.ignore();
            for (int i = 0; i < numSkills; i++) {
                string sk;
                cout << "  Skill " << i + 1 << ": ";
                getline(cin, sk);
                skills.push_back(sk);
            }

            cout << "How many interests? ";
            cin >> numInterests;
            cin.ignore();
            for (int i = 0; i < numInterests; i++) {
                string in;
                cout << "  Interest " << i + 1 << ": ";
                getline(cin, in);
                interests.push_back(in);
            }

            Student s(nextStudentId++, name, skills, interests, department, year);
            store.addStudent(s);

        } else if (choice == 2) {
            store.displayAllStudents();

        } else if (choice == 3) {
            int id;
            cout << "Enter Student ID: ";
            cin >> id;
            Student* s = store.findStudentById(id);
            if (s) s->display();
            else cout << "Student not found." << endl;

        } else if (choice == 4) {
            string skill;
            cout << "Enter Skill to search: ";
            getline(cin, skill);
            vector<Student> result = store.matchBySkill(skill);
            if (result.empty()) cout << "No students found with skill: " << skill << endl;
            else for (Student& s : result) s.display();

        } else if (choice == 5) {
            string interest;
            cout << "Enter Interest to search: ";
            getline(cin, interest);
            vector<Student> result = store.matchByInterest(interest);
            if (result.empty()) cout << "No students found with interest: " << interest << endl;
            else for (Student& s : result) s.display();

        } else if (choice == 6) {
            string dept;
            cout << "Enter Department to search: ";
            getline(cin, dept);
            vector<Student> result = store.matchByDepartment(dept);
            if (result.empty()) cout << "No students found in department: " << dept << endl;
            else for (Student& s : result) s.display();

        } else if (choice == 7) {
            // Create Group
            string gName, gDesc;
            int adminId;
            cout << "Enter Group Name: ";
            getline(cin, gName);
            cout << "Enter Description: ";
            getline(cin, gDesc);
            cout << "Enter Your Student ID (Admin): ";
            cin >> adminId;
            cin.ignore();

            Student* admin = store.findStudentById(adminId);
            if (!admin) {
                cout << "Student ID not found. Cannot create group." << endl;
            } else {
                Group g(nextGroupId++, gName, gDesc, adminId);
                store.addGroup(g);
            }

        } else if (choice == 8) {
            store.displayAllGroups();

        } else if (choice == 9) {
            // Join Group
            int gId, sId;
            cout << "Enter Group ID: ";
            cin >> gId;
            cout << "Enter Your Student ID: ";
            cin >> sId;
            cin.ignore();
            Group* g = store.findGroupById(gId);
            if (!g) cout << "Group not found." << endl;
            else g->addMember(sId);

        } else if (choice == 10) {
            // Leave Group
            int gId, sId;
            cout << "Enter Group ID: ";
            cin >> gId;
            cout << "Enter Your Student ID: ";
            cin >> sId;
            cin.ignore();
            Group* g = store.findGroupById(gId);
            if (!g) cout << "Group not found." << endl;
            else g->removeMember(sId);

        } else if (choice == 11) {
            // Create Global Post
            int sId;
            string content;
            cout << "Enter Your Student ID: ";
            cin >> sId;
            cin.ignore();
            Student* s = store.findStudentById(sId);
            if (!s) {
                cout << "Student not found." << endl;
            } else {
                cout << "Enter Post Content: ";
                getline(cin, content);
                Post p(store.getNextPostId(), sId, s->getName(), content, getCurrentTime());
                store.addPost(p);
                cout << "Post published!" << endl;
            }

        } else if (choice == 12) {
            store.displayAllPosts();

        } else if (choice == 13) {
            // Post in Group
            int gId, sId;
            string content;
            cout << "Enter Group ID: ";
            cin >> gId;
            cout << "Enter Your Student ID: ";
            cin >> sId;
            cin.ignore();
            Group* g = store.findGroupById(gId);
            Student* s = store.findStudentById(sId);
            if (!g) cout << "Group not found." << endl;
            else if (!s) cout << "Student not found." << endl;
            else if (!g->isMember(sId)) cout << "You are not a member of this group." << endl;
            else {
                cout << "Enter Post Content: ";
                getline(cin, content);
                Post p(store.getNextPostId(), sId, s->getName(), content, getCurrentTime());
                g->addPost(p);
                cout << "Posted in group '" << g->getGroupName() << "'!" << endl;
            }

        } else if (choice == 14) {
            // View Group Posts
            int gId;
            cout << "Enter Group ID: ";
            cin >> gId;
            cin.ignore();
            Group* g = store.findGroupById(gId);
            if (!g) cout << "Group not found." << endl;
            else g->displayPosts();

        } else if (choice != 0) {
            cout << "Invalid choice. Try again." << endl;
        }

    } while (choice != 0);

    cout << "Goodbye! See you on Campus!" << endl;
    return 0;
}
