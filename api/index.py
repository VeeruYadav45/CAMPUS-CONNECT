from http.server import BaseHTTPRequestHandler
import json
import urllib.request
import ssl
import os
from urllib.parse import urlparse, parse_qs

# Neon PostgreSQL Connection URL (loaded securely from Vercel Environment Variables)
DB_URL = os.environ.get("DATABASE_URL")
if not DB_URL and os.path.exists(".env"):
    with open(".env") as f:
        for line in f:
            if line.startswith("DATABASE_URL="):
                DB_URL = line.strip().split("=", 1)[1].strip('"\'')
                break

def query_neon(sql):
    host = DB_URL.split("@")[1].split("/")[0].split("?")[0]
    req = urllib.request.Request(
        f"https://{host}/sql",
        data=json.dumps({"query": sql}).encode("utf-8"),
        headers={
            "Neon-Connection-String": DB_URL,
            "Content-Type": "application/json"
        }
    )
    ctx = ssl.create_default_context()
    with urllib.request.urlopen(req, context=ctx, timeout=10) as resp:
        return json.loads(resp.read().decode("utf-8"))

class handler(BaseHTTPRequestHandler):
    def send_json(self, data, status=200):
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()
        self.wfile.write(json.dumps({"ok": status == 200, "data": data}).encode('utf-8'))

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
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

        # 2. GET /api/messages/conversation
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

        # 3. GET /api/messages/partners/:id
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
            name = body.get('name', 'Student')
            dept = body.get('department', 'CSE')
            year = body.get('year', 2)
            email = body.get('email', f"{name.lower().replace(' ', '.')}@geu.ac.in")
            try:
                sql = f"INSERT INTO students (name, email, department, year, skills, interests) VALUES ('{name}', '{email}', '{dept}', {year}, ARRAY['C++','Python','DSA'], ARRAY['Web Dev','AI']) RETURNING id, name, department, year;"
                res = query_neon(sql)
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 2. POST /api/messages (Send direct message)
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