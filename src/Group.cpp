#include "../include/Group.h"
#include <iostream>
#include <algorithm>
using namespace std;

// Parameterized constructor
Group::Group(int groupId, string groupName, string description, int adminId)
    : groupId(groupId), groupName(groupName), description(description), adminId(adminId) {
    memberIds.push_back(adminId); // admin is the first member
}

// Getters
int Group::getGroupId() const { return groupId; }
string Group::getGroupName() const { return groupName; }
string Group::getDescription() const { return description; }
int Group::getAdminId() const { return adminId; }
vector<int> Group::getMemberIds() const { return memberIds; }
vector<Post> Group::getPosts() const { return posts; }

// Add a member
void Group::addMember(int studentId) {
    if (isMember(studentId)) {
        cout << "Student " << studentId << " is already in '" << groupName << "'." << endl;
        return;
    }
    memberIds.push_back(studentId);
    cout << "Student " << studentId << " joined '" << groupName << "'!" << endl;
}

// Remove a member
void Group::removeMember(int studentId) {
    if (studentId == adminId) {
        cout << "Admin cannot leave the group!" << endl;
        return;
    }
    auto it = find(memberIds.begin(), memberIds.end(), studentId);
    if (it != memberIds.end()) {
        memberIds.erase(it);
        cout << "Student " << studentId << " left '" << groupName << "'." << endl;
    } else {
        cout << "Student " << studentId << " is not a member of '" << groupName << "'." << endl;
    }
}

// Check membership
bool Group::isMember(int studentId) const {
    return find(memberIds.begin(), memberIds.end(), studentId) != memberIds.end();
}

// Add a post to group
void Group::addPost(Post post) {
    posts.push_back(post);
}

// Display all group posts
void Group::displayPosts() const {
    if (posts.empty()) {
        cout << "No posts in this group yet." << endl;
        return;
    }
    cout << "\n--- Posts in Group: " << groupName << " ---" << endl;
    for (const Post& p : posts) {
        p.display();
    }
}

// Display group info
void Group::display() const {
    cout << "=============================" << endl;
    cout << "Group ID    : " << groupId << endl;
    cout << "Group Name  : " << groupName << endl;
    cout << "Description : " << description << endl;
    cout << "Admin ID    : " << adminId << endl;
    cout << "Members (" << memberIds.size() << ") : ";
    for (size_t i = 0; i < memberIds.size(); i++) {
        cout << memberIds[i];
        if (i != memberIds.size() - 1) cout << ", ";
    }
    cout << endl;
    cout << "Total Posts : " << posts.size() << endl;
    cout << "=============================" << endl;
}
