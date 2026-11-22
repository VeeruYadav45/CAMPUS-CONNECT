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
CREATE TABLE IF NOT EXISTS group_posts (
    id SERIAL PRIMARY KEY,
    group_id INTEGER REFERENCES groups(id) ON DELETE CASCADE,
    student_id INTEGER REFERENCES students(id) ON DELETE CASCADE,
    content TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
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

        # DIAGNOSTIC: GET /api/diag — check tables & try inserts
        if path.endswith('/api/diag'):
            report = {}
            # Check which tables exist
            try:
                res = query_neon("SELECT table_name FROM information_schema.tables WHERE table_schema='public' ORDER BY table_name;")
                report["tables"] = [r["table_name"] for r in res.get("rows", [])]
            except Exception as e:
                report["tables_error"] = str(e)
            # Test project_posts insert
            try:
                query_neon("INSERT INTO project_posts (author_id, author_name, project_title, required_skills, team_size_needed, content) VALUES (12, 'Test', 'DiagProject', ARRAY['Python'], 2, 'diag') RETURNING id;")
                report["project_posts_insert"] = "OK"
                query_neon("DELETE FROM project_posts WHERE project_title='DiagProject';")
            except Exception as e:
                report["project_posts_insert_error"] = str(e)
            # Test groups insert
            try:
                res2 = query_neon("INSERT INTO groups (name, description, leader_id) VALUES ('DiagGroup', 'diag', 12) RETURNING id;")
                gid = res2["rows"][0]["id"]
                report["groups_insert"] = "OK"
                query_neon(f"DELETE FROM groups WHERE id={gid};")
            except Exception as e:
                report["groups_insert_error"] = str(e)
            # Test group_members
            try:
                cols = query_neon("SELECT column_name FROM information_schema.columns WHERE table_name='group_members' ORDER BY ordinal_position;")
                report["group_members_columns"] = [r["column_name"] for r in cols.get("rows", [])]
            except Exception as e:
                report["group_members_error"] = str(e)
            self.send_json(report)
            return

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
                res = query_neon("SELECT student_a as \"a\", student_b as \"b\" FROM connections;")
                self.send_json(res.get("rows", []))
            except Exception:
                self.send_json([])
            return

        # 3. GET /api/groups
        if path.endswith('/api/groups') or path.endswith('/groups'):
            try:
                res = query_neon("""
                    SELECT g.id, g.name, g.description, g.leader_id as "leaderId",
                           COALESCE(array_agg(gm.student_id) FILTER (WHERE gm.student_id IS NOT NULL), '{}') as "memberIds",
                           COUNT(gm.student_id) as "memberCount",
                           FALSE as "isProjectGroup"
                    FROM groups g
                    LEFT JOIN group_members gm ON gm.group_id = g.id
                    GROUP BY g.id ORDER BY g.id DESC;
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

        # 5. GET /api/posts (feed posts)
        if path.endswith('/api/posts') or path.endswith('/posts'):
            try:
                res = query_neon("SELECT id, author_id as \"authorId\", author_name as \"authorName\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp FROM posts ORDER BY id DESC;")
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

        # 8. GET /api/students/:id/friends  — direct connections of a student
        if '/students/' in path and path.endswith('/friends'):
            parts = path.split('/')
            s_id = parts[parts.index('students') + 1]
            try:
                sql = f"""
                    SELECT s.id, s.name, s.department, s.year, s.skills, s.interests
                    FROM students s
                    JOIN connections c ON (c.student_a = {s_id} AND c.student_b = s.id)
                                      OR (c.student_b = {s_id} AND c.student_a = s.id)
                    ORDER BY s.name;
                """
                res = query_neon(sql)
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 9. GET /api/students/:id/similar  — Jaccard similarity (Python-side)
        if '/students/' in path and path.endswith('/similar'):
            parts = path.split('/')
            s_id = int(parts[parts.index('students') + 1])
            try:
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
                self.send_json({"error": str(e)}, 500)
            return

        # 10. GET /api/connections/mutual?a=&b=  — mutual friends between two students
        if 'connections/mutual' in path:
            qs = parse_qs(parsed.query)
            a = qs.get('a', ['0'])[0]
            b = qs.get('b', ['0'])[0]
            try:
                sql = f"""
                    SELECT s.id, s.name, s.department FROM students s
                    WHERE s.id IN (
                        SELECT CASE WHEN student_a={a} THEN student_b ELSE student_a END
                        FROM connections WHERE student_a={a} OR student_b={a}
                    )
                    AND s.id IN (
                        SELECT CASE WHEN student_a={b} THEN student_b ELSE student_a END
                        FROM connections WHERE student_a={b} OR student_b={b}
                    );
                """
                res = query_neon(sql)
                self.send_json(res.get("rows", []))
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 11. GET /api/project-posts/:id/top-k  — Top-K teammate matching (Jaccard, Python-side)
        if '/project-posts/' in path and 'top-k' in path:
            parts = path.split('/')
            proj_id = parts[parts.index('project-posts') + 1]
            qs = parse_qs(parsed.query)
            k = int(qs.get('k', ['6'])[0])
            try:
                p_res = query_neon(f"SELECT required_skills FROM project_posts WHERE id = {proj_id};")
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
                self.send_json({"error": str(e)}, 500)
            return

        # 12. GET /api/groups/:id/posts  — group discussion posts
        if '/groups/' in path and path.endswith('/posts'):
            parts = path.split('/')
            g_id = parts[parts.index('groups') + 1]
            try:
                sql = f"""
                    SELECT id, student_id as "studentId", content,
                           to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp
                    FROM group_posts WHERE group_id = {g_id} ORDER BY id ASC;
                """
                res = query_neon(sql)
                self.send_json(res.get("rows", []))
            except Exception:
                self.send_json([])
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

        # 4. POST /api/project-posts (Add project collaboration post) — MUST come before /posts check
        if '/project-posts' in path and not any(x in path for x in ['top-k', 'create-group']):
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

        # 5. POST /api/posts (Add feed post)
        if path.endswith('/api/posts') or path.endswith('/posts'):
            author_id = body.get('authorId') or body.get('studentId')
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

        # 6. POST /api/groups or /api/project-posts/:id/create-group (Create Team / Group)
        if 'create-group' in path or path.endswith('/groups'):
            leader_id = body.get('leaderId') or body.get('adminId') or 1
            name = (body.get('groupName') or body.get('name', 'Team')).replace("'", "''")
            desc = body.get('description', 'Collaboration team').replace("'", "''")
            member_ids = body.get('memberIds', [])
            try:
                sql = f"INSERT INTO groups (name, description, leader_id) VALUES ('{name}', '{desc}', {leader_id}) RETURNING id, name, description, leader_id as \"leaderId\";"
                res = query_neon(sql)
                g_id = res["rows"][0]["id"]
                member_count = 1
                # Add leader as member
                try:
                    query_neon(f"INSERT INTO group_members (group_id, student_id) VALUES ({g_id}, {leader_id}) ON CONFLICT DO NOTHING;")
                except Exception:
                    pass
                # Add all selected members
                for m_id in member_ids:
                    if int(m_id) != int(leader_id):
                        try:
                            query_neon(f"INSERT INTO group_members (group_id, student_id) VALUES ({g_id}, {int(m_id)}) ON CONFLICT DO NOTHING;")
                            member_count += 1
                        except Exception:
                            pass
                result = res["rows"][0]
                result["memberCount"] = member_count
                result["memberIds"] = [leader_id] + [int(m) for m in member_ids if int(m) != int(leader_id)]
                self.send_json(result, 200)
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 7. POST /api/groups/:id/join
        if '/groups/' in path and path.endswith('/join'):
            parts = path.split('/')
            g_id = parts[parts.index('groups') + 1]
            student_id = body.get('studentId')
            try:
                query_neon(f"INSERT INTO group_members (group_id, student_id) VALUES ({g_id}, {student_id}) ON CONFLICT DO NOTHING;")
                self.send_json({"groupId": int(g_id), "studentId": student_id})
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 8. POST /api/groups/:id/leave
        if '/groups/' in path and path.endswith('/leave'):
            parts = path.split('/')
            g_id = parts[parts.index('groups') + 1]
            student_id = body.get('studentId')
            try:
                query_neon(f"DELETE FROM group_members WHERE group_id = {g_id} AND student_id = {student_id};")
                self.send_json({"groupId": int(g_id), "studentId": student_id})
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 9. POST /api/groups/:id/posts  — post to group discussion
        if '/groups/' in path and path.endswith('/posts'):
            parts = path.split('/')
            g_id = parts[parts.index('groups') + 1]
            student_id = body.get('studentId')
            content = body.get('content', '').replace("'", "''")
            try:
                sql = f"INSERT INTO group_posts (group_id, student_id, content) VALUES ({g_id}, {student_id}, '{content}') RETURNING id, student_id as \"studentId\", content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp;"
                res = query_neon(sql)
                self.send_json(res["rows"][0], 200)
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        # 10. POST /api/messages (Send direct message)
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
                    sk_arr = "ARRAY[" + ",".join([f"'{x}'" for x in sk]) + "]"
                    it_arr = "ARRAY[" + ",".join([f"'{x}'" for x in it]) + "]"
                    r = query_neon(f"INSERT INTO students (name,email,department,year,skills,interests) VALUES ('{nm}','{em}','{dp}',{yr},{sk_arr},{it_arr}) ON CONFLICT (email) DO UPDATE SET name=EXCLUDED.name RETURNING id;")
                    ids.append(r["rows"][0]["id"])
                # Connect them
                pairs = [(0,1),(0,2),(1,2),(2,3),(3,4),(0,4)]
                for (i,j) in pairs:
                    a, b = min(ids[i],ids[j]), max(ids[i],ids[j])
                    try: query_neon(f"INSERT INTO connections (student_a,student_b) VALUES ({a},{b}) ON CONFLICT DO NOTHING;")
                    except: pass
                # Demo feed post
                try: query_neon(f"INSERT INTO posts (author_id,author_name,content) VALUES ({ids[0]},'Alice Sharma','Welcome to Campus Connect! 🎓 Connect, collaborate, and build amazing projects together.') ON CONFLICT DO NOTHING;")
                except: pass
                # Demo project post
                try: query_neon(f"INSERT INTO project_posts (author_id,author_name,project_title,required_skills,team_size_needed,content) VALUES ({ids[0]},'Alice Sharma','AI Campus Assistant',ARRAY['Python','ML','React'],3,'Building an AI assistant for campus navigation and scheduling.') ON CONFLICT DO NOTHING;")
                except: pass
                self.send_json({"message": "Demo data loaded!", "studentCount": len(ids)})
            except Exception as e:
                self.send_json({"error": str(e)}, 500)
            return

        self.send_json({"error": "Endpoint not found"}, 404)
