#include "../include/Group.h"
#include <iostream>
#include <algorithm>
using namespace std;

// Constructor
Group::Group(int groupId, string groupName, string description, int adminId)
    : groupId(groupId), groupName(groupName), description(description), adminId(adminId) {
    memberIds.push_back(adminId); // Admin is automatically a member
}

// Getters
int Group::getGroupId() const { return groupId; }
string Group::getGroupName() const { return groupName; }
string Group::getDescription() const { return description; }
int Group::getAdminId() const { return adminId; }
vector<int> Group::getMemberIds() const { return memberIds; }

// Add a member
void Group::addMember(int studentId) {
    if (!isMember(studentId)) {
        memberIds.push_back(studentId);
        cout << "Student " << studentId << " added to group '" << groupName << "'." << endl;
    } else {
        cout << "Student " << studentId << " is already a member." << endl;
    }
}

// Remove a member
void Group::removeMember(int studentId) {
    if (studentId == adminId) {
        cout << "Cannot remove admin from group." << endl;
        return;
    }
    auto it = find(memberIds.begin(), memberIds.end(), studentId);
    if (it != memberIds.end()) {
        memberIds.erase(it);
        cout << "Student " << studentId << " removed from group." << endl;
    } else {
        cout << "Student " << studentId << " is not a member." << endl;
    }
}

// Check membership
bool Group::isMember(int studentId) const {
    return find(memberIds.begin(), memberIds.end(), studentId) != memberIds.end();
}

// Add a post using individual fields
void Group::addPost(int postId, int authorId, string authorName,
                    string content, string timestamp, string type, string skill) {
    GroupPost gp;
    gp.postId = postId;
    gp.authorId = authorId;
    gp.authorName = authorName;
    gp.content = content;
    gp.timestamp = timestamp;
    gp.type = type;
    gp.skill = skill;
    groupPosts.push_back(gp);
}

// Display all group posts
void Group::displayPosts() const {
    if (groupPosts.empty()) {
        cout << "No posts in this group yet." << endl;
        return;
    }
    cout << "\n--- Posts in Group: " << groupName << " ---" << endl;
    for (const GroupPost& gp : groupPosts) {
        gp.display();
    }
}

// Display group info
void Group::display() const {
    cout << "====" << endl;
    cout << "Group ID    : " << groupId << endl;
    cout << "Group Name  : " << groupName << endl;
    cout << "Description : " << description << endl;
    cout << "Admin ID    : " << adminId << endl;
    cout << "Members     : ";
    for (int i = 0; i < memberIds.size(); i++) {
        cout << memberIds[i];
        if (i != memberIds.size() - 1) cout << ", ";
    }
    cout << endl;
    cout << "Total Posts : " << groupPosts.size() << endl;
    cout << "====" << endl;
}
