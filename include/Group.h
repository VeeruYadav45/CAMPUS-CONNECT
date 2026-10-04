#ifndef GROUP_H
#define GROUP_H

#include <string>
#include <vector>
#include <iostream>
#include "post.h"
using namespace std;

class Group {
private:
    int groupId;
    string groupName;
    string description;
    int adminId;              // Student ID of the group admin
    vector<int> memberIds;    // IDs of members
    vector<Post> posts;       // Posts made inside the group

public:
    Group(int groupId, string groupName, string description, int adminId);
    Group() : groupId(0), adminId(0) {}

    int getGroupId() const;
    string getGroupName() const;
    string getDescription() const;
    int getAdminId() const;
    vector<int> getMemberIds() const;
    vector<Post> getPosts() const;

    void addMember(int studentId);
    void removeMember(int studentId);
    bool isMember(int studentId) const;
    void addPost(Post post);
    void displayPosts() const;
    void display() const;
};

#endif
