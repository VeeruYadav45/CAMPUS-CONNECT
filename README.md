# CAMPUS-CONNECT
PBL sem 3

A campus collaboration platform where students post help requests/offers, find teammates, and discover study partners.

## Team Members & Contributions

| Member | DSA | OOP |
| :--- | :--- | :--- |
| **Vivekanand** | Trie (Prefix Tree, Autocomplete & Full-Text Search) | `HelpRequest`, `HelpOffer` (Polymorphic Posts) |
| **Veeru** | Unordered Map (Hashing) & Inverted Index | `Student`, `Post`, `Group`, `DataStore` |
| **Aditya** | Graph + BFS Traversal | `MatchingStrategy`, `JaccardMatching`, `Graph` |
| **Tanvay Jain** | Priority Queue (Max-Heap) | `ProjectPost`, `ProjectGroup`, `TopKRanker` |

---

## Architecture Overview

```
                      ┌──────────────────────┐
                      │   DataStore (Veeru)   │
                      │ Hash Maps + InvIndex │
                      └──────────┬───────────┘
                                 │
         ┌───────────────────────┼────────────────────────┐
         │                       │                        │
         ▼                       ▼                        ▼
┌──────────────────┐    ┌─────────────────┐    ┌────────────────────┐
│   Trie (Anand)   │    │  Graph (Aditya) │    │  Ranker (Tanvay)   │
│ Prefix / Keyword │    │   BFS Search    │    │ PriorityQueue Heap │
└──────────────────┘    └────────┬────────┘    └──────────┬─────────┘
                                 │                        │
                                 └───────────┬────────────┘
                                             ▼
                                ┌─────────────────────────┐
                                │ MatchingStrategy Jaccard │
                                └─────────────────────────┘
```

---

## Build & Run

### Prerequisites
- g++ compiler with C++11 support (macOS, Linux, or MinGW on Windows)
- Make (optional, but recommended)

### 1. Interactive Terminal App
```bash
make clean
make
./campus_connect
```

Select option **29** to run the full automated system demo showcasing all team members' contributions.

### 2. Web Server & Frontend (Optional)
```bash
make server
./server
```
Then open `frontend/index.html` in your web browser.

---

## File Structure

```
CAMPUS-CONNECT/
├── include/
│   ├── student.h          # Veeru: Student class
│   ├── post.h             # Veeru: Post base class (virtual polymorphism)
│   ├── HelpRequest.h      # Anand: HelpRequest (inherits Post)
│   ├── HelpOffer.h        # Anand: HelpOffer (inherits Post)
│   ├── Trie.h             # Anand: Trie data structure & prefix search
│   ├── Group.h            # Veeru: Group class
│   ├── DataStore.h        # Veeru & Team: unified storage layer
│   ├── Graph.h            # Aditya: adjacency list + BFS
│   ├── MatchingStrategy.h # Aditya: abstract matching interface
│   ├── JaccardMatching.h  # Aditya: Jaccard similarity
│   ├── ProjectPost.h      # Tanvay: teammate-seeking post (inherits Post)
│   ├── ProjectGroup.h     # Tanvay: project team (inherits Group)
│   └── TopKRanker.h       # Tanvay: max-heap ranking
├── src/
│   ├── student.cpp
│   ├── post.cpp
│   ├── HelpRequest.cpp    # Anand
│   ├── HelpOffer.cpp      # Anand
│   ├── Trie.cpp           # Anand
│   ├── Group.cpp
│   ├── DataStore.cpp
│   ├── Graph.cpp
│   ├── JaccardMatching.cpp
│   ├── ProjectPost.cpp    # Tanvay
│   ├── ProjectGroup.cpp   # Tanvay
│   └── TopKRanker.cpp     # Tanvay
├── frontend/              # Web application UI
├── server.cpp             # REST API server
├── main.cpp               # Interactive terminal application
├── Makefile               # Cross-platform build script
├── demo_input.txt
└── README.md
```
