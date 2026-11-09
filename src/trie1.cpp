#include "../include/Trie.h"
#include <iostream>
#include <unordered_set>
using namespace std;
Trie::Trie() { root = new TrieNode(); }

string Trie::toLower(const string &str) const {
  string result = str;
  for (int i = 0; i < result.size(); i++) {
    result[i] = tolower(result[i]);
  }
  return result;
}

 void Trie::insert(const string &word, int postId) {
  string lowerWord = toLower(word); 
  TrieNode *current = root;         
  for (int i = 0; i < lowerWord.size(); i++) {
    char ch = lowerWord[i];

    if (current->children.find(ch) == current->children.end()) {
      current->children[ch] = new TrieNode();
    }

    
    current = current->children[ch];
  }


  current->isEndOfWord = true;
  bool alreadyExists = false;
  for (int i = 0; i < current->postIds.size(); i++) {
    if (current->postIds[i] == postId) {
      alreadyExists = true;
      break;
    }
  }
  if (!alreadyExists) {
    current->postIds.push_back(postId);
  }
}
bool Trie::search(const string &word) const {
  string lowerWord = toLower(word);
  TrieNode *current = root;

  for (int i = 0; i < lowerWord.size(); i++) {
    char ch = lowerWord[i];

    // If character not found, word doesn't exist
    if (current->children.find(ch) == current->children.end()) {
      return false;
    }
    current = current->children[ch];
  }

  // Must be end of a complete word (not just a prefix)
  return current->isEndOfWord;
}
vector<int> Trie::getPostIds(const string &word) const {
  string lowerWord = toLower(word);
  TrieNode *current = root;

  for (int i = 0; i < lowerWord.size(); i++) {
    char ch = lowerWord[i];
    if (current->children.find(ch) == current->children.end()) {
      return {}; // empty vector = word not found
    }
    current = current->children[ch];
  }

  if (current->isEndOfWord) {
    return current->postIds;
  }
  return {};
}

void Trie::collectWords(TrieNode *node, string currentWord,
                        vector<string> &results) const {
  if (node->isEndOfWord) {
    results.push_back(currentWord);
  }

  // Visit all children alphabetically
  for (auto &pair : node->children) {
    collectWords(pair.second, currentWord + pair.first, results);
  }
}

void Trie::collectPostIds(TrieNode *node, vector<int> &ids) const {
  // Add this node's post IDs
  for (int id : node->postIds) {
    ids.push_back(id);
  }

  // Recurse into children
  for (auto &pair : node->children) {
    collectPostIds(pair.second, ids);
  }
}

vector<string> Trie::autocomplete(const string &prefix) const {
  string lowerPrefix = toLower(prefix);
  TrieNode *current = root;

  // Navigate to the end of the prefix
  for (int i = 0; i < lowerPrefix.size(); i++) {
    char ch = lowerPrefix[i];
    if (current->children.find(ch) == current->children.end()) {
      return {}; // prefix not found → no suggestions
    }
    current = current->children[ch];
  }

  // Collect all words that start with this prefix
  vector<string> results;
  collectWords(current, lowerPrefix, results);
  return results;
}

vector<int> Trie::searchByPrefix(const string &prefix) const {
  string lowerPrefix = toLower(prefix);
  TrieNode *current = root;

  // Navigate to end of prefix
  for (int i = 0; i < lowerPrefix.size(); i++) {
    char ch = lowerPrefix[i];
    if (current->children.find(ch) == current->children.end()) {
      return {};
    }
    current = current->children[ch];
  }

  // Collect all post IDs from this point downward
  vector<int> ids;
  collectPostIds(current, ids);

  // Remove duplicate post IDs
  unordered_set<int> unique(ids.begin(), ids.end());
  return vector<int>(unique.begin(), unique.end());
}

void Trie::displayAllWords() const {
  vector<string> words;
  collectWords(root, "", words);

  if (words.empty()) {
    cout << "Trie is empty. No keywords indexed yet." << endl;
    return;
  }

  cout << "\n===== TRIE KEYWORDS =====" << endl;
  for (int i = 0; i < words.size(); i++) {
    cout << "  " << i + 1 << ". " << words[i];

    // Also show linked post IDs
    vector<int> ids = getPostIds(words[i]);
    if (!ids.empty()) {
      cout << "  → Posts: [";
      for (int j = 0; j < ids.size(); j++) {
        cout << ids[j];
        if (j != ids.size() - 1)
          cout << ", ";
      }
      cout << "]";
    }
    cout << endl;
  }
  cout << "=========================" << endl;
}
