#include "../include/HelpRequest.h"
#include <iostream>
using namespace std;

// ── Constructor ──
// Calls the parent Post constructor first, then sets its own fields
HelpRequest::HelpRequest(int postId, int authorId, string authorName,
                         string content, string timestamp,
                         string requiredSkill, string urgency)
    : Post(postId, authorId, authorName, content, timestamp),
      requiredSkill(requiredSkill), urgency(urgency) {}

// ── Getters ──
string HelpRequest::getRequiredSkill() const { return requiredSkill; }
string HelpRequest::getUrgency() const { return urgency; }

// ── getType(): tells us this is a "HelpRequest" ──
// This is POLYMORPHISM: same function name, different behavior per class
string HelpRequest::getType() const {
    return "HelpRequest";
}

// ── display(): shows all info including parent + child fields ──
void HelpRequest::display() const {
    cout << "====== HELP REQUEST ======" << endl;
    cout << "Post ID        : " << postId << endl;
    cout << "Author         : " << authorName << " (ID: " << authorId << ")" << endl;
    cout << "Time           : " << timestamp << endl;
    cout << "Needed Skill   : " << requiredSkill << endl;
    cout << "Urgency        : " << urgency << endl;
    cout << "Description    : " << content << endl;
    cout << "Likes          : " << likes << endl;
    cout << "==========================" << endl;
}
