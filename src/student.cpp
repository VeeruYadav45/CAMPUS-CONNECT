#include "../include/student.h"
#include <iostream>
using namespace std;

// Parameterized constructor
Student::Student(int id, string name, vector<string> skills, vector<string> interests, string department, int year)
    : id(id), name(name), skills(skills), interests(interests), department(department), year(year) {}

// Getters
int Student::getId() const { return id; }
string Student::getName() const { return name; }
vector<string> Student::getSkills() const { return skills; }
vector<string> Student::getInterests() const { return interests; }
string Student::getDepartment() const { return department; }
int Student::getYear() const { return year; }

// Add a skill
void Student::addskill(string skill) {
    skills.push_back(skill);
}

// Add an interest
void Student::addinterest(string interest) {
    interests.push_back(interest);
}

// Display student info
void Student::display() {
    cout << "----" << endl;
    cout << "ID         : " << id << endl;
    cout << "Name       : " << name << endl;
    cout << "Department : " << department << endl;
    cout << "Year       : " << year << endl;

    cout << "Skills     : ";
    for (int i = 0; i < skills.size(); i++) {
        cout << skills[i];
        if (i != skills.size() - 1) cout << ", ";
    }
    cout << endl;

    cout << "Interests  : ";
    for (int i = 0; i < interests.size(); i++) {
        cout << interests[i];
        if (i != interests.size() - 1) cout << ", ";
    }
    cout << endl;
    cout << "----" << endl;
}
