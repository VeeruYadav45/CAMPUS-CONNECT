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

def query_neon(sql):
    db_url = get_db_url()
    if not db_url:
        raise ValueError("DATABASE_URL not set!")
    host = db_url.split("@")[1].split("/")[0].split("?")[0]
    req = urllib.request.Request(
        f"https://{host}/sql",
        data=json.dumps({"query": sql}).encode("utf-8"),
        headers={
            "Neon-Connection-String": db_url,
            "Content-Type": "application/json"
        }
    )
    ctx = ssl.create_default_context()
    with urllib.request.urlopen(req, context=ctx, timeout=10) as resp:
        return json.loads(resp.read().decode("utf-8"))

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
                self.send_json({"error": str(e)}, 500)
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
            u1 = qs.get('user1', ['0'])[0]
            u2 = qs.get('user2', ['0'])[0]
            try:
                sql = f"SELECT id, sender_id as \"senderId\", receiver_id as \"receiverId\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp FROM messages WHERE (sender_id={u1} AND receiver_id={u2}) OR (sender_id={u2} AND receiver_id={u1}) ORDER BY id;"
                res = query_neon(sql)
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 7. GET /api/messages/partners/:id
        if 'messages/partners' in path:
            s_id = path.split('/')[-1]
            try:
                sql = f"SELECT DISTINCT s.id, s.name, s.department, s.year, s.skills, s.interests FROM students s JOIN messages m ON (s.id = m.sender_id AND m.receiver_id = {s_id}) OR (s.id = m.receiver_id AND m.sender_id = {s_id});"
                res = query_neon(sql)
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        self.send_json({"error": "Endpoint not found"}, 404)

    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path.rstrip('/')
        content_len = int(self.headers.get('Content-Length', 0))
        body = json.loads(self.rfile.read(content_len).decode('utf-8')) if content_len > 0 else {}

        # 1. POST /api/students (Register/Login via Microsoft)
        if path.endswith('/api/students') or path.endswith('/students'):
            name = body.get('name', 'Student').replace("'", "''")
            dept = body.get('department', 'CSE').replace("'", "''")
            year = body.get('year', 2)
            email = body.get('email', f"{name.lower().replace(' ', '.')}@geu.ac.in").replace("'", "''")
            skills = body.get('skills', ['C++', 'Python', 'DSA'])
            interests = body.get('interests', ['Web Dev', 'AI'])
            skills_arr = "ARRAY[" + ",".join([f"'{s}'" for s in skills]) + "]"
            interests_arr = "ARRAY[" + ",".join([f"'{i}'" for i in interests]) + "]"
            try:
                sql = f"INSERT INTO students (name, email, department, year, skills, interests) VALUES ('{name}', '{email}', '{dept}', {year}, {skills_arr}, {interests_arr}) RETURNING id, name, department, year, skills, interests;"
                res = query_neon(sql)
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 2. POST /api/students/:id/update (Update Skills & Interests)
        if '/api/students/' in path and 'update' in path:
            parts = path.split('/')
            s_id = parts[parts.index('students') + 1]
            dept = body.get('department', 'CSE').replace("'", "''")
            year = body.get('year', 2)
            skills = body.get('skills', [])
            interests = body.get('interests', [])
            skills_arr = "ARRAY[" + ",".join([f"'{s}'" for s in skills]) + "]" if skills else "ARRAY[]::TEXT[]"
            interests_arr = "ARRAY[" + ",".join([f"'{i}'" for i in interests]) + "]" if interests else "ARRAY[]::TEXT[]"
            try:
                sql = f"UPDATE students SET department = '{dept}', year = {year}, skills = {skills_arr}, interests = {interests_arr} WHERE id = {s_id} RETURNING id, name, department, year, skills, interests;"
                res = query_neon(sql)
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 3. POST /api/connections (Connect with another student)
        if path.endswith('/api/connections') or path.endswith('/connections'):
            a = body.get('a') or body.get('studentA')
            b = body.get('b') or body.get('studentB')
            if a and b:
                try:
                    s1 = min(int(a), int(b))
                    s2 = max(int(a), int(b))
                    sql = f"INSERT INTO connections (student_a, student_b) VALUES ({s1}, {s2}) ON CONFLICT DO NOTHING;"
                    query_neon(sql)
                    self.send_json({"studentA": s1, "studentB": s2}, 200)
                except Exception as e:
                    self.send_json({"error": str(e)}, 500)
            else:
                self.send_json({"error": "Missing student IDs"}, 400)
            return

        # 4. POST /api/posts (Add feed post)
        if path.endswith('/api/posts') or path.endswith('/posts'):
            author_id = body.get('authorId')
            content = body.get('content', '').replace("'", "''")
            try:
                s_res = query_neon(f"SELECT name FROM students WHERE id = {author_id};")
                author_name = s_res['rows'][0]['name'] if s_res.get('rows') else "Student"
                sql = f"INSERT INTO posts (author_id, author_name, content) VALUES ({author_id}, '{author_name}', '{content}') RETURNING id, author_id as \"authorId\", author_name as \"authorName\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql)
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 5. POST /api/project-posts (Add project collaboration post)
        if path.endswith('/api/project-posts') or path.endswith('/project-posts'):
            author_id = body.get('authorId')
            p_title = body.get('projectTitle', 'Project').replace("'", "''")
            skills = body.get('requiredSkills', [])
            skills_arr = "ARRAY[" + ",".join([f"'{s}'" for s in skills]) + "]" if skills else "ARRAY[]::TEXT[]"
            size = body.get('teamSizeNeeded', 3)
            content = body.get('content', '').replace("'", "''")
            try:
                s_res = query_neon(f"SELECT name FROM students WHERE id = {author_id};")
                author_name = s_res['rows'][0]['name'] if s_res.get('rows') else "Student"
                sql = f"INSERT INTO project_posts (author_id, author_name, project_title, required_skills, team_size_needed, content) VALUES ({author_id}, '{author_name}', '{p_title}', {skills_arr}, {size}, '{content}') RETURNING id, author_id as \"authorId\", author_name as \"authorName\", content, project_title as \"projectTitle\", required_skills as \"requiredSkills\", team_size_needed as \"teamSizeNeeded\", to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql)
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 6. POST /api/groups or /api/project-posts/:id/create-group (Create Team / Group)
        if 'create-group' in path or path.endswith('/groups'):
            leader_id = body.get('leaderId', 1)
            name = (body.get('groupName') or body.get('name', 'Team')).replace("'", "''")
            desc = body.get('description', 'Collaboration team').replace("'", "''")
            try:
                sql = f"INSERT INTO groups (name, description, leader_id) VALUES ('{name}', '{desc}', {leader_id}) RETURNING id, name, description, leader_id as \"leaderId\";"
                res = query_neon(sql)
                g_id = res["rows"][0]["id"]
                # Add leader as member
                try:
                    query_neon(f"INSERT INTO group_members (group_id, student_id) VALUES ({g_id}, {leader_id}) ON CONFLICT DO NOTHING;")
                except Exception:
                    pass
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 7. POST /api/messages (Send direct message)
        if path.endswith('/api/messages') or path.endswith('/messages'):
            s_id = body.get('senderId')
            r_id = body.get('receiverId')
            content = body.get('content', '').replace("'", "''")
            try:
                sql = f"INSERT INTO messages (sender_id, receiver_id, content) VALUES ({s_id}, {r_id}, '{content}') RETURNING id, sender_id as \"senderId\", receiver_id as \"receiverId\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql)
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        self.send_json({"error": "Endpoint not found"}, 404)
