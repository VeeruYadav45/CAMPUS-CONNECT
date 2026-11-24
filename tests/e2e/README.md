# End-to-end tests for the Vercel API (`api/index.py`)

These run the real `api/index.py` against a **throwaway local PostgreSQL**. They never touch Neon or the live site.

| File | What it checks |
|---|---|
| `api_security_test.py` | Microsoft sign-in verification with real RS256 tokens signed by a test key (forged, expired, wrong app/tenant, non-GEU accounts), impersonation on every write route, private messages, connection requests, team invitations, edit/delete permissions, founders, input limits and Trie search. |
| `browser_test.py` | Real frontend + Python API in headless Firefox: names, XSS escaping, double-click protection, edit/delete buttons, project teams, search, request buttons and chat contacts. Sign-in is simulated with `test-<email>` tokens. |

## Run

```bash
# 1. Start a disposable PostgreSQL (removed afterwards)
podman run -d --rm --name cc-pgtest -p 127.0.0.1:55432:5432 \
  -e POSTGRES_PASSWORD=localtest -e POSTGRES_DB=neondb docker.io/library/postgres:17-alpine

# 2. Python packages (in a virtualenv)
pip install "psycopg[binary]>=3.2" cryptography selenium

# 3. Run without any DATABASE_URL so nothing can reach Neon
env -u DATABASE_URL python tests/e2e/api_security_test.py api/index.py
env -u DATABASE_URL python tests/e2e/browser_test.py . /tmp      # needs Firefox

podman rm -f cc-pgtest
```

Each script prints `N/N passed`. Start a fresh database for each run, because the scripts create their own tables. Optional: `FOUNDER=1` runs the browser test as a founder.
