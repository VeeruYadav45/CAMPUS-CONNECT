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
        student_id INTEGER REFERENCES students(id) ON DELETE CASCADE,
        content TEXT NOT NULL,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
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

        # 6. GET /api/messages/conversation?user1=&user2=
        if 'messages/conversation' in path:
            qs = parse_qs(parsed.query)
            try:
                u1 = to_int(qs.get('user1', ['0'])[0], "user1")
                u2 = to_int(qs.get('user2', ['0'])[0], "user2")
                sql = "SELECT id, sender_id as \"senderId\", receiver_id as \"receiverId\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp FROM messages WHERE (sender_id = $1 AND receiver_id = $2) OR (sender_id = $2 AND receiver_id = $1) ORDER BY id;"
                res = query_neon(sql, [u1, u2])
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_error_json(e)
            return

        # 7. GET /api/messages/partners/:id
        if 'messages/partners' in path:
            try:
                s_id = to_int(path.split('/')[-1], "student id")
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
                me_set = set(me.get("skills") or []) | set(me.get("interests") or [])
                results = []
                for s in students:
                    if s["id"] == s_id:
                        continue
                    other_set = set(s.get("skills") or []) | set(s.get("interests") or [])
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
                p_res = query_neon("SELECT required_skills FROM project_posts WHERE id = $1;", [proj_id])
                if not p_res.get("rows"):
                    self.send_json({"results": []})
                    return
                req_skills = set(p_res["rows"][0].get("required_skills") or [])
                s_res = query_neon("SELECT id, name, department, year, skills, interests FROM students ORDER BY id;")
                students = s_res.get("rows", [])
                candidates = []
                for s in students:
                    s_skills = set(s.get("skills") or [])
                    union = req_skills | s_skills
                    inter = req_skills & s_skills
                    score = len(inter) / len(union) if union else 0.0
                    candidates.append({
                        "studentId": s["id"], "name": s["name"],
                        "department": s["department"], "year": s["year"],
                        "score": round(score, 4), "student": s,
                        "matchedSkills": list(inter)
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
                    SELECT id, student_id as "studentId", content,
                           to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp
                    FROM group_posts WHERE group_id = $1 ORDER BY id ASC;
                """
                res = query_neon(sql, [g_id])
                self.send_json(res.get("rows", []))
            except BadRequest as e:
                self.send_error_json(e)
            except Exception:
                self.send_json([])
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

        # 1. POST /api/students (Register/Login via Microsoft)
        if path.endswith('/api/students') or path.endswith('/students'):
            try:
                name = str(body.get('name', 'Student'))
                dept = str(body.get('department', 'CSE'))
                year = to_int(body.get('year', 2), "year")
                email = str(body.get('email', f"{name.lower().replace(' ', '.')}@geu.ac.in"))
                skills = pg_text_array(body.get('skills', ['C++', 'Python', 'DSA']), "skills")
                interests = pg_text_array(body.get('interests', ['Web Dev', 'AI']), "interests")
                sql = "INSERT INTO students (name, email, department, year, skills, interests) VALUES ($1, $2, $3, $4, $5::text[], $6::text[]) RETURNING id, name, department, year, skills, interests;"
                res = query_neon(sql, [name, email, dept, year, skills, interests])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 2. POST /api/students/:id/update (Update Skills & Interests)
        if '/api/students/' in path and 'update' in path:
            try:
                s_id = path_id(path, 'students')
                dept = str(body.get('department', 'CSE'))
                year = to_int(body.get('year', 2), "year")
                skills = pg_text_array(body.get('skills', []), "skills")
                interests = pg_text_array(body.get('interests', []), "interests")
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
                    a = to_int(a, "a")
                    b = to_int(b, "b")
                    s1, s2 = min(a, b), max(a, b)
                    query_neon("INSERT INTO connections (student_a, student_b) VALUES ($1, $2) ON CONFLICT DO NOTHING;", [s1, s2])
                    self.send_json({"studentA": s1, "studentB": s2}, 200)
                except Exception as e:
                    self.send_error_json(e)
            else:
                self.send_json({"error": "Missing student IDs"}, 400)
            return

        # 4. POST /api/project-posts (Add project collaboration post) — MUST come before /posts check
        if '/project-posts' in path and not any(x in path for x in ['top-k', 'create-group']):
            author_id = body.get('authorId') or body.get('studentId') or 12
            try:
                author_id = int(author_id)
            except Exception:
                author_id = 12
            try:
                p_title = str(body.get('projectTitle', 'Project'))
                skills = pg_text_array(body.get('requiredSkills', []), "requiredSkills")
                size = to_int(body.get('teamSizeNeeded', 3), "teamSizeNeeded")
                content = str(body.get('content') or p_title or 'Project collaboration')
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
            author_id = body.get('authorId') or body.get('studentId') or 12
            try:
                author_id = int(author_id)
            except Exception:
                author_id = 12
            try:
                content = str(body.get('content', ''))
                s_res = query_neon("SELECT name FROM students WHERE id = $1;", [author_id])
                author_name = s_res['rows'][0]['name'] if s_res.get('rows') else "Student"
                sql = "INSERT INTO posts (author_id, author_name, content) VALUES ($1, $2, $3) RETURNING id, author_id as \"authorId\", author_name as \"authorName\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql, [author_id, author_name, content])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 6. POST /api/groups or /api/project-posts/:id/create-group (Create Team / Group)
        if 'create-group' in path or path.endswith('/groups'):
            raw_admin = body.get('leaderId') or body.get('adminId') or 12
            try:
                admin_id = int(raw_admin)
            except Exception:
                admin_id = 12
            try:
                name = str(body.get('groupName') or body.get('name', 'Team'))
                desc = str(body.get('description', 'Collaboration team'))
                raw_members = body.get('memberIds', [])
                if not isinstance(raw_members, list):
                    raise BadRequest("'memberIds' must be a list")
                member_ids = []
                for m in raw_members:
                    m_id = to_int(m, "memberIds")
                    if m_id != admin_id and m_id not in member_ids:
                        member_ids.append(m_id)
                is_project = 'create-group' in path
                sql = "INSERT INTO groups (name, description, admin_id, is_project_group) VALUES ($1, $2, $3, $4::boolean) RETURNING id, name, description, admin_id as \"leaderId\", is_project_group as \"isProjectGroup\";"
                res = query_neon(sql, [name, desc, admin_id, "true" if is_project else "false"])
                g_id = res["rows"][0]["id"]
                member_count = 1
                # Add admin as member
                try:
                    query_neon("INSERT INTO group_members (group_id, student_id) VALUES ($1, $2) ON CONFLICT DO NOTHING;", [g_id, admin_id])
                except Exception:
                    pass
                # Add all selected members
                for m_id in member_ids:
                    try:
                        query_neon("INSERT INTO group_members (group_id, student_id) VALUES ($1, $2) ON CONFLICT DO NOTHING;", [g_id, m_id])
                        member_count += 1
                    except Exception:
                        pass
                result = res["rows"][0]
                result["memberCount"] = member_count
                result["memberIds"] = [admin_id] + member_ids
                self.send_json(result, 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 7. POST /api/groups/:id/join
        if '/groups/' in path and path.endswith('/join'):
            try:
                g_id = path_id(path, 'groups')
                student_id = to_int(body.get('studentId'), "studentId")
                query_neon("INSERT INTO group_members (group_id, student_id) VALUES ($1, $2) ON CONFLICT DO NOTHING;", [g_id, student_id])
                self.send_json({"groupId": g_id, "studentId": student_id})
            except Exception as e:
                self.send_error_json(e)
            return

        # 8. POST /api/groups/:id/leave
        if '/groups/' in path and path.endswith('/leave'):
            try:
                g_id = path_id(path, 'groups')
                student_id = to_int(body.get('studentId'), "studentId")
                query_neon("DELETE FROM group_members WHERE group_id = $1 AND student_id = $2;", [g_id, student_id])
                self.send_json({"groupId": g_id, "studentId": student_id})
            except Exception as e:
                self.send_error_json(e)
            return

        # 9. POST /api/groups/:id/posts  — post to group discussion
        if '/groups/' in path and path.endswith('/posts'):
            try:
                g_id = path_id(path, 'groups')
                student_id = to_int(body.get('studentId'), "studentId")
                content = str(body.get('content', ''))
                sql = "INSERT INTO group_posts (group_id, student_id, content) VALUES ($1, $2, $3) RETURNING id, student_id as \"studentId\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql, [g_id, student_id, content])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 10. POST /api/messages (Send direct message)
        if path.endswith('/api/messages') or path.endswith('/messages'):
            try:
                s_id = to_int(body.get('senderId'), "senderId")
                r_id = to_int(body.get('receiverId'), "receiverId")
                content = str(body.get('content', ''))
                sql = "INSERT INTO messages (sender_id, receiver_id, content) VALUES ($1, $2, $3) RETURNING id, sender_id as \"senderId\", receiver_id as \"receiverId\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql, [s_id, r_id, content])
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_error_json(e)
            return

        # 11. POST /api/demo/load  — seed demo data
        if 'demo/load' in path:
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
