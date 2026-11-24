"""Sign-in security test for api/index.py on a throwaway local PostgreSQL.

We play Microsoft: generate an RSA key, publish it as the JWKS, and sign ID tokens
with it. The API's own pure-Python RS256 verification is what gets tested.
"""
import sys, io, json, time, base64, os, importlib.util
import psycopg
from psycopg import RawCursor
from cryptography.hazmat.primitives.asymmetric import rsa, padding
from cryptography.hazmat.primitives import hashes

conn = psycopg.connect("host=127.0.0.1 port=55432 dbname=neondb user=postgres password=localtest",
                       autocommit=True, cursor_factory=RawCursor)
class UnknownStr(psycopg.adapt.Dumper):
    oid = 0
    def dump(self, obj): return obj.encode()
conn.adapters.register_dumper(str, UnknownStr)
for s in [
    "CREATE TABLE students (id SERIAL PRIMARY KEY, name VARCHAR(100) NOT NULL, email VARCHAR(150) UNIQUE NOT NULL, department VARCHAR(50) NOT NULL, year INT NOT NULL, skills TEXT[] DEFAULT '{}', interests TEXT[] DEFAULT '{}')",
    "CREATE TABLE connections (student_a INTEGER REFERENCES students(id) ON DELETE CASCADE, student_b INTEGER REFERENCES students(id) ON DELETE CASCADE, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (student_a, student_b), CHECK (student_a < student_b))",
    "CREATE TABLE groups (id SERIAL PRIMARY KEY, name VARCHAR(150) NOT NULL, description TEXT, admin_id INTEGER REFERENCES students(id) ON DELETE CASCADE, is_project_group BOOLEAN DEFAULT FALSE, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)",
    "CREATE TABLE group_members (group_id INTEGER REFERENCES groups(id) ON DELETE CASCADE, student_id INTEGER REFERENCES students(id) ON DELETE CASCADE, joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (group_id, student_id))",
    "CREATE TABLE posts (id SERIAL PRIMARY KEY, author_id INTEGER REFERENCES students(id), author_name VARCHAR(100), content TEXT NOT NULL, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)",
    "CREATE TABLE messages (id SERIAL PRIMARY KEY, sender_id INTEGER REFERENCES students(id), receiver_id INTEGER REFERENCES students(id), content TEXT NOT NULL, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)",
]:
    conn.execute(s)

spec = importlib.util.spec_from_file_location("api", sys.argv[1])
api = importlib.util.module_from_spec(spec); spec.loader.exec_module(api)

def real_query(sql, params=None):
    p = [None if v is None else str(v) for v in (params or [])]
    cur = conn.execute(sql, p)
    if cur.description is None: return {"rows": []}
    cols = [d.name for d in cur.description]
    return {"rows": json.loads(json.dumps([dict(zip(cols, r)) for r in cur.fetchall()], default=str))}
api.query_neon = real_query
for s in api.SCHEMA_STATEMENTS: real_query(s)

# ---- play Microsoft --------------------------------------------------------
KEY = rsa.generate_private_key(public_exponent=65537, key_size=2048)
OTHER_KEY = rsa.generate_private_key(public_exponent=65537, key_size=2048)
pub = KEY.public_key().public_numbers()
jwks_fetches = []
def fake_fetch_jwks():
    jwks_fetches.append(1)
    return {"kid-1": (pub.n, pub.e)}
api.fetch_jwks = fake_fetch_jwks          # the only network call we replace

b64 = lambda b: base64.urlsafe_b64encode(b).rstrip(b"=").decode()
def token(email, name="Test User", key=KEY, kid="kid-1", alg="RS256", **over):
    now = int(time.time())
    claims = {"iss": api.MS_ISSUER, "aud": api.MS_CLIENT_ID, "tid": api.MS_TENANT_ID,
              "iat": now, "nbf": now, "exp": now + 3600, "preferred_username": email, "name": name}
    claims.update(over)
    h = b64(json.dumps({"alg": alg, "typ": "JWT", "kid": kid}).encode())
    p = b64(json.dumps(claims).encode())
    sig = key.sign(f"{h}.{p}".encode(), padding.PKCS1v15(), hashes.SHA256())
    return f"{h}.{p}.{b64(sig)}"

class H(api.handler):
    def __init__(self, method, path, body=None, tok=None):
        self.path = path
        raw = json.dumps(body).encode() if body is not None else b""
        self.headers = {"Content-Length": str(len(raw))}
        if tok: self.headers["Authorization"] = "Bearer " + tok
        self.rfile = io.BytesIO(raw); self.wfile = io.BytesIO(); self.status = None
    def send_response(self, s): self.status = s
    def send_header(self, *a): pass
    def end_headers(self): pass
def call(m, p, b=None, tok=None):
    h = H(m, p, b, tok); getattr(h, "do_" + m)()
    return h.status, json.loads(h.wfile.getvalue())["data"]

results = []
def check(name, cond, detail=""):
    results.append(bool(cond))
    print(("PASS " if cond else "FAIL ") + name + ("" if cond else "  -> " + str(detail)[:200]))

ALICE, BOB, CAROL = token("alice@geu.ac.in", "Alice"), token("Bob@GEU.ac.in", "Bob"), token("carol@graphicera.ac.in", "Carol")

print("== token verification")
st, d = call("POST", "/api/students", {}); check("no token -> 401", st == 401, (st, d))
check("valid token accepted", call("POST", "/api/students", {}, ALICE)[0] == 200)
bad = ALICE[:-6] + ("AAAAAA" if not ALICE.endswith("AAAAAA") else "BBBBBB")
check("tampered signature -> 401", call("POST", "/api/students", {}, bad)[0] == 401)
h, p, s = ALICE.split(".")
forged_payload = b64(json.dumps({**json.loads(base64.urlsafe_b64decode(p + "==")), "preferred_username": "bob@geu.ac.in"}).encode())
check("payload changed (pretend to be Bob) -> 401", call("POST", "/api/students", {}, f"{h}.{forged_payload}.{s}")[0] == 401)
check("signed by another key -> 401", call("POST", "/api/students", {}, token("alice@geu.ac.in", key=OTHER_KEY))[0] == 401)
check("unknown key id -> 401", call("POST", "/api/students", {}, token("alice@geu.ac.in", kid="nope"))[0] == 401)
check("alg=none -> 401", call("POST", "/api/students", {}, f"{b64(json.dumps({'alg':'none','kid':'kid-1'}).encode())}.{p}.")[0] == 401)
check("wrong app (aud) -> 401", call("POST", "/api/students", {}, token("alice@geu.ac.in", aud="some-other-app"))[0] == 401)
check("wrong tenant -> 401", call("POST", "/api/students", {}, token("alice@geu.ac.in", tid="00000000-0000-0000-0000-000000000000"))[0] == 401)
check("wrong issuer -> 401", call("POST", "/api/students", {}, token("alice@geu.ac.in", iss="https://evil.example/v2.0"))[0] == 401)
check("expired -> 401", call("POST", "/api/students", {}, token("alice@geu.ac.in", exp=int(time.time()) - 3600))[0] == 401)
check("not valid yet -> 401", call("POST", "/api/students", {}, token("alice@geu.ac.in", nbf=int(time.time()) + 3600))[0] == 401)
check("garbage token -> 401", call("POST", "/api/students", {}, "abc.def")[0] == 401)
st, d = call("POST", "/api/students", {}, token("someone@gmail.com"))
check("non-GEU account -> 403", st == 403, (st, d))
check("JWKS fetched once and cached (+1 refetch allowed for unknown kid)", len(jwks_fetches) <= 2, len(jwks_fetches))

print("== sign in / profile")
st, a = call("POST", "/api/students", {"name": "Hacker", "email": "bob@geu.ac.in", "department": "ECE", "year": 3}, ALICE)
check("sign-in returns own profile; name/email from token, not body", st == 200 and a["name"] == "Alice" and a["email"] == "alice@geu.ac.in", a)
st, a2 = call("POST", "/api/students", {"department": "XYZ"}, ALICE)
check("second sign-in returns same profile (no duplicate)", st == 200 and a2["id"] == a["id"] and a2["department"] == a["department"], a2)
_, b = call("POST", "/api/students", {}, BOB)
check("email case-insensitive (Bob@GEU -> bob@geu)", b["email"] == "bob@geu.ac.in", b)
_, c = call("POST", "/api/students", {}, CAROL)
st, lst = call("GET", "/api/students")
check("public student list has no emails", st == 200 and len(lst) == 3 and all("email" not in s for s in lst), lst)
check("update own profile", call("POST", f"/api/students/{a['id']}/update", {"skills": ["C++"], "interests": [], "year": 2}, ALICE)[0] == 200)
check("update someone else's profile -> 403", call("POST", f"/api/students/{b['id']}/update", {"skills": ["x"]}, ALICE)[0] == 403)
check("update without sign-in -> 401", call("POST", f"/api/students/{a['id']}/update", {"skills": ["x"]})[0] == 401)
st, d = call("POST", "/api/posts", {"content": "hi"}, token("dave@geu.ac.in"))
check("signed in but no profile yet -> 403", st == 403, (st, d))

print("== connections")
st, d = call("POST", "/api/connections", {"a": a["id"], "b": b["id"]}, ALICE)
check("old connect route now only SENDS a request", st == 200 and d["status"] == "pending", d)
check("no connection yet (needs consent)", conn.execute("select count(*) from connections").fetchone()[0] == 0)
check("connect two OTHER students -> 403", call("POST", "/api/connections", {"a": b["id"], "b": c["id"]}, ALICE)[0] == 403)
check("connect with self -> 400", call("POST", "/api/connections", {"a": a["id"], "b": a["id"]}, ALICE)[0] == 400)
check("connect with unknown student -> 404", call("POST", "/api/connections", {"a": a["id"], "b": 9999}, ALICE)[0] == 404)
check("connect anonymously -> 401", call("POST", "/api/connections", {"a": a["id"], "b": c["id"]})[0] == 401)

print("== connection requests")
st, d = call("POST", "/api/connection-requests", {"toId": b["id"]}, ALICE)
check("re-sending a pending request is idempotent", st == 200 and d["status"] == "pending", d)
st, reqs_b = call("GET", "/api/connection-requests", tok=BOB)
check("receiver sees incoming request", st == 200 and [r["student"]["id"] for r in reqs_b["incoming"]] == [a["id"]], reqs_b)
_, reqs_a = call("GET", "/api/connection-requests", tok=ALICE)
check("sender sees outgoing request", [r["student"]["id"] for r in reqs_a["outgoing"]] == [b["id"]], reqs_a)
check("anonymous cannot list requests -> 401", call("GET", "/api/connection-requests")[0] == 401)
rid = reqs_b["incoming"][0]["id"]
check("sender cannot accept own request -> 403", call("POST", f"/api/connection-requests/{rid}/accept", {}, ALICE)[0] == 403)
check("outsider cannot accept -> 403", call("POST", f"/api/connection-requests/{rid}/accept", {}, CAROL)[0] == 403)
st, d = call("POST", f"/api/connection-requests/{rid}/accept", {}, BOB)
check("receiver accepts -> connected", st == 200 and d["status"] == "connected" and conn.execute("select count(*) from connections").fetchone()[0] == 1, d)
check("answered request is gone -> 404", call("POST", f"/api/connection-requests/{rid}/accept", {}, BOB)[0] == 404)
_, d = call("POST", "/api/connection-requests", {"toId": b["id"]}, ALICE)
check("request to existing connection -> already connected", d["status"] == "connected", d)
# Carol -> Alice, Alice declines; Carol can't retry for 7 days
_, d = call("POST", "/api/connection-requests", {"toId": a["id"]}, CAROL)
check("decline", call("POST", f"/api/connection-requests/{d['requestId']}/decline", {}, ALICE)[0] == 200)
check("re-request right after decline -> 429", call("POST", "/api/connection-requests", {"toId": a["id"]}, CAROL)[0] == 429)
conn.execute("update connection_requests set responded_at = now() - interval '8 days' where status='declined'")
st, d = call("POST", "/api/connection-requests", {"toId": a["id"]}, CAROL)
check("allowed again after 7 days", st == 200 and d["status"] == "pending", d)
check("sender cancels", call("POST", f"/api/connection-requests/{d['requestId']}/cancel", {}, CAROL)[0] == 200)
# mutual: Carol asks Alice, then Alice 'requests' Carol -> auto-accept
call("POST", "/api/connection-requests", {"toId": a["id"]}, CAROL)
_, d = call("POST", "/api/connection-requests", {"toId": c["id"]}, ALICE)
check("asking someone who already asked you connects you", d["status"] == "connected", d)
check("request to self -> 400", call("POST", "/api/connection-requests", {"toId": a["id"]}, ALICE)[0] == 400)
check("request to unknown student -> 404", call("POST", "/api/connection-requests", {"toId": 9999}, ALICE)[0] == 404)

print("== posts / projects / groups")
st, p1 = call("POST", "/api/posts", {"authorId": b["id"], "content": "posted as?"}, ALICE)
check("feed post always authored by signer (authorId ignored)", st == 200 and p1["authorId"] == a["id"] and p1["authorName"] == "Alice", p1)
check("feed post anonymously -> 401", call("POST", "/api/posts", {"content": "x"})[0] == 401)
st, pp = call("POST", "/api/project-posts", {"authorId": b["id"], "projectTitle": "AI app", "requiredSkills": ["C++"]}, ALICE)
check("project post authored by signer", st == 200 and pp["authorId"] == a["id"], pp)
check("someone else turns my project into a team -> 403", call("POST", f"/api/project-posts/{pp['id']}/create-group", {"memberIds": [c["id"]]}, BOB)[0] == 403)
st, g = call("POST", f"/api/project-posts/{pp['id']}/create-group", {"leaderId": b["id"], "memberIds": [b["id"]]}, ALICE)
check("author creates team; admin = signer; picked student INVITED not added", st == 200 and g["created"] and g["leaderId"] == a["id"]
      and g["memberIds"] == [a["id"]] and g["invited"] == [b["id"]], g)
st, g_again = call("POST", f"/api/project-posts/{pp['id']}/create-group", {"memberIds": [b["id"], c["id"]]}, ALICE)
check("second Assemble reuses the SAME team (no duplicate)", st == 200 and g_again["id"] == g["id"] and not g_again["created"], g_again)
check("re-invite skipped; only new student invited", g_again["invited"] == [c["id"]], g_again)
check("still exactly one project team group", conn.execute("select count(*) from groups where is_project_group").fetchone()[0] == 1)
st, t = call("GET", f"/api/project-posts/{pp['id']}/team", tok=ALICE)
check("author sees team + pending invites", st == 200 and t["groupId"] == g["id"] and sorted(t["invitedIds"]) == sorted([b["id"], c["id"]]), t)
st, t = call("GET", f"/api/project-posts/{pp['id']}/team")
check("others see members but not who was invited", st == 200 and t["memberIds"] == [a["id"]] and t["invitedIds"] == [], t)
st, inv_b = call("GET", "/api/team-invites", tok=BOB)
check("invited student sees the invitation", st == 200 and len(inv_b) == 1 and inv_b[0]["group"]["id"] == g["id"] and inv_b[0]["invitedBy"]["name"] == "Alice", inv_b)
check("someone else can't accept it -> 403", call("POST", f"/api/team-invites/{inv_b[0]['id']}/accept", {}, CAROL)[0] == 403)
st, d = call("POST", f"/api/team-invites/{inv_b[0]['id']}/accept", {}, BOB)
check("accepting joins the team", st == 200 and b["id"] in call("GET", f"/api/project-posts/{pp['id']}/team")[1]["memberIds"], d)
check("answered invitation is gone -> 404", call("POST", f"/api/team-invites/{inv_b[0]['id']}/accept", {}, BOB)[0] == 404)
_, inv_c = call("GET", "/api/team-invites", tok=CAROL)
check("decline: not added to team", call("POST", f"/api/team-invites/{inv_c[0]['id']}/decline", {}, CAROL)[0] == 200
      and c["id"] not in call("GET", f"/api/project-posts/{pp['id']}/team")[1]["memberIds"])
_, g3 = call("POST", f"/api/project-posts/{pp['id']}/create-group", {"memberIds": [c["id"]]}, ALICE)
_, inv_c = call("GET", "/api/team-invites", tok=CAROL)
check("admin cancels a pending invitation", call("POST", f"/api/team-invites/{inv_c[0]['id']}/cancel", {}, ALICE)[0] == 200
      and call("GET", "/api/team-invites", tok=CAROL)[1] == [])
check("create-group for missing project -> 404", call("POST", "/api/project-posts/9999/create-group", {}, ALICE)[0] == 404)
st, g2 = call("POST", "/api/groups", {"adminId": c["id"], "name": "Study"}, BOB)
check("normal group admin = signer", st == 200 and g2["leaderId"] == b["id"], g2)
st, g4 = call("POST", "/api/groups", {"name": "Club", "memberIds": [a["id"]]}, CAROL)
check("club creation with memberIds invites instead of adding", st == 200 and g4["memberIds"] == [c["id"]] and g4["invited"] == [a["id"]], g4)
check("non-admin can't delete a group -> 403", call("POST", f"/api/groups/{g4['id']}/delete", {}, ALICE)[0] == 403)
check("admin deletes own group (invites go too)", call("POST", f"/api/groups/{g4['id']}/delete", {}, CAROL)[0] == 200
      and conn.execute("select count(*) from team_invites where group_id=%d" % g4["id"]).fetchone()[0] == 0)
check("delete unknown group -> 404", call("POST", "/api/groups/9999/delete", {}, CAROL)[0] == 404)
st, d = call("POST", f"/api/groups/{g2['id']}/join", {"studentId": a["id"]}, CAROL)
check("join adds the signer, not body studentId", st == 200 and d["studentId"] == c["id"], d)
st, d = call("POST", f"/api/groups/{g2['id']}/leave", {"studentId": b["id"]}, CAROL)
check("leave removes the signer only", st == 200 and d["studentId"] == c["id"], d)
check("non-member posts in group -> 403", call("POST", f"/api/groups/{g2['id']}/posts", {"content": "x"}, CAROL)[0] == 403)
st, d = call("POST", f"/api/groups/{g2['id']}/posts", {"studentId": c["id"], "content": "hello team"}, BOB)
check("member posts in group as self", st == 200 and d["studentId"] == b["id"], d)
check("nobody can self-join a project team -> 403", call("POST", f"/api/groups/{g['id']}/join", {}, CAROL)[0] == 403)
check("join unknown group -> 404", call("POST", "/api/groups/9999/join", {}, CAROL)[0] == 404)

print("== private messages")
check("message a non-connection -> 403", call("POST", "/api/messages", {"receiverId": b["id"], "content": "hi"}, CAROL)[0] == 403)
st, m = call("POST", "/api/messages", {"senderId": b["id"], "receiverId": c["id"], "content": "secret"}, ALICE)
check("sender is always the signer (senderId ignored)", st == 200 and m["senderId"] == a["id"] and m["receiverId"] == c["id"], m)
check("message yourself -> 400", call("POST", "/api/messages", {"receiverId": a["id"], "content": "x"}, ALICE)[0] == 400)
check("message anonymously -> 401", call("POST", "/api/messages", {"receiverId": c["id"], "content": "x"})[0] == 401)
conv = f"/api/messages/conversation?user1={a['id']}&user2={c['id']}"
st, d = call("GET", conv, tok=ALICE); check("participant A reads conversation", st == 200 and len(d) == 1, d)
st, d = call("GET", conv, tok=CAROL); check("participant B reads conversation", st == 200 and len(d) == 1, d)
check("outsider reads conversation -> 403", call("GET", conv, tok=BOB)[0] == 403)
check("anonymous reads conversation -> 401", call("GET", conv)[0] == 401)
check("own chat list", call("GET", f"/api/messages/partners/{a['id']}", tok=ALICE)[0] == 200)
check("someone else's chat list -> 403", call("GET", f"/api/messages/partners/{a['id']}", tok=BOB)[0] == 403)

st, d = call("POST", "/api/connections/remove", {"studentId": b["id"]}, ALICE)
check("remove connection", st == 200 and conn.execute("select count(*) from connections where student_a=%s and student_b=%s" % (min(a['id'],b['id']), max(a['id'],b['id']))).fetchone()[0] == 0, d)
check("can't message after removal -> 403", call("POST", "/api/messages", {"receiverId": b["id"], "content": "x"}, ALICE)[0] == 403)

print("== public reads still work without sign-in")
for pth in ["/api/students", "/api/connections", "/api/groups", "/api/posts", "/api/project-posts",
            f"/api/students/{a['id']}/friends", f"/api/students/{a['id']}/similar", f"/api/students/{a['id']}/suggestions",
            f"/api/connections/mutual?a={a['id']}&b={b['id']}", f"/api/project-posts/{pp['id']}/top-k?k=3", f"/api/groups/{g2['id']}/posts"]:
    check(f"GET {pth}", call("GET", pth)[0] == 200)

print("== demo data")
check("demo/load disabled by default -> 403", call("POST", "/api/demo/load", {})[0] == 403)
os.environ["ALLOW_DEMO_LOAD"] = "1"
check("demo/load works only with ALLOW_DEMO_LOAD=1", call("POST", "/api/demo/load", {})[0] == 200)
del os.environ["ALLOW_DEMO_LOAD"]


print("== limits / case-insensitive matching")
check("empty post -> 400", call("POST", "/api/posts", {"content": "   "}, ALICE)[0] == 400)
check("5000-char post -> 400", call("POST", "/api/posts", {"content": "x"*5000}, ALICE)[0] == 400)
check("60-char department -> 400 (not DB error)", call("POST", f"/api/students/{a['id']}/update", {"department": "d"*60, "skills": []}, ALICE)[0] == 400)
check("too many skills -> 400", call("POST", f"/api/students/{a['id']}/update", {"department": "CSE", "skills": [str(i) for i in range(30)]}, ALICE)[0] == 400)
check("bad k -> 400", call("GET", f"/api/project-posts/{pp['id']}/top-k?k=-1")[0] == 400)
call("POST", f"/api/students/{b['id']}/update", {"department": "CSE", "skills": ["c++", "Python"], "interests": []}, BOB)
st, d = call("GET", f"/api/project-posts/{pp['id']}/top-k?k=5")
bob_row = [x for x in d["results"] if x["studentId"] == b["id"]][0]
check("top-k matches 'c++' with required 'C++' (case-insensitive), shows student's spelling", bob_row["score"] > 0 and bob_row["matchedSkills"] == ["c++"], bob_row)

print("== edit / delete posts")
_, fp = call("POST", "/api/posts", {"content": "alice original"}, ALICE)
st, d = call("POST", f"/api/posts/{fp['id']}/edit", {"content": "alice edited"}, ALICE)
check("author edits own feed post", st == 200 and d["content"] == "alice edited", (st, d))
check("someone else edits it -> 403", call("POST", f"/api/posts/{fp['id']}/edit", {"content": "hacked"}, BOB)[0] == 403)
check("someone else deletes it -> 403", call("POST", f"/api/posts/{fp['id']}/delete", {}, BOB)[0] == 403)
check("anonymous delete -> 401", call("POST", f"/api/posts/{fp['id']}/delete", {})[0] == 401)
check("edit to empty -> 400", call("POST", f"/api/posts/{fp['id']}/edit", {"content": " "}, ALICE)[0] == 400)
check("author deletes own feed post", call("POST", f"/api/posts/{fp['id']}/delete", {}, ALICE)[0] == 200)
check("deleted post is gone -> 404", call("POST", f"/api/posts/{fp['id']}/delete", {}, ALICE)[0] == 404)
# group g2: admin Bob; Carol joins and posts
call("POST", f"/api/groups/{g2['id']}/join", {}, CAROL)
_, gp = call("POST", f"/api/groups/{g2['id']}/posts", {"content": "carol in group"}, CAROL)
check("group post author edits", call("POST", f"/api/groups/{g2['id']}/posts/{gp['id']}/edit", {"content": "carol edited"}, CAROL)[0] == 200)
check("group admin cannot EDIT others' post -> 403", call("POST", f"/api/groups/{g2['id']}/posts/{gp['id']}/edit", {"content": "x"}, BOB)[0] == 403)
check("non-admin non-author delete -> 403", call("POST", f"/api/groups/{g2['id']}/posts/{gp['id']}/delete", {}, ALICE)[0] == 403)
check("wrong group id in URL -> 404", call("POST", f"/api/groups/{g['id']}/posts/{gp['id']}/delete", {}, CAROL)[0] == 404)
check("group admin deletes a member's post", call("POST", f"/api/groups/{g2['id']}/posts/{gp['id']}/delete", {}, BOB)[0] == 200)
st, d = call("POST", f"/api/project-posts/{pp['id']}/edit", {"projectTitle": "AI app v2", "requiredSkills": ["C++", "ML"], "teamSizeNeeded": 4}, ALICE)
check("author edits own project", st == 200 and d["projectTitle"] == "AI app v2" and d["requiredSkills"] == ["C++", "ML"] and d["teamSizeNeeded"] == 4, (st, d))
check("someone else edits project -> 403", call("POST", f"/api/project-posts/{pp['id']}/edit", {"projectTitle": "x"}, BOB)[0] == 403)
check("someone else deletes project -> 403", call("POST", f"/api/project-posts/{pp['id']}/delete", {}, BOB)[0] == 403)
check("edit project with empty title -> 400", call("POST", f"/api/project-posts/{pp['id']}/edit", {"projectTitle": ""}, ALICE)[0] == 400)
check("anonymous project delete -> 401", call("POST", f"/api/project-posts/{pp['id']}/delete", {})[0] == 401)
check("author deletes own project", call("POST", f"/api/project-posts/{pp['id']}/delete", {}, ALICE)[0] == 200)
check("deleting the project keeps its team", conn.execute("select count(*) from groups where id=%d" % g["id"]).fetchone()[0] == 1)
check("deleted project -> 404", call("POST", f"/api/project-posts/{pp['id']}/edit", {"projectTitle": "x"}, ALICE)[0] == 404)

print("== group posts on the live table layout / admin leave")
st, d = call("GET", f"/api/groups/{g2['id']}/posts")
check("group posts list returns authorName", st == 200 and all("authorName" in x and "studentId" in x for x in d), d)
check("admin cannot leave own group -> 400", call("POST", f"/api/groups/{g2['id']}/leave", {}, BOB)[0] == 400)

print("== founders")
conn.execute("INSERT INTO site_roles (student_id, role) VALUES (%d, 'founder')" % c["id"])
st, lst = call("GET", "/api/students")
check("directory shows founder role", [s["role"] for s in lst if s["id"] == c["id"]] == ["founder"] and all(s["role"] is None for s in lst if s["id"] != c["id"]), lst)
_, fp2 = call("POST", "/api/posts", {"content": "unfit post"}, BOB)
check("non-founder can't remove others' post -> 403", call("POST", f"/api/posts/{fp2['id']}/delete", {}, ALICE)[0] == 403)
check("founder can NOT edit others' post -> 403", call("POST", f"/api/posts/{fp2['id']}/edit", {"content": "x"}, CAROL)[0] == 403)
check("founder removes any feed post", call("POST", f"/api/posts/{fp2['id']}/delete", {}, CAROL)[0] == 200)
_, pp2 = call("POST", "/api/project-posts", {"projectTitle": "spam"}, BOB)
check("founder removes any project post", call("POST", f"/api/project-posts/{pp2['id']}/delete", {}, CAROL)[0] == 200)
_, g5 = call("POST", "/api/groups", {"name": "spam club"}, BOB)
_, gp5 = call("POST", f"/api/groups/{g5['id']}/posts", {"content": "spam"}, BOB)
check("founder removes any group post", call("POST", f"/api/groups/{g5['id']}/posts/{gp5['id']}/delete", {}, CAROL)[0] == 200)
check("founder deletes any group", call("POST", f"/api/groups/{g5['id']}/delete", {}, CAROL)[0] == 200)
check("founder still can't read others' DMs -> 403", call("GET", f"/api/messages/conversation?user1={a['id']}&user2={b['id']}", tok=CAROL)[0] == 403)

print("== keyword search (Trie)")
call("POST", "/api/posts", {"content": "Looking for a PyTorch and Python mentor"}, ALICE)
call("POST", "/api/project-posts", {"projectTitle": "Robot Arm", "requiredSkills": ["C++", "ROS"]}, BOB)
st, d = call("GET", "/api/search?q=py")
check("prefix finds post; suggestions alphabetical", st == 200 and len(d["posts"]) == 1 and d["suggestions"][:2] == ["python", "pytorch"], d)
st, d = call("GET", "/api/search?q=robot%20c%2B%2B")
check("AND across words, project via title + skill (c++)", st == 200 and [p["projectTitle"] for p in d["projects"]] == ["Robot Arm"] and d["posts"] == [], d)
check("no match -> empty", call("GET", "/api/search?q=zzzz")[1] == {"suggestions": [], "posts": [], "projects": []})
check("empty query -> empty", call("GET", "/api/search?q=")[1]["posts"] == [])
check("too-long query -> 400", call("GET", "/api/search?q=" + "a"*300)[0] == 400)

print(f"\n{sum(results)}/{len(results)} passed")
sys.exit(0 if all(results) else 1)
