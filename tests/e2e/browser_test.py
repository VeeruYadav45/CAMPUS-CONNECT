"""Browser test of the real frontend + the Vercel Python API + local PostgreSQL.

Sign-in is simulated: the dev server accepts "test-<email>" tokens instead of
Microsoft ones (the real verification is covered by auth_e2e.py).
"""
import sys, os, json, time, threading, importlib.util, mimetypes
from http.server import ThreadingHTTPServer
import psycopg
from psycopg import RawCursor
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait

REPO, OUT = sys.argv[1], sys.argv[2]
FRONTEND = os.path.join(REPO, "frontend")
conn = psycopg.connect("host=127.0.0.1 port=55432 dbname=neondb user=postgres password=localtest",
                       autocommit=True, cursor_factory=RawCursor)
class UnknownStr(psycopg.adapt.Dumper):
    oid = 0
    def dump(self, obj): return obj.encode()
conn.adapters.register_dumper(str, UnknownStr)
lock = threading.Lock()
for s in [
    "CREATE TABLE students (id SERIAL PRIMARY KEY, name VARCHAR(100) NOT NULL, email VARCHAR(150) UNIQUE NOT NULL, department VARCHAR(50) NOT NULL, year INT NOT NULL, skills TEXT[] DEFAULT '{}', interests TEXT[] DEFAULT '{}')",
    "CREATE TABLE connections (student_a INTEGER REFERENCES students(id) ON DELETE CASCADE, student_b INTEGER REFERENCES students(id) ON DELETE CASCADE, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (student_a, student_b), CHECK (student_a < student_b))",
    "CREATE TABLE groups (id SERIAL PRIMARY KEY, name VARCHAR(150) NOT NULL, description TEXT, admin_id INTEGER REFERENCES students(id) ON DELETE CASCADE, is_project_group BOOLEAN DEFAULT FALSE, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)",
    "CREATE TABLE group_members (group_id INTEGER REFERENCES groups(id) ON DELETE CASCADE, student_id INTEGER REFERENCES students(id) ON DELETE CASCADE, joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (group_id, student_id))",
    "CREATE TABLE posts (id SERIAL PRIMARY KEY, author_id INTEGER REFERENCES students(id), author_name VARCHAR(100), content TEXT NOT NULL, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)",
    "CREATE TABLE messages (id SERIAL PRIMARY KEY, sender_id INTEGER REFERENCES students(id), receiver_id INTEGER REFERENCES students(id), content TEXT NOT NULL, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)",
]:
    conn.execute(s)
XSS = '<img src=x onerror=document.body.dataset.pwned=1>'
conn.execute("INSERT INTO students (name,email,department,year,skills,interests) VALUES ('Aditya Bagri','aditya@geu.ac.in','CSE',2,'{C++}','{AI}'), ('Mallory','mallory@geu.ac.in',$1,2,'{C++}','{AI}'), ('Ravi','ravi@geu.ac.in','ECE',3,'{C++}','{AI}')", [XSS])
conn.execute("INSERT INTO posts (author_id,author_name,content) VALUES (2,'Mallory','post by someone else'), (1,'Aditya Bagri','hello from python api')")
conn.execute("INSERT INTO connections VALUES (1,2)")
FOUNDER_SEED = True
conn.execute("INSERT INTO project_posts (author_id,author_name,project_title,required_skills,team_size_needed,content) VALUES (1,'Aditya Bagri','Old Title','{C++}',3,'Old Title'), (2,'Mallory','Mallory Project','{AI}',2,'x')") if False else None

spec = importlib.util.spec_from_file_location("api", os.path.join(REPO, "api/index.py"))
api = importlib.util.module_from_spec(spec); spec.loader.exec_module(api)
def real_query(sql, params=None):
    with lock:
        cur = conn.execute(sql, [None if v is None else str(v) for v in (params or [])])
        if cur.description is None: return {"rows": []}
        cols = [d.name for d in cur.description]
        return {"rows": json.loads(json.dumps([dict(zip(cols, r)) for r in cur.fetchall()], default=str))}
api.query_neon = real_query
for s in api.SCHEMA_STATEMENTS: real_query(s)
if os.environ.get("FOUNDER"): conn.execute("INSERT INTO site_roles VALUES (1, 'founder')")
post_requests = []
def fake_verify(tok):
    if not tok.startswith("test-"): raise api.AuthError("invalid sign-in token")
    return tok[5:], "Aditya Bagri"
api.verify_ms_id_token = fake_verify

class Dev(api.handler):
    def do_GET(self):
        if self.path.startswith("/api/"):
            return super().do_GET()
        rel = self.path.split("?")[0].lstrip("/") or "index.html"
        fp = os.path.join(FRONTEND, rel)
        if not os.path.isfile(fp):
            self.send_response(404); self.end_headers(); return
        data = open(fp, "rb").read()
        self.send_response(200)
        self.send_header("Content-Type", mimetypes.guess_type(fp)[0] or "application/octet-stream")
        self.end_headers(); self.wfile.write(data)
    def do_POST(self):
        post_requests.append(self.path)
        time.sleep(0.8)            # make requests slow like a cold Vercel start
        return super().do_POST()
    def log_message(self, *a): pass

srv = ThreadingHTTPServer(("127.0.0.1", 8090), Dev)
threading.Thread(target=srv.serve_forever, daemon=True).start()

results = []
def check(name, cond, detail=""):
    results.append(bool(cond)); print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  -> {detail}"))

o = webdriver.FirefoxOptions(); o.add_argument("-headless")
d = webdriver.Firefox(options=o); d.set_window_size(1440, 1000)
inject = lambda js: d.execute_script("const s=document.createElement('script'); s.textContent=arguments[0]; document.body.appendChild(s);", js)
try:
    d.get("http://127.0.0.1:8090/")
    WebDriverWait(d, 20).until(lambda x: len(x.find_elements(By.CSS_SELECTOR, ".student-card")) == 3)
    time.sleep(1)
    cards = " ".join(c.text for c in d.find_elements(By.CSS_SELECTOR, ".student-card"))
    check("malicious department shown as plain text", "<img src=x" in cards, cards[:200])
    check("malicious department did NOT run", d.find_element(By.TAG_NAME, "body").get_attribute("data-pwned") is None)

    d.execute_script("arguments[0].click()", d.find_element(By.CSS_SELECTOR, '.nav-item[data-tab="feed"]')); time.sleep(1)
    authors = [e.text for e in d.find_elements(By.CSS_SELECTOR, "#feed-timeline .post-author")]
    check("feed shows author names (not 'Student #undefined')", authors == ["Aditya Bagri", "Mallory"], authors)
    check("no edit/delete buttons when signed out", len(d.find_elements(By.CSS_SELECTOR, "#feed-timeline .post-action-btn")) == 0)

    # Simulate a signed-in user, then double-click Publish
    inject("""sessionStorage.setItem('geu_student_email','aditya@geu.ac.in');
              sessionStorage.setItem('geu_student_id','1');
              window.getIdToken = async () => 'test-aditya@geu.ac.in';
              setActiveStudent(1);""")
    time.sleep(0.5)
    before = conn.execute("select count(*) from posts").fetchone()[0]
    d.find_element(By.ID, "feed-post-content").send_keys("double click test")
    inject("const b=document.getElementById('btn-submit-feed-post'); b.click(); b.click(); setTimeout(()=>b.click(),300);")
    time.sleep(4)
    after = conn.execute("select count(*) from posts").fetchone()[0]
    check("double/triple click creates exactly one post", after == before + 1, (before, after, post_requests))
    authors = [e.text for e in d.find_elements(By.CSS_SELECTOR, "#feed-timeline .post-author")]
    check("new post shows signer's name", authors and authors[0] == "Aditya Bagri", authors)
    cards = d.find_elements(By.CSS_SELECTOR, "#feed-timeline .feed-post-card")
    btns = [len(c.find_elements(By.CSS_SELECTOR, ".post-action-btn")) for c in cards]
    names = [c.find_element(By.CSS_SELECTOR, ".post-author").text for c in cards]
    check("Edit+Delete only on my own posts", all((n == 2) == (nm == "Aditya Bagri") for n, nm in zip(btns, names)), list(zip(names, btns)))
    d.save_screenshot(OUT + "/feed_fixed.png")
    if os.environ.get("FOUNDER"):
        check("founder name styled in feed", len(d.find_elements(By.CSS_SELECTOR, "#feed-timeline .founder-name")) >= 1)
        check("founder sees Remove on someone else's post", "Remove" in d.find_elements(By.CSS_SELECTOR, "#feed-timeline .feed-post-card")[-1].text)
        d.execute_script("arguments[0].click()", d.find_element(By.CSS_SELECTOR, '.nav-item[data-tab="students"]')); time.sleep(1.5)
        d.save_screenshot(OUT + "/founder_students.png")
        raise SystemExit(0 if all(results) else 1)
    before = conn.execute("select count(*) from posts").fetchone()[0]
    inject("window.confirm = () => true;")
    cards[0].find_element(By.CSS_SELECTOR, ".post-action-btn.danger").click()
    time.sleep(3)
    after = conn.execute("select count(*) from posts").fetchone()[0]
    check("Delete button removes my post", after == before - 1, (before, after))
    check("someone else's post untouched", conn.execute("select count(*) from posts where author_id=2").fetchone()[0] == 1)

    # Projects: edit via the modal, delete with confirm (only on my project)
    conn.execute("INSERT INTO project_posts (author_id,author_name,project_title,required_skills,team_size_needed,content) VALUES (1,'Aditya Bagri','Old Title','{C++}',3,'Old Title'), (2,'Mallory','Mallory Project','{AI}',2,'x')")
    inject("refreshAllData()"); time.sleep(2)
    d.execute_script("arguments[0].click()", d.find_element(By.CSS_SELECTOR, '.nav-item[data-tab="projects"]')); time.sleep(1)
    mine_id, other_id = [r[0] for r in conn.execute("select id from project_posts order by id").fetchall()]
    inject(f"selectProjectPost({other_id})"); time.sleep(2)
    check("no Edit/Delete on someone else's project", len(d.find_elements(By.CSS_SELECTOR, "#project-active-details .post-action-btn")) == 0)
    inject(f"selectProjectPost({mine_id})"); time.sleep(2)
    d.find_element(By.CSS_SELECTOR, "#project-active-details .post-action-btn:not(.danger)").click(); time.sleep(0.5)
    check("edit modal opens prefilled", d.find_element(By.ID, "proj-title").get_attribute("value") == "Old Title"
          and "Edit Project" in d.find_element(By.CSS_SELECTOR, "#modal-create-project h2").text)
    t = d.find_element(By.ID, "proj-title"); t.clear(); t.send_keys("New Title")
    sk = d.find_element(By.ID, "proj-skills"); sk.clear(); sk.send_keys("C++, Python")
    d.find_element(By.CSS_SELECTOR, '#form-create-project button[type="submit"]').click(); time.sleep(3)
    row = conn.execute("select project_title, required_skills from project_posts where id=$1", [mine_id]).fetchone()
    check("Save Changes updates my project", row[0] == "New Title" and row[1] == ["C++", "Python"], row)
    check("no duplicate project created", conn.execute("select count(*) from project_posts").fetchone()[0] == 2)
    d.find_element(By.ID, "btn-open-create-project").click(); time.sleep(0.5)
    check("'Post New Project' is back in create mode", "Post New Project" in d.find_element(By.CSS_SELECTOR, "#modal-create-project h2").text
          and d.find_element(By.ID, "proj-title").get_attribute("value") == "")
    inject("document.querySelector('#modal-create-project').classList.add('hidden'); window.confirm = () => true;")
    inject(f"selectProjectPost({mine_id})"); time.sleep(2)
    d.find_element(By.CSS_SELECTOR, "#project-active-details .post-action-btn.danger").click(); time.sleep(3)
    check("Delete removes my project only", [r[0] for r in conn.execute("select id from project_posts").fetchall()] == [other_id])

    # Network tab friends list with malicious department
    d.execute_script("arguments[0].click()", d.find_element(By.CSS_SELECTOR, '.nav-item[data-tab="network"]')); time.sleep(2)
    fl = d.find_element(By.ID, "active-user-friends-list").text
    check("friends list shows department as text", "<img src=x" in fl, fl)
    check("still nothing executed", d.find_element(By.TAG_NAME, "body").get_attribute("data-pwned") is None)
    # Project team: assemble twice -> one team; picked students are invited, not added
    pid = conn.execute("INSERT INTO project_posts (author_id,author_name,project_title,required_skills,team_size_needed,content) VALUES (1,'Aditya Bagri','Robot','{C++}',3,'Robot') RETURNING id").fetchone()[0]
    inject("refreshAllData()"); time.sleep(2)
    d.execute_script("arguments[0].click()", d.find_element(By.CSS_SELECTOR, '.nav-item[data-tab="projects"]')); time.sleep(1)
    inject(f"selectProjectPost({pid})"); time.sleep(3)
    boxes = d.find_elements(By.CSS_SELECTOR, ".candidate-checkbox")
    check("checkboxes start unticked", boxes and not any(b.is_selected() for b in boxes), [b.is_selected() for b in boxes])
    ravi_box = d.find_element(By.CSS_SELECTOR, '.candidate-checkbox[value="3"]'); ravi_box.click()
    d.find_element(By.ID, "btn-create-project-group").click(); time.sleep(4)
    check("first Assemble: one team, Ravi invited (not a member)",
          conn.execute("select count(*) from groups where is_project_group").fetchone()[0] == 1
          and conn.execute("select count(*) from team_invites where status='pending' and student_id=3").fetchone()[0] == 1
          and conn.execute("select count(*) from group_members gm join groups g on g.id=gm.group_id where g.is_project_group").fetchone()[0] == 1)
    card = d.find_element(By.ID, "candidate-card-3").text
    check("Ravi's card now says 'Invited'", "Invited" in card, card)
    btn = d.find_element(By.ID, "btn-create-project-group").text
    check("button changed to 'Invite selected'", "Invite selected" in btn, btn)
    d.find_element(By.ID, "btn-create-project-group").click(); time.sleep(3)
    check("second click creates NO new group", conn.execute("select count(*) from groups where is_project_group").fetchone()[0] == 1)

    # Keyword search (Trie)
    d.execute_script("arguments[0].click()", d.find_element(By.CSS_SELECTOR, '.nav-item[data-tab="feed"]')); time.sleep(1)
    box = d.find_element(By.ID, "input-keyword-search"); box.send_keys("pyth"); time.sleep(2.5)
    res = d.find_element(By.ID, "search-results").text
    chips = [c.text for c in d.find_elements(By.CSS_SELECTOR, "#search-suggestions .suggestion-chip")]
    check("typing 'pyth' finds the python post", "hello from python api" in res, res)
    check("autocomplete suggests 'python'", "python" in chips, chips)
    d.find_elements(By.CSS_SELECTOR, "#search-suggestions .suggestion-chip")[0].click(); time.sleep(1.5)
    check("clicking a suggestion completes the word", d.find_element(By.ID, "input-keyword-search").get_attribute("value").startswith("python"))
    box.clear(); box.send_keys("<img src=x onerror=document.body.dataset.pwned=1>"); time.sleep(2.5)
    check("search echo is escaped", d.find_element(By.TAG_NAME, "body").get_attribute("data-pwned") is None)

    # Connection requests UI (signed in as Aditya, connected to Mallory, not to Ravi)
    d.execute_script("arguments[0].click()", d.find_element(By.CSS_SELECTOR, '.nav-item[data-tab="students"]')); time.sleep(2)
    def card_text(name):
        for c in d.find_elements(By.CSS_SELECTOR, ".student-card"):
            if name in c.find_element(By.CSS_SELECTOR, ".student-name").text: return c.text
        return ""
    check("connected student shows '✓ Connected'", "Connected" in card_text("Mallory"), card_text("Mallory"))
    check("other student shows '+ Connect'", "+ Connect" in card_text("Ravi"), card_text("Ravi"))
    for c in d.find_elements(By.CSS_SELECTOR, ".student-card"):
        if "Ravi" in c.find_element(By.CSS_SELECTOR, ".student-name").text:
            c.find_element(By.XPATH, ".//button[contains(., 'Connect')]").click()
    time.sleep(3)
    check("clicking Connect sends a request (no instant connection)",
          conn.execute("select count(*) from connection_requests where status='pending'").fetchone()[0] == 1
          and conn.execute("select count(*) from connections").fetchone()[0] == 1)
    check("button turns into 'Requested'", "Requested" in card_text("Ravi"), card_text("Ravi"))
    d.execute_script("arguments[0].click()", d.find_element(By.CSS_SELECTOR, '.nav-item[data-tab="network"]')); time.sleep(2)
    check("Requests panel shows the outgoing request", "Ravi" in d.find_element(By.ID, "requests-list").text)
    d.execute_script("arguments[0].click()", d.find_element(By.CSS_SELECTOR, '.nav-item[data-tab="messages"]')); time.sleep(2)
    contacts = d.find_element(By.ID, "chat-partners-list").text
    check("chat lists only connections", "Mallory" in contacts and "Ravi" not in contacts, contacts)
finally:
    d.quit(); srv.shutdown()
print(f"\n{sum(results)}/{len(results)} passed")
sys.exit(0 if all(results) else 1)
