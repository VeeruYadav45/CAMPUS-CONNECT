#include "JaccardMatching.h"

#include <set>
#include <string>
#include <algorithm>
#include <cctype>

using namespace std;

// Helper: convert a string to all-lowercase for case-insensitive comparison
static string toLower(const string& s) {
    string result = s;
    transform(result.begin(), result.end(), result.begin(),
              [](unsigned char c) { return tolower(c); });
    return result;
}

double JaccardMatching::calculateSimilarity(
    const Student& student1,
    const Student& student2
) const {

    set<string> set1;
    set<string> set2;

    // Normalize to lowercase before inserting so "DSA" == "dsa" == "Dsa"
    for (const string& skill : student1.getSkills()) {
        set1.insert(toLower(skill));
    }

    for (const string& skill : student2.getSkills()) {
        set2.insert(toLower(skill));
    }

    for (const string& interest : student1.getInterests()) {
        set1.insert(toLower(interest));
    }

    for (const string& interest : student2.getInterests()) {
        set2.insert(toLower(interest));
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
