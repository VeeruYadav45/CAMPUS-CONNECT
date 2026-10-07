# CAMPUS-CONNECT
PBL sem 3

A campus collaboration platform where students post help requests/offers and find teammates.

## Team Members & Contributions

| Member       | DSA                     | OOP                         |
|-------------|-------------------------|------------------------------|
| Veeru       | Unordered Map (Hashing) | Student, Post, Group, DataStore |
| Aditya      | Graph + BFS             | MatchingStrategy, JaccardMatching, Graph |
| Tanvay Jain | Priority Queue (Max-Heap) | ProjectPost, ProjectGroup, TopKRanker |

---

## Tanvay's Part: Priority Queue + ProjectPost + ProjectGroup

### What I Built

1. **ProjectPost** (`include/ProjectPost.h`, `src/ProjectPost.cpp`)
   - A subclass of `Post` for teammate-seeking posts
   - Adds project title, required skills, team size needed
   - Overrides `display()` using polymorphism

2. **ProjectGroup** (`include/ProjectGroup.h`, `src/ProjectGroup.cpp`)
   - Extends `Group` to represent a project team
   - Links to a `ProjectPost` and tracks the project leader
   - Reuses Group's add/remove member logic

3. **TopKRanker** (`include/TopKRanker.h`, `src/TopKRanker.cpp`)
   - Uses `std::priority_queue` (max-heap) to rank students by compatibility
   - `getTopK()` — ranks all candidates by Jaccard score, returns top K
   - `getTopKUsingBFS()` — first uses BFS to find connected students in the graph, then ranks only those

### How My Code Connects to Others

```
Veeru's DataStore ──> getAllStudents() ──> candidates
                                              │
Aditya's Graph ────> bfs(startId) ──> scoped candidate IDs
                                              │
Aditya's MatchingStrategy ──> calculateSimilarity() ──> scores
                                              │
Tanvay's TopKRanker ──> priority_queue (max-heap) ──> Top-K results
                                              │
Tanvay's ProjectGroup ──> auto-add top matches as members
```

The max-heap (priority queue) sits at the **ranking layer**: it takes raw similarity scores from Aditya's Jaccard matching and efficiently extracts the K best matches. BFS on Aditya's graph scopes the candidates first (only connected students), and student data comes from Veeru's DataStore (no duplication).

### Changes to Existing Code

Only **one file** was modified:
- `include/post.h` — Changed `private` to `protected` (so `ProjectPost` can inherit), added `virtual` to `display()` and a virtual destructor. This is the standard C++ pattern for enabling inheritance.

---

## Build & Run

### Prerequisites
- g++ with C++11 support (MinGW on Windows, or gcc on Linux/Mac)

### Compile
```bash
g++ -std=c++11 -Wall -I include -o campus_connect.exe main.cpp src/student.cpp src/post.cpp src/Group.cpp src/DataStore.cpp src/Graph.cpp src/JaccardMatching.cpp src/ProjectPost.cpp src/ProjectGroup.cpp src/TopKRanker.cpp
```

Or using Make:
```bash
make clean
make
```

### Run
```bash
./campus_connect.exe
```

Then select option **20** to run the full automated demo with 8 sample students.

Or pipe the demo input:
```bash
./campus_connect.exe < demo_input.txt
```

---

## File Structure

```
CAMPUS-CONNECT/
├── include/
│   ├── student.h          # Veeru: Student class
│   ├── post.h             # Veeru: Post base class (modified: virtual + protected)
│   ├── Group.h            # Veeru: Group class
│   ├── DataStore.h        # Veeru: unordered_map storage layer
│   ├── Graph.h            # Aditya: adjacency list + BFS
│   ├── MatchingStrategy.h # Aditya: abstract matching interface
│   ├── JaccardMatching.h  # Aditya: Jaccard similarity
│   ├── ProjectPost.h      # Tanvay: teammate-seeking post (inherits Post)
│   ├── ProjectGroup.h     # Tanvay: project team (inherits Group)
│   └── TopKRanker.h       # Tanvay: max-heap ranking
├── src/
│   ├── student.cpp
│   ├── post.cpp
│   ├── Group.cpp
│   ├── DataStore.cpp
│   ├── Graph.cpp
│   ├── JaccardMatching.cpp
│   ├── ProjectPost.cpp    # Tanvay
│   ├── ProjectGroup.cpp   # Tanvay
│   └── TopKRanker.cpp     # Tanvay
├── main.cpp               # Terminal driver (extended with options 16-20)
├── Makefile
├── demo_input.txt
└── README.md
```
