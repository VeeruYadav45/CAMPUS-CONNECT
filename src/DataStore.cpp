#include "../include/DataStore.h"
#include <iostream>
using namespace std;
void DataStore::addStudent(Student student) {
    int id = student.getId();
    students[id] = student;
    // Build invertedindex for skills
    for (const string& skill : student.getSkills()) {
        skillIndex[skill].push_back(id);
    }
    // Build inverted index for interests
    for (const string& interest : student.getInterests()) {
        interestIndex[interest].push_back(id);
    }

    // Build inverted index for department
    departmentIndex[student.getDepartment()].push_back(id);

    cout << "Student '" << student.getName() << "' added (ID: " << id << ")." << endl;
}

// O(1) average lookup using hash
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
    groups[id] = group;  // O(1) insert
    cout << "Group '" << group.getGroupName() << "' created (ID: " << id << ")." << endl;
}

// O(1) average lookup using hash
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
    posts[post.getPostId()] = post;  // O(1) insert
}

void DataStore::displayAllPosts() const {
    if (posts.empty()) {
        cout << "No posts yet." << endl;
        return;
    }
    cout << "\n===== ALL POSTS =====" << endl;
    for (auto& pair : posts) {
        pair.second.display();
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
