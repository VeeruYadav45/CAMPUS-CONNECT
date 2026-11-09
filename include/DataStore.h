#ifndef DATASTORE_H
#define DATASTORE_H
#include "Message.h"
#include <vector>
#include <string>
#include <unordered_map>
#include "student.h"
#include "Group.h"
#include "post.h"
using namespace std;

class DataStore {
private:
    unordered_map<int, Student> students;
    unordered_map<int, Group>groups;
    unordered_map<int, Post>posts;
    unordered_map<string, vector<int>> skillIndex;
    unordered_map<string, vector<int>> interestIndex;
    unordered_map<string, vector<int>> departmentIndex;
    int nextPostId;
    vector<Message> messages;
    int nextMessageId;
public:
    DataStore() : nextPostId(1), nextMessageId(1) {}
    void clearAll() {
        students.clear();
        groups.clear();
        posts.clear();
        skillIndex.clear();
        interestIndex.clear();
        departmentIndex.clear();
        nextPostId = 1;
        messages.clear();
        nextMessageId = 1;
    }
    void addStudent(Student student);
    Student* findStudentById(int id);
    void displayAllStudents() const;
    void addGroup(Group group);
    Group* findGroupById(int id);
    void displayAllGroups() const;
    void addPost(Post post);
    void displayAllPosts() const;
    int getNextPostId();
    vector<Student> matchBySkill(string skill);
    vector<Student> matchByInterest(string interest);
    vector<Student> matchByDepartment(string department);
    vector<Student> getAllStudents() const;
    vector<Group> getAllGroups() const;
    vector<Post> getAllPosts() const;
    Post* findPostById(int id);
    void addMessage(Message message);
    vector<Message> getConversation(int studentA, int studentB) const;
    vector<int> getChatPartners(int studentId) const;
    int getNextMessageId();
    vector<Message> getAllMessages() const;
};
#endif
