#include "include/compat.h"
#include "include/third_party/httplib.h"
#include "include/third_party/json.hpp"

#include "include/DataStore.h"
#include "include/Graph.h"
#include "include/Group.h"
#include "include/JaccardMatching.h"
#include "include/ProjectGroup.h"
#include "include/ProjectPost.h"
#include "include/TopKRanker.h"
#include "include/post.h"
#include "include/student.h"

#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <algorithm>
#include <unordered_set>
#include <mutex>

using namespace std;
using json = nlohmann::json;

// Global backend state and thread safety lock
static DataStore store;
static Graph graph;
static int nextStudentId = 1;
static int nextGroupId = 1;
static std::mutex serverMutex;

// Timestamp helper
static string getCurrentTime() {
    time_t now = time(0);
    string dt = ctime(&now);
    if (!dt.empty() && dt.back() == '\n') {
        dt.pop_back();
    }
    return dt;
}

// Global registry for ProjectPost specific fields (since DataStore stores Post value types)
struct ProjectPostMeta {
    string projectTitle;
    vector<string> requiredSkills;
    int teamSizeNeeded;
};
static unordered_map<int, ProjectPostMeta> projectPostMetaMap;

// Global registry for ProjectGroup specific fields
struct ProjectGroupMeta {
    int leaderId;
    int linkedPostId;
};
static unordered_map<int, ProjectGroupMeta> projectGroupMetaMap;

// Helper: Seed demo data silently
static void loadDemoData() {
    store.clearAll();
    graph.clearAll();
    nextStudentId = 1;
    nextGroupId = 1;
    projectPostMetaMap.clear();
    projectGroupMetaMap.clear();

    // 8 Sample students
    Student s1(nextStudentId++, "Tanvay Jain", {"C++", "Python", "DSA"}, {"AI", "Web Dev"}, "CSE", 2);
    store.addStudent(s1);
    graph.addStudent(s1.getId());

    Student s2(nextStudentId++, "Ravi Kumar", {"Python", "C++", "ML"}, {"AI", "Data Science"}, "CSE", 2);
    store.addStudent(s2);
    graph.addStudent(s2.getId());

    Student s3(nextStudentId++, "Priya Sharma", {"Java", "Python", "Web Dev"}, {"Web Dev", "Open Source"}, "CSE", 3);
    store.addStudent(s3);
    graph.addStudent(s3.getId());

    Student s4(nextStudentId++, "Ankit Singh", {"C++", "Embedded", "IoT"}, {"Robotics", "IoT"}, "ECE", 2);
    store.addStudent(s4);
    graph.addStudent(s4.getId());

    Student s5(nextStudentId++, "Neha Gupta", {"Python", "DSA", "ML"}, {"AI", "Competitive Programming"}, "CSE", 2);
    store.addStudent(s5);
    graph.addStudent(s5.getId());

    Student s6(nextStudentId++, "Arjun Mehta", {"C++", "DSA", "Python"}, {"Competitive Programming", "AI"}, "CSE", 3);
    store.addStudent(s6);
    graph.addStudent(s6.getId());

    Student s7(nextStudentId++, "Sanya Verma", {"Java", "Spring", "SQL"}, {"Backend Dev", "Cloud"}, "IT", 3);
    store.addStudent(s7);
    graph.addStudent(s7.getId());

    Student s8(nextStudentId++, "Karan Patel", {"C++", "DSA", "System Design"}, {"AI", "Open Source"}, "CSE", 2);
    store.addStudent(s8);
    graph.addStudent(s8.getId());

    // Posts
    Post regularPost(store.getNextPostId(), s1.getId(), s1.getName(),
                     "Hello everyone! Excited to connect on Campus-Connect!",
                     getCurrentTime());
    store.addPost(regularPost);

    int ppId = store.getNextPostId();
    ProjectPost projPost(ppId, s1.getId(), s1.getName(),
                         "AI Study Group Project", {"Python", "ML", "DSA"}, 3,
                         getCurrentTime());
    store.addPost(projPost);
    projectPostMetaMap[ppId] = { "AI Study Group Project", {"Python", "ML", "DSA"}, 3 };

    // Graph connections
    graph.addConnection(s1.getId(), s2.getId());
    graph.addConnection(s1.getId(), s5.getId());
    graph.addConnection(s2.getId(), s5.getId());
    graph.addConnection(s2.getId(), s6.getId());
    graph.addConnection(s3.getId(), s7.getId());
    graph.addConnection(s5.getId(), s6.getId());
    graph.addConnection(s5.getId(), s8.getId());
    graph.addConnection(s6.getId(), s8.getId());
    graph.addConnection(s4.getId(), s1.getId());

    // ProjectGroup
    int pgId = nextGroupId++;
    ProjectGroup pGroup(pgId, "AI Study Group",
                        "A team for the AI Study Group Project",
                        s1.getId(), ppId);
    // Add top 2 matches from BFS
    JaccardMatching matcher;
    vector<RankedResult> topBFS = TopKRanker::getTopKUsingBFS(s1, graph, store, matcher, 5);
    int added = 0;
    for (size_t i = 0; i < topBFS.size() && added < 2; i++) {
        pGroup.addMember(topBFS[i].studentId);
        added++;
    }
    store.addGroup(pGroup);
    projectGroupMetaMap[pgId] = { s1.getId(), ppId };
}

// Helper: JSON response helpers
static void sendJsonOk(httplib::Response &res, const json &data) {
    json j;
    j["ok"] = true;
    j["data"] = data;
    res.set_content(j.dump(), "application/json");
}

static void sendJsonError(httplib::Response &res, const string &msg, int status = 400) {
    json j;
    j["ok"] = false;
    j["error"] = msg;
    res.status = status;
    res.set_content(j.dump(), "application/json");
}

static json studentToJson(const Student &s) {
    json j;
    j["id"] = s.getId();
    j["name"] = s.getName();
    j["skills"] = s.getSkills();
    j["interests"] = s.getInterests();
    j["department"] = s.getDepartment();
    j["year"] = s.getYear();
    return j;
}

static json postToJson(const Post &p) {
    json j;
    int id = p.getPostId();
    j["id"] = id;
    j["authorId"] = p.getAuthorId();
    j["studentId"] = p.getAuthorId();
    j["authorName"] = p.getAuthorName();
    j["studentName"] = p.getAuthorName();
    j["content"] = p.getContent();
    j["timestamp"] = p.getTimestamp();
    j["likes"] = p.getLikes();

    auto it = projectPostMetaMap.find(id);
    if (it != projectPostMetaMap.end()) {
        j["isProjectPost"] = true;
        j["projectTitle"] = it->second.projectTitle;
        j["requiredSkills"] = it->second.requiredSkills;
        j["teamSizeNeeded"] = it->second.teamSizeNeeded;
    } else {
        j["isProjectPost"] = false;
    }
    return j;
}

static json groupToJson(const Group &g) {
    json j;
    int id = g.getGroupId();
    j["id"] = id;
    j["name"] = g.getGroupName();
    j["description"] = g.getDescription();
    j["adminId"] = g.getAdminId();

    Student *admin = store.findStudentById(g.getAdminId());
    j["adminName"] = admin ? admin->getName() : "Unknown";

    vector<int> mIds = g.getMemberIds();
    j["memberIds"] = mIds;
    j["memberCount"] = mIds.size();

    json membersList = json::array();
    for (int mid : mIds) {
        Student *ms = store.findStudentById(mid);
        json mj;
        mj["id"] = mid;
        mj["name"] = ms ? ms->getName() : ("Student #" + to_string(mid));
        membersList.push_back(mj);
    }
    j["members"] = membersList;

    auto it = projectGroupMetaMap.find(id);
    if (it != projectGroupMetaMap.end()) {
        j["isProjectGroup"] = true;
        j["leaderId"] = it->second.leaderId;
        j["linkedPostId"] = it->second.linkedPostId;
    } else {
        j["isProjectGroup"] = false;
    }
    return j;
}

int main() {
    httplib::Server svr;

    // CORS preflight and headers
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With"}
    });

    svr.Options(".*", [](const httplib::Request &, httplib::Response &res) {
        res.status = 204;
    });

    // 1. POST /api/students
    svr.Post("/api/students", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        try {
            auto body = json::parse(req.body);
            if (!body.contains("name") || !body.contains("department") || !body.contains("year")) {
                return sendJsonError(res, "Missing required fields: name, department, year");
            }
            string name = body["name"].get<string>();
            string dept = body["department"].get<string>();
            int year = body["year"].get<int>();

            if (name.empty()) return sendJsonError(res, "Name cannot be empty");
            if (dept.empty()) return sendJsonError(res, "Department cannot be empty");
            if (year < 1 || year > 4) return sendJsonError(res, "Year must be between 1 and 4");

            vector<string> skills;
            if (body.contains("skills") && body["skills"].is_array()) {
                skills = body["skills"].get<vector<string>>();
            }
            vector<string> interests;
            if (body.contains("interests") && body["interests"].is_array()) {
                interests = body["interests"].get<vector<string>>();
            }

            int id = nextStudentId++;
            Student s(id, name, skills, interests, dept, year);
            store.addStudent(s);
            graph.addStudent(id);

            sendJsonOk(res, studentToJson(s));
        } catch (const exception &e) {
            sendJsonError(res, string("Invalid request: ") + e.what());
        }
    });

    // 2. GET /api/students
    svr.Get("/api/students", [](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        vector<Student> all = store.getAllStudents();
        // Sort by ID ascending
        sort(all.begin(), all.end(), [](const Student &a, const Student &b) {
            return a.getId() < b.getId();
        });
        json arr = json::array();
        for (const auto &s : all) {
            arr.push_back(studentToJson(s));
        }
        sendJsonOk(res, arr);
    });

    // 3. GET /api/students/search?skill=|interest=|department=
    svr.Get("/api/students/search", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        vector<Student> results;
        if (req.has_param("skill")) {
            results = store.matchBySkill(req.get_param_value("skill"));
        } else if (req.has_param("interest")) {
            results = store.matchByInterest(req.get_param_value("interest"));
        } else if (req.has_param("department")) {
            results = store.matchByDepartment(req.get_param_value("department"));
        } else {
            results = store.getAllStudents();
        }

        json arr = json::array();
        for (const auto &s : results) {
            arr.push_back(studentToJson(s));
        }
        sendJsonOk(res, arr);
    });

    // 4. GET /api/students/:id/similar
    svr.Get(R"(/api/students/(\d+)/similar)", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        try {
            int targetId = stoi(req.matches[1]);
            Student *targetPtr = store.findStudentById(targetId);
            if (!targetPtr) {
                return sendJsonError(res, "Student not found", 404);
            }
            Student targetCopy = *targetPtr;

            JaccardMatching matcher;
            vector<Student> all = store.getAllStudents();
            vector<RankedResult> ranked = TopKRanker::getTopK(targetCopy, all, matcher, (int)all.size());

            const vector<string> &tSkills = targetCopy.getSkills();
            unordered_set<string> targetSkills(tSkills.begin(), tSkills.end());
            json arr = json::array();
            for (const auto &r : ranked) {
                Student *s = store.findStudentById(r.studentId);
                if (s) {
                    json item;
                    item["student"] = studentToJson(*s);
                    item["studentId"] = r.studentId;
                    item["name"] = r.studentName;
                    item["similarity"] = r.score;
                    item["score"] = r.score;
                    item["scorePercent"] = (int)(r.score * 100 + 0.5);
                    vector<string> matchedSkills;
                    for (const auto &sk : s->getSkills()) {
                        if (targetSkills.count(sk)) matchedSkills.push_back(sk);
                    }
                    item["matchedSkills"] = matchedSkills;
                    arr.push_back(item);
                }
            }

            sendJsonOk(res, arr);
        } catch (const exception &e) {
            sendJsonError(res, string("Error computing similarity: ") + e.what(), 500);
        }
    });

    // 5. GET /api/students/:id/connections
    svr.Get(R"(/api/students/(\d+)/connections)", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        try {
            int id = stoi(req.matches[1]);
            Student *target = store.findStudentById(id);
            if (!target) {
                return sendJsonError(res, "Student not found", 404);
            }

            vector<int> connIds = graph.getConnections(id);
            json arr = json::array();
            for (int cid : connIds) {
                Student *cs = store.findStudentById(cid);
                if (cs) {
                    arr.push_back(studentToJson(*cs));
                }
            }
            sendJsonOk(res, arr);
        } catch (const exception &e) {
            sendJsonError(res, string("Error fetching connections: ") + e.what(), 500);
        }
    });

    // 5b. GET /api/students/:id/friends (alias for connections — used by frontend)
    svr.Get(R"(/api/students/(\d+)/friends)", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        try {
            int id = stoi(req.matches[1]);
            vector<int> connIds = graph.getConnections(id);
            json arr = json::array();
            for (int cid : connIds) {
                Student *cs = store.findStudentById(cid);
                if (cs) arr.push_back(studentToJson(*cs));
            }
            sendJsonOk(res, arr);
        } catch (const exception &e) {
            sendJsonError(res, string("Error: ") + e.what(), 500);
        }
    });

    // 6. GET /api/students/:id (exact match)
    svr.Get(R"(/api/students/(\d+)$)", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        try {
            int id = stoi(req.matches[1]);
            Student *s = store.findStudentById(id);
            if (!s) {
                return sendJsonError(res, "Student not found", 404);
            }
            sendJsonOk(res, studentToJson(*s));
        } catch (const exception &e) {
            sendJsonError(res, string("Error fetching student: ") + e.what(), 500);
        }
    });

    // 7. POST /api/connections
    svr.Post("/api/connections", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        try {
            auto body = json::parse(req.body);
            if (!body.contains("a") || !body.contains("b")) {
                return sendJsonError(res, "Missing connection endpoints 'a' and 'b'");
            }
            int a = body["a"].get<int>();
            int b = body["b"].get<int>();

            if (a == b) return sendJsonError(res, "Cannot connect a student to themselves");
            if (!store.findStudentById(a)) return sendJsonError(res, "Student A not found", 404);
            if (!store.findStudentById(b)) return sendJsonError(res, "Student B not found", 404);

            graph.addConnection(a, b);
            json data;
            data["a"] = a;
            data["b"] = b;
            data["message"] = "Connection created successfully";
            sendJsonOk(res, data);
        } catch (const exception &e) {
            sendJsonError(res, string("Invalid request: ") + e.what());
        }
    });

    // 7b. GET /api/connections
    svr.Get("/api/connections", [](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        json arr = json::array();
        vector<Student> all = store.getAllStudents();
        for (const auto &s : all) {
            vector<int> conns = graph.getConnections(s.getId());
            for (int cid : conns) {
                if (cid > s.getId()) { // avoid duplicates
                    json edge;
                    edge["a"] = s.getId();
                    edge["b"] = cid;
                    arr.push_back(edge);
                }
            }
        }
        sendJsonOk(res, arr);
    });

    // 7c. GET /api/connections/mutual?a=X&b=Y
    svr.Get("/api/connections/mutual", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        if (!req.has_param("a") || !req.has_param("b"))
            return sendJsonError(res, "Missing params a and b");
        int a = stoi(req.get_param_value("a"));
        int b = stoi(req.get_param_value("b"));
        vector<int> aConns = graph.getConnections(a);
        vector<int> bConns = graph.getConnections(b);
        unordered_set<int> bSet(bConns.begin(), bConns.end());
        json arr = json::array();
        for (int id : aConns) {
            if (bSet.count(id) && id != a && id != b) {
                Student *s = store.findStudentById(id);
                if (s) arr.push_back(studentToJson(*s));
            }
        }
        sendJsonOk(res, arr);
    });

    // 8. POST /api/groups
    svr.Post("/api/groups", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        try {
            auto body = json::parse(req.body);
            if (!body.contains("name") || !body.contains("adminId")) {
                return sendJsonError(res, "Missing group name or adminId");
            }
            string name = body["name"].get<string>();
            string desc = body.value("description", "");
            int adminId = body["adminId"].get<int>();

            if (name.empty()) return sendJsonError(res, "Group name cannot be empty");
            if (!store.findStudentById(adminId)) {
                return sendJsonError(res, "Admin student ID not found", 404);
            }

            int id = nextGroupId++;
            Group g(id, name, desc, adminId);
            store.addGroup(g);

            sendJsonOk(res, groupToJson(g));
        } catch (const exception &e) {
            sendJsonError(res, string("Invalid request: ") + e.what());
        }
    });

    // 9. GET /api/groups
    svr.Get("/api/groups", [](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        vector<Group> all = store.getAllGroups();
        sort(all.begin(), all.end(), [](const Group &a, const Group &b) {
            return a.getGroupId() < b.getGroupId();
        });
        json arr = json::array();
        for (const auto &g : all) {
            arr.push_back(groupToJson(g));
        }
        sendJsonOk(res, arr);
    });

    // 10. POST /api/groups/:id/join
    svr.Post(R"(/api/groups/(\d+)/join)", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        int gId = stoi(req.matches[1]);
        Group *g = store.findGroupById(gId);
        if (!g) return sendJsonError(res, "Group not found", 404);

        try {
            auto body = json::parse(req.body);
            if (!body.contains("studentId")) return sendJsonError(res, "Missing studentId");
            int sId = body["studentId"].get<int>();
            if (!store.findStudentById(sId)) return sendJsonError(res, "Student not found", 404);

            if (g->isMember(sId)) {
                return sendJsonError(res, "Student is already a member of this group");
            }
            g->addMember(sId);
            sendJsonOk(res, groupToJson(*g));
        } catch (const exception &e) {
            sendJsonError(res, string("Invalid request: ") + e.what());
        }
    });

    // 11. POST /api/groups/:id/leave
    svr.Post(R"(/api/groups/(\d+)/leave)", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        int gId = stoi(req.matches[1]);
        Group *g = store.findGroupById(gId);
        if (!g) return sendJsonError(res, "Group not found", 404);

        try {
            auto body = json::parse(req.body);
            if (!body.contains("studentId")) return sendJsonError(res, "Missing studentId");
            int sId = body["studentId"].get<int>();
            if (!store.findStudentById(sId)) return sendJsonError(res, "Student not found", 404);

            if (!g->isMember(sId)) {
                return sendJsonError(res, "Student is not a member of this group");
            }
            if (sId == g->getAdminId()) {
                return sendJsonError(res, "Group admin cannot leave the group");
            }
            g->removeMember(sId);
            sendJsonOk(res, groupToJson(*g));
        } catch (const exception &e) {
            sendJsonError(res, string("Invalid request: ") + e.what());
        }
    });

    // 12. GET /api/groups/:id/posts
    svr.Get(R"(/api/groups/(\d+)/posts)", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        int gId = stoi(req.matches[1]);
        Group *g = store.findGroupById(gId);
        if (!g) return sendJsonError(res, "Group not found", 404);

        vector<Post> gPosts = g->getPosts();
        json arr = json::array();
        for (const auto &p : gPosts) {
            arr.push_back(postToJson(p));
        }
        sendJsonOk(res, arr);
    });

    // 13. POST /api/groups/:id/posts
    svr.Post(R"(/api/groups/(\d+)/posts)", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        int gId = stoi(req.matches[1]);
        Group *g = store.findGroupById(gId);
        if (!g) return sendJsonError(res, "Group not found", 404);

        try {
            auto body = json::parse(req.body);
            if (!body.contains("studentId") || !body.contains("content")) {
                return sendJsonError(res, "Missing studentId or content");
            }
            int sId = body["studentId"].get<int>();
            string content = body["content"].get<string>();

            if (content.empty()) return sendJsonError(res, "Post content cannot be empty");
            Student *s = store.findStudentById(sId);
            if (!s) return sendJsonError(res, "Student not found", 404);

            if (!g->isMember(sId)) {
                return sendJsonError(res, "Only members can post in this group", 403);
            }

            int pId = store.getNextPostId();
            Post p(pId, sId, s->getName(), content, getCurrentTime());
            g->addPost(p);
            sendJsonOk(res, postToJson(p));
        } catch (const exception &e) {
            sendJsonError(res, string("Invalid request: ") + e.what());
        }
    });

    // 14. GET /api/posts
    svr.Get("/api/posts", [](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        vector<Post> all = store.getAllPosts();
        // Sort descending by post ID (chronological reverse)
        sort(all.begin(), all.end(), [](const Post &a, const Post &b) {
            return a.getPostId() > b.getPostId();
        });
        json arr = json::array();
        for (const auto &p : all) {
            arr.push_back(postToJson(p));
        }
        sendJsonOk(res, arr);
    });

    // 15. POST /api/posts
    svr.Post("/api/posts", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        try {
            auto body = json::parse(req.body);
            if (!body.contains("studentId") || !body.contains("content")) {
                return sendJsonError(res, "Missing studentId or content");
            }
            int sId = body["studentId"].get<int>();
            string content = body["content"].get<string>();

            if (content.empty()) return sendJsonError(res, "Content cannot be empty");
            Student *s = store.findStudentById(sId);
            if (!s) return sendJsonError(res, "Student not found", 404);

            int pId = store.getNextPostId();
            Post p(pId, sId, s->getName(), content, getCurrentTime());
            store.addPost(p);
            sendJsonOk(res, postToJson(p));
        } catch (const exception &e) {
            sendJsonError(res, string("Invalid request: ") + e.what());
        }
    });

    // 16. POST /api/project-posts
    svr.Post("/api/project-posts", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        try {
            auto body = json::parse(req.body);
            if (!body.contains("studentId") || !body.contains("title") || !body.contains("teamSize")) {
                return sendJsonError(res, "Missing studentId, title, or teamSize");
            }
            int sId = body["studentId"].get<int>();
            string title = body["title"].get<string>();
            int teamSize = body["teamSize"].get<int>();

            if (title.empty()) return sendJsonError(res, "Title cannot be empty");
            if (teamSize < 1) return sendJsonError(res, "Team size must be at least 1");
            Student *s = store.findStudentById(sId);
            if (!s) return sendJsonError(res, "Student not found", 404);

            vector<string> reqSkills;
            if (body.contains("requiredSkills") && body["requiredSkills"].is_array()) {
                reqSkills = body["requiredSkills"].get<vector<string>>();
            }

            int pId = store.getNextPostId();
            ProjectPost pp(pId, sId, s->getName(), title, reqSkills, teamSize, getCurrentTime());
            store.addPost(pp);
            projectPostMetaMap[pId] = { title, reqSkills, teamSize };

            sendJsonOk(res, postToJson(pp));
        } catch (const exception &e) {
            sendJsonError(res, string("Invalid request: ") + e.what());
        }
    });

    // 17. GET /api/project-posts
    svr.Get("/api/project-posts", [](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        vector<Post> all = store.getAllPosts();
        sort(all.begin(), all.end(), [](const Post &a, const Post &b) {
            return a.getPostId() > b.getPostId();
        });
        json arr = json::array();
        for (const auto &p : all) {
            if (projectPostMetaMap.find(p.getPostId()) != projectPostMetaMap.end()) {
                arr.push_back(postToJson(p));
            }
        }
        sendJsonOk(res, arr);
    });

    // 18. GET /api/project-posts/:postId/top-k?studentId=&k=5&mode=all|bfs
    svr.Get(R"(/api/project-posts/(\d+)/top-k)", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        int postId = stoi(req.matches[1]);
        auto metaIt = projectPostMetaMap.find(postId);
        if (metaIt == projectPostMetaMap.end()) {
            return sendJsonError(res, "ProjectPost not found", 404);
        }

        Post *postObj = store.findPostById(postId);
        if (!postObj) return sendJsonError(res, "Post not found", 404);

        int studentId = postObj->getAuthorId();
        if (req.has_param("studentId")) {
            studentId = stoi(req.get_param_value("studentId"));
        }
        Student *target = store.findStudentById(studentId);
        if (!target) return sendJsonError(res, "Target student not found", 404);

        int k = 5;
        if (req.has_param("k")) {
            k = max(1, stoi(req.get_param_value("k")));
        }

        string mode = "all";
        if (req.has_param("mode")) {
            mode = req.get_param_value("mode");
        }

        JaccardMatching matcher;
        vector<RankedResult> ranked;

        // Create a temporary synthetic student representing the project requirements
        Student searchTarget(target->getId(), target->getName(),
                             metaIt->second.requiredSkills,
                             target->getInterests(),
                             target->getDepartment(),
                             target->getYear());

        if (mode == "bfs") {
            ranked = TopKRanker::getTopKUsingBFS(searchTarget, graph, store, matcher, k);
        } else {
            vector<Student> all = store.getAllStudents();
            ranked = TopKRanker::getTopK(searchTarget, all, matcher, k);
        }

        json arr = json::array();
        unordered_set<string> reqSkillSet(metaIt->second.requiredSkills.begin(), metaIt->second.requiredSkills.end());

        for (size_t i = 0; i < ranked.size(); i++) {
            json item;
            item["rank"] = i + 1;
            item["studentId"] = ranked[i].studentId;
            item["name"] = ranked[i].studentName;
            item["score"] = ranked[i].score;
            item["scorePercent"] = (int)(ranked[i].score * 100 + 0.5);

            Student *matchedStudent = store.findStudentById(ranked[i].studentId);
            vector<string> matchedSkills;
            if (matchedStudent) {
                item["student"] = studentToJson(*matchedStudent);
                item["department"] = matchedStudent->getDepartment();
                item["year"] = matchedStudent->getYear();
                for (const auto &sk : matchedStudent->getSkills()) {
                    if (reqSkillSet.count(sk)) {
                        matchedSkills.push_back(sk);
                    }
                }
            }
            item["matchedSkills"] = matchedSkills;
            arr.push_back(item);
        }

        json data;
        data["mode"] = mode;
        data["k"] = k;
        data["postId"] = postId;
        data["projectTitle"] = metaIt->second.projectTitle;
        data["requiredSkills"] = metaIt->second.requiredSkills;
        data["results"] = arr;
        sendJsonOk(res, data);
    });

    // 19. POST /api/project-posts/:postId/create-group
    svr.Post(R"(/api/project-posts/(\d+)/create-group)", [](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        int postId = stoi(req.matches[1]);
        auto metaIt = projectPostMetaMap.find(postId);
        if (metaIt == projectPostMetaMap.end()) {
            return sendJsonError(res, "ProjectPost not found", 404);
        }

        try {
            auto body = json::parse(req.body);
            if (!body.contains("leaderId") || !body.contains("groupName")) {
                return sendJsonError(res, "Missing leaderId or groupName");
            }
            int leaderId = body["leaderId"].get<int>();
            string gName = body["groupName"].get<string>();

            if (gName.empty()) return sendJsonError(res, "Group name cannot be empty");
            if (!store.findStudentById(leaderId)) return sendJsonError(res, "Leader not found", 404);

            int gId = nextGroupId++;
            ProjectGroup pg(gId, gName, "Team for: " + metaIt->second.projectTitle, leaderId, postId);

            if (body.contains("memberIds") && body["memberIds"].is_array()) {
                vector<int> members = body["memberIds"].get<vector<int>>();
                for (int m : members) {
                    if (m != leaderId && store.findStudentById(m)) {
                        pg.addMember(m);
                    }
                }
            }

            store.addGroup(pg);
            projectGroupMetaMap[gId] = { leaderId, postId };
            sendJsonOk(res, groupToJson(pg));
        } catch (const exception &e) {
            sendJsonError(res, string("Invalid request: ") + e.what());
        }
    });

    // 20. POST /api/demo/load
    svr.Post("/api/demo/load", [](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(serverMutex);
        loadDemoData();
        json data;
        data["message"] = "Demo data loaded successfully (8 students, posts, connections, project group)";
        data["studentCount"] = store.getAllStudents().size();
        data["groupCount"] = store.getAllGroups().size();
        data["postCount"] = store.getAllPosts().size();
        sendJsonOk(res, data);
    });

    // Serve static files from frontend/ directory
    svr.set_mount_point("/", "./frontend");

    cout << "============================================" << endl;
    cout << "  CAMPUS-CONNECT HTTP API Server Starting  " << endl;
    cout << "============================================" << endl;
    cout << "Server listening on http://localhost:8080" << endl;
    cout << "Serving frontend from ./frontend" << endl;
    cout << "Press Ctrl+C to stop." << endl;

    if (!svr.listen("0.0.0.0", 8080)) {
        cerr << "Failed to start server on port 8080. Port may already be in use." << endl;
        return 1;
    }

    return 0;
}
