#include "../include/Trie.h"
#include <iostream>
#include <unordered_set>
using namespace std;

// ──────────────────────────────────────────────────
// Constructor: create root node (empty)
// ──────────────────────────────────────────────────
Trie::Trie() { root = new TrieNode(); }

// ──────────────────────────────────────────────────
// Helper: convert to lowercase so search is case-insensitive
// "C++" → "c++", "DSA" → "dsa", "Machine Learning" → "machine learning"
// ──────────────────────────────────────────────────
string Trie::toLower(const string &str) const {
  string result = str;
  for (int i = 0; i < result.size(); i++) {
    result[i] = tolower(result[i]);
  }
  return result;
}

// ──────────────────────────────────────────────────
// INSERT a word into the Trie and link it to a post ID
//
// How it works step by step:
//   Word: "DSA", postId: 5
//
//   Step 1: Start at root
//   Step 2: Look for child 'd' → not found → create it → move to it
//   Step 3: Look for child 's' → not found → create it → move to it
//   Step 4: Look for child 'a' → not found → create it → move to it
//   Step 5: Mark this node as end of word
//   Step 6: Add postId 5 to this node's postIds list
//
// Time: O(length of word)
// ──────────────────────────────────────────────────
void Trie::insert(const string &word, int postId) {
  string lowerWord = toLower(word); // case-insensitive
  TrieNode *current = root;         // start from root

  // Traverse character by character
  for (int i = 0; i < lowerWord.size(); i++) {
    char ch = lowerWord[i];

    // If this character doesn't exist as a child, create a new node
    if (current->children.find(ch) == current->children.end()) {
      current->children[ch] = new TrieNode();
    }

    // Move to the next node
    current = current->children[ch];
  }

  // Mark end of word
  current->isEndOfWord = true;

  // Link this word to the post ID (avoid duplicates)
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

// ──────────────────────────────────────────────────
// SEARCH: check if an exact word exists in the Trie
//
// Example: search("dsa") → true (if inserted before)
//          search("xyz") → false
//
// Time: O(length of word)
// ──────────────────────────────────────────────────
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

// ──────────────────────────────────────────────────
// GET POST IDs for an exact keyword
//
// Example: getPostIds("c++") → [1, 3, 7]
//          means posts 1, 3, 7 all have keyword "c++"
// ──────────────────────────────────────────────────
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

// ──────────────────────────────────────────────────
// HELPER: recursively collect all complete words from a node
//
// Used by autocomplete.
// Walks all children and collects every path that ends at a word.
// ──────────────────────────────────────────────────
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

// ──────────────────────────────────────────────────
// HELPER: recursively collect all post IDs from node and all descendants
// ──────────────────────────────────────────────────
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

// ──────────────────────────────────────────────────
// AUTOCOMPLETE: given a prefix, find all matching keywords
//
// Example:
//   Trie has: "c++", "css", "cat", "dsa", "python"
//   autocomplete("c") → ["c++", "css", "cat"]
//   autocomplete("ds") → ["dsa"]
//   autocomplete("py") → ["python"]
//
// This is what powers the search bar suggestions!
//
// Time: O(prefix length + total characters in matching words)
// ──────────────────────────────────────────────────
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

// ──────────────────────────────────────────────────
// SEARCH BY PREFIX: get all post IDs whose keywords
//                   start with the given prefix
//
// Example:
//   searchByPrefix("c") → post IDs for "c++", "css", "cat", etc.
//
// This is the main search function for Campus Connect!
// User types a prefix → we return all matching posts.
// ──────────────────────────────────────────────────
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

// ──────────────────────────────────────────────────
// DISPLAY all words stored in the Trie
// Useful for debugging
// ──────────────────────────────────────────────────
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
