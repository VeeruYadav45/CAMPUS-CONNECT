from http.server import BaseHTTPRequestHandler
import json
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
    with urllib.request.urlopen(req, context=ctx, timeout=10) as resp:
        return json.loads(resp.read().decode("utf-8"))

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

def pg_text_array(items, field):
    """Encode a list of strings as a Postgres array literal, e.g. {"C++","AI"}.
    Passed as a parameter with ::text[], so quotes in items cannot escape."""
    if items is None:
        items = []
    if not isinstance(items, list):
        raise BadRequest(f"'{field}' must be a list")
    parts = []
    for item in items:
        text = str(item).replace("\\", "\\\\").replace('"', '\\"')
        parts.append(f'"{text}"')
    return "{" + ",".join(parts) + "}"

# Ensure tables exist
SCHEMA_INIT = """
CREATE TABLE IF NOT EXISTS project_posts (
    id SERIAL PRIMARY KEY,
    author_id INTEGER REFERENCES students(id) ON DELETE CASCADE,
    author_name VARCHAR(100),
    project_title VARCHAR(200) NOT NULL,
    required_skills TEXT[] DEFAULT '{}',
    team_size_needed INTEGER DEFAULT 3,
    content TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS connections (
    student_a INTEGER NOT NULL,
    student_b INTEGER NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (student_a, student_b)
);
"""
try:
    query_neon(SCHEMA_INIT)
except Exception:
    pass

class handler(BaseHTTPRequestHandler):
    def send_json(self, data, status=200):
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()
        self.wfile.write(json.dumps({"ok": status in [200, 201], "data": data}).encode('utf-8'))

    def send_error_json(self, e):
        """400 for invalid user input, 500 for everything else."""
        if isinstance(e, BadRequest):
            self.send_json({"error": str(e)}, 400)
        else:
            self.send_json({"error": str(e)}, 500)

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path.rstrip('/')

        # 1. GET /api/students
        if path.endswith('/api/students') or path.endswith('/students'):
            try:
                res = query_neon("SELECT id, name, email, department, year, skills, interests FROM students ORDER BY id;")
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_error_json(e)
            return

        # 2. GET /api/connections
        if path.endswith('/api/connections') or path.endswith('/connections'):
            try:
                res = query_neon("SELECT student_a as \"studentA\", student_b as \"studentB\" FROM connections;")
                self.send_json(res.get("rows", []))
            except Exception:
                self.send_json([])
            return

        # 3. GET /api/groups
        if path.endswith('/api/groups') or path.endswith('/groups'):
            try:
                res = query_neon("SELECT id, name, description, leader_id as \"leaderId\" FROM groups ORDER BY id DESC;")
                self.send_json(res.get("rows", []))
            except Exception:
                self.send_json([])
            return

        # 4. GET /api/posts
        if path.endswith('/api/posts') or path.endswith('/posts'):
            try:
                res = query_neon("SELECT id, author_id as \"authorId\", author_name as \"authorName\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp FROM posts ORDER BY id DESC;")
                self.send_json(res.get("rows", []))
            except Exception:
                self.send_json([])
            return

        # 5. GET /api/project-posts
        if path.endswith('/api/project-posts') or path.endswith('/project-posts'):
            try:
                res = query_neon("SELECT id, author_id as \"authorId\", author_name as \"authorName\", content, project_title as \"projectTitle\", required_skills as \"requiredSkills\", team_size_needed as \"teamSizeNeeded\", to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp FROM project_posts ORDER BY id DESC;")
                self.send_json(res.get("rows", []))
            except Exception:
                self.send_json([])
            return

        # 6. GET /api/messages/conversation
        if 'messages/conversation' in path:
            qs = parse_qs(parsed.query)
            try:
                u1 = to_int(qs.get('user1', ['0'])[0], 'user1')
                u2 = to_int(qs.get('user2', ['0'])[0], 'user2')
                sql = "SELECT id, sender_id as \"senderId\", receiver_id as \"receiverId\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp FROM messages WHERE (sender_id=$1 AND receiver_id=$2) OR (sender_id=$2 AND receiver_id=$1) ORDER BY id;"
                res = query_neon(sql, [u1, u2])
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_error_json(e)
            return

        # 7. GET /api/messages/partners/:id
        if 'messages/partners' in path:
            try:
                s_id = to_int(path.split('/')[-1], 'id')
                sql = "SELECT DISTINCT s.id, s.name, s.department, s.year, s.skills, s.interests FROM students s JOIN messages m ON (s.id = m.sender_id AND m.receiver_id = $1) OR (s.id = m.receiver_id AND m.sender_id = $1);"
                res = query_neon(sql, [s_id])
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_error_json(e)
            return

        self.send_json({"error": "Endpoint not found"}, 404)

    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path.rstrip('/')
        content_len = int(self.headers.get('Content-Length', 0))
        body = json.loads(self.rfile.read(content_len).decode('utf-8')) if content_len > 0 else {}

        # 1. POST /api/students (Register/Login via Microsoft)
        if path.endswith('/api/students') or path.endswith('/students'):
            try:
                name = str(body.get('name', 'Student'))
                dept = str(body.get('department', 'CSE'))
                year = to_int(body.get('year', 2), 'year')
                email = str(body.get('email', f"{name.lower().replace(' ', '.')}@geu.ac.in"))
                skills = pg_text_array(body.get('skills', ['C++', 'Python', 'DSA']), 'skills')
                interests = pg_text_array(body.get('interests', ['Web Dev', 'AI']), 'interests')
                sql = "INSERT INTO students (name, email, department, year, skills, interests) VALUES ($1, $2, $3, $4, $5::text[], $6::text[]) RETURNING id, name, department, year, skills, interests;"
                res = query_neon(sql, [name, email, dept, year, skills, interests])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 2. POST /api/students/:id/update (Update Skills & Interests)
        if '/api/students/' in path and 'update' in path:
            parts = path.split('/')
            try:
                s_id = to_int(parts[parts.index('students') + 1], 'id')
                dept = str(body.get('department', 'CSE'))
                year = to_int(body.get('year', 2), 'year')
                skills = pg_text_array(body.get('skills', []), 'skills')
                interests = pg_text_array(body.get('interests', []), 'interests')
                sql = "UPDATE students SET department = $1, year = $2, skills = $3::text[], interests = $4::text[] WHERE id = $5 RETURNING id, name, department, year, skills, interests;"
                res = query_neon(sql, [dept, year, skills, interests, s_id])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 3. POST /api/connections (Connect with another student)
        if path.endswith('/api/connections') or path.endswith('/connections'):
            a = body.get('a') or body.get('studentA')
            b = body.get('b') or body.get('studentB')
            if a and b:
                try:
                    a, b = to_int(a, 'a'), to_int(b, 'b')
                    s1, s2 = min(a, b), max(a, b)
                    sql = "INSERT INTO connections (student_a, student_b) VALUES ($1, $2) ON CONFLICT DO NOTHING;"
                    query_neon(sql, [s1, s2])
                    self.send_json({"studentA": s1, "studentB": s2}, 200)
                except Exception as e:
                    self.send_error_json(e)
            else:
                self.send_json({"error": "Missing student IDs"}, 400)
            return

        # 4. POST /api/posts (Add feed post)
        if path.endswith('/api/posts') or path.endswith('/posts'):
            try:
                author_id = to_int(body.get('authorId'), 'authorId')
                content = str(body.get('content', ''))
                s_res = query_neon("SELECT name FROM students WHERE id = $1;", [author_id])
                author_name = s_res['rows'][0]['name'] if s_res.get('rows') else "Student"
                sql = "INSERT INTO posts (author_id, author_name, content) VALUES ($1, $2, $3) RETURNING id, author_id as \"authorId\", author_name as \"authorName\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql, [author_id, author_name, content])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 5. POST /api/project-posts (Add project collaboration post)
        if path.endswith('/api/project-posts') or path.endswith('/project-posts'):
            try:
                author_id = to_int(body.get('authorId'), 'authorId')
                p_title = str(body.get('projectTitle', 'Project'))
                skills = pg_text_array(body.get('requiredSkills', []), 'requiredSkills')
                size = to_int(body.get('teamSizeNeeded', 3), 'teamSizeNeeded')
                content = str(body.get('content', ''))
                s_res = query_neon("SELECT name FROM students WHERE id = $1;", [author_id])
                author_name = s_res['rows'][0]['name'] if s_res.get('rows') else "Student"
                sql = "INSERT INTO project_posts (author_id, author_name, project_title, required_skills, team_size_needed, content) VALUES ($1, $2, $3, $4::text[], $5, $6) RETURNING id, author_id as \"authorId\", author_name as \"authorName\", content, project_title as \"projectTitle\", required_skills as \"requiredSkills\", team_size_needed as \"teamSizeNeeded\", to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql, [author_id, author_name, p_title, skills, size, content])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 6. POST /api/groups or /api/project-posts/:id/create-group (Create Team / Group)
        if 'create-group' in path or path.endswith('/groups'):
            try:
                leader_id = to_int(body.get('leaderId', 1), 'leaderId')
                name = str(body.get('groupName') or body.get('name', 'Team'))
                desc = str(body.get('description', 'Collaboration team'))
                sql = "INSERT INTO groups (name, description, leader_id) VALUES ($1, $2, $3) RETURNING id, name, description, leader_id as \"leaderId\";"
                res = query_neon(sql, [name, desc, leader_id])
                g_id = res["rows"][0]["id"]
                # Add leader as member
                try:
                    query_neon("INSERT INTO group_members (group_id, student_id) VALUES ($1, $2) ON CONFLICT DO NOTHING;", [g_id, leader_id])
                except Exception:
                    pass
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 7. POST /api/messages (Send direct message)
        if path.endswith('/api/messages') or path.endswith('/messages'):
            try:
                s_id = to_int(body.get('senderId'), 'senderId')
                r_id = to_int(body.get('receiverId'), 'receiverId')
                content = str(body.get('content', ''))
                sql = "INSERT INTO messages (sender_id, receiver_id, content) VALUES ($1, $2, $3) RETURNING id, sender_id as \"senderId\", receiver_id as \"receiverId\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql, [s_id, r_id, content])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        self.send_json({"error": "Endpoint not found"}, 404)
