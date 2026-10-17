#include "../include/ProjectGroup.h"
#include <iostream>
using namespace std;

// -------------------------------------------------------
// ProjectGroup constructor
// Passes groupId, name, description, and leaderId to the
// base Group constructor (leader = admin of the group).
// Also stores the linked ProjectPost ID.
// -------------------------------------------------------
ProjectGroup::ProjectGroup(int groupId, string groupName, string description,
                           int leaderId, int linkedPostId)
    : Group(groupId, groupName, description, leaderId),
      leaderId(leaderId),
      linkedPostId(linkedPostId) {}

// Getters
int ProjectGroup::getLeaderId() const { return leaderId; }
int ProjectGroup::getLinkedPostId() const { return linkedPostId; }

// -------------------------------------------------------
// display(): Shows group info plus project-specific fields.
// Calls the base Group::display() first, then adds extras.
// -------------------------------------------------------
void ProjectGroup::display() const {
    Group::display();  // Reuse Veeru's display logic
    cout << "  Leader ID     : " << leaderId << endl;
    cout << "  Linked Post   : " << linkedPostId << endl;
    cout << "=============================" << endl;
}
