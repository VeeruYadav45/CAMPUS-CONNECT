#ifndef HELPREQUEST_H
#define HELPREQUEST_H
#include "post.h"
#include <string>
#include <vector>
using namespace std;
class HelpRequest : public Post {
private:
    string requiredSkill; 
    string urgency;             
public:
    HelpRequest(int postId, int authorId, string authorName,
                string content, string timestamp,
                string requiredSkill, string urgency);
    HelpRequest() : Post(), urgency("Medium") {}
    string getRequiredSkill() const;
    string getUrgency() const;
    string getType() const override;
    void display() const override;
};
#endif
