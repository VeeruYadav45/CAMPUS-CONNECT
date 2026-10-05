#include "JaccardMatching.h"

#include <set>
#include <string>

using namespace std;

double JaccardMatching::calculateSimilarity(
    const Student& student1,
    const Student& student2
) const {

    set<string> set1;
    set<string> set2;

    for (const string& skill : student1.getSkills()) {
        set1.insert(skill);
    }

    for (const string& skill : student2.getSkills()) {
        set2.insert(skill);
    }

    for (const string& interest : student1.getInterests()) {
        set1.insert(interest);
    }

    for (const string& interest : student2.getInterests()) {
        set2.insert(interest);
    }

    int intersection = 0;

    for (const string& item : set1) {
        if (set2.find(item) != set2.end()) {
            intersection++;
        }
    }

    set<string> unionSet = set1;

    for (const string& item : set2) {
        unionSet.insert(item);
    }

    if (unionSet.empty()) {
        return 0.0;
    }

    return static_cast<double>(intersection) / unionSet.size();
}
