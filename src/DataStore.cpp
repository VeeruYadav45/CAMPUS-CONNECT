#include "../include/DataStore.h"
#include <iostream>
using namespace std;

DataStore::~DataStore() {
    for (auto& pair : posts) {
        delete pair.second;
    }
    posts.clear();
}

void DataStore::clearAll() {
    students.clear();
    groups.clear();
    for (auto& pair : posts) {
        delete pair.second;
    }
    posts.clear();
    skillIndex.clear();
    interestIndex.clear();
    departmentIndex.clear();
    trie = Trie();
    nextPostId = 1;
}

void DataStore::addStudent(Student student) {
    int id = student.getId();
    students[id] = student;

    // Build inverted index for skills and insert into Trie
    for (const string& skill : student.getSkills()) {
        skillIndex[skill].push_back(id);
        trie.insert(skill, -id); // negative ID represents student
    }

    // Build inverted index for interests and insert into Trie
    for (const string& interest : student.getInterests()) {
        interestIndex[interest].push_back(id);
        trie.insert(interest, -id);
    }

    // Build inverted index for department
    departmentIndex[student.getDepartment()].push_back(id);

    cout << "Student '" << student.getName() << "' added (ID: " << id << ")." << endl;
}

Student* DataStore::findStudentById(int id) {
    auto it = students.find(id);
    if (it != students.end()) return &it->second;
    return nullptr;
}

void DataStore::displayAllStudents() const {
    if (students.empty()) {
        cout << "No students registered yet." << endl;
        return;
    }
    cout << "\n===== ALL STUDENTS =====" << endl;
    for (auto& pair : students) {
        const_cast<Student&>(pair.second).display();
    }
}

void DataStore::addGroup(Group group) {
    int id = group.getGroupId();
    groups[id] = group;
    cout << "Group '" << group.getGroupName() << "' created (ID: " << id << ")." << endl;
}

Group* DataStore::findGroupById(int id) {
    auto it = groups.find(id);
    if (it != groups.end()) return &it->second;
    return nullptr;
}

void DataStore::displayAllGroups() const {
    if (groups.empty()) {
        cout << "No groups created yet." << endl;
        return;
    }
    cout << "\n===== ALL GROUPS =====" << endl;
    for (auto& pair : groups) {
        pair.second.display();
    }
}

void DataStore::addPost(Post post) {
    int pid = post.getPostId();
    if (posts.find(pid) != posts.end()) {
        delete posts[pid];
    }
    posts[pid] = new Post(post);

    // Index content words in Trie
    string content = post.getContent();
    string word = "";
    for (size_t i = 0; i < content.size(); i++) {
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

void DataStore::addHelpRequest(HelpRequest* req) {
    int pid = req->getPostId();
    if (posts.find(pid) != posts.end()) {
        delete posts[pid];
    }
    posts[pid] = req;

    trie.insert(req->getRequiredSkill(), pid);

    string content = req->getContent();
    string word = "";
    for (size_t i = 0; i < content.size(); i++) {
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

void DataStore::addHelpOffer(HelpOffer* offer) {
    int pid = offer->getPostId();
    if (posts.find(pid) != posts.end()) {
        delete posts[pid];
    }
    posts[pid] = offer;

    trie.insert(offer->getOfferedSkill(), pid);

    string content = offer->getContent();
    string word = "";
    for (size_t i = 0; i < content.size(); i++) {
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

void DataStore::displayAllPosts() const {
    if (posts.empty()) {
        cout << "No posts yet." << endl;
        return;
    }
    cout << "\n===== ALL POSTS =====" << endl;
    for (auto& pair : posts) {
        if (pair.second) {
            pair.second->display();
        }
    }
}

void DataStore::displayHelpRequests() const {
    bool found = false;
    cout << "\n===== HELP REQUESTS =====" << endl;
    for (auto& pair : posts) {
        if (pair.second && pair.second->getType() == "Help Request") {
            pair.second->display();
            found = true;
        }
    }
    if (!found) cout << "No help requests yet." << endl;
}

void DataStore::displayHelpOffers() const {
    bool found = false;
    cout << "\n===== HELP OFFERS =====" << endl;
    for (auto& pair : posts) {
        if (pair.second && pair.second->getType() == "Help Offer") {
            pair.second->display();
            found = true;
        }
    }
    if (!found) cout << "No help offers yet." << endl;
}

int DataStore::getNextPostId() {
    return nextPostId++;
}

void DataStore::searchByKeyword(const string& keyword) const {
    vector<int> postIds = trie.getPostIds(keyword);

    if (postIds.empty()) {
        cout << "No posts found for keyword \"" << keyword << "\"." << endl;
        return;
    }

    cout << "\n===== POSTS matching \"" << keyword << "\" =====" << endl;
    for (int pid : postIds) {
        if (pid > 0) {
            auto it = posts.find(pid);
            if (it != posts.end() && it->second) {
                it->second->display();
            }
        }
    }
}

void DataStore::autocompleteSearch(const string& prefix) const {
    vector<string> suggestions = trie.autocomplete(prefix);

    if (suggestions.empty()) {
        cout << "No suggestions found starting with \"" << prefix << "\"." << endl;
        return;
    }

    cout << "\n===== Suggestions for \"" << prefix << "\" =====" << endl;
    for (size_t i = 0; i < suggestions.size(); i++) {
        cout << "  " << i + 1 << ". " << suggestions[i] << endl;
    }

    vector<int> postIds = trie.searchByPrefix(prefix);
    if (!postIds.empty()) {
        cout << "\n--- Matching Posts ---" << endl;
        for (int pid : postIds) {
            if (pid > 0) {
                auto it = posts.find(pid);
                if (it != posts.end() && it->second) {
                    it->second->display();
                }
            }
        }
    }
}

void DataStore::displayTrieKeywords() const {
    trie.displayAllWords();
}

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

vector<Student> DataStore::getAllStudents() const {
    vector<Student> result;
    for (const auto& pair : students) {
        result.push_back(pair.second);
    }
    return result;
}

vector<Group> DataStore::getAllGroups() const {
    vector<Group> result;
    for (const auto& pair : groups) {
        result.push_back(pair.second);
    }
    return result;
}

vector<Post> DataStore::getAllPosts() const {
    vector<Post> result;
    for (const auto& pair : posts) {
        if (pair.second) {
            result.push_back(*(pair.second));
        }
    }
    return result;
}

Post* DataStore::findPostById(int id) {
    auto it = posts.find(id);
    if (it != posts.end()) return it->second;
    return nullptr;
}
