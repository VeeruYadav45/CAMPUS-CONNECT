#include "../include/Message.h"
#include <iostream>
using namespace std;
Message::Message(int mId, int sId, int rId, string sName, string msg, string time)
    : messageId(mId), senderId(sId), receiverId(rId), senderName(sName), content(msg), timestamp(time) {}
int Message::getMessageId() const {
    return messageId;
}
int Message::getSenderId() const {
    return senderId;
}
int Message::getReceiverId() const {
    return receiverId;
}
string Message::getSenderName() const {
    return senderName;
}
string Message::getContent() const {
    return content;
}
string Message::getTimestamp() const {
    return timestamp;
}
void Message::display() const {
    cout << "[" << timestamp << "] " << senderName 
         << " (ID " << senderId << " -> " << receiverId << "): " 
         << content << endl;
}