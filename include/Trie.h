#ifndef TRIE_H
#define TRIE_H
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;
struct TrieNode {
    unordered_map<char, TrieNode*> children;   
    bool isEndOfWord;                          
    vector<int> postIds;                       
    TrieNode() : isEndOfWord(false) {}
};
class Trie {
private:
    TrieNode* root;                            
    string toLower(const string& str) const;
    void collectWords(TrieNode* node, string currentWord, vector<string>& results) const;
    void collectPostIds(TrieNode* node, vector<int>& ids) const;
public:
    Trie();
    void insert(const string& word, int postId);
    bool search(const string& word) const;
    vector<int> getPostIds(const string& word) const;
    vector<string> autocomplete(const string& prefix) const;
    vector<int> searchByPrefix(const string& prefix) const;
    void displayAllWords() const;
};
#endif
