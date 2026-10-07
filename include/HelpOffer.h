#ifndef HELPOFFER_H
#define HELPOFFER_H
#include "post.h"
#include <string>
#include <vector>
using namespace std;
class HelpOffer : public Post {
private:
    string offeredSkill;   
    string availability;        
public:
    HelpOffer(int postId, int authorId, string authorName,
              string content, string timestamp,
              string offeredSkill, string availability);
    HelpOffer() : Post(), availability("Anytime") {}
    string getOfferedSkill() const;
    string getAvailability() const;
    string getType() const override;
    void display() const override;
};
#endif
