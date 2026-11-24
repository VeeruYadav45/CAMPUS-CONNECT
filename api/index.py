from http.server import BaseHTTPRequestHandler
import base64
import hashlib
import hmac
import json
import time
import urllib.error
import urllib.request
import ssl
import os
from urllib.parse import urlparse, parse_qs

def get_db_url():
    for key in ["DATABASE_URL", "DATABASE_URL_POOLED", "POSTGRES_URL", "POSTGRES_PRISMA_URL"]:
        val = os.environ.get(key)
        if val and val.strip():
            return val.strip().strip('"\'')
    if os.path.exists(".env"):
        with open(".env") as f:
            for line in f:
                if line.startswith("DATABASE_URL="):
                    return line.strip().split("=", 1)[1].strip('"\'')
    return None

def query_neon(sql, params=None):
    """Run SQL on Neon's HTTP endpoint.

    User input must ONLY be passed through `params` ($1, $2, ...), never
    pasted into the SQL string: Postgres then treats it purely as data,
    which prevents SQL injection.
    """
    db_url = get_db_url()
    if not db_url:
        raise ValueError("DATABASE_URL not set!")
    host = db_url.split("@")[1].split("/")[0].split("?")[0]
    payload = {"query": sql, "params": [None if v is None else str(v) for v in (params or [])]}
    req = urllib.request.Request(
        f"https://{host}/sql",
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Neon-Connection-String": db_url,
            "Content-Type": "application/json"
        }
    )
    ctx = ssl.create_default_context()
    try:
        with urllib.request.urlopen(req, context=ctx, timeout=10) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        try:
            detail = json.loads(e.read().decode("utf-8")).get("message", "")
        except Exception:
            detail = ""
        print("Neon query failed:", e.code, detail, "| SQL:", sql[:200])   # Vercel function logs
        raise RuntimeError(f"database error: {detail or e.reason}")

class BadRequest(Exception):
    pass

def to_int(value, field):
    """Parse an integer ID/number from user input, or reject the request."""
    try:
        if isinstance(value, bool):
            raise ValueError
        return int(value)
    except (TypeError, ValueError):
        raise BadRequest(f"'{field}' must be an integer")

def pg_text_array(items, field, max_items=20, max_len=50):
    """Encode a list of strings as a Postgres array literal, e.g. {"C++","AI"}.
    Passed as a parameter with ::text[], so quotes in items cannot escape.
    Items are trimmed; empty ones dropped; size limits give a clear 400."""
    if items is None:
        items = []
    if not isinstance(items, list):
        raise BadRequest(f"'{field}' must be a list")
    cleaned = [str(item).strip() for item in items if str(item).strip()]
    if len(cleaned) > max_items:
        raise BadRequest(f"'{field}' can have at most {max_items} items")
    parts = []
    for text in cleaned:
        if len(text) > max_len:
            raise BadRequest(f"each item in '{field}' must be at most {max_len} characters")
        text = text.replace("\\", "\\\\").replace('"', '\\"')
        parts.append(f'"{text}"')
    return "{" + ",".join(parts) + "}"

def text_field(value, field, max_len, required=False):
    """Trimmed text from user input with a length limit (400 instead of a DB error)."""
    text = "" if value is None else str(value).strip()
    if required and not text:
        raise BadRequest(f"'{field}' cannot be empty")
    if len(text) > max_len:
        raise BadRequest(f"'{field}' must be at most {max_len} characters")
    return text

def lower_set(items):
    """Case-insensitive set of skills/interests ("Python" == "python")."""
    return {str(x).strip().lower() for x in (items or []) if str(x).strip()}

class AuthError(Exception):
    """401 = not signed in / invalid token, 403 = signed in but not allowed."""
    def __init__(self, message, status=401):
        super().__init__(message)
        self.status = status

# ---------------------------------------------------------------------------
# Microsoft sign-in verification
#
# The browser signs in with Microsoft (MSAL) and sends the ID token it got as
# "Authorization: Bearer <token>". We never trust student IDs sent by the
# browser for writes; instead we verify the token here and use the email
# inside it. Verification = RS256 signature check against Microsoft's
# published keys + issuer/audience/tenant/expiry checks. Standard library
# only, so the Vercel function needs no extra packages.
# ---------------------------------------------------------------------------
MS_TENANT_ID = os.environ.get("MS_TENANT_ID", "1490b17d-5dc9-4cbf-aeba-a2e854f521b8")
MS_CLIENT_ID = os.environ.get("MS_CLIENT_ID", "690b6eff-568c-4b04-8f26-3e27c1089010")
MS_ISSUER = f"https://login.microsoftonline.com/{MS_TENANT_ID}/v2.0"
MS_JWKS_URL = f"https://login.microsoftonline.com/{MS_TENANT_ID}/discovery/v2.0/keys"
ALLOWED_EMAIL_DOMAINS = ("@geu.ac.in", "@graphicera.ac.in")
CLOCK_SKEW_SECONDS = 300

# DER prefix of a SHA-256 DigestInfo (PKCS#1 v1.5, RFC 8017 section 9.2)
_SHA256_DIGEST_INFO = bytes.fromhex("3031300d060960864801650304020105000420")

_jwks_cache = {"keys": {}, "fetched_at": 0.0}

def _b64url_decode(text):
    return base64.urlsafe_b64decode(text + "=" * (-len(text) % 4))

def fetch_jwks():
    """Download Microsoft's public signing keys: {kid: (n, e)}."""
    with urllib.request.urlopen(MS_JWKS_URL, context=ssl.create_default_context(), timeout=10) as resp:
        data = json.loads(resp.read().decode("utf-8"))
    keys = {}
    for k in data.get("keys", []):
        if k.get("kty") == "RSA" and "n" in k and "e" in k and "kid" in k:
            keys[k["kid"]] = (int.from_bytes(_b64url_decode(k["n"]), "big"),
                              int.from_bytes(_b64url_decode(k["e"]), "big"))
    return keys

def _signing_key(kid):
    """Cached key lookup; refresh at most every 5 minutes (keys rotate)."""
    now = time.time()
    if kid not in _jwks_cache["keys"] and now - _jwks_cache["fetched_at"] > 300:
        _jwks_cache["keys"] = fetch_jwks()
        _jwks_cache["fetched_at"] = now
    return _jwks_cache["keys"].get(kid)

def _rs256_valid(signing_input, signature, n, e):
    """RSASSA-PKCS1-v1_5 with SHA-256: compare the full expected encoding."""
    k = (n.bit_length() + 7) // 8
    if len(signature) != k:
        return False
    s = int.from_bytes(signature, "big")
    if s >= n:
        return False
    em = pow(s, e, n).to_bytes(k, "big")
    t = _SHA256_DIGEST_INFO + hashlib.sha256(signing_input).digest()
    if k < len(t) + 11:
        return False
    expected = b"\x00\x01" + b"\xff" * (k - len(t) - 3) + b"\x00" + t
    return hmac.compare_digest(em, expected)

def verify_ms_id_token(token):
    """Return (email, name) for a valid Graphic Era Microsoft ID token, else raise AuthError."""
    try:
        header_b64, payload_b64, sig_b64 = token.split(".")
        header = json.loads(_b64url_decode(header_b64))
        claims = json.loads(_b64url_decode(payload_b64))
        signature = _b64url_decode(sig_b64)
    except Exception:
        raise AuthError("invalid sign-in token")
    if header.get("alg") != "RS256":
        raise AuthError("invalid sign-in token")
    key = _signing_key(header.get("kid"))
    if not key or not _rs256_valid(f"{header_b64}.{payload_b64}".encode("ascii"), signature, *key):
        raise AuthError("invalid sign-in token")
    now = time.time()
    if claims.get("iss") != MS_ISSUER or claims.get("tid") != MS_TENANT_ID or claims.get("aud") != MS_CLIENT_ID:
        raise AuthError("sign-in token is not for Campus Connect")
    if not isinstance(claims.get("exp"), (int, float)) or claims["exp"] < now - CLOCK_SKEW_SECONDS:
        raise AuthError("sign-in expired, please sign in again")
    if isinstance(claims.get("nbf"), (int, float)) and claims["nbf"] > now + CLOCK_SKEW_SECONDS:
        raise AuthError("invalid sign-in token")
    email = str(claims.get("preferred_username") or claims.get("email") or claims.get("upn") or "").strip().lower()
    if not email.endswith(ALLOWED_EMAIL_DOMAINS):
        raise AuthError("please sign in with your Graphic Era account", 403)
    return email, str(claims.get("name") or email.split("@")[0])

class ApiError(Exception):
    """Any other HTTP error with a message (404, 409, 429...)."""
    def __init__(self, message, status):
        super().__init__(message)
        self.status = status

REQUEST_COOLDOWN_DAYS = 7   # after a decline, the same sender must wait this long

def are_connected(a, b):
    rows = query_neon("SELECT 1 FROM connections WHERE student_a = $1 AND student_b = $2;", [min(a, b), max(a, b)])
    return bool(rows.get("rows"))

def connect_pair(a, b):
    query_neon("INSERT INTO connections (student_a, student_b) VALUES ($1, $2) ON CONFLICT DO NOTHING;", [min(a, b), max(a, b)])

def send_connection_request(me, other):
    """Ask `other` to connect. Returns {"status": "pending"|"connected", ...}.
    If `other` had already asked me, this accepts their request instead."""
    if other == me:
        raise BadRequest("you cannot connect with yourself")
    if not query_neon("SELECT 1 FROM students WHERE id = $1;", [other]).get("rows"):
        raise ApiError("Student not found", 404)
    if are_connected(me, other):
        return {"status": "connected", "studentId": other}
    reverse = query_neon("SELECT id FROM connection_requests WHERE from_id = $1 AND to_id = $2 AND status = 'pending';", [other, me]).get("rows", [])
    if reverse:
        connect_pair(me, other)
        query_neon("DELETE FROM connection_requests WHERE id = $1;", [reverse[0]["id"]])
        return {"status": "connected", "studentId": other}
    pending = query_neon("SELECT id FROM connection_requests WHERE from_id = $1 AND to_id = $2 AND status = 'pending';", [me, other]).get("rows", [])
    if pending:
        return {"status": "pending", "requestId": pending[0]["id"], "studentId": other}
    recent_decline = query_neon(
        f"SELECT 1 FROM connection_requests WHERE from_id = $1 AND to_id = $2 AND status = 'declined' "
        f"AND responded_at > now() - interval '{REQUEST_COOLDOWN_DAYS} days';", [me, other]).get("rows")
    if recent_decline:
        raise ApiError("you can't send this student another request yet", 429)
    query_neon("DELETE FROM connection_requests WHERE from_id = $1 AND to_id = $2 AND status <> 'pending';", [me, other])
    rows = query_neon("INSERT INTO connection_requests (from_id, to_id) VALUES ($1, $2) ON CONFLICT DO NOTHING RETURNING id;", [me, other]).get("rows", [])
    if not rows:
        rows = query_neon("SELECT id FROM connection_requests WHERE from_id = $1 AND to_id = $2 AND status = 'pending';", [me, other]).get("rows", [])
    return {"status": "pending", "requestId": rows[0]["id"] if rows else None, "studentId": other}

def is_founder(student_id):
    """Founders moderate the site: they may delete any post, project or group."""
    rows = query_neon("SELECT 1 FROM site_roles WHERE student_id = $1 AND role = 'founder';", [student_id])
    return bool(rows.get("rows"))

def invite_to_group(group_id, inviter_id, student_ids):
    """Invite students to a group. Skips unknown students, current members and
    pending invitations. Returns the IDs actually invited."""
    invited = []
    for sid in student_ids:
        if sid == inviter_id:
            continue
        if not query_neon("SELECT 1 FROM students WHERE id = $1;", [sid]).get("rows"):
            continue
        if query_neon("SELECT 1 FROM group_members WHERE group_id = $1 AND student_id = $2;", [group_id, sid]).get("rows"):
            continue
        rows = query_neon("INSERT INTO team_invites (group_id, student_id, invited_by) VALUES ($1, $2, $3) "
                          "ON CONFLICT DO NOTHING RETURNING id;", [group_id, sid, inviter_id]).get("rows", [])
        if rows:
            invited.append(sid)
    return invited

def group_summary(group_id):
    rows = query_neon("""
        SELECT g.id, g.name, g.description, g.admin_id as "leaderId", g.is_project_group as "isProjectGroup",
               COALESCE(array_agg(gm.student_id) FILTER (WHERE gm.student_id IS NOT NULL), '{}') as "memberIds"
        FROM groups g LEFT JOIN group_members gm ON gm.group_id = g.id
        WHERE g.id = $1 GROUP BY g.id;""", [group_id]).get("rows", [])
    if not rows:
        return None
    g = rows[0]
    g["memberCount"] = len(g["memberIds"])
    return g

def path_id(path, segment):
    """Return the integer that follows `segment` in the URL path, e.g. /groups/7/join -> 7."""
    parts = path.split('/')
    try:
        return to_int(parts[parts.index(segment) + 1], f"{segment} id")
    except (ValueError, IndexError):
        raise BadRequest(f"missing {segment} id")

# Ensure tables exist — run each statement separately (Neon HTTP API is single-statement only)
SCHEMA_STATEMENTS = [
    """CREATE TABLE IF NOT EXISTS project_posts (
        id SERIAL PRIMARY KEY,
        author_id INTEGER REFERENCES students(id) ON DELETE CASCADE,
        author_name VARCHAR(100),
        project_title VARCHAR(200) NOT NULL,
        required_skills TEXT[] DEFAULT '{}',
        team_size_needed INTEGER DEFAULT 3,
        content TEXT NOT NULL DEFAULT '',
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
    )""",
    """CREATE TABLE IF NOT EXISTS connections (
        student_a INTEGER NOT NULL,
        student_b INTEGER NOT NULL,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        PRIMARY KEY (student_a, student_b)
    )""",
    """CREATE TABLE IF NOT EXISTS group_posts (
        id SERIAL PRIMARY KEY,
        group_id INTEGER REFERENCES groups(id) ON DELETE CASCADE,
        author_id INTEGER REFERENCES students(id) ON DELETE CASCADE,
        author_name VARCHAR(100) NOT NULL,
        content TEXT NOT NULL,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
    )""",
    # Connection requests: a connection exists only after the other student accepts
    """CREATE TABLE IF NOT EXISTS connection_requests (
        id SERIAL PRIMARY KEY,
        from_id INTEGER NOT NULL REFERENCES students(id) ON DELETE CASCADE,
        to_id INTEGER NOT NULL REFERENCES students(id) ON DELETE CASCADE,
        status VARCHAR(10) NOT NULL DEFAULT 'pending',
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        responded_at TIMESTAMP,
        CHECK (from_id <> to_id)
    )""",
    """CREATE UNIQUE INDEX IF NOT EXISTS connection_requests_one_pending
        ON connection_requests (from_id, to_id) WHERE status = 'pending'""",
    # One team per project post (deleting the post keeps the team, drops the link)
    """CREATE TABLE IF NOT EXISTS project_teams (
        project_post_id INTEGER PRIMARY KEY REFERENCES project_posts(id) ON DELETE CASCADE,
        group_id INTEGER NOT NULL UNIQUE REFERENCES groups(id) ON DELETE CASCADE
    )""",
    # Group/team invitations: invitees join only if they accept
    """CREATE TABLE IF NOT EXISTS team_invites (
        id SERIAL PRIMARY KEY,
        group_id INTEGER NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
        student_id INTEGER NOT NULL REFERENCES students(id) ON DELETE CASCADE,
        invited_by INTEGER REFERENCES students(id) ON DELETE SET NULL,
        status VARCHAR(10) NOT NULL DEFAULT 'pending',
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        responded_at TIMESTAMP
    )""",
    """CREATE UNIQUE INDEX IF NOT EXISTS team_invites_one_pending
        ON team_invites (group_id, student_id) WHERE status = 'pending'""",
    # Site roles (e.g. 'founder'): kept in the database, never in the public code
    """CREATE TABLE IF NOT EXISTS site_roles (
        student_id INTEGER PRIMARY KEY REFERENCES students(id) ON DELETE CASCADE,
        role VARCHAR(20) NOT NULL
    )""",
]
for _stmt in SCHEMA_STATEMENTS:
    try:
        query_neon(_stmt)
    except Exception:
        pass

class handler(BaseHTTPRequestHandler):
    def send_json(self, data, status=200):
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type, Authorization')
        self.end_headers()
        self.wfile.write(json.dumps({"ok": status in [200, 201], "data": data}).encode('utf-8'))

    def send_error_json(self, e):
        """400 for invalid user input, 401/403 for sign-in problems, 500 for everything else."""
        if isinstance(e, BadRequest):
            self.send_json({"error": str(e)}, 400)
        elif isinstance(e, (AuthError, ApiError)):
            self.send_json({"error": str(e)}, e.status)
        else:
            self.send_json({"error": str(e)}, 500)

    # --- who is calling? -------------------------------------------------
    def signed_in_user(self):
        """(email, name) from the verified Microsoft token, or raise AuthError(401)."""
        auth = self.headers.get('Authorization') or ''
        if not auth.startswith('Bearer '):
            raise AuthError("please sign in with your GEU Outlook account")
        return verify_ms_id_token(auth[len('Bearer '):].strip())

    def current_student_id(self):
        """ID of the signed-in student's profile, or raise AuthError."""
        email, _ = self.signed_in_user()
        res = query_neon("SELECT id FROM students WHERE lower(email) = $1;", [email])
        rows = res.get("rows", [])
        if not rows:
            raise AuthError("no profile yet, please sign in again to create one", 403)
        return rows[0]["id"]

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type, Authorization')
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path.rstrip('/')

        # 0a. GET /api/project-posts/:id/team — the project's team (one per project).
        #     Everyone sees members; only the author also sees pending invitations.
        if '/project-posts/' in path and path.endswith('/team'):
            try:
                proj_id = path_id(path, 'project-posts')
                proj = query_neon("SELECT author_id FROM project_posts WHERE id = $1;", [proj_id]).get("rows", [])
                if not proj:
                    raise ApiError("Project post not found", 404)
                link = query_neon("SELECT group_id FROM project_teams WHERE project_post_id = $1;", [proj_id]).get("rows", [])
                if not link:
                    self.send_json({"groupId": None, "memberIds": [], "invitedIds": []})
                    return
                g = group_summary(link[0]["group_id"])
                invited = []
                try:
                    if self.current_student_id() == proj[0]["author_id"]:
                        invited = [r["student_id"] for r in query_neon(
                            "SELECT student_id FROM team_invites WHERE group_id = $1 AND status = 'pending';", [g["id"]]).get("rows", [])]
                except AuthError:
                    pass
                self.send_json({"groupId": g["id"], "groupName": g["name"], "memberIds": g["memberIds"], "invitedIds": invited})
            except Exception as e:
                self.send_error_json(e)
            return

        # 0b. GET /api/team-invites — invitations waiting for you
        if path.endswith('/api/team-invites'):
            try:
                me = self.current_student_id()
                rows = query_neon("""
                    SELECT i.id, to_char(i.created_at, 'YYYY-MM-DD HH24:MI:SS') as "createdAt",
                           g.id as group_id, g.name as group_name, g.is_project_group,
                           s.id as inviter_id, s.name as inviter_name
                    FROM team_invites i JOIN groups g ON g.id = i.group_id
                    LEFT JOIN students s ON s.id = i.invited_by
                    WHERE i.student_id = $1 AND i.status = 'pending' ORDER BY i.id DESC;""", [me]).get("rows", [])
                self.send_json([{"id": r["id"], "createdAt": r["createdAt"],
                                 "group": {"id": r["group_id"], "name": r["group_name"], "isProjectGroup": r["is_project_group"]},
                                 "invitedBy": {"id": r["inviter_id"], "name": r["inviter_name"]}} for r in rows])
            except Exception as e:
                self.send_error_json(e)
            return

        # 0. GET /api/connection-requests  — your incoming and outgoing pending requests
        if path.endswith('/api/connection-requests'):
            try:
                me = self.current_student_id()
                def listing(sql):
                    return [{"id": r["id"], "createdAt": r["created_at"],
                             "student": {"id": r["sid"], "name": r["name"], "department": r["department"], "year": r["year"]}}
                            for r in query_neon(sql, [me]).get("rows", [])]
                cols = "r.id, to_char(r.created_at, 'YYYY-MM-DD HH24:MI:SS') as created_at, s.id as sid, s.name, s.department, s.year"
                self.send_json({
                    "incoming": listing(f"SELECT {cols} FROM connection_requests r JOIN students s ON s.id = r.from_id WHERE r.to_id = $1 AND r.status = 'pending' ORDER BY r.id DESC;"),
                    "outgoing": listing(f"SELECT {cols} FROM connection_requests r JOIN students s ON s.id = r.to_id WHERE r.from_id = $1 AND r.status = 'pending' ORDER BY r.id DESC;"),
                })
            except Exception as e:
                self.send_error_json(e)
            return

        # 1. GET /api/students  — public directory (no emails: privacy-first)
        if path.endswith('/api/students') or path.endswith('/students'):
            try:
                res = query_neon("SELECT s.id, s.name, s.department, s.year, s.skills, s.interests, r.role "
                                 "FROM students s LEFT JOIN site_roles r ON r.student_id = s.id ORDER BY s.id;")
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_error_json(e)
            return

        # 2. GET /api/connections
        if path.endswith('/api/connections') or path.endswith('/connections'):
            try:
                res = query_neon("SELECT student_a as \"a\", student_b as \"b\" FROM connections;")
                self.send_json(res.get("rows", []))
            except Exception:
                self.send_json([])
            return

        # 3. GET /api/groups
        if path.endswith('/api/groups') or path.endswith('/groups'):
            try:
                res = query_neon("""
                    SELECT g.id, g.name, g.description, g.admin_id as "leaderId",
                           g.is_project_group as "isProjectGroup",
                           COALESCE(array_agg(gm.student_id) FILTER (WHERE gm.student_id IS NOT NULL), '{}') as "memberIds",
                           COUNT(gm.student_id) as "memberCount"
                    FROM groups g
                    LEFT JOIN group_members gm ON gm.group_id = g.id
                    GROUP BY g.id, g.admin_id, g.is_project_group ORDER BY g.id DESC;
                """)
                rows = res.get("rows", [])
                for r in rows:
                    r["memberCount"] = int(r.get("memberCount", 0))
                self.send_json(rows)
            except Exception:
                self.send_json([])
            return

        # 4. GET /api/project-posts — MUST come before /posts check
        if '/project-posts' in path and not any(x in path for x in ['top-k', 'create-group']):
            try:
                res = query_neon("SELECT id, author_id as \"authorId\", author_name as \"authorName\", content, project_title as \"projectTitle\", required_skills as \"requiredSkills\", team_size_needed as \"teamSizeNeeded\", to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp FROM project_posts ORDER BY id DESC;")
                self.send_json(res.get("rows", []))
            except Exception:
                self.send_json([])
            return

        # 5. GET /api/posts (feed posts) — group posts (/groups/:id/posts) are handled below
        if (path.endswith('/api/posts') or path.endswith('/posts')) and '/groups/' not in path:
            try:
                res = query_neon("SELECT id, author_id as \"authorId\", author_name as \"authorName\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp FROM posts ORDER BY id DESC;")
                self.send_json(res.get("rows", []))
            except Exception:
                self.send_json([])
            return

        # 6. GET /api/messages/conversation?user1=&user2=  — only the two participants may read it
        if 'messages/conversation' in path:
            qs = parse_qs(parsed.query)
            try:
                u1 = to_int(qs.get('user1', ['0'])[0], "user1")
                u2 = to_int(qs.get('user2', ['0'])[0], "user2")
                if self.current_student_id() not in (u1, u2):
                    raise AuthError("you can only read your own conversations", 403)
                sql = "SELECT id, sender_id as \"senderId\", receiver_id as \"receiverId\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp FROM messages WHERE (sender_id = $1 AND receiver_id = $2) OR (sender_id = $2 AND receiver_id = $1) ORDER BY id;"
                res = query_neon(sql, [u1, u2])
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_error_json(e)
            return

        # 7. GET /api/messages/partners/:id  — only your own chat list
        if 'messages/partners' in path:
            try:
                s_id = to_int(path.split('/')[-1], "student id")
                if self.current_student_id() != s_id:
                    raise AuthError("you can only see your own chats", 403)
                sql = "SELECT DISTINCT s.id, s.name, s.department, s.year, s.skills, s.interests FROM students s JOIN messages m ON (s.id = m.sender_id AND m.receiver_id = $1) OR (s.id = m.receiver_id AND m.sender_id = $1);"
                res = query_neon(sql, [s_id])
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_error_json(e)
            return

        # 8. GET /api/students/:id/friends  — direct connections of a student
        if '/students/' in path and path.endswith('/friends'):
            try:
                s_id = path_id(path, 'students')
                sql = """
                    SELECT s.id, s.name, s.department, s.year, s.skills, s.interests
                    FROM students s
                    JOIN connections c ON (c.student_a = $1 AND c.student_b = s.id)
                                      OR (c.student_b = $1 AND c.student_a = s.id)
                    ORDER BY s.name;
                """
                res = query_neon(sql, [s_id])
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_error_json(e)
            return

        # 8b. GET /api/students/:id/suggestions?limit=10  — "People you may know"
        # Same algorithm as Graph::suggestFriends in C++: walk 2 levels of the
        # connection graph; every friend F that links me to a student C is a
        # mutual friend; rank by most mutual friends, ties by smaller ID.
        if '/students/' in path and path.endswith('/suggestions'):
            qs = parse_qs(parsed.query)
            try:
                s_id = path_id(path, 'students')
                limit = to_int(qs.get('limit', ['10'])[0], "limit")
                if limit < 1 or limit > 50:
                    raise BadRequest("limit must be between 1 and 50")
                students = query_neon("SELECT id, name, department, year, skills, interests FROM students ORDER BY id;").get("rows", [])
                by_id = {s["id"]: s for s in students}
                if s_id not in by_id:
                    self.send_json({"error": "Student not found"}, 404)
                    return
                # Adjacency list from the connections table (undirected)
                adjacency = {}
                for e in query_neon("SELECT student_a, student_b FROM connections;").get("rows", []):
                    a, b = e["student_a"], e["student_b"]
                    if a == b:
                        continue
                    adjacency.setdefault(a, set()).add(b)
                    adjacency.setdefault(b, set()).add(a)
                my_friends = adjacency.get(s_id, set())
                mutuals = {}                                   # candidate -> [mutual friend IDs]
                for f in my_friends:                           # level 1
                    for c in adjacency.get(f, ()):             # level 2
                        if c != s_id and c not in my_friends:
                            mutuals.setdefault(c, []).append(f)
                ranked = sorted(mutuals.items(), key=lambda kv: (-len(kv[1]), kv[0]))
                results = []
                for c, mids in ranked:
                    if c not in by_id:
                        continue
                    results.append({
                        "student": by_id[c],
                        "mutualCount": len(mids),
                        "mutualFriends": [{"id": m, "name": by_id[m]["name"] if m in by_id else f"Student #{m}"}
                                          for m in sorted(mids)],
                    })
                    if len(results) >= limit:
                        break
                self.send_json(results)
            except Exception as e:
                self.send_error_json(e)
            return

        # 9. GET /api/students/:id/similar  — Jaccard similarity (Python-side)
        if '/students/' in path and path.endswith('/similar'):
            try:
                s_id = path_id(path, 'students')
                res = query_neon("SELECT id, name, department, year, skills, interests FROM students ORDER BY id;")
                students = res.get("rows", [])
                me = next((s for s in students if s["id"] == s_id), None)
                if not me:
                    self.send_json([], 200)
                    return
                me_set = lower_set(me.get("skills")) | lower_set(me.get("interests"))
                results = []
                for s in students:
                    if s["id"] == s_id:
                        continue
                    other_set = lower_set(s.get("skills")) | lower_set(s.get("interests"))
                    union = me_set | other_set
                    inter = me_set & other_set
                    score = len(inter) / len(union) if union else 0.0
                    results.append({"student": s, "similarity": round(score, 4)})
                results.sort(key=lambda x: x["similarity"], reverse=True)
                self.send_json(results[:10])
            except Exception as e:
                self.send_error_json(e)
            return

        # 10. GET /api/connections/mutual?a=&b=  — mutual friends between two students
        if 'connections/mutual' in path:
            qs = parse_qs(parsed.query)
            try:
                a = to_int(qs.get('a', ['0'])[0], "a")
                b = to_int(qs.get('b', ['0'])[0], "b")
                sql = """
                    SELECT s.id, s.name, s.department FROM students s
                    WHERE s.id IN (
                        SELECT CASE WHEN student_a = $1 THEN student_b ELSE student_a END
                        FROM connections WHERE student_a = $1 OR student_b = $1
                    )
                    AND s.id IN (
                        SELECT CASE WHEN student_a = $2 THEN student_b ELSE student_a END
                        FROM connections WHERE student_a = $2 OR student_b = $2
                    );
                """
                res = query_neon(sql, [a, b])
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_error_json(e)
            return

        # 11. GET /api/project-posts/:id/top-k  — Top-K teammate matching (Jaccard, Python-side)
        if '/project-posts/' in path and 'top-k' in path:
            qs = parse_qs(parsed.query)
            try:
                proj_id = path_id(path, 'project-posts')
                k = to_int(qs.get('k', ['6'])[0], "k")
                if k < 1 or k > 50:
                    raise BadRequest("k must be between 1 and 50")
                p_res = query_neon("SELECT required_skills FROM project_posts WHERE id = $1;", [proj_id])
                if not p_res.get("rows"):
                    self.send_json({"results": []})
                    return
                req_skills = lower_set(p_res["rows"][0].get("required_skills"))
                s_res = query_neon("SELECT id, name, department, year, skills, interests FROM students ORDER BY id;")
                students = s_res.get("rows", [])
                candidates = []
                for s in students:
                    s_skills = lower_set(s.get("skills"))
                    union = req_skills | s_skills
                    inter = req_skills & s_skills
                    score = len(inter) / len(union) if union else 0.0
                    candidates.append({
                        "studentId": s["id"], "name": s["name"],
                        "department": s["department"], "year": s["year"],
                        "score": round(score, 4), "student": s,
                        # show the student's own spelling of each matched skill
                        "matchedSkills": [sk for sk in (s.get("skills") or []) if str(sk).strip().lower() in inter]
                    })
                candidates.sort(key=lambda x: x["score"], reverse=True)
                self.send_json({"results": candidates[:k]})
            except Exception as e:
                self.send_error_json(e)
            return

        # 12. GET /api/groups/:id/posts  — group discussion posts
        if '/groups/' in path and path.endswith('/posts'):
            try:
                g_id = path_id(path, 'groups')
                sql = """
                    SELECT id, author_id as "studentId", author_name as "authorName", content,
                           to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp
                    FROM group_posts WHERE group_id = $1 ORDER BY id ASC;
                """
                res = query_neon(sql, [g_id])
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_error_json(e)
            return

        self.send_json({"error": "Endpoint not found"}, 404)

    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path.rstrip('/')
        try:
            content_len = int(self.headers.get('Content-Length', 0))
            body = json.loads(self.rfile.read(content_len).decode('utf-8')) if content_len > 0 else {}
            if not isinstance(body, dict):
                raise BadRequest("request body must be a JSON object")
        except BadRequest as e:
            self.send_error_json(e)
            return
        except (ValueError, UnicodeDecodeError):
            self.send_json({"error": "invalid JSON body"}, 400)
            return

        # 0. Edit / delete posts
        #    POST /api/posts/:id/edit {content}            — feed post, author only
        #    POST /api/posts/:id/delete                    — feed post, author only
        #    POST /api/groups/:gid/posts/:pid/edit {content} — group post, author only
        #    POST /api/groups/:gid/posts/:pid/delete       — group post, author or group admin
        if path.endswith('/edit') or path.endswith('/delete'):
            parts = path.split('/')
            action = parts[-1]
            try:
                if len(parts) >= 3 and parts[-3] == 'posts' and '/groups/' in path:
                    g_id = path_id(path, 'groups')
                    p_id = to_int(parts[-2], "post id")
                    me = self.current_student_id()
                    rows = query_neon("SELECT gp.author_id, g.admin_id FROM group_posts gp JOIN groups g ON g.id = gp.group_id WHERE gp.id = $1 AND gp.group_id = $2;", [p_id, g_id]).get("rows", [])
                    if not rows:
                        self.send_json({"error": "Post not found"}, 404)
                        return
                    is_author = rows[0]["author_id"] == me
                    if action == 'edit':
                        if not is_author:
                            raise AuthError("you can only edit your own posts", 403)
                        content = text_field(body.get('content'), "content", 2000, required=True)
                        res = query_neon("UPDATE group_posts SET content = $1 WHERE id = $2 RETURNING id, author_id as \"studentId\", author_name as \"authorName\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;", [content, p_id])
                        self.send_json(res["rows"][0])
                    else:
                        if not (is_author or rows[0]["admin_id"] == me or is_founder(me)):
                            raise AuthError("only the author or the group admin can delete this post", 403)
                        query_neon("DELETE FROM group_posts WHERE id = $1;", [p_id])
                        self.send_json({"deleted": p_id})
                    return
                if len(parts) >= 3 and parts[-3] == 'groups' and action == 'delete':
                    # POST /api/groups/:id/delete — the group admin only (posts, members, invites go too)
                    g_id = to_int(parts[-2], "group id")
                    me = self.current_student_id()
                    rows = query_neon("SELECT admin_id FROM groups WHERE id = $1;", [g_id]).get("rows", [])
                    if not rows:
                        raise ApiError("Group not found", 404)
                    if rows[0]["admin_id"] != me and not is_founder(me):
                        raise AuthError("only the group admin can delete the group", 403)
                    query_neon("DELETE FROM groups WHERE id = $1;", [g_id])
                    self.send_json({"deleted": g_id})
                    return
                if len(parts) >= 3 and parts[-3] == 'project-posts':
                    # POST /api/project-posts/:id/edit {projectTitle, requiredSkills, teamSizeNeeded, content}
                    # POST /api/project-posts/:id/delete      — author only
                    p_id = to_int(parts[-2], "project id")
                    me = self.current_student_id()
                    rows = query_neon("SELECT author_id FROM project_posts WHERE id = $1;", [p_id]).get("rows", [])
                    if not rows:
                        self.send_json({"error": "Project post not found"}, 404)
                        return
                    if rows[0]["author_id"] != me and not (action == 'delete' and is_founder(me)):
                        raise AuthError(f"you can only {action} your own project posts", 403)
                    if action == 'edit':
                        title = text_field(body.get('projectTitle'), "projectTitle", 200, required=True)
                        skills = pg_text_array(body.get('requiredSkills', []), "requiredSkills")
                        size = to_int(body.get('teamSizeNeeded', 3), "teamSizeNeeded")
                        if size < 1 or size > 20:
                            raise BadRequest("teamSizeNeeded must be between 1 and 20")
                        content = text_field(body.get('content') or title, "content", 2000) or title
                        res = query_neon("UPDATE project_posts SET project_title = $1, required_skills = $2::text[], team_size_needed = $3, content = $4 WHERE id = $5 RETURNING id, author_id as \"authorId\", author_name as \"authorName\", content, project_title as \"projectTitle\", required_skills as \"requiredSkills\", team_size_needed as \"teamSizeNeeded\", to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;",
                                         [title, skills, size, content, p_id])
                        self.send_json(res["rows"][0])
                    else:
                        query_neon("DELETE FROM project_posts WHERE id = $1;", [p_id])
                        self.send_json({"deleted": p_id})
                    return
                if len(parts) >= 3 and parts[-3] == 'posts' and '/project-posts/' not in path:
                    p_id = to_int(parts[-2], "post id")
                    me = self.current_student_id()
                    rows = query_neon("SELECT author_id FROM posts WHERE id = $1;", [p_id]).get("rows", [])
                    if not rows:
                        self.send_json({"error": "Post not found"}, 404)
                        return
                    if rows[0]["author_id"] != me and not (action == 'delete' and is_founder(me)):
                        raise AuthError(f"you can only {action} your own posts", 403)
                    if action == 'edit':
                        content = text_field(body.get('content'), "content", 2000, required=True)
                        res = query_neon("UPDATE posts SET content = $1 WHERE id = $2 RETURNING id, author_id as \"authorId\", author_name as \"authorName\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;", [content, p_id])
                        self.send_json(res["rows"][0])
                    else:
                        query_neon("DELETE FROM posts WHERE id = $1;", [p_id])
                        self.send_json({"deleted": p_id})
                    return
            except Exception as e:
                self.send_error_json(e)
                return
            self.send_json({"error": "Endpoint not found"}, 404)
            return

        # 0a. Team invitations
        #     POST /api/team-invites/:id/accept|decline — the invited student
        #     POST /api/team-invites/:id/cancel        — the group admin or whoever invited
        if '/api/team-invites/' in path:
            try:
                me = self.current_student_id()
                parts = path.split('/')
                action = parts[-1]
                inv_id = to_int(parts[-2], "invitation id")
                rows = query_neon("SELECT i.group_id, i.student_id, i.invited_by, g.admin_id FROM team_invites i JOIN groups g ON g.id = i.group_id "
                                  "WHERE i.id = $1 AND i.status = 'pending';", [inv_id]).get("rows", [])
                if not rows:
                    raise ApiError("Invitation not found (it may have been answered already)", 404)
                inv = rows[0]
                if action in ('accept', 'decline') and inv["student_id"] != me:
                    raise AuthError("this invitation is for someone else", 403)
                if action == 'cancel' and me not in (inv["invited_by"], inv["admin_id"]):
                    raise AuthError("only the group admin can cancel this invitation", 403)
                if action == 'accept':
                    query_neon("INSERT INTO group_members (group_id, student_id) VALUES ($1, $2) ON CONFLICT DO NOTHING;", [inv["group_id"], me])
                    query_neon("DELETE FROM team_invites WHERE id = $1;", [inv_id])
                    self.send_json({"status": "joined", "groupId": inv["group_id"]})
                elif action == 'decline':
                    query_neon("UPDATE team_invites SET status = 'declined', responded_at = now() WHERE id = $1;", [inv_id])
                    self.send_json({"status": "declined"})
                elif action == 'cancel':
                    query_neon("DELETE FROM team_invites WHERE id = $1;", [inv_id])
                    self.send_json({"status": "cancelled"})
                else:
                    self.send_json({"error": "Endpoint not found"}, 404)
            except Exception as e:
                self.send_error_json(e)
            return

        # 0b. Connection requests
        #     POST /api/connection-requests {toId}            — send (or accept theirs if they asked first)
        #     POST /api/connection-requests/:id/accept|decline — the receiver
        #     POST /api/connection-requests/:id/cancel        — the sender
        #     POST /api/connections/remove {studentId}        — disconnect
        if '/api/connection-requests' in path or path.endswith('/api/connections/remove'):
            try:
                me = self.current_student_id()
                if path.endswith('/api/connection-requests'):
                    self.send_json(send_connection_request(me, to_int(body.get('toId'), "toId")))
                    return
                if path.endswith('/api/connections/remove'):
                    other = to_int(body.get('studentId'), "studentId")
                    query_neon("DELETE FROM connections WHERE student_a = $1 AND student_b = $2;", [min(me, other), max(me, other)])
                    self.send_json({"removed": other})
                    return
                parts = path.split('/')
                action = parts[-1]
                req_id = to_int(parts[-2], "request id")
                rows = query_neon("SELECT from_id, to_id FROM connection_requests WHERE id = $1 AND status = 'pending';", [req_id]).get("rows", [])
                if not rows:
                    raise ApiError("Request not found (it may have been answered already)", 404)
                from_id, to_id = rows[0]["from_id"], rows[0]["to_id"]
                if action in ('accept', 'decline') and to_id != me:
                    raise AuthError("only the person who received this request can answer it", 403)
                if action == 'cancel' and from_id != me:
                    raise AuthError("only the sender can cancel this request", 403)
                if action == 'accept':
                    connect_pair(from_id, to_id)
                    query_neon("DELETE FROM connection_requests WHERE id = $1;", [req_id])
                    self.send_json({"status": "connected", "studentId": from_id})
                elif action == 'decline':
                    query_neon("UPDATE connection_requests SET status = 'declined', responded_at = now() WHERE id = $1;", [req_id])
                    self.send_json({"status": "declined"})
                elif action == 'cancel':
                    query_neon("DELETE FROM connection_requests WHERE id = $1;", [req_id])
                    self.send_json({"status": "cancelled"})
                else:
                    self.send_json({"error": "Endpoint not found"}, 404)
            except Exception as e:
                self.send_error_json(e)
            return

        # 1. POST /api/students  — sign in: returns your profile, creating it on first sign-in.
        #    Name and email come from the verified Microsoft token, never from the request body.
        if path.endswith('/api/students') or path.endswith('/students'):
            try:
                email, ms_name = self.signed_in_user()
                find = "SELECT id, name, email, department, year, skills, interests FROM students WHERE lower(email) = $1;"
                existing = query_neon(find, [email]).get("rows", [])
                if existing:
                    self.send_json(existing[0], 200)
                    return
                dept = text_field(body.get('department', 'CSE'), "department", 50) or 'CSE'
                year = to_int(body.get('year', 2), "year")
                skills = pg_text_array(body.get('skills', ['C++', 'Python', 'DSA']), "skills")
                interests = pg_text_array(body.get('interests', ['Web Dev', 'AI']), "interests")
                sql = "INSERT INTO students (name, email, department, year, skills, interests) VALUES ($1, $2, $3, $4, $5::text[], $6::text[]) ON CONFLICT (email) DO NOTHING RETURNING id, name, email, department, year, skills, interests;"
                rows = query_neon(sql, [ms_name[:100], email, dept, year, skills, interests]).get("rows", [])
                if not rows:   # created by a parallel request a moment ago
                    rows = query_neon(find, [email]).get("rows", [])
                self.send_json(rows[0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 2. POST /api/students/:id/update (Update Skills & Interests) — your own profile only
        if '/api/students/' in path and 'update' in path:
            try:
                s_id = path_id(path, 'students')
                if self.current_student_id() != s_id:
                    raise AuthError("you can only edit your own profile", 403)
                dept = text_field(body.get('department', 'CSE'), "department", 50, required=True)
                year = to_int(body.get('year', 2), "year")
                if year < 1 or year > 6:
                    raise BadRequest("year must be between 1 and 6")
                skills = pg_text_array(body.get('skills', []), "skills")
                interests = pg_text_array(body.get('interests', []), "interests")
                sql = "UPDATE students SET department = $1, year = $2, skills = $3::text[], interests = $4::text[] WHERE id = $5 RETURNING id, name, department, year, skills, interests;"
                res = query_neon(sql, [dept, year, skills, interests, s_id])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 3. POST /api/connections (Connect with another student) — one side must be you
        if path.endswith('/api/connections') or path.endswith('/connections'):
            a = body.get('a') or body.get('studentA')
            b = body.get('b') or body.get('studentB')
            if a and b:
                try:
                    a = to_int(a, "a")
                    b = to_int(b, "b")
                    me = self.current_student_id()
                    if me not in (a, b):
                        raise AuthError("you can only add your own connections", 403)
                    other = b if a == me else a
                    # No more instant connections: this sends a request the other
                    # student must accept (or accepts theirs if they asked first).
                    self.send_json(send_connection_request(me, other))
                except Exception as e:
                    self.send_error_json(e)
            else:
                self.send_json({"error": "Missing student IDs"}, 400)
            return

        # 4. POST /api/project-posts (Add project collaboration post) — MUST come before /posts check
        if '/project-posts' in path and not any(x in path for x in ['top-k', 'create-group']):
            try:
                author_id = self.current_student_id()   # always you, whatever the form says
                p_title = text_field(body.get('projectTitle', 'Project'), "projectTitle", 200, required=True)
                skills = pg_text_array(body.get('requiredSkills', []), "requiredSkills")
                size = to_int(body.get('teamSizeNeeded', 3), "teamSizeNeeded")
                if size < 1 or size > 20:
                    raise BadRequest("teamSizeNeeded must be between 1 and 20")
                content = text_field(body.get('content') or p_title, "content", 2000) or 'Project collaboration'
                s_res = query_neon("SELECT name FROM students WHERE id = $1;", [author_id])
                author_name = s_res['rows'][0]['name'] if s_res.get('rows') else "Student"
                sql = "INSERT INTO project_posts (author_id, author_name, project_title, required_skills, team_size_needed, content) VALUES ($1, $2, $3, $4::text[], $5, $6) RETURNING id, author_id as \"authorId\", author_name as \"authorName\", content, project_title as \"projectTitle\", required_skills as \"requiredSkills\", team_size_needed as \"teamSizeNeeded\", to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql, [author_id, author_name, p_title, skills, size, content])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 5. POST /api/posts (Add feed post) — group posts (/groups/:id/posts) are handled below
        if (path.endswith('/api/posts') or path.endswith('/posts')) and '/groups/' not in path:
            try:
                author_id = self.current_student_id()
                content = text_field(body.get('content'), "content", 2000, required=True)
                s_res = query_neon("SELECT name FROM students WHERE id = $1;", [author_id])
                author_name = s_res['rows'][0]['name'] if s_res.get('rows') else "Student"
                sql = "INSERT INTO posts (author_id, author_name, content) VALUES ($1, $2, $3) RETURNING id, author_id as \"authorId\", author_name as \"authorName\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql, [author_id, author_name, content])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 6. POST /api/groups                       — create a club; you are the admin
        #    POST /api/project-posts/:id/create-group — the project's team: created once,
        #       later calls reuse it. In both, memberIds get INVITATIONS (never auto-added).
        if 'create-group' in path or path.endswith('/groups'):
            try:
                admin_id = self.current_student_id()
                raw_members = body.get('memberIds', [])
                if not isinstance(raw_members, list):
                    raise BadRequest("'memberIds' must be a list")
                member_ids = []
                for m in raw_members:
                    m_id = to_int(m, "memberIds")
                    if m_id != admin_id and m_id not in member_ids:
                        member_ids.append(m_id)
                insert_group = "INSERT INTO groups (name, description, admin_id, is_project_group) VALUES ($1, $2, $3, $4::boolean) RETURNING id;"
                created = False
                if 'create-group' in path:
                    proj_id = path_id(path, 'project-posts')
                    proj = query_neon("SELECT author_id, project_title FROM project_posts WHERE id = $1;", [proj_id]).get("rows", [])
                    if not proj:
                        raise ApiError("Project post not found", 404)
                    if proj[0]["author_id"] != admin_id:
                        raise AuthError("only the project author can build its team", 403)
                    link = query_neon("SELECT group_id FROM project_teams WHERE project_post_id = $1;", [proj_id]).get("rows", [])
                    if link:
                        g_id = link[0]["group_id"]
                    else:
                        name = text_field(body.get('groupName') or f"{proj[0]['project_title']} Team", "name", 150, required=True)
                        desc = text_field(body.get('description', 'Project team'), "description", 1000)
                        g_id = query_neon(insert_group, [name, desc, admin_id, "true"])["rows"][0]["id"]
                        query_neon("INSERT INTO group_members (group_id, student_id) VALUES ($1, $2) ON CONFLICT DO NOTHING;", [g_id, admin_id])
                        won = query_neon("INSERT INTO project_teams (project_post_id, group_id) VALUES ($1, $2) ON CONFLICT DO NOTHING RETURNING group_id;",
                                         [proj_id, g_id]).get("rows", [])
                        if won:
                            created = True
                        else:   # a parallel click created the team first: use that one
                            query_neon("DELETE FROM groups WHERE id = $1;", [g_id])
                            g_id = query_neon("SELECT group_id FROM project_teams WHERE project_post_id = $1;", [proj_id])["rows"][0]["group_id"]
                else:
                    name = text_field(body.get('groupName') or body.get('name', 'Team'), "name", 150, required=True)
                    desc = text_field(body.get('description', 'Collaboration team'), "description", 1000)
                    g_id = query_neon(insert_group, [name, desc, admin_id, "false"])["rows"][0]["id"]
                    query_neon("INSERT INTO group_members (group_id, student_id) VALUES ($1, $2) ON CONFLICT DO NOTHING;", [g_id, admin_id])
                    created = True
                invited = invite_to_group(g_id, admin_id, member_ids)
                result = group_summary(g_id)
                result["created"] = created
                result["invited"] = invited
                self.send_json(result, 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 7. POST /api/groups/:id/join
        if '/groups/' in path and path.endswith('/join'):
            try:
                g_id = path_id(path, 'groups')
                student_id = self.current_student_id()
                grp = query_neon("SELECT is_project_group FROM groups WHERE id = $1;", [g_id]).get("rows", [])
                if not grp:
                    raise ApiError("Group not found", 404)
                if grp[0]["is_project_group"]:
                    raise AuthError("project teams are formed by the project author", 403)
                query_neon("INSERT INTO group_members (group_id, student_id) VALUES ($1, $2) ON CONFLICT DO NOTHING;", [g_id, student_id])
                self.send_json({"groupId": g_id, "studentId": student_id})
            except Exception as e:
                self.send_error_json(e)
            return

        # 8. POST /api/groups/:id/leave
        if '/groups/' in path and path.endswith('/leave'):
            try:
                g_id = path_id(path, 'groups')
                student_id = self.current_student_id()
                admin = query_neon("SELECT admin_id FROM groups WHERE id = $1;", [g_id]).get("rows", [])
                if admin and admin[0]["admin_id"] == student_id:
                    raise BadRequest("the group admin cannot leave the group")
                query_neon("DELETE FROM group_members WHERE group_id = $1 AND student_id = $2;", [g_id, student_id])
                self.send_json({"groupId": g_id, "studentId": student_id})
            except Exception as e:
                self.send_error_json(e)
            return

        # 9. POST /api/groups/:id/posts  — post to group discussion (members only)
        if '/groups/' in path and path.endswith('/posts'):
            try:
                g_id = path_id(path, 'groups')
                student_id = self.current_student_id()
                member = query_neon("SELECT 1 FROM group_members WHERE group_id = $1 AND student_id = $2;", [g_id, student_id])
                if not member.get("rows"):
                    raise AuthError("join the group to post in it", 403)
                content = text_field(body.get('content'), "content", 2000, required=True)
                name_rows = query_neon("SELECT name FROM students WHERE id = $1;", [student_id]).get("rows", [])
                author_name = name_rows[0]["name"] if name_rows else "Student"
                sql = "INSERT INTO group_posts (group_id, author_id, author_name, content) VALUES ($1, $2, $3, $4) RETURNING id, author_id as \"studentId\", author_name as \"authorName\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql, [g_id, student_id, author_name, content])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 10. POST /api/messages (Send direct message)
        if path.endswith('/api/messages') or path.endswith('/messages'):
            try:
                s_id = self.current_student_id()   # you are always the sender
                r_id = to_int(body.get('receiverId'), "receiverId")
                if r_id == s_id:
                    raise BadRequest("you cannot message yourself")
                if not are_connected(s_id, r_id):
                    raise AuthError("connect with this student before messaging them", 403)
                content = text_field(body.get('content'), "content", 2000, required=True)
                sql = "INSERT INTO messages (sender_id, receiver_id, content) VALUES ($1, $2, $3) RETURNING id, sender_id as \"senderId\", receiver_id as \"receiverId\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql, [s_id, r_id, content])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 11. POST /api/demo/load  — seed demo data. Off unless ALLOW_DEMO_LOAD=1 is set
        #     (never set it on the public site: it inserts fake students into the real DB).
        if 'demo/load' in path:
            if os.environ.get("ALLOW_DEMO_LOAD") != "1":
                self.send_json({"error": "demo data is disabled on this server"}, 403)
                return
            try:
                demo_students = [
                    ("Alice Sharma", "alice.sharma@geu.ac.in", "CSE", 2, ["Python","ML","React"], ["AI","Web Dev"]),
                    ("Bob Verma",   "bob.verma@geu.ac.in",   "ECE", 3, ["C++","Embedded","IoT"], ["Robotics","Hardware"]),
                    ("Priya Singh", "priya.singh@geu.ac.in", "AI-DS", 1, ["Python","Data Analysis","SQL"], ["AI","Research"]),
                    ("Rahul Negi",  "rahul.negi@geu.ac.in",  "IT",  2, ["JavaScript","React","Node.js"], ["Web Dev","Open Source"]),
                    ("Sneha Joshi", "sneha.joshi@geu.ac.in", "CSE", 4, ["Java","Spring","DSA"], ["Competitive Prog","Backend"]),
                ]
                ids = []
                for (nm, em, dp, yr, sk, it) in demo_students:
                    r = query_neon(
                        "INSERT INTO students (name,email,department,year,skills,interests) VALUES ($1,$2,$3,$4,$5::text[],$6::text[]) ON CONFLICT (email) DO UPDATE SET name=EXCLUDED.name RETURNING id;",
                        [nm, em, dp, yr, pg_text_array(sk, "skills"), pg_text_array(it, "interests")])
                    ids.append(r["rows"][0]["id"])
                # Connect them
                pairs = [(0,1),(0,2),(1,2),(2,3),(3,4),(0,4)]
                for (i,j) in pairs:
                    a, b = min(ids[i],ids[j]), max(ids[i],ids[j])
                    try: query_neon("INSERT INTO connections (student_a,student_b) VALUES ($1,$2) ON CONFLICT DO NOTHING;", [a, b])
                    except Exception: pass
                # Demo feed post
                try: query_neon("INSERT INTO posts (author_id,author_name,content) VALUES ($1,$2,$3) ON CONFLICT DO NOTHING;",
                                [ids[0], 'Alice Sharma', 'Welcome to Campus Connect! 🎓 Connect, collaborate, and build amazing projects together.'])
                except Exception: pass
                # Demo project post
                try: query_neon("INSERT INTO project_posts (author_id,author_name,project_title,required_skills,team_size_needed,content) VALUES ($1,$2,$3,$4::text[],$5,$6) ON CONFLICT DO NOTHING;",
                                [ids[0], 'Alice Sharma', 'AI Campus Assistant', pg_text_array(['Python','ML','React'], "skills"), 3, 'Building an AI assistant for campus navigation and scheduling.'])
                except Exception: pass
                self.send_json({"message": "Demo data loaded!", "studentCount": len(ids)})
            except Exception as e:
                self.send_error_json(e)
            return

        self.send_json({"error": "Endpoint not found"}, 404)
