#ifndef MESSAGE_H
#define MESSAGE_H
#include <string>
#include <iostream>
using namespace std;
class Message {
private:
    int messageId;
    int senderId;
    int receiverId;
    string senderName;
    string content;
    string timestamp;
public:
    Message(int mId, int sId, int rId, string sName, string msg, string time);
    Message() : messageId(0), senderId(0), receiverId(0) {}
    int getMessageId() const;
    int getSenderId() const;
    int getReceiverId() const;
    string getSenderName() const;
    string getContent() const;
    string getTimestamp() const;
    void display() const;
};
#endif