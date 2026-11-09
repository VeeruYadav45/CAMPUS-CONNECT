import sys
import os
import json
import urllib.request
import ssl
import socket

# Ensure reliable connection to Neon Cloud across local networks
ORIG_GETADDRINFO = socket.getaddrinfo
def neon_dns_resolver(host, port, family=0, type=0, proto=0, flags=0):
    if "neon.tech" in host:
        return [(socket.AF_INET, socket.SOCK_STREAM, 6, "", ("52.220.170.93", port))]
    return ORIG_GETADDRINFO(host, port, family, type, proto, flags)

socket.getaddrinfo = neon_dns_resolver

def get_db_url():
    if not os.path.exists(".env"):
        raise FileNotFoundError(".env file missing!")
    with open(".env") as f:
        for line in f:
            if line.startswith("DATABASE_URL="):
                return line.strip().split("=", 1)[1].strip('"\'')
    raise ValueError("DATABASE_URL not found in .env")

def query_neon(sql):
    db_url = get_db_url()
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

def pull_all():
    """Fetch students and messages from Neon and save to neon_cache.json for server.exe"""
    students_res = query_neon("SELECT id, name, skills, interests, department as branch, year FROM students ORDER BY id;")
    messages_res = query_neon("SELECT id, sender_id, receiver_id, content, to_char(created_at, 'YYYY-MM-DD HH24:MI:SS') as timestamp FROM messages ORDER BY id;")
    
    data = {
        "students": students_res.get("rows", []),
        "messages": messages_res.get("rows", [])
    }
    with open("neon_cache.json", "w") as f:
        json.dump(data, f, indent=2)
    print(f"Synced {len(data['students'])} students and {len(data['messages'])} messages from Neon Cloud.")

def add_message(sender_id, receiver_id, content):
    escaped = content.replace("'", "''")
    sql = f"INSERT INTO messages (sender_id, receiver_id, content) VALUES ({sender_id}, {receiver_id}, '{escaped}') RETURNING id;"
    res = query_neon(sql)
    print(f"Inserted message ID {res['rows'][0]['id']} into Neon.")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        cmd = sys.argv[1]
        if cmd == "pull":
            pull_all()
        elif cmd == "add_message" and len(sys.argv) >= 5:
            add_message(int(sys.argv[2]), int(sys.argv[3]), sys.argv[4])
        else:
            pull_all()
    else:
        pull_all()
