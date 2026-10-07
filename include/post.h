#ifndef POST_H
#define POST_H
#include <string>
#include <iostream>
using namespace std;
class Post {
protected:                     
    int postId;
    int authorId;
    string authorName;
    string content;
    string timestamp;
    int likes;
public:
    Post(int postId, int authorId, string authorName, string content, string timestamp);
    Post() : postId(0), authorId(0), likes(0) {}
    virtual ~Post() {}              
    int getPostId() const;
    int getAuthorId() const;
    string getAuthorName() const;
    string getContent() const;
    string getTimestamp() const;
    int getLikes() const;
    void addLike();
    virtual string getType() const = 0; 
    virtual void display() const = 0;        
};
#endif
