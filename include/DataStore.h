#ifndef DATASTORE_H
#define DATASTORE_H

#include <vector>
#include <string>
#include <unordered_map>
#include "student.h"
#include "Group.h"
#include "post.h"
#include "HelpRequest.h"
#include "HelpOffer.h"
#include "Trie.h"

using namespace std;

class DataStore {
private:
    // Hash maps: key = ID, value = object / pointer (O(1) lookup)
    unordered_map<int, Student> students;        // studentId → Student
    unordered_map<int, Group>   groups;          // groupId   → Group
    unordered_map<int, Post*>   posts;           // postId    → Post* (polymorphism)

    // Inverted index maps for fast search (O(1) average)
    unordered_map<string, vector<int>> skillIndex;       // skill      → [studentIds]
    unordered_map<string, vector<int>> interestIndex;    // interest   → [studentIds]
    unordered_map<string, vector<int>> departmentIndex;  // department → [studentIds]

    Trie trie;
    int nextPostId;

public:
    DataStore() : nextPostId(1) {}
    ~DataStore();

    void clearAll();

    // Student operations
    void addStudent(Student student);
    Student* findStudentById(int id);
    void displayAllStudents() const;

    // Group operations
    void addGroup(Group group);
    Group* findGroupById(int id);
    void displayAllGroups() const;

    // Post operations
    void addPost(Post post);
    void addHelpRequest(HelpRequest* req);
    void addHelpOffer(HelpOffer* offer);
    void displayAllPosts() const;
    void displayHelpRequests() const;
    void displayHelpOffers() const;
    int getNextPostId();

    // Trie search operations (Vivekanand)
    void searchByKeyword(const string& keyword) const;
    void autocompleteSearch(const string& prefix) const;
    void displayTrieKeywords() const;
    vector<int> searchPostsByPrefix(const string& prefix) const {
        return trie.searchByPrefix(prefix);
    }

    // Inverted index operations (Veeru)
    vector<Student> matchBySkill(string skill);
    vector<Student> matchByInterest(string interest);
    vector<Student> matchByDepartment(string department);

    // Get all items (API layer / Jaccard)
    vector<Student> getAllStudents() const;
    vector<Group> getAllGroups() const;
    vector<Post> getAllPosts() const;
    Post* findPostById(int id);
};

#endif
