#ifndef STUDENT_H
#define STUDENT_H//prevent file from being included twice
#include<string>
#include<vector>
#include<iostream>
using namespace std;
class Student{
    private:
    int id;
    string name;
    vector<string>skills;
    vector<string>interests;
    string department;
    int year;
    public:
    Student(int id,string name,vector<string>skills,vector<string>interests,string department,int year);
    Student():id(0),year(0){}
  int getId() const;
string getName() const;
vector<string> getSkills() const;
vector<string> getInterests() const;
string getDepartment() const;
int getYear() const;

    void addskill(string skill);
    void addinterest(string interest);
    void display();
    
};
#endif