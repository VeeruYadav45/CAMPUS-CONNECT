#include "../include/HelpOffer.h"
#include <iostream>
using namespace std;

// ── Constructor ──
// Calls parent Post constructor, then sets its own fields
HelpOffer::HelpOffer(int postId, int authorId, string authorName,
                     string content, string timestamp,
                     string offeredSkill, string availability)
    : Post(postId, authorId, authorName, content, timestamp),
      offeredSkill(offeredSkill), availability(availability) {}

// ── Getters ──
string HelpOffer::getOfferedSkill() const { return offeredSkill; }
string HelpOffer::getAvailability() const { return availability; }

// ── getType(): tells us this is a "HelpOffer" ──
string HelpOffer::getType() const {
    return "HelpOffer";
}

// ── display(): shows all info including parent + child fields ──
void HelpOffer::display() const {
    cout << "====== HELP OFFER ========" << endl;
    cout << "Post ID        : " << postId << endl;
    cout << "Author         : " << authorName << " (ID: " << authorId << ")" << endl;
    cout << "Time           : " << timestamp << endl;
    cout << "Offered Skill  : " << offeredSkill << endl;
    cout << "Availability   : " << availability << endl;
    cout << "Description    : " << content << endl;
    cout << "Likes          : " << likes << endl;
    cout << "==========================" << endl;
}
