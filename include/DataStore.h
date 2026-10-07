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
    unordered_map<int, Student> students;     
    unordered_map<int, Group>   groups;
    unordered_map<int, Post*>   posts;        
        unordered_map<string, vector<int>> skillIndex;      
    unordered_map<string, vector<int>> interestIndex;  
    unordered_map<string, vector<int>> departmentIndex;  
    Trie trie;
    int nextPostId;
public:
    DataStore() : nextPostId(1) {}
    void addStudent(Student student);
    Student* findStudentById(int id);      
    void displayAllStudents() const;
    void addGroup(Group group);
    Group* findGroupById(int id);          
    void displayAllGroups() const;
    void addHelpRequest(HelpRequest* req);  
    void addHelpOffer(HelpOffer* offer);     
    void displayAllPosts() const;
    void displayHelpRequests() const;         
    void displayHelpOffers() const;          
    int getNextPostId();
    void searchByKeyword(const string& keyword) const;
    void autocompleteSearch(const string& prefix) const;
    void displayTrieKeywords() const;
    vector<Student> matchBySkill(string skill);
    vector<Student> matchByInterest(string interest);
    vector<Student> matchByDepartment(string department);
};
#endif
