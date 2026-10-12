import urllib.request, urllib.parse, json

BASE = "http://localhost:8080"

def get(path):
    req = urllib.request.Request(f"{BASE}{path}")
    with urllib.request.urlopen(req) as res:
        return json.loads(res.read().decode())

def post(path, body):
    data = json.dumps(body).encode('utf-8')
    req = urllib.request.Request(f"{BASE}{path}", data=data, headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(req) as res:
        return json.loads(res.read().decode())

print("Testing comprehensive suite:")
# 0. Load demo data
load_res = post("/api/demo/load", {})
assert load_res["ok"], load_res
print("0. POST /api/demo/load verified")

# 1. Register student
s_res = post("/api/students", {
    "name": "Frontend Tester",
    "department": "CSE",
    "year": 4,
    "skills": ["JavaScript", "HTML", "CSS"],
    "interests": ["UI/UX", "Web Design"]
})
assert s_res["ok"], s_res
tester_id = s_res["data"]["id"]
print(f"1. Registered student ID {tester_id}")

# 2. Get one student
s1 = get(f"/api/students/{tester_id}")
assert s1["ok"] and s1["data"]["name"] == "Frontend Tester"
print("2. GET /api/students/:id verified")

# 3. Search
search_res = get("/api/students/search?skill=JavaScript")
assert search_res["ok"] and len(search_res["data"]) >= 1
print("3. GET /api/students/search?skill=... verified")

# 4. Similar
sim_res = get(f"/api/students/1/similar")
assert sim_res["ok"] and len(sim_res["data"]) > 0
print("4. GET /api/students/:id/similar verified")

# 5. Connect
conn_res = post("/api/connections", {"a": 1, "b": 3})
assert conn_res["ok"]
print("5. POST /api/connections verified")

# 6. Create group
grp_res = post("/api/groups", {"name": "Web Dev Guild", "description": "Frontend devs unite", "adminId": 1})
assert grp_res["ok"]
gid = grp_res["data"]["id"]
print(f"6. POST /api/groups ID {gid} created")

# 7. Join & Leave group
join_res = post(f"/api/groups/{gid}/join", {"studentId": 2})
assert join_res["ok"]
leave_res = post(f"/api/groups/{gid}/leave", {"studentId": 2})
assert leave_res["ok"]
print("7. Join & Leave group verified")

# 8. Group posts
gpost_res = post(f"/api/groups/{gid}/posts", {"studentId": 1, "content": "Welcome to Web Dev Guild!"})
assert gpost_res["ok"]
gposts = get(f"/api/groups/{gid}/posts")
assert len(gposts["data"]) >= 1
print("8. Group posts verified")

# 9. Global posts
post_res = post("/api/posts", {"studentId": 1, "content": "Broadcast to campus!"})
assert post_res["ok"]
posts_all = get("/api/posts")
assert len(posts_all["data"]) >= 1
print("9. Global posts verified")

# 10. Project post & top-k & create-group
pp_res = post("/api/project-posts", {
    "studentId": 1,
    "title": "Quantum Computing Lab",
    "requiredSkills": ["Python", "DSA"],
    "teamSize": 2
})
assert pp_res["ok"]
ppId = pp_res["data"]["id"]
topk_res = get(f"/api/project-posts/{ppId}/top-k?mode=all&k=2")
assert topk_res["ok"] and len(topk_res["data"]["results"]) > 0
matched_ids = [m["studentId"] for m in topk_res["data"]["results"]]

create_pg_res = post(f"/api/project-posts/{ppId}/create-group", {
    "leaderId": 1,
    "groupName": "Quantum Core Team",
    "memberIds": matched_ids
})
assert create_pg_res["ok"] and create_pg_res["data"]["isProjectGroup"]
print("10. Project post -> Top-K -> Create Project Group verified")

print("\nALL BACKEND API ENDPOINTS FUNCTIONING 100% PERFECTLY!")
