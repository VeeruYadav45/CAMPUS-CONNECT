#include "../include/ProjectPost.h"
#include <iostream>
using namespace std;
ProjectPost::ProjectPost(int postId, int authorId, string authorName,
                         string projectTitle, vector<string> requiredSkills,
                         int teamSizeNeeded, string timestamp)
    : Post(postId, authorId, authorName,
           "Looking for teammates for project: " + projectTitle,
           timestamp),
      projectTitle(projectTitle),
      requiredSkills(requiredSkills),
      teamSizeNeeded(teamSizeNeeded) {}


string ProjectPost::getProjectTitle() const { return projectTitle; }
vector<string> ProjectPost::getRequiredSkills() const { return requiredSkills; }
int ProjectPost::getTeamSizeNeeded() const { return teamSizeNeeded; }


void ProjectPost::display() const {
    cout << "========= PROJECT POST =========" << endl;
    cout << "Post ID    : " << postId << endl;
    cout << "Author     : " << authorName << " (ID: " << authorId << ")" << endl;
    cout << "Time       : " << timestamp << endl;
    cout << "Project    : " << projectTitle << endl;
    cout << "Team Size  : " << teamSizeNeeded << " members needed" << endl;
    cout << "Skills Req : ";
    for (int i = 0; i < (int)requiredSkills.size(); i++) {
        cout << requiredSkills[i];
        if (i != (int)requiredSkills.size() - 1) cout << ", ";
    }
    cout << endl;
    cout << "Likes      : " << likes << endl;
    cout << "================================" << endl;
}
