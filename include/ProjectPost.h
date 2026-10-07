#ifndef PROJECTPOST_H
#define PROJECTPOST_H

#include "post.h"
#include <vector>
#include <string>
using namespace std;

// -------------------------------------------------------
// ProjectPost: A specialized Post for finding teammates.
// Inherits from Post (Veeru's base class) and adds fields
// for project title, required skills, and team size.
// This is Tanvay's OOP contribution (inheritance + polymorphism).
// -------------------------------------------------------
class ProjectPost : public Post {
private:
    string projectTitle;            // Name of the project
    vector<string> requiredSkills;  // Skills needed for the project
    int teamSizeNeeded;             // How many teammates are needed

public:
    // Constructor: calls Post's constructor, then sets project-specific fields
    ProjectPost(int postId, int authorId, string authorName,
                string projectTitle, vector<string> requiredSkills,
                int teamSizeNeeded, string timestamp);

    // Default constructor
    ProjectPost() : Post(), teamSizeNeeded(0) {}

    // Getters
    string getProjectTitle() const;
    vector<string> getRequiredSkills() const;
    int getTeamSizeNeeded() const;

    // Override display to show project-specific info (polymorphism)
    void display() const override;
};

#endif
