#include "../include/DataStore.h"
#include <iostream>
#include <algorithm>
using namespace std;
// Add id under key, skipping it if the same student already listed it
// (e.g. skills {"C++", "C++"}). Ids for one student are added together,
// so a duplicate is always the last element: O(1) check.
void DataStore::indexId(unordered_map<string, vector<int>>& index, const string& key, int id) {
    vector<int>& ids = index[key];
    if (ids.empty() || ids.back() != id) ids.push_back(id);
}

// Remove id from the list stored under key (drop the key if it becomes empty)
void DataStore::unindexId(unordered_map<string, vector<int>>& index, const string& key, int id) {
    auto it = index.find(key);
    if (it == index.end()) return;
    vector<int>& ids = it->second;
    ids.erase(remove(ids.begin(), ids.end(), id), ids.end());
    if (ids.empty()) index.erase(it);
}

void DataStore::addStudent(Student student) {
    int id = student.getId();

    // Re-adding an existing ID updates that student: first remove the old
    // index entries, otherwise searches return them for skills they no
    // longer have (and list them twice for skills they kept).
    auto old = students.find(id);
    if (old != students.end()) {
        for (const string& skill : old->second.getSkills()) unindexId(skillIndex, skill, id);
        for (const string& interest : old->second.getInterests()) unindexId(interestIndex, interest, id);
        unindexId(departmentIndex, old->second.getDepartment(), id);
    }

    students[id] = student;
    for (const string& skill : student.getSkills()) {
        indexId(skillIndex, skill, id);
    }

    for (const string& interest : student.getInterests()) {
        indexId(interestIndex, interest, id);
    }

    indexId(departmentIndex, student.getDepartment(), id);

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
void DataStore::addGroup(const Group& group) {
    int id = group.getGroupId();
    groups[id] = unique_ptr<Group>(group.clone());  // O(1) insert, keeps the real type
    cout << "Group '" << group.getGroupName() << "' created (ID: " << id << ")." << endl;
}

Group* DataStore::findGroupById(int id) {
    auto it = groups.find(id);
    if (it != groups.end()) return it->second.get();
    return nullptr;
}

void DataStore::displayAllGroups() const {
    if (groups.empty()) {
        cout << "No groups created yet." << endl;
        return;
    }
    cout << "\n===== ALL GROUPS =====" << endl;
    for (auto& pair : groups) {
        pair.second->display();  // virtual: ProjectGroup shows leader + linked post
    }
}
void DataStore::addPost(const Post& post) {
    posts[post.getPostId()] = unique_ptr<Post>(post.clone());  // O(1) insert, keeps the real type
}

void DataStore::displayAllPosts() const {
    if (posts.empty()) {
        cout << "No posts yet." << endl;
        return;
    }
    cout << "\n===== ALL POSTS =====" << endl;
    for (auto& pair : posts) {
        pair.second->display();  // virtual: ProjectPost shows title + required skills
    }
}

int DataStore::getNextPostId() {
    return nextPostId++;
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
        result.push_back(*pair.second);
    }
    return result;
}

vector<Post> DataStore::getAllPosts() const {
    vector<Post> result;
    for (const auto& pair : posts) {
        result.push_back(*pair.second);
    }
    return result;
}

Post* DataStore::findPostById(int id) {
    auto it = posts.find(id);
    if (it != posts.end()) return it->second.get();
    return nullptr;
}
// Add a message
void DataStore::addMessage(Message message) {
    messages.push_back(message);
}

// Get all messages between two students in chronological order
vector<Message> DataStore::getConversation(int studentA, int studentB) const {
    vector<Message> conversation;
    for (const auto& m : messages) {
        if ((m.getSenderId() == studentA && m.getReceiverId() == studentB) ||
            (m.getSenderId() == studentB && m.getReceiverId() == studentA)) {
            conversation.push_back(m);
        }
    }
    return conversation;
}

// Get all unique student IDs that studentId has chatted with
vector<int> DataStore::getChatPartners(int studentId) const {
    vector<int> partners;
    for (const auto& m : messages) {
        int partner = 0;
        if (m.getSenderId() == studentId) partner = m.getReceiverId();
        else if (m.getReceiverId() == studentId) partner = m.getSenderId();

        if (partner != 0) {
            // Add if not already in list
            if (find(partners.begin(), partners.end(), partner) == partners.end()) {
                partners.push_back(partner);
            }
        }
    }
    return partners;
}

// Get next message ID
int DataStore::getNextMessageId() {
    return nextMessageId++;
}

// Get all messages
vector<Message> DataStore::getAllMessages() const {
    return messages;
}

