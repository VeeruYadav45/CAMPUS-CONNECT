# CAMPUS-CONNECT
PBL sem 3

A campus collaboration platform where students post help requests/offers, find teammates, and discover study partners.

## Team Members & Contributions

| Member | DSA | OOP |
|---|---|---|
| **Vivekanand** | Trie (Prefix Tree, Autocomplete & Full-Text Search) | `HelpRequest`, `HelpOffer` (Polymorphic Posts) |
| **Veeru** | Unordered Map (Hashing) & Inverted Index | `Student`, `Post`, `Group`, `DataStore` |
| **Aditya** | Graph + BFS Traversal | `MatchingStrategy`, `JaccardMatching`, `Graph` |
| **Tanvay Jain** | Priority Queue (Max-Heap) | `ProjectPost`, `ProjectGroup`, `TopKRanker` |

---

## How Everything Connects

```
Veeru's DataStore --> getAllStudents() --> candidates
                                              |
Aditya's Graph -----> bfs(startId) --> scoped candidate IDs
                                              |
Vivekanand's Trie --> searchByPrefix() --> post IDs
                                              |
Aditya's MatchingStrategy --> calculateSimilarity() --> scores
                                              |
Tanvay's TopKRanker --> priority_queue (max-heap) --> Top-K results
                                              |
Tanvay's ProjectGroup --> auto-add top matches as members
```

---

## Build & Run

### Prerequisites
- g++ with C++11 support (MinGW on Windows, gcc on Linux/Mac)

### Compile & Run (Interactive Terminal App)
```bash
make clean
make
./campus_connect
```
Select option **29** to run the full automated system demo.

### Compile & Run (Web Server)
```bash
make server
./server
```
Then open `frontend/index.html` in your browser.

---

## File Structure

```
CAMPUS-CONNECT/
├── include/
│   ├── student.h              # Veeru: Student class
│   ├── post.h                 # Veeru: Post base class (virtual + protected)
│   ├── HelpRequest.h          # Vivekanand: HelpRequest (inherits Post)
│   ├── HelpOffer.h            # Vivekanand: HelpOffer (inherits Post)
│   ├── Trie.h                 # Vivekanand: Trie prefix search
│   ├── Group.h                # Veeru: Group class
│   ├── DataStore.h            # Veeru & Team: unified storage layer
│   ├── Graph.h                # Aditya: adjacency list + BFS
│   ├── MatchingStrategy.h     # Aditya: abstract matching interface
│   ├── JaccardMatching.h      # Aditya: Jaccard similarity
│   ├── ProjectPost.h          # Tanvay: teammate-seeking post
│   ├── ProjectGroup.h         # Tanvay: project team (inherits Group)
│   └── TopKRanker.h           # Tanvay: max-heap ranking
├── src/
│   ├── student.cpp
│   ├── post.cpp
│   ├── HelpRequest.cpp        # Vivekanand
│   ├── HelpOffer.cpp          # Vivekanand
│   ├── Trie.cpp               # Vivekanand
│   ├── Group.cpp
│   ├── DataStore.cpp
│   ├── Graph.cpp
│   ├── JaccardMatching.cpp
│   ├── ProjectPost.cpp        # Tanvay
│   ├── ProjectGroup.cpp       # Tanvay
│   └── TopKRanker.cpp         # Tanvay
├── frontend/                  # Web application UI
├── server.cpp                 # REST API server
├── main.cpp                   # Interactive terminal app (29 options)
├── Makefile
├── demo_input.txt
└── README.md
```
