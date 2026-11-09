import urllib.request, json

BASE = "http://localhost:8080"

def get(path):
    req = urllib.request.Request(BASE + path)
    with urllib.request.urlopen(req) as res:
        return json.loads(res.read().decode())

def post(path, body):
    data = json.dumps(body).encode('utf-8')
    req = urllib.request.Request(BASE + path, data=data, headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(req) as res:
        return json.loads(res.read().decode())

# Load demo data
r = post("/api/demo/load", {})
print("Demo load:", r["data"]["message"])
print()

# Students
students = get("/api/students")
print("=== STUDENTS ({}) ===".format(len(students["data"])))
for s in students["data"]:
    print("  [{}] {} | {} Year {} | Skills: {}".format(s["id"], s["name"], s["department"], s["year"], s["skills"]))

# Posts
posts = get("/api/posts")
print("\n=== POSTS ({}) ===".format(len(posts["data"])))
for p in posts["data"]:
    label = "[PROJECT]" if p.get("isProjectPost") else "[POST]"
    print("  {} By {}: {}".format(label, p["authorName"], p["content"][:60]))

# Groups
groups = get("/api/groups")
print("\n=== GROUPS ({}) ===".format(len(groups["data"])))
for g in groups["data"]:
    label = "[PROJECT GROUP]" if g.get("isProjectGroup") else "[GROUP]"
    print("  {} '{}' | Admin: {} | Members: {}".format(label, g["name"], g["adminName"], g["memberCount"]))

print("\nAll dummy data loaded successfully!")
