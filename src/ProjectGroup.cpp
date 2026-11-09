#include "../include/ProjectGroup.h"
#include <iostream>
using namespace std;
ProjectGroup::ProjectGroup(int groupId, string groupName, string description,
                           int leaderId, int linkedPostId)
    : Group(groupId, groupName, description, leaderId),
      leaderId(leaderId),
      linkedPostId(linkedPostId) {}

int ProjectGroup::getLeaderId() const { return leaderId; }
int ProjectGroup::getLinkedPostId() const { return linkedPostId; }
void ProjectGroup::display() const {
    Group::display();
    cout << "  Leader ID     : " << leaderId << endl;
    cout << "  Linked Post   : " << linkedPostId << endl;
    cout << "=============================" << endl;
}
