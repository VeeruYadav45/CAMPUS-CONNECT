#ifndef DATASTORE_H
#define DATASTORE_H

#include <vector>
#include <string>
#include <unordered_map>
#include "student.h"
#include "Group.h"
#include "post.h"
using namespace std;

class DataStore {
private:
    // Hash maps: key = ID, value = object (O(1) lookup)
    unordered_map<int, Student> students;        // studentId → Student
    unordered_map<int, Group>   groups;          // groupId   → Group
    unordered_map<int, Post>    posts;           // postId    → Post

    // Inverted index maps for fast search (O(1) average)
    unordered_map<string, vector<int>> skillIndex;       // skill      → [studentIds]
    unordered_map<string, vector<int>> interestIndex;    // interest   → [studentIds]
    unordered_map<string, vector<int>> departmentIndex;  // department → [studentIds]

    int nextPostId;

public:
    DataStore() : nextPostId(1) {}

    // Student operations
    void addStudent(Student student);
    Student* findStudentById(int id);         // O(1) hash lookup
    void displayAllStudents() const;

    // Group operations
    void addGroup(Group group);
    Group* findGroupById(int id);             // O(1) hash lookup
    void displayAllGroups() const;

    // Post operations
    void addPost(Post post);
    void displayAllPosts() const;
    int getNextPostId();

    // Search using inverted index (O(1) average)
    vector<Student> matchBySkill(string skill);
    vector<Student> matchByInterest(string interest);
    vector<Student> matchByDepartment(string department);

    // Get all students (for Jaccard comparison)
    vector<Student> getAllStudents() const;
};

#endif
