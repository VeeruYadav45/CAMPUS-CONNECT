#ifndef GROUP_H
#define GROUP_H
#include <string>
#include <vector>
#include <iostream>
using namespace std;
struct GroupPost {
    int postId;
    int authorId;
    string authorName;
    string content;
    string timestamp;
    string type;       
    string skill;      
    void display() const {
        cout << "=====" << endl;
        cout << "Post ID   : " << postId << endl;
        cout << "Type      : " << type << endl;
        cout << "Author    : " << authorName << " (ID: " << authorId << ")" << endl;
        cout << "Skill     : " << skill << endl;
        cout << "Time      : " << timestamp << endl;
        cout << "Content   : " << content << endl;
        cout << "====" << endl;
    }
};
class Group {
private:
    int groupId;
    string groupName;
    string description;
    int adminId;                   
    vector<int> memberIds;         
    vector<GroupPost> groupPosts;    
public:
    Group(int groupId, string groupName, string description, int adminId);
    Group() : groupId(0), adminId(0) {}
    int getGroupId() const;
    string getGroupName() const;
    string getDescription() const;
    int getAdminId() const;
    vector<int> getMemberIds() const;
    void addMember(int studentId);
    void removeMember(int studentId);
    bool isMember(int studentId) const;
    void addPost(int postId, int authorId, string authorName,
                 string content, string timestamp, string type, string skill);
    template<typename T>
    void addPost(const T& post) {
        GroupPost gp;
        gp.postId = post.getPostId();
        gp.authorId = post.getAuthorId();
        gp.authorName = post.getAuthorName();
        gp.content = post.getContent();
        gp.timestamp = post.getTimestamp();
        gp.type = post.getType();
        gp.skill = "";
        groupPosts.push_back(gp);
    }
    void displayPosts() const;
    void display() const;
};
#endif
