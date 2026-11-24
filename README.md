# Campus Connect

A campus-only peer-discovery and collaboration platform for Graphic Era (Deemed to be University). Students list their skills and interests, find each other, connect, form project teams and message one another. Sign-in uses the university's Microsoft (GEU Outlook) accounts.

**Live site:** https://campus-connect-rose-delta.vercel.app

Semester 3 PBL project (Data Structures + OOP), Dept. of CSE.

## Features

| Feature | How it works |
|---|---|
| Student directory and search | Hash maps by ID plus inverted indexes for skill, interest and department, giving O(1) average lookups |
| Similar students | Jaccard similarity over skills and interests |
| Connection network | Undirected graph stored as an adjacency list, drawn with D3 |
| People You May Know | Two-level BFS (friends of friends), ranked by mutual-friend count |
| Project calls and Top-K teammates | Jaccard score plus a bounded min-heap, O(N log k); turn a project call into a team in one click |
| Groups, group discussions, feed | Create, join and leave groups; members-only group posts |
| Direct messages | Private, readable only by the two participants |
| Sign-in | Microsoft (MSAL) in the browser; the server verifies the ID token (RS256, tenant, audience, expiry) |

## Architecture

The same frontend (`frontend/`) runs against either backend:

- **Local C++ server** (`server.cpp`, port 8080): uses the in-memory DSA core in `include/` and `src/`. This is the graded part.
- **Public site on Vercel:** `api/index.py` is a serverless Python function backed by Neon PostgreSQL. It uses parameterized SQL only, and writes require a verified sign-in.

## Run locally

```bash
make server          # build the HTTP server
./server             # open http://localhost:8080
curl -X POST localhost:8080/api/demo/load   # load the 10-student demo data
```

Other targets:

```bash
make                 # CLI menu demo -> ./campus_connect
make test            # unit tests for the DSA core ("All N checks passed.")
make bench           # benchmark -> tests/results.txt
python3 test_api.py  # API checks against a running local server
```

You need g++ with C++11 on Linux or MinGW on Windows; the Makefile handles both.

Do **not** create a `.env` with the production `DATABASE_URL` for local testing. With it present, the local server writes messages to the live database.

## Repository layout

```
include/, src/   DSA core: Student, Post/ProjectPost, Group/ProjectGroup, DataStore,
                 Graph, MatchingStrategy/JaccardMatching, TopKRanker, Message
server.cpp       local REST API (cpp-httplib + nlohmann/json, one mutex)
main.cpp         CLI demo
api/index.py     Vercel serverless API (Neon PostgreSQL)
frontend/        single-page web app (HTML/CSS/JS, D3, MSAL)
tests/           unit tests (test_dsa.cpp, unit_tests.cpp) and benchmark
```

## Team

| Member | Modules |
|---|---|
| Veeru Yadav (lead) | Student, Post, Group, DataStore (hash maps, inverted index), cloud deployment |
| Aditya Bagri | Graph (BFS, People You May Know), MatchingStrategy, JaccardMatching |
| Tanvay Jain | TopKRanker (heap), ProjectPost, ProjectGroup, frontend |
| Vivekanand | Trie (keyword and prefix search) |

Mentor: Mr. Kanishka Bhatt.

See `CONTRIBUTING.md` for the rules for working on the repo: branches and PRs, tests, and safe SQL.
