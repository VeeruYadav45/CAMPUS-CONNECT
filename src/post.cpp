#include "../include/post.h"
#include <iostream>
using namespace std;

// ── Constructor ──
Post::Post(int postId, int authorId, string authorName, string content, string timestamp)
    : postId(postId), authorId(authorId), authorName(authorName),
      content(content), timestamp(timestamp), likes(0) {}

// ── Getters ──
int Post::getPostId() const { return postId; }
int Post::getAuthorId() const { return authorId; }
string Post::getAuthorName() const { return authorName; }
string Post::getContent() const { return content; }
string Post::getTimestamp() const { return timestamp; }
int Post::getLikes() const { return likes; }

// ── Like the post ──
void Post::addLike() {
    likes++;
}

// NOTE: display() and getType() are pure virtual.
//       They are implemented in HelpRequest, HelpOffer, etc.
