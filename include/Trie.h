#ifndef TRIE_H
#define TRIE_H

#include <map>
#include <string>
#include <vector>
using namespace std;

// One node per character. std::map keeps children sorted, so autocomplete
// returns words in alphabetical order.
struct TrieNode {
    map<char, TrieNode *> children;
    bool isEndOfWord;
    vector<int> postIds;   // posts containing the word that ends at this node

    TrieNode() : isEndOfWord(false) {}
};

// Keyword index over post text (Vivekanand's module).
//   insert(word, id)       O(L)                 L = word length
//   search / getPostIds    O(L)
//   autocomplete(prefix)   O(P + size of the subtree below the prefix)
//   searchByPrefix(prefix) O(P + size of the subtree), duplicates removed
// All lookups are case-insensitive.
class Trie {
private:
    TrieNode *root;

    string toLower(const string &str) const;
    void collectWords(TrieNode *node, string currentWord, vector<string> &results) const;
    void collectPostIds(TrieNode *node, vector<int> &ids) const;

    static void freeNode(TrieNode *node) {
        for (auto &pair : node->children) freeNode(pair.second);
        delete node;
    }

public:
    Trie();
    ~Trie() { freeNode(root); }
    Trie(const Trie &) = delete;              // owns raw pointers: no copies
    Trie &operator=(const Trie &) = delete;

    // Remove every word (used when the index is rebuilt)
    void clear() {
        freeNode(root);
        root = new TrieNode();
    }

    void insert(const string &word, int postId);
    bool search(const string &word) const;
    vector<int> getPostIds(const string &word) const;
    vector<string> autocomplete(const string &prefix) const;
    vector<int> searchByPrefix(const string &prefix) const;
    void displayAllWords() const;

    // Split text into lower-case keywords: letters, digits, '+' and '#'
    // (so "C++" and "C#" stay searchable). Everything else separates words.
    static vector<string> tokenize(const string &text) {
        vector<string> words;
        string cur;
        for (char raw : text) {
            char ch = (char)tolower((unsigned char)raw);
            if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '+' || ch == '#') {
                cur += ch;
            } else if (!cur.empty()) {
                words.push_back(cur);
                cur.clear();
            }
        }
        if (!cur.empty()) words.push_back(cur);
        return words;
    }

    // Index every keyword of a post's text under its ID
    void indexText(const string &text, int postId) {
        for (const string &w : tokenize(text)) insert(w, postId);
    }
};

#endif
