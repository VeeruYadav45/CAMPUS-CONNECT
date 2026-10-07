#ifndef PROJECTGROUP_H
#define PROJECTGROUP_H

#include "Group.h"
#include "ProjectPost.h"
#include <string>
#include <vector>
#include <iostream>
using namespace std;

// -------------------------------------------------------
// ProjectGroup: Extends Veeru's Group class to represent
// a project team. Links to a ProjectPost and tracks the
// leader separately from the admin concept in Group.
// This is Tanvay's OOP contribution (inheritance).
// -------------------------------------------------------
class ProjectGroup : public Group {
private:
    int leaderId;              // Student ID of the project leader
    int linkedPostId;          // The ProjectPost this group is for

public:
    // Constructor: creates a project group linked to a ProjectPost
    ProjectGroup(int groupId, string groupName, string description,
                 int leaderId, int linkedPostId);

    // Default constructor
    ProjectGroup() : Group(), leaderId(0), linkedPostId(0) {}

    // Getters
    int getLeaderId() const;
    int getLinkedPostId() const;

    // Override display to show project-group-specific info
    void display() const;
};

#endif
