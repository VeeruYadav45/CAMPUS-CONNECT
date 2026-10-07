#include "include/DataStore.h"
#include "include/Graph.h"
#include "include/Group.h"
#include "include/JaccardMatching.h"
#include "include/ProjectGroup.h"
#include "include/ProjectPost.h"
#include "include/TopKRanker.h"
#include "include/post.h"
#include "include/student.h"
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
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
  cout << "15. Find Similar Students (Jaccard)" << endl;
  cout << "16. Create Project Post (Find Teammates)" << endl;
  cout << "17. Top-K Ranked Matches (Max-Heap)" << endl;
  cout << "18. Top-K via BFS + Jaccard (Max-Heap)" << endl;
  cout << "19. Create Project Group from Post" << endl;
  cout << "20. Run Full Demo (Auto)" << endl;
  cout << "0. Exit" << endl;
  cout << "================================" << endl;
  cout << "Enter choice: ";
}

// -------------------------------------------------------
// runFullDemo: Automated demo that shows the complete flow:
//   1. Register 8 sample students with overlapping skills
//   2. Create regular posts and a ProjectPost
//   3. Build a graph with connections
//   4. Show Top-K ranking using max-heap
//   5. Show Top-K via BFS + Jaccard
//   6. Create a ProjectGroup with top matches
// -------------------------------------------------------
void runFullDemo(DataStore &store, Graph &graph, int &nextStudentId,
                 int &nextGroupId) {
  cout << "\n============================================" << endl;
  cout << "     CAMPUS-CONNECT: FULL DEMO (AUTO)       " << endl;
  cout << "============================================" << endl;

  // --- Step 1: Register 8 sample students ---
  cout << "\n--- Step 1: Registering Sample Students ---" << endl;

  Student s1(nextStudentId++, "Tanvay Jain", {"C++", "Python", "DSA"},
             {"AI", "Web Dev"}, "CSE", 2);
  store.addStudent(s1);

  Student s2(nextStudentId++, "Ravi Kumar", {"Python", "C++", "ML"},
             {"AI", "Data Science"}, "CSE", 2);
  store.addStudent(s2);

  Student s3(nextStudentId++, "Priya Sharma", {"Java", "Python", "Web Dev"},
             {"Web Dev", "Open Source"}, "CSE", 3);
  store.addStudent(s3);

  Student s4(nextStudentId++, "Ankit Singh", {"C++", "Embedded", "IoT"},
             {"Robotics", "IoT"}, "ECE", 2);
  store.addStudent(s4);

  Student s5(nextStudentId++, "Neha Gupta", {"Python", "DSA", "ML"},
             {"AI", "Competitive Programming"}, "CSE", 2);
  store.addStudent(s5);

  Student s6(nextStudentId++, "Arjun Mehta", {"C++", "DSA", "Python"},
             {"Competitive Programming", "AI"}, "CSE", 3);
  store.addStudent(s6);

  Student s7(nextStudentId++, "Sanya Verma", {"Java", "Spring", "SQL"},
             {"Backend Dev", "Cloud"}, "IT", 3);
  store.addStudent(s7);

  Student s8(nextStudentId++, "Karan Patel", {"C++", "DSA", "System Design"},
             {"AI", "Open Source"}, "CSE", 2);
  store.addStudent(s8);

  // --- Step 2: Create a regular post and a ProjectPost ---
  cout << "\n--- Step 2: Creating Posts ---" << endl;

  Post regularPost(store.getNextPostId(), s1.getId(), s1.getName(),
                   "Hello everyone! Excited to connect on Campus-Connect!",
                   getCurrentTime());
  store.addPost(regularPost);
  cout << "Regular post created by " << s1.getName() << "." << endl;

  ProjectPost projPost(store.getNextPostId(), s1.getId(), s1.getName(),
                       "AI Study Group Project", {"Python", "ML", "DSA"}, 3,
                       getCurrentTime());
  store.addPost(projPost);
  cout << "ProjectPost created by " << s1.getName() << ":" << endl;
  projPost.display();

  // --- Step 3: Build the social graph with connections ---
  cout << "\n--- Step 3: Building Social Graph ---" << endl;

  // Add all students to the graph
  for (int i = s1.getId(); i <= s8.getId(); i++) {
    graph.addStudent(i);
  }

  // Add connections (friendships / study-buddy links)
  graph.addConnection(s1.getId(), s2.getId()); // Tanvay -- Ravi
  graph.addConnection(s1.getId(), s5.getId()); // Tanvay -- Neha
  graph.addConnection(s2.getId(), s5.getId()); // Ravi -- Neha
  graph.addConnection(s2.getId(), s6.getId()); // Ravi -- Arjun
  graph.addConnection(s3.getId(), s7.getId()); // Priya -- Sanya
  graph.addConnection(s5.getId(), s6.getId()); // Neha -- Arjun
  graph.addConnection(s5.getId(), s8.getId()); // Neha -- Karan
  graph.addConnection(s6.getId(), s8.getId()); // Arjun -- Karan
  graph.addConnection(s4.getId(), s1.getId()); // Ankit -- Tanvay

  cout << "Graph connections:" << endl;
  graph.display();

  // --- Step 4: Top-K ranking using MAX-HEAP (all students) ---
  cout << "\n--- Step 4: Top-K Matches for " << s1.getName()
       << " (All Students) ---" << endl;

  JaccardMatching matcher;
  vector<Student> allStudents = store.getAllStudents();
  int k = 5;

  cout << "Using Priority Queue (Max-Heap) with K = " << k << endl;
  vector<RankedResult> topAll =
      TopKRanker::getTopK(s1, allStudents, matcher, k);
  TopKRanker::displayResults(topAll);

  // --- Step 5: Top-K via BFS + Jaccard ---
  cout << "\n--- Step 5: Top-K via BFS + Jaccard for " << s1.getName() << " ---"
       << endl;
  cout << "BFS finds connected students first, then ranks them." << endl;

  vector<RankedResult> topBFS =
      TopKRanker::getTopKUsingBFS(s1, graph, store, matcher, k);
  TopKRanker::displayResults(topBFS);

  // --- Step 6: Create a ProjectGroup with top matches ---
  cout << "\n--- Step 6: Creating Project Group ---" << endl;

  ProjectGroup pGroup(nextGroupId++, "AI Study Group",
                      "A team for the AI Study Group Project",
                      s1.getId(), // Tanvay is the leader
                      projPost.getPostId());

  // Add top 2 matches as team members
  int addCount = 0;
  for (int i = 0; i < (int)topBFS.size() && addCount < 2; i++) {
    pGroup.addMember(topBFS[i].studentId);
    addCount++;
  }

  cout << "\nProject Group details:" << endl;
  pGroup.display();

  // Also store it in DataStore
  store.addGroup(pGroup);

  cout << "\n============================================" << endl;
  cout << "     FULL DEMO COMPLETE!                     " << endl;
  cout << "============================================" << endl;
}

int main() {
  DataStore store;
  Graph graph;
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
      Student *s = store.findStudentById(id);
      if (s)
        s->display();
      else
        cout << "Student not found." << endl;

    } else if (choice == 4) {
      string skill;
      cout << "Enter Skill to search: ";
      getline(cin, skill);
      vector<Student> result = store.matchBySkill(skill);
      if (result.empty())
        cout << "No students found with skill: " << skill << endl;
      else
        for (Student &s : result)
          s.display();

    } else if (choice == 5) {
      string interest;
      cout << "Enter Interest to search: ";
      getline(cin, interest);
      vector<Student> result = store.matchByInterest(interest);
      if (result.empty())
        cout << "No students found with interest: " << interest << endl;
      else
        for (Student &s : result)
          s.display();

    } else if (choice == 6) {
      string dept;
      cout << "Enter Department to search: ";
      getline(cin, dept);
      vector<Student> result = store.matchByDepartment(dept);
      if (result.empty())
        cout << "No students found in department: " << dept << endl;
      else
        for (Student &s : result)
          s.display();

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

      Student *admin = store.findStudentById(adminId);
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
      Group *g = store.findGroupById(gId);
      if (!g)
        cout << "Group not found." << endl;
      else
        g->addMember(sId);

    } else if (choice == 10) {
      // Leave Group
      int gId, sId;
      cout << "Enter Group ID: ";
      cin >> gId;
      cout << "Enter Your Student ID: ";
      cin >> sId;
      cin.ignore();
      Group *g = store.findGroupById(gId);
      if (!g)
        cout << "Group not found." << endl;
      else
        g->removeMember(sId);

    } else if (choice == 11) {
      // Create Global Post
      int sId;
      string content;
      cout << "Enter Your Student ID: ";
      cin >> sId;
      cin.ignore();
      Student *s = store.findStudentById(sId);
      if (!s) {
        cout << "Student not found." << endl;
      } else {
        cout << "Enter Post Content: ";
        getline(cin, content);
        Post p(store.getNextPostId(), sId, s->getName(), content,
               getCurrentTime());
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
      Group *g = store.findGroupById(gId);
      Student *s = store.findStudentById(sId);
      if (!g)
        cout << "Group not found." << endl;
      else if (!s)
        cout << "Student not found." << endl;
      else if (!g->isMember(sId))
        cout << "You are not a member of this group." << endl;
      else {
        cout << "Enter Post Content: ";
        getline(cin, content);
        Post p(store.getNextPostId(), sId, s->getName(), content,
               getCurrentTime());
        g->addPost(p);
        cout << "Posted in group '" << g->getGroupName() << "'!" << endl;
      }

    } else if (choice == 14) {
      // View Group Posts
      int gId;
      cout << "Enter Group ID: ";
      cin >> gId;
      cin.ignore();
      Group *g = store.findGroupById(gId);
      if (!g)
        cout << "Group not found." << endl;
      else
        g->displayPosts();

    } else if (choice == 15) {
      // Find Similar Students using Jaccard Similarity
      int sId;
      cout << "Enter Your Student ID: ";
      cin >> sId;
      cin.ignore();
      Student *target = store.findStudentById(sId);
      if (!target) {
        cout << "Student not found." << endl;
      } else {
        JaccardMatching matcher;
        vector<Student> all = store.getAllStudents();
        cout << "\n===== SIMILAR STUDENTS (Jaccard) ====" << endl;
        cout << "Comparing with: " << target->getName() << endl;
        cout << "--------------------------------------" << endl;
        bool anyFound = false;
        for (Student &s : all) {
          if (s.getId() == sId)
            continue;
          double score = matcher.calculateSimilarity(*target, s);
          if (score > 0) {
            anyFound = true;
            cout << "Student: " << s.getName() << " | ID: " << s.getId()
                 << " | Similarity: " << (int)(score * 100) << "%" << endl;
          }
        }
        if (!anyFound)
          cout << "No similar students found." << endl;
      }

    } else if (choice == 16) {
      // Create Project Post (Tanvay's feature)
      int sId, teamSize, numSkills;
      string title;
      vector<string> reqSkills;

      cout << "Enter Your Student ID: ";
      cin >> sId;
      cin.ignore();
      Student *s = store.findStudentById(sId);
      if (!s) {
        cout << "Student not found." << endl;
      } else {
        cout << "Enter Project Title: ";
        getline(cin, title);
        cout << "How many team members needed? ";
        cin >> teamSize;
        cout << "How many required skills? ";
        cin >> numSkills;
        cin.ignore();
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

    } else if (choice == 17) {
      // Top-K Ranked Matches using Max-Heap (all students)
      int sId, k;
      cout << "Enter Your Student ID: ";
      cin >> sId;
      cout << "Enter K (how many top matches): ";
      cin >> k;
      cin.ignore();

      Student *target = store.findStudentById(sId);
      if (!target) {
        cout << "Student not found." << endl;
      } else {
        JaccardMatching matcher;
        vector<Student> all = store.getAllStudents();
        cout << "\n===== TOP-" << k << " MATCHES (Max-Heap) =====" << endl;
        cout << "Finding matches for: " << target->getName() << endl;
        vector<RankedResult> results =
            TopKRanker::getTopK(*target, all, matcher, k);
        TopKRanker::displayResults(results);
      }

    } else if (choice == 18) {
      // Top-K via BFS + Jaccard (graph-scoped)
      int sId, k;
      cout << "Enter Your Student ID: ";
      cin >> sId;
      cout << "Enter K (how many top matches): ";
      cin >> k;
      cin.ignore();

      Student *target = store.findStudentById(sId);
      if (!target) {
        cout << "Student not found." << endl;
      } else {
        JaccardMatching matcher;
        cout << "\n===== TOP-" << k << " MATCHES (BFS + Heap) =====" << endl;
        cout << "BFS from: " << target->getName() << ", then ranking..."
             << endl;
        vector<RankedResult> results =
            TopKRanker::getTopKUsingBFS(*target, graph, store, matcher, k);
        TopKRanker::displayResults(results);
      }

    } else if (choice == 19) {
      // Create Project Group from top matches
      int sId, postId, teamSize;
      string gName;
      cout << "Enter Your Student ID (Leader): ";
      cin >> sId;
      cout << "Enter ProjectPost ID to link: ";
      cin >> postId;
      cin.ignore();
      cout << "Enter Group Name: ";
      getline(cin, gName);
      cout << "How many top matches to auto-add? ";
      cin >> teamSize;
      cin.ignore();

      Student *leader = store.findStudentById(sId);
      if (!leader) {
        cout << "Student not found." << endl;
      } else {
        ProjectGroup pg(nextGroupId++, gName,
                        "Project team for post #" + to_string(postId), sId,
                        postId);

        // Auto-add top matches
        JaccardMatching matcher;
        vector<Student> all = store.getAllStudents();
        vector<RankedResult> results =
            TopKRanker::getTopK(*leader, all, matcher, teamSize);

        for (int i = 0; i < (int)results.size(); i++) {
          pg.addMember(results[i].studentId);
        }

        pg.display();
        store.addGroup(pg);
      }

    } else if (choice == 20) {
      // Run full automated demo
      runFullDemo(store, graph, nextStudentId, nextGroupId);

    } else if (choice != 0) {
      cout << "Invalid choice. Try again." << endl;
    }

  } while (choice != 0);

  cout << "Goodbye! See you on Campus!" << endl;
  return 0;
}
