#include "../include/DataStore.h"
#include <iostream>
using namespace std;
void DataStore::addStudent(Student student) {
  int id = student.getId();
  students[id] = student;

  for (const string &skill : student.getSkills()) {
    skillIndex[skill].push_back(id);

    trie.insert(skill, -id); // ID = student (to differentiate from posts)
  }

  for (const string &interest : student.getInterests()) {
    interestIndex[interest].push_back(id);

    // Also insert interests into TRIE
    trie.insert(interest, -id);
  }

  // Build inverted index for department
  departmentIndex[student.getDepartment()].push_back(id);

  cout << "Student '" << student.getName() << "' added (ID: " << id << ")."
       << endl;
}

// O(1) average lookup using hash
Student *DataStore::findStudentById(int id) {
  auto it = students.find(id);
  if (it != students.end())
    return &it->second;
  return nullptr;
}

void DataStore::displayAllStudents() const {
  if (students.empty()) {
    cout << "No students registered yet." << endl;
    return;
  }
  cout << "\n===== ALL STUDENTS =====" << endl;
  for (auto &pair : students) {
    const_cast<Student &>(pair.second).display();
  }
}
void DataStore::addGroup(Group group) {
  int id = group.getGroupId();
  groups[id] = group;
  cout << "Group '" << group.getGroupName() << "' created (ID: " << id << ")."
       << endl;
}
Group *DataStore::findGroupById(int id) {
  auto it = groups.find(id);
  if (it != groups.end())
    return &it->second;
  return nullptr;
}

void DataStore::displayAllGroups() const {
  if (groups.empty()) {
    cout << "No groups created yet." << endl;
    return;
  }
  cout << "\n===== ALL GROUPS =====" << endl;
  for (auto &pair : groups) {
    pair.second.display();
  }
}
void DataStore::addHelpRequest(HelpRequest *req) {
  int pid = req->getPostId();
  posts[pid] = req; // stored as Post* → POLYMORPHISM!

  // Index the required skill in the Trie
  trie.insert(req->getRequiredSkill(), pid);

  // Also index words from the content for full-text search
  // Split content by spaces and insert each word
  string content = req->getContent();
  string word = "";
  for (int i = 0; i < content.size(); i++) {
    if (content[i] == ' ') {
      if (word.size() >= 3) { // only index words with 3+ characters
        trie.insert(word, pid);
      }
      word = "";
    } else {
      word += content[i];
    }
  }
  if (word.size() >= 3) {
    trie.insert(word, pid);
  }
}

// ── Add a HelpOffer post ──
void DataStore::addHelpOffer(HelpOffer *offer) {
  int pid = offer->getPostId();
  posts[pid] = offer; // stored as Post* → POLYMORPHISM!

  // Index the offered skill in the Trie
  trie.insert(offer->getOfferedSkill(), pid);

  // Also index words from the content
  string content = offer->getContent();
  string word = "";
  for (int i = 0; i < content.size(); i++) {
    if (content[i] == ' ') {
      if (word.size() >= 3) {
        trie.insert(word, pid);
      }
      word = "";
    } else {
      word += content[i];
    }
  }
  if (word.size() >= 3) {
    trie.insert(word, pid);
  }
}

// ── Display ALL posts (polymorphic call) ──
// Because display() is virtual, it calls the correct version
// based on actual type (HelpRequest or HelpOffer)
void DataStore::displayAllPosts() const {
  if (posts.empty()) {
    cout << "No posts yet." << endl;
    return;
  }
  cout << "\n===== ALL POSTS =====" << endl;
  for (auto &pair : posts) {
    pair.second->display(); // POLYMORPHISM: calls correct display()
  }
}

// ── Display only HelpRequest posts ──
void DataStore::displayHelpRequests() const {
  bool found = false;
  cout << "\n===== HELP REQUESTS =====" << endl;
  for (auto &pair : posts) {
    if (pair.second->getType() == "HelpRequest") {
      pair.second->display();
      found = true;
    }
  }
  if (!found)
    cout << "No help requests yet." << endl;
}

// ── Display only HelpOffer posts ──
void DataStore::displayHelpOffers() const {
  bool found = false;
  cout << "\n===== HELP OFFERS =====" << endl;
  for (auto &pair : posts) {
    if (pair.second->getType() == "HelpOffer") {
      pair.second->display();
      found = true;
    }
  }
  if (!found)
    cout << "No help offers yet." << endl;
}

int DataStore::getNextPostId() { return nextPostId++; }

// ══════════════════════════════════════════════════
//      TRIE SEARCH OPERATIONS (Vivekanand's DSA)
// ══════════════════════════════════════════════════

// ── Search posts by exact keyword ──
void DataStore::searchByKeyword(const string &keyword) const {
  vector<int> postIds = trie.getPostIds(keyword);

  if (postIds.empty()) {
    cout << "No posts found for keyword: \"" << keyword << "\"" << endl;
    return;
  }

  cout << "\n===== POSTS matching \"" << keyword << "\" =====" << endl;
  for (int pid : postIds) {
    if (pid > 0) { // positive ID = post
      auto it = posts.find(pid);
      if (it != posts.end()) {
        it->second->display(); // POLYMORPHISM!
      }
    }
  }
}

// ── Autocomplete: show suggestions as user types ──
void DataStore::autocompleteSearch(const string &prefix) const {
  vector<string> suggestions = trie.autocomplete(prefix);

  if (suggestions.empty()) {
    cout << "No suggestions for: \"" << prefix << "\"" << endl;
    return;
  }

  cout << "\n===== Suggestions for \"" << prefix << "\" =====" << endl;
  for (int i = 0; i < suggestions.size(); i++) {
    cout << "  " << i + 1 << ". " << suggestions[i] << endl;
  }

  // Also show matching posts
  vector<int> postIds = trie.searchByPrefix(prefix);
  if (!postIds.empty()) {
    cout << "\n--- Matching Posts ---" << endl;
    for (int pid : postIds) {
      if (pid > 0) {
        auto it = posts.find(pid);
        if (it != posts.end()) {
          it->second->display();
        }
      }
    }
  }
}

// ── Show all indexed keywords ──
void DataStore::displayTrieKeywords() const { trie.displayAllWords(); }

// ══════════════════════════════════════════════════
//       INVERTED INDEX SEARCH (Veeru's part)
// ══════════════════════════════════════════════════

vector<Student> DataStore::matchBySkill(string skill) {
  vector<Student> result;
  auto it = skillIndex.find(skill);
  if (it != skillIndex.end()) {
    for (int id : it->second) {
      auto s = students.find(id);
      if (s != students.end())
        result.push_back(s->second);
    }
  }
  return result;
}

vector<Student> DataStore::matchByInterest(string interest) {
  vector<Student> result;
  auto it = interestIndex.find(interest);
  if (it != interestIndex.end()) {
    for (int id : it->second) {
      auto s = students.find(id);
      if (s != students.end())
        result.push_back(s->second);
    }
  }
  return result;
}

vector<Student> DataStore::matchByDepartment(string department) {
  vector<Student> result;
  auto it = departmentIndex.find(department);
  if (it != departmentIndex.end()) {
    for (int id : it->second) {
      auto s = students.find(id);
      if (s != students.end())
        result.push_back(s->second);
    }
  }
  return result;
}
