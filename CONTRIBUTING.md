# CAMPUS-CONNECT

Semester PBL project: a campus networking app built around a C++ data-structures core.
**Grading focus is the DSA core (C++).** The web frontend and the Vercel Python API are secondary.

## Layout
- `include/`, `src/`: DSA core
  - `DataStore`: hash maps (students/posts/groups by ID) + inverted indexes (skill, interest, department)
  - `Graph`: adjacency list; `bfs`, `peopleYouMayKnow` (BFS with distance levels), `suggestFriends` (same, ranked by mutual friends; served at `GET /api/students/:id/suggestions`)
  - `JaccardMatching` (implements `MatchingStrategy`): skill/interest similarity
  - `TopKRanker`: top-k with a bounded min-heap, O(N log k)
  - `Post` → `ProjectPost`, `Group` → `ProjectGroup`: stored via `unique_ptr` + virtual `clone()` (avoids object slicing)
- `main.cpp`: CLI menu demo
- `server.cpp`: local HTTP API (cpp-httplib) serving `frontend/`
- `api/index.py`: Vercel serverless API backed by Neon Postgres (separate from `server.cpp`; the field names differ)
- `tests/test_dsa.cpp`: unit tests; `tests/benchmark.cpp` → `tests/results.txt` (report table)

## Commands
```
make              # CLI  -> ./campus_connect
make server       # HTTP server on :8080
make test         # unit tests, must print "All N checks passed."
make bench        # optimised benchmark -> tests/results.txt
make clean
```
C++11, g++ (Linux) or MinGW (Windows); the Makefile handles both.

## Rules
- Run `make test` after any change to `src/` or `include/`; add a test for every bug fix.
- When comparing data structures in the benchmark, use the `rank_*` columns (ranking only). The `topk_heap` / `full_sort` columns are dominated by Jaccard scoring cost.
- Never commit `.env`, real credentials, or compiled files (`*.o`, `*.exe`, binaries).
- Every SQL query in `api/index.py` / `db_sync.py` must pass user input as parameters (`$1, $2…`), never in f-strings.
- Never put user input into `system()` / shell commands (`server.cpp` hex-encodes message text for `db_sync.py`).
- If a `.env` file is present, `server.cpp`'s `POST /api/messages` writes to the **live** Neon database. Don't use it for testing.
- Work on a branch and open a PR into `main`. Don't push to `main` directly.
