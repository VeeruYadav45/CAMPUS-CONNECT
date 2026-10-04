#ifndef POST_H
#define POST_H

#include <string>
#include <iostream>
using namespace std;

class Post {
private:
    int postId;
    int authorId;      // Student ID who created the post
    string authorName;
    string content;
    string timestamp;
    int likes;

public:
    Post(int postId, int authorId, string authorName, string content, string timestamp);
    Post() : postId(0), authorId(0), likes(0) {}

    int getPostId() const;
    int getAuthorId() const;
    string getAuthorName() const;
    string getContent() const;
    string getTimestamp() const;
    int getLikes() const;

    void addLike();
    void display() const;
};

#endif
