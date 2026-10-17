/* =====================================================================
   1. SAMPLE DATA  (will come from the C++ DataStore later)
   ===================================================================== */
const ALLOWED_DOMAINS = ["geu.ac.in", "gehu.ac.in"]; // confirm your college's real email domains

const db = {
  students: [
    {id:1, name:"Ananya Rawat",   email:"ananya@geu.ac.in", branch:"CSE",      year:2, skills:["C++","DSA","Web Development"], interests:["Hackathons","Open Source"]},
    {id:2, name:"Rohan Negi",     email:"rohan@geu.ac.in",  branch:"CSE",      year:2, skills:["C++","DSA","Python"], interests:["Competitive Programming","Hackathons"]},
    {id:3, name:"Priya Bisht",    email:"priya@geu.ac.in",  branch:"CSE (AI)", year:3, skills:["Python","Machine Learning","Data Analysis"], interests:["Research","Open Source"]},
    {id:4, name:"Karan Thapa",    email:"karan@geu.ac.in",  branch:"IT",       year:2, skills:["JavaScript","React","Web Development"], interests:["UI Design","Hackathons"]},
    {id:5, name:"Sneha Joshi",    email:"sneha@geu.ac.in",  branch:"ECE",      year:3, skills:["IoT","Arduino","C"], interests:["Robotics","Research"]},
    {id:6, name:"Arjun Chauhan",  email:"arjun@geu.ac.in",  branch:"CSE",      year:1, skills:["Python","C++"], interests:["Competitive Programming","Gaming"]},
    {id:7, name:"Meera Pant",     email:"meera@geu.ac.in",  branch:"CSE",      year:3, skills:["Figma","UI Design","JavaScript"], interests:["Design","Photography"]},
    {id:8, name:"Vikram Rana",    email:"vikram@geu.ac.in", branch:"CSE (DS)", year:2, skills:["SQL","Python","Data Analysis"], interests:["Finance","Research"]},
    {id:9, name:"Ishita Kandpal", email:"ishita@geu.ac.in", branch:"CSE",      year:2, skills:["Java","DSA","Spring Boot"], interests:["Open Source","Competitive Programming"]},
    {id:10,name:"Aman Bhatt",     email:"aman@geu.ac.in",   branch:"ME",       year:3, skills:["CAD","Python","Arduino"], interests:["Robotics","Hackathons"]},
    {id:11,name:"Diya Sharma",    email:"diya@geu.ac.in",   branch:"CSE",      year:1, skills:["HTML","CSS","JavaScript"], interests:["Web Development","Design"]},
    {id:12,name:"Nikhil Rautela", email:"nikhil@geu.ac.in", branch:"CSE (AI)", year:3, skills:["Machine Learning","Python","Deep Learning"], interests:["Research","Hackathons"]}
  ],
  connections: [[1,2],[1,4],[2,6],[2,9],[4,7],[4,11],[3,12],[3,8],[5,10],[9,6],[7,11],[12,10]],
  posts: [
    {id:1, authorId:6, type:"request", title:"Stuck on segment trees", body:"Range updates keep giving wrong answers. Can someone explain lazy propagation?", tags:["DSA","C++"]},
    {id:2, authorId:3, type:"offer",   title:"Reviewing ML project reports", body:"Happy to review evaluation sections and check for data leakage.", tags:["Machine Learning","Python"]},
    {id:3, authorId:4, type:"project", title:"Backend developer for a hackathon team", body:"Frontend and design are covered. Need someone comfortable with APIs.", tags:["Web Development","Hackathons"]},
    {id:4, authorId:5, type:"request", title:"Arduino serial data is garbled", body:"Readings look fine in the IDE monitor but break when read from Python.", tags:["Arduino","C"]},
    {id:5, authorId:7, type:"offer",   title:"Figma basics, two sessions a week", body:"Short sessions for anyone building a project interface.", tags:["Figma","UI Design"]}
  ]
};
const nextId = {student:13, post:6};

// DEMO ONLY: real passwords must be hashed and checked on the C++ server, never stored in the browser.
const demoPasswords = new Map(db.students.map(s => [s.email, "campus123"]));

// Emails that finished OTP verification (sample students are pre-verified)
const verifiedEmails = new Set(db.students.map(s => s.email));

// Pending codes: email -> {code, expires, attempts, sentAt}
// DEMO ONLY: the real server stores a hash of the code, not the code itself.
const pendingCodes = new Map();
const OTP_TTL_MS = 10 * 60 * 1000;      // code valid for 10 minutes
const OTP_MAX_ATTEMPTS = 5;
const RESEND_COOLDOWN_MS = 60 * 1000;   // 60 s between resends

function generateCode(){
  // crypto.getRandomValues is secure randomness; Math.random() is predictable
  const n = crypto.getRandomValues(new Uint32Array(1))[0] % 1000000;
  return String(n).padStart(6, "0");
}

/* =====================================================================
   2. LOGIC STAND-INS  (real versions are in C++:
      Graph::bfs, JaccardMatching, Trie search, Top-K heap)
   ===================================================================== */
const key = s => String(s).trim().toLowerCase();
const esc = s => String(s).replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
const parseList = str => [...new Map(String(str).split(",").map(x => x.trim()).filter(Boolean).map(x => [key(x), x])).values()];

function findStudent(id){
  const s = db.students.find(x => x.id === id);
  if (!s) throw new Error(`No student with ID ${id}.`);
  return s;
}
// What other users are allowed to see (no email)
const publicView = s => ({id:s.id, name:s.name, branch:s.branch, year:s.year, skills:[...(s.skills || [])], interests:[...(s.interests || [])]});

function neighbours(id){
  const out = [];
  for (const [a, b] of db.connections){ if (a === id) out.push(b); else if (b === id) out.push(a); }
  return out;
}

// BFS with levels: 1 = direct connection, 2 = "people you may know"
function bfsLevels(start){
  const dist = new Map([[start, 0]]), queue = [start];
  for (let head = 0; head < queue.length; head++){
    const cur = queue[head];
    for (const n of neighbours(cur)){
      if (!dist.has(n)){ dist.set(n, dist.get(cur) + 1); queue.push(n); }
    }
  }
  return dist;
}

// Jaccard similarity = |A ∩ B| / |A ∪ B| over skills + interests
function jaccard(a, b){
  const label = new Map();
  const toSet = s => new Set([...(s.skills || []), ...(s.interests || [])].map(x => { const k = key(x); if (!label.has(k)) label.set(k, x); return k; }));
  const A = toSet(a), B = toSet(b);
  const shared = [...A].filter(k => B.has(k));
  const union = new Set([...A, ...B]);
  return {
    score: union.size ? shared.length / union.size : 0,
    shared: shared.map(k => label.get(k)),
    onlyThem: [...B].filter(k => !A.has(k)).map(k => label.get(k)),
    unionSize: union.size
  };
}

// Prefix search on any word, like a Trie lookup
const matchesQuery = (text, q) => { const t = key(text), k = key(q); return k && (t.startsWith(k) || t.split(/\s+/).some(w => w.startsWith(k))); };

/* =====================================================================
   3. API LAYER — the ONLY part that changes when the C++ server is ready.
      Each function returns the JSON its endpoint will return.
   ===================================================================== */
const api = {
  // POST /api/login   body: {email, password}
  async login(email, password){
    const e = key(email);
    const s = db.students.find(x => x.email === e);
    if (!s || demoPasswords.get(e) !== password) throw new Error("Email or password is incorrect.");
    if (!verifiedEmails.has(e)){
      const err = new Error("Verify your email before logging in.");
      err.needsVerify = true;
      throw err;
    }
    return publicView(s);
  },

  // POST /api/otp/send   body: {email}   (the real server emails the code)
  async sendCode(email){
    const e = key(email);
    if (!db.students.some(s => s.email === e)) throw new Error("No account with this email.");
    if (verifiedEmails.has(e)) throw new Error("This email is already verified. Log in instead.");
    const old = pendingCodes.get(e);
    if (old && Date.now() - old.sentAt < RESEND_COOLDOWN_MS){
      const wait = Math.ceil((RESEND_COOLDOWN_MS - (Date.now() - old.sentAt)) / 1000);
      throw new Error(`Please wait ${wait} seconds before requesting a new code.`);
    }
    const code = generateCode();
    pendingCodes.set(e, {code, expires: Date.now() + OTP_TTL_MS, attempts: 0, sentAt: Date.now()});
    return {sent: true, demoCode: code}; // demoCode exists ONLY in the prototype
  },

  // POST /api/otp/verify   body: {email, code}
  async verifyCode(email, code){
    const e = key(email);
    const entry = pendingCodes.get(e);
    if (!entry) throw new Error("No active code. Request a new one.");
    if (Date.now() > entry.expires){ pendingCodes.delete(e); throw new Error("This code has expired. Request a new one."); }
    if (entry.attempts >= OTP_MAX_ATTEMPTS){ pendingCodes.delete(e); throw new Error("Too many wrong attempts. Request a new code."); }
    if (String(code).trim() !== entry.code){
      entry.attempts++;
      const left = OTP_MAX_ATTEMPTS - entry.attempts;
      throw new Error(left > 0 ? `Wrong code. ${left} attempt${left === 1 ? "" : "s"} left.` : "Too many wrong attempts. Request a new code.");
    }
    pendingCodes.delete(e);   // a code works only once
    verifiedEmails.add(e);
    return publicView(db.students.find(s => s.email === e));
  },

  // POST /api/students   body: {name, email, password, branch, year, skills, interests}
  async createStudent(input){
    const name = input.name.trim();
    if (!name) throw new Error("Enter your name.");
    const email = key(input.email);
    if (!ALLOWED_DOMAINS.includes(email.split("@")[1] || ""))
      throw new Error(`Use your college email ending in ${ALLOWED_DOMAINS.map(d => "@" + d).join(" or ")}.`);
    if (db.students.some(s => s.email === email)) throw new Error("This email is already registered. Log in instead.");
    if ((input.password || "").length < 8) throw new Error("Password must be at least 8 characters.");
    const s = {id:nextId.student, name, email, branch:input.branch.trim() || "Not given", year:Number(input.year) || 1,
               skills:parseList(input.skills), interests:parseList(input.interests)};
    if (!s.skills.length && !s.interests.length) throw new Error("Add at least one skill or interest so others can find you.");
    nextId.student++;
    db.students.push(s);
    demoPasswords.set(email, input.password);
    // Account exists but stays unverified until the OTP is entered
    const {demoCode} = await api.sendCode(email);
    return {student: publicView(s), demoCode};
  },

  // GET /api/students
  async listStudents(){ return db.students.map(publicView); },

  // GET /api/students/:id
  async getStudent(id){ return publicView(findStudent(id)); },

  // GET /api/search?q=python
  async search(q){
    if (!key(q)) return [];
    return db.students.filter(s => [s.name, ...(s.skills || []), ...(s.interests || [])].some(x => matchesQuery(x, q))).map(publicView);
  },

  // GET /api/match/:id?k=5
  async getMatches(id, k = 5){
    const me = findStudent(id);
    return db.students.filter(s => s.id !== id)
      .map(s => ({student:publicView(s), ...jaccard(me, s)}))
      .filter(m => m.score > 0)
      .sort((a, b) => b.score - a.score || a.student.name.localeCompare(b.student.name))
      .slice(0, k);
  },

  // GET /api/connections/:id
  async getConnections(id){ findStudent(id); return neighbours(id).map(n => publicView(findStudent(n))); },

  // GET /api/suggestions/:id
  async getSuggestions(id){
    findStudent(id);
    const mine = new Set(neighbours(id)), out = [];
    for (const [sid, d] of bfsLevels(id)){
      if (d === 2) out.push({student:publicView(findStudent(sid)), mutual:neighbours(sid).filter(n => mine.has(n)).map(n => findStudent(n).name)});
    }
    return out.sort((a, b) => b.mutual.length - a.mutual.length);
  },

  // POST /api/connections   body: {from, to}
  async connect(from, to){
    if (from === to) throw new Error("You can't connect with yourself.");
    findStudent(from); findStudent(to);
    if (neighbours(from).includes(to)) throw new Error("You're already connected.");
    db.connections.push([from, to]);
    return {ok:true};
  },

  // GET /api/posts?type=request
  async listPosts(type){
    return db.posts.filter(p => !type || p.type === type).sort((a, b) => b.id - a.id)
      .map(p => ({...p, author:findStudent(p.authorId).name}));
  },

  // POST /api/posts   body: {authorId, type, title, body, tags}
  async createPost(input){
    if (!["request","offer","project"].includes(input.type)) throw new Error("Choose a post type.");
    const title = input.title.trim();
    if (!title) throw new Error("Give your post a title.");
    findStudent(input.authorId);
    const p = {id:nextId.post++, authorId:input.authorId, type:input.type, title, body:input.body.trim(), tags:parseList(input.tags)};
    db.posts.push(p);
    return p;
  }
};

/* =====================================================================
   4. APP USER INTERFACE
   ===================================================================== */
// authMode: "landing" | "login" | "register" | "verify"
const state = {me:null, tab:"discover", filter:"", query:"", authMode:"landing", pendingEmail:"", demoCode:"", prefillEmail:""};
const main = document.getElementById("view");
const KIND = {request:"Needs help", offer:"Offering help", project:"Project team"};

function toast(msg){
  const t = document.getElementById("toast");
  t.textContent = msg; t.classList.add("show");
  clearTimeout(toast.timer); toast.timer = setTimeout(() => t.classList.remove("show"), 2400);
}
const meta = s => `${esc(s.branch)}, year ${s.year}`;
const chipList = (items, isShared) => items.map(x => `<li class="chip${isShared(x) ? " shared" : ""}">${esc(x)}</li>`).join("");
function connectButton(id, connected){
  if (id === state.me) return `<button class="btn ghost" disabled>This is you</button>`;
  return connected.has(id) ? `<button class="btn ghost" disabled>Connected</button>`
                           : `<button class="btn" data-connect="${id}">Connect</button>`;
}
const connectedSet = async () => new Set((await api.getConnections(state.me)).map(s => s.id));
const backHome = `<p class="backhome"><button class="linkbtn" data-auth="landing">← Back to home</button></p>`;

/* ---------- Logged-out screens ---------- */
const authViews = {
  login(){
    return `<section class="auth">
      <h2>Log in</h2>
      <p class="lede">Find classmates by skill, get help, and build project teams.</p>
      <form id="login" class="card" novalidate>
        <div class="field"><label for="lemail">College email</label>
          <input id="lemail" name="email" type="email" autocomplete="username" placeholder="name@${ALLOWED_DOMAINS[0]}" value="${esc(state.prefillEmail)}"></div>
        <div class="field"><label for="lpass">Password</label>
          <input id="lpass" name="password" type="password" autocomplete="current-password"></div>
        <button class="btn full" type="submit">Log in</button>
        <p class="error" id="login-error"></p>
      </form>
      <p>New to Campus Connect? <button class="linkbtn" data-auth="register">Create an account</button></p>
      <p class="demo">Demo login: <strong>ananya@geu.ac.in</strong> with password <strong>campus123</strong>. Every sample student uses the same password.</p>
      ${backHome}
    </section>`;
  },
  register(){
    return `<section class="auth">
      <h2>Create your account</h2>
      <p class="lede">Your skills and interests decide who you get matched with.</p>
      <form id="register" class="card" novalidate>
        <div class="field"><label for="rname">Full name</label><input id="rname" name="name" autocomplete="name"></div>
        <div class="field"><label for="remail">College email</label>
          <input id="remail" name="email" type="email" autocomplete="username" placeholder="name@${ALLOWED_DOMAINS[0]}"></div>
        <div class="field"><label for="rpass">Password</label>
          <input id="rpass" name="password" type="password" autocomplete="new-password">
          <span class="hint">At least 8 characters.</span></div>
        <div class="row">
          <div class="field"><label for="rbranch">Branch</label><input id="rbranch" name="branch" placeholder="CSE"></div>
          <div class="field"><label for="ryear">Year</label>
            <select id="ryear" name="year"><option>1</option><option>2</option><option>3</option><option>4</option></select></div>
        </div>
        <div class="field"><label for="rskills">Skills</label><input id="rskills" name="skills" placeholder="C++, DSA, Figma"></div>
        <div class="field"><label for="rint">Interests</label><input id="rint" name="interests" placeholder="Hackathons, Research">
          <span class="hint">Separate with commas.</span></div>
        <button class="btn full" type="submit">Create account</button>
        <p class="error" id="reg-error"></p>
      </form>
      <p>Already registered? <button class="linkbtn" data-auth="login">Log in</button></p>
      ${backHome}
    </section>`;
  },
  verify(){
    return `<section class="auth">
      <h2>Check your email</h2>
      <p class="lede">We sent a 6-digit code to <strong>${esc(state.pendingEmail)}</strong>. It expires in 10 minutes.</p>
      <form id="verify" class="card" novalidate>
        <div class="field"><label for="otp">Verification code</label>
          <input id="otp" name="code" class="otp" inputmode="numeric" autocomplete="one-time-code" maxlength="6"></div>
        <button class="btn full" type="submit">Verify and continue</button>
        <p class="error" id="otp-error"></p>
      </form>
      <p>Didn't get it? <button class="linkbtn" data-resend>Send a new code</button></p>
      <p class="demo">Demo mode: your code is <strong>${esc(state.demoCode)}</strong>. In the real app it is only sent by email.</p>
      <p><button class="linkbtn" data-auth="login">Back to log in</button></p>
    </section>`;
  }
};

/* ---------- Logged-in screens ---------- */
const views = {
  async discover(){
    const [matches, connected] = await Promise.all([api.getMatches(state.me, 5), connectedSet()]);
    const cards = matches.map(m => {
      const pct = Math.round(m.score * 100);
      return `<article class="card">
        <div class="person"><div><p class="name">${esc(m.student.name)}</p><p class="meta">${meta(m.student)}</p></div>
          <div class="score">${pct}%<small>match</small></div></div>
        <div class="meterbar" aria-hidden="true"><span style="width:${pct}%"></span></div>
        <ul class="chips">${chipList(m.shared, () => true)}${chipList(m.onlyThem, () => false)}</ul>
        <p class="overlap">${m.shared.length} in common out of ${m.unionSize} skills and interests between you</p>
        <div class="actions">${connectButton(m.student.id, connected)}</div></article>`;
    }).join("");
    return `<h2>Students like you</h2>
      <p class="lede">Ranked by overlap: what you both listed, divided by everything either of you listed. Highlighted tags are shared.</p>
      ${cards || `<p class="empty">No overlaps yet. Add more skills or interests to your profile.</p>`}`;
  },

  async search(){
    return `<h2>Find a student</h2>
      <p class="lede">Search by name, skill, or interest. Results update as you type.</p>
      <div class="field"><label for="q">Search</label>
        <input id="q" type="search" value="${esc(state.query)}" placeholder="Try python, design, or a name" autocomplete="off"></div>
      <div id="results" aria-live="polite"></div>`;
  },

  async network(){
    const [conns, sugg, connected] = await Promise.all([api.getConnections(state.me), api.getSuggestions(state.me), connectedSet()]);
    const suggCards = sugg.map(s => `<article class="card">
        <p class="name">${esc(s.student.name)}</p><p class="meta">${meta(s.student)}</p>
        <p class="overlap">Also connected with ${s.mutual.map(esc).join(" and ")}</p>
        <div class="actions">${connectButton(s.student.id, connected)}</div></article>`).join("");
    const connCards = conns.map(s => `<article class="card"><p class="name">${esc(s.name)}</p><p class="meta">${meta(s)}</p>
        <ul class="chips">${chipList([...s.skills, ...s.interests], () => false)}</ul></article>`).join("");
    return `<h2>Your network</h2>
      <p class="lede">People you may know are connected to someone you're already connected to.</p>
      <h3 class="sub">People you may know</h3>
      ${suggCards || `<p class="empty">No suggestions yet. Connect with a few students from Matches first.</p>`}
      <h3 class="sub">Your connections (${conns.length})</h3>
      ${connCards || `<p class="empty">You haven't connected with anyone yet.</p>`}`;
  },

  async board(){
    const posts = await api.listPosts(state.filter);
    const filters = [["","All"],["request","Needs help"],["offer","Offering help"],["project","Project teams"]]
      .map(([v, l]) => `<button class="pill" data-filter="${v}" aria-pressed="${state.filter === v}">${l}</button>`).join("");
    const cards = posts.map(p => `<article class="card"><p class="kind">${KIND[p.type]}</p>
        <h3 class="post-title">${esc(p.title)}</h3>${p.body ? `<p class="post-body">${esc(p.body)}</p>` : ""}
        <ul class="chips">${chipList(p.tags, () => false)}</ul><p class="overlap">Posted by ${esc(p.author)}</p></article>`).join("");
    return `<h2>Help board</h2>
      <p class="lede">Ask for help, offer what you know, or find people for a project.</p>
      <details class="card"><summary>Write a post</summary>
        <form id="newpost" novalidate>
          <div class="field"><label for="ptype">Type</label>
            <select id="ptype" name="type"><option value="request">I need help</option><option value="offer">I can help</option><option value="project">Looking for teammates</option></select></div>
          <div class="field"><label for="ptitle">Title</label><input id="ptitle" name="title" maxlength="100"></div>
          <div class="field"><label for="pbody">Details</label><textarea id="pbody" name="body" maxlength="600"></textarea></div>
          <div class="field"><label for="ptags">Tags</label><input id="ptags" name="tags" placeholder="DSA, C++">
            <span class="hint">Separate tags with commas.</span></div>
          <button class="btn" type="submit">Publish post</button><p class="error" id="post-error"></p>
        </form></details>
      <div class="filters" role="group" aria-label="Filter posts">${filters}</div>
      ${cards || `<p class="empty">No posts of this type yet. Be the first to write one.</p>`}`;
  },

  async profile(){
    const me = await api.getStudent(state.me);
    return `<h2>${esc(me.name)}</h2><p class="lede">${meta(me)}</p>
      <article class="card"><p class="name">Skills</p><ul class="chips">${chipList(me.skills, () => false)}</ul>
        <p class="name" style="margin-top:16px">Interests</p><ul class="chips">${chipList(me.interests, () => false)}</ul></article>
      <p class="footnote">Sample data mode: changes reset when the page reloads.</p>`;
  }
};

async function renderResults(){
  const el = document.getElementById("results");
  if (!el) return;
  const q = state.query;
  if (!key(q)){ el.innerHTML = `<p class="empty">Type a skill like C++ to see who knows it.</p>`; return; }
  const [res, connected] = await Promise.all([api.search(q), connectedSet()]);
  el.innerHTML = res.length ? res.map(s => `<article class="card">
      <p class="name">${esc(s.name)}</p><p class="meta">${meta(s)}</p>
      <ul class="chips">${chipList([...s.skills, ...s.interests], x => matchesQuery(x, q))}</ul>
      <div class="actions">${connectButton(s.id, connected)}</div></article>`).join("")
    : `<p class="empty">No students match “${esc(q)}”. Try a shorter word.</p>`;
}

const landingEl = document.getElementById("landing");
const appHeader = document.getElementById("app-header");

async function render(){
  const loggedIn = state.me !== null;
  const onLanding = !loggedIn && state.authMode === "landing";
  // Landing and app share one page: show exactly one of them
  landingEl.hidden = !onLanding;
  appHeader.hidden = onLanding;
  main.hidden = onLanding;
  document.body.classList.toggle("lp-active", onLanding);
  document.getElementById("tabs").hidden = !loggedIn;
  document.getElementById("who").hidden = !loggedIn;
  try {
    if (onLanding){ await showLanding(); return; }
    if (!loggedIn){ main.innerHTML = authViews[state.authMode](); return; }
    document.getElementById("who-name").textContent = (await api.getStudent(state.me)).name;
    document.querySelectorAll(".tab").forEach(t => t.setAttribute("aria-selected", String(t.dataset.tab === state.tab)));
    main.innerHTML = await views[state.tab]();
    if (state.tab === "search") await renderResults();
  } catch (e) {
    main.hidden = false;
    main.innerHTML = `<p class="error">${esc(e.message)}</p>`;
  }
}

/* ---------- App events (attached once) ---------- */
document.addEventListener("click", async e => {
  const auth = e.target.closest("[data-auth]");
  if (auth){
    if (auth.tagName === "A") e.preventDefault();
    state.authMode = auth.dataset.auth;
    state.prefillEmail = auth.dataset.email || "";
    await render();
    window.scrollTo({top:0, behavior:"instant"});
    // Put the cursor where the user will type next
    if (!main.hidden) (state.prefillEmail ? main.querySelector("#lpass") : main.querySelector("input"))?.focus();
    return;
  }
  if (e.target.closest("[data-resend]")){
    try {
      const {demoCode} = await api.sendCode(state.pendingEmail);
      state.demoCode = demoCode; await render(); toast("New code sent");
    } catch (err) { toast(err.message); }
    return;
  }
  const tab = e.target.closest("[data-tab]");
  if (tab){ state.tab = tab.dataset.tab; render(); window.scrollTo(0, 0); return; }
  const f = e.target.closest("[data-filter]");
  if (f){ state.filter = f.dataset.filter; render(); return; }
  const c = e.target.closest("[data-connect]");
  if (c){
    try {
      const other = await api.getStudent(Number(c.dataset.connect));
      await api.connect(state.me, other.id);
      toast(`Connected with ${other.name}`);
      state.tab === "search" ? renderResults() : render();
    } catch (err) { toast(err.message); }
  }
});

document.getElementById("logout").addEventListener("click", async () => {
  Object.assign(state, {me:null, tab:"discover", filter:"", query:"", authMode:"landing", prefillEmail:""});
  await render();
  window.scrollTo({top:0, behavior:"instant"});
  toast("Logged out");
});

document.addEventListener("input", e => {
  if (e.target.id === "q"){ state.query = e.target.value; renderResults(); }
});

document.addEventListener("submit", async e => {
  e.preventDefault();
  const data = Object.fromEntries(new FormData(e.target));
  try {
    if (e.target.id === "login"){
      try {
        const s = await api.login(data.email, data.password);
        state.me = s.id; state.tab = "discover";
        await render(); toast(`Welcome back, ${s.name.split(" ")[0]}`);
      } catch (err) {
        if (!err.needsVerify) throw err;
        // Unverified account: send a fresh code and go to the verify screen
        state.pendingEmail = key(data.email);
        try { state.demoCode = (await api.sendCode(state.pendingEmail)).demoCode; } catch (cooldown) { /* keep earlier code */ }
        state.authMode = "verify"; await render();
        toast("Verify your email to continue");
      }
    }
    if (e.target.id === "register"){
      const {demoCode} = await api.createStudent(data);
      state.pendingEmail = key(data.email); state.demoCode = demoCode;
      state.authMode = "verify"; await render();
      toast(`Code sent to ${state.pendingEmail}`);
    }
    if (e.target.id === "verify"){
      const s = await api.verifyCode(state.pendingEmail, data.code);
      state.me = s.id; state.tab = "discover"; state.pendingEmail = ""; state.demoCode = "";
      await render(); toast(`Email verified. Welcome, ${s.name.split(" ")[0]}`);
    }
    if (e.target.id === "newpost"){
      await api.createPost({...data, authorId:state.me});
      state.filter = ""; await render(); toast("Post published");
    }
  } catch (err) {
    const box = e.target.querySelector(".error");
    if (box) box.textContent = err.message;
  }
});

/* =====================================================================
   5. LANDING PAGE (shown when logged out)
      Reuses the sample data (section 1) and the API layer (section 3).
      All listeners are attached ONCE to #landing; re-rendering only
      replaces its HTML, so nothing accumulates.
   ===================================================================== */
const lp = {selected:1, preview:null, skill:null, tab:"teammates", filter:"", observer:null, token:0};
const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");

// Hand-placed positions (SVG viewBox 560 x 400). Two friend groups that are not yet linked.
const NODE_POS = {1:[215,171],2:[125,106],9:[160,40],6:[52,144],4:[235,275],7:[140,338],11:[305,353],
                  3:[420,149],8:[505,86],12:[425,261],10:[384,345],5:[492,338]};
const SPARE_POS = [[330,225],[300,56],[46,270],[515,209]]; // for students who register later

function netStudents(){
  let spare = 0;
  return db.students.map(s => {
    const pos = NODE_POS[s.id] || SPARE_POS[spare++];
    return pos ? {s, x:pos[0], y:pos[1]} : null;
  }).filter(Boolean);
}
const initials = name => String(name || "?").split(/\s+/).filter(Boolean).map(w => w[0]).slice(0, 2).join("").toUpperCase();
const tagsOf = s => [...(s.skills || []), ...(s.interests || [])];
const studentsWith = tag => db.students.filter(s => tagsOf(s).some(t => key(t) === key(tag)));

// Most common skills/interests in the data, used for the "Explore by skill" chips
function topTags(limit){
  const count = new Map(), label = new Map();
  db.students.forEach(s => tagsOf(s).forEach(t => { const k = key(t); count.set(k, (count.get(k) || 0) + 1); if (!label.has(k)) label.set(k, t); }));
  return [...count].sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0])).slice(0, limit).map(([k]) => label.get(k));
}

// Best Jaccard match among students this person is NOT connected to yet
function bestNewMatch(id){
  const me = db.students.find(s => s.id === id);
  if (!me) return null;
  const known = new Set(neighbours(id));
  let best = null;
  for (const s of db.students){
    if (s.id === id || known.has(s.id)) continue;
    const m = jaccard(me, s);
    if (m.score > 0 && (!best || m.score > best.score)) best = {student:s, ...m};
  }
  return best;
}
const pct = x => Math.round(x * 100);

function logoMark(){
  return `<svg width="26" height="26" viewBox="0 0 26 26" aria-hidden="true">
    <path d="M6 18 L13 7 L20 17 Z" fill="none" stroke="#6FE3F5" stroke-width="1.6" stroke-linejoin="round"/>
    <circle cx="6" cy="18" r="3.4" fill="#0A0F1F" stroke="#6FE3F5" stroke-width="1.6"/>
    <circle cx="20" cy="17" r="3.4" fill="#F0CF65"/>
    <circle cx="13" cy="7" r="3.8" fill="#6FE3F5"/></svg>`;
}

function networkSVG(){
  const nodes = netStudents();
  const at = new Map(nodes.map(n => [n.s.id, n]));
  const edges = db.connections.filter(([a, b]) => at.has(a) && at.has(b)).map(([a, b]) =>
    `<line class="lp-edge" data-a="${a}" data-b="${b}" x1="${at.get(a).x}" y1="${at.get(a).y}" x2="${at.get(b).x}" y2="${at.get(b).y}"/>`).join("");
  const dots = nodes.map(({s, x, y}) => {
    const tags = tagsOf(s);
    const label = `${s.name}, ${s.branch || "branch not given"}, year ${s.year || "not given"}. ${tags.length ? "Skills and interests: " + tags.join(", ") : "No skills listed"}. ${neighbours(s.id).length} connections.`;
    const edge = x < 100 ? " is-edge-l" : x > 460 ? " is-edge-r" : ""; // keeps long names inside the drawing on phones
    return `<g class="lp-node${edge}" data-lp-node="${s.id}" transform="translate(${x} ${y})" tabindex="0" role="button" aria-pressed="false" aria-label="${esc(label)}">
      <circle class="lp-ring" r="25"/><circle class="lp-dot" r="19"/>
      <text class="lp-ini">${esc(initials(s.name))}</text>
      <text class="lp-name" y="34">${esc(s.name)}</text></g>`;
  }).join("");
  return `<svg class="lp-net" viewBox="0 0 560 400" role="group" aria-label="Campus network of sample students. Each student is a button.">
    <g>${edges}</g>
    <line class="lp-matchline" id="lp-matchline" x1="0" y1="0" x2="0" y2="0" visibility="hidden"/>
    <g class="lp-matchtag" id="lp-matchtag" visibility="hidden"><rect rx="9" height="20" width="92" x="-46" y="-10"/><text text-anchor="middle" dominant-baseline="central"></text></g>
    <g>${dots}</g></svg>`;
}

function landingHTML(){
  const chips = topTags(7).map(t => `<li><button class="lp-chip" data-lp-skill="${esc(t)}" aria-pressed="false">${esc(t)}</button></li>`).join("");
  const tabs = [["teammates","Find teammates"],["network","People you may know"],["help","Requests you could answer"]]
    .map(([v, l]) => `<button class="lp-tab" role="tab" data-lp-tab="${v}" aria-selected="false" aria-controls="lp-preview-grid">${l}</button>`).join("");
  const filters = [["","All"],["request","Needs help"],["offer","Offering help"],["project","Project teams"]]
    .map(([v, l]) => `<button class="lp-chip" data-lp-filter="${v}" aria-pressed="false">${l}</button>`).join("");
  return `
  <nav class="lp-nav" aria-label="Main">
    <div class="lp-wrap lp-nav__inner">
      <a class="lp-brand" href="#lp-top">${logoMark()}<span>Campus Connect</span></a>
      <button class="lp-menu-btn" type="button" data-lp-menu aria-expanded="false" aria-controls="lp-menu">Menu</button>
      <div class="lp-menu" id="lp-menu">
        <ul class="lp-links">
          <li><a href="#lp-discover" data-lp-close>Discover</a></li>
          <li><a href="#lp-how" data-lp-close>How it works</a></li>
          <li><a href="#lp-community" data-lp-close>Community</a></li>
        </ul>
        <div class="lp-actions">
          <button class="lp-btn lp-btn--ghost lp-btn--sm" data-auth="login">Sign in</button>
          <button class="lp-btn lp-btn--primary lp-btn--sm" data-auth="register">Get started</button>
        </div>
      </div>
    </div>
  </nav>

  <section class="lp-hero" id="lp-top" aria-labelledby="lp-title">
    <div class="lp-wrap lp-hero__grid">
      <div>
        <p class="lp-eyebrow"><i aria-hidden="true"></i>A student project for Graphic Era University</p>
        <h1 class="lp-title" id="lp-title">Find the people your next project is <span class="lp-mark">missing</span>.</h1>
        <p class="lp-lede">Campus Connect matches students by skills and interests, so finding a teammate, a study partner or quick help doesn't depend on who you already know.</p>
        <div class="lp-cta">
          <button class="lp-btn lp-btn--primary" data-auth="register">Get started</button>
          <a class="lp-btn lp-btn--ghost" href="#lp-discover">See it in action</a>
        </div>
        <div class="lp-skills">
          <p class="lp-skills__label" id="lp-skills-label">Explore by skill</p>
          <ul class="lp-chiprow" aria-labelledby="lp-skills-label">${chips}</ul>
        </div>
      </div>

      <div class="lp-netpanel">
        <div class="lp-netpanel__head"><span>Campus network: hover or tab to preview, click to select</span><span class="lp-badge">Sample data</span></div>
        ${networkSVG()}
        <div class="lp-card" id="lp-card"></div>
      </div>
    </div>
  </section>

  <section class="lp-section" id="lp-discover" aria-labelledby="lp-discover-title">
    <div class="lp-wrap" data-reveal>
      <div class="lp-section__head">
        <div>
          <p class="lp-kicker">Discover</p>
          <h2 class="lp-h2" id="lp-discover-title">See why someone is a match, not just that they are.</h2>
          <p class="lp-sub">Everything below is computed in your browser from sample data, using the same algorithms the C++ backend is being built around.</p>
        </div>
        <div class="lp-tabs" role="tablist" aria-label="Preview">${tabs}</div>
      </div>
      <p class="lp-viewer" id="lp-viewer"></p>
      <div class="lp-grid" id="lp-preview-grid" role="tabpanel" aria-live="polite"></div>
    </div>
  </section>

  <section class="lp-section" id="lp-how" aria-labelledby="lp-how-title">
    <div class="lp-wrap" data-reveal>
      <p class="lp-kicker">How it works</p>
      <h2 class="lp-h2" id="lp-how-title">Three steps from stranger to teammate.</h2>
      <ol class="lp-steps" style="margin-top:32px">
        <li class="lp-step"><span class="lp-step__n" aria-hidden="true">1</span>
          <h3>Create your profile</h3>
          <p>Sign up with your college email, confirm it with a one-time code, and list what you know and what you're into.</p>
          <p class="lp-tech"><b>Under the hood:</b> email verification by one-time code (simulated in this prototype)</p></li>
        <li class="lp-step"><span class="lp-step__n" aria-hidden="true">2</span>
          <h3>Discover compatible students</h3>
          <p>Matches are ranked by how much your skills and interests overlap, and every match shows exactly what you share.</p>
          <p class="lp-tech"><b>Under the hood:</b> Jaccard similarity, ranked for the top results</p></li>
        <li class="lp-step"><span class="lp-step__n" aria-hidden="true">3</span>
          <h3>Connect and collaborate</h3>
          <p>Connect, meet friends of friends, and post help requests, offers or project calls on the Help board.</p>
          <p class="lp-tech"><b>Under the hood:</b> a connection graph explored with breadth-first search</p></li>
      </ol>
      <p class="lp-note">In this prototype the algorithms run in JavaScript. Their C++ versions are being built as the project backend.</p>
    </div>
  </section>

  <section class="lp-section" id="lp-community" aria-labelledby="lp-community-title">
    <div class="lp-wrap" data-reveal>
      <div class="lp-section__head">
        <div>
          <p class="lp-kicker">Community</p>
          <h2 class="lp-h2" id="lp-community-title">Ask for help. Offer what you know.</h2>
          <p class="lp-sub">The Help board turns requests lost in group chats into posts the right people can find. These are sample posts.</p>
        </div>
        <div class="lp-filters" role="group" aria-label="Filter sample posts">${filters}</div>
      </div>
      <div class="lp-grid" id="lp-posts" aria-live="polite"></div>
      <p class="lp-more">Posting and the full board are inside the app. <button class="lp-link" data-auth="login">Sign in to open the Help board</button></p>
    </div>
  </section>

  <section class="lp-final" aria-labelledby="lp-final-title">
    <div class="lp-wrap" data-reveal>
      <div class="lp-final__box">
        <h2 class="lp-h2" id="lp-final-title">Your next teammate might be one building away.</h2>
        <p class="lp-sub">Create a profile in a minute, or explore the prototype with a sample account.</p>
        <div class="lp-cta">
          <button class="lp-btn lp-btn--primary" data-auth="register">Create your account</button>
          <button class="lp-btn lp-btn--ghost" data-auth="login" data-email="ananya@geu.ac.in">Try the demo account</button>
        </div>
        <p class="lp-demo">Demo login: <code>ananya@geu.ac.in</code> / <code>campus123</code></p>
      </div>
    </div>
  </section>

  <footer class="lp-foot"><div class="lp-wrap">
    <p>Campus Connect is a PBL Semester 3 project by Veeru Yadav, Aditya Bagri, Vivekanand and Tanvay Jain at Graphic Era University. It is a prototype running on sample data, not an official university service.</p>
  </div></footer>`;
}

/* ---------- Landing: partial updates ---------- */
function profileCard(id, best){
  const s = db.students.find(x => x.id === id);
  if (!s) return `<p class="lp-card__meta">Select a student to preview them.</p>`;
  const sharedKeys = new Set((best ? best.shared : []).map(key));
  const tags = tagsOf(s).map(t => `<li class="lp-tag${sharedKeys.has(key(t)) ? " is-shared" : ""}">${esc(t)}</li>`).join("");
  const friends = neighbours(id).map(n => db.students.find(x => x.id === n)).filter(Boolean)
    .map(f => `<button class="lp-namebtn" data-lp-select="${f.id}">${esc(f.name)}</button>`).join(" ");
  const match = best
    ? `Best new match: <button class="lp-namebtn" data-lp-select="${best.student.id}">${esc(best.student.name)}</button> <strong>${pct(best.score)}%</strong>, sharing ${best.shared.map(esc).join(" and ")}`
    : "No new matches yet";
  return `<div class="lp-card__top">
      <div><p class="lp-card__name">${esc(s.name)}</p><p class="lp-card__meta">${esc(s.branch || "Branch not given")}, year ${esc(s.year || "?")}</p></div>
      <button class="lp-btn lp-btn--ghost lp-btn--sm" data-auth="login" data-email="${esc(s.email)}">Sign in as ${esc(s.name.split(" ")[0])}</button>
    </div>
    <ul class="lp-tags">${tags}</ul>
    <p class="lp-card__facts">Connected with: ${friends || "nobody yet"}</p>
    <p class="lp-card__facts">${match}</p>`;
}

function skillCard(tag){
  const people = studentsWith(tag);
  return `<div class="lp-card__top">
      <div><p class="lp-card__name">${people.length} student${people.length === 1 ? "" : "s"} list ${esc(tag)}</p>
      <p class="lp-card__meta">Highlighted in the network above. Pick one to see their profile.</p></div>
      <button class="lp-btn lp-btn--ghost lp-btn--sm" data-lp-clear>Clear</button>
    </div>
    <p class="lp-card__facts">${people.map(p => `<button class="lp-namebtn" data-lp-select="${p.id}">${esc(p.name)}</button>`).join(" ")}</p>`;
}

function updateNetwork(){
  const svg = landingEl.querySelector(".lp-net");
  if (!svg) return;
  const shownId = lp.preview ?? lp.selected;
  const skillIds = lp.skill ? new Set(studentsWith(lp.skill).map(s => s.id)) : null;
  const near = new Set(neighbours(shownId));
  const best = skillIds ? null : bestNewMatch(shownId);
  const bestId = best ? best.student.id : null;

  svg.querySelectorAll(".lp-node").forEach(n => {
    const id = Number(n.dataset.lpNode);
    n.classList.toggle("is-active", !skillIds && id === shownId);
    n.classList.toggle("is-near", !skillIds && near.has(id));
    n.classList.toggle("is-match", !skillIds && id === bestId);
    n.classList.toggle("is-skill", !!skillIds && skillIds.has(id));
    n.classList.toggle("is-dim", skillIds ? !skillIds.has(id) : !(id === shownId || near.has(id) || id === bestId));
    n.setAttribute("aria-pressed", String(!skillIds && id === lp.selected));
  });
  svg.querySelectorAll(".lp-edge").forEach(edge => {
    const a = Number(edge.dataset.a), b = Number(edge.dataset.b);
    const lit = skillIds ? (skillIds.has(a) && skillIds.has(b)) : (a === shownId || b === shownId);
    edge.classList.toggle("is-lit", lit);
    edge.classList.toggle("is-dim", !lit);
  });

  // Dashed yellow line = best match you're not connected to (Jaccard), versus solid lines = connections (graph)
  const line = svg.querySelector("#lp-matchline"), tagEl = svg.querySelector("#lp-matchtag");
  const pos = new Map(netStudents().map(n => [n.s.id, n]));
  if (best && pos.has(shownId) && pos.has(bestId)){
    const p1 = pos.get(shownId), p2 = pos.get(bestId);
    line.setAttribute("x1", p1.x); line.setAttribute("y1", p1.y); line.setAttribute("x2", p2.x); line.setAttribute("y2", p2.y);
    // Place the "% match" label where it covers the fewest student circles/names
    const blocks = [...pos.values()].flatMap(o => {
      const w = String(o.s.name).length * 7.4 + 10;
      return [[o.x - 22, o.y - 22, 44, 44], [o.x - w / 2, o.y + 24, w, 16]];
    });
    const overlap = (x, y) => blocks.reduce((sum, [bx, by, bw, bh]) =>
      sum + Math.max(0, Math.min(x + 54, bx + bw) - Math.max(x - 54, bx)) * Math.max(0, Math.min(y + 16, by + bh) - Math.max(y - 16, by)), 0);
    const len = Math.hypot(p2.x - p1.x, p2.y - p1.y) || 1, nx = -(p2.y - p1.y) / len, ny = (p2.x - p1.x) / len;
    let spot = null, bestCost = Infinity;
    for (const t of [0.5, 0.4, 0.6, 0.3, 0.7]) for (const off of [0, 22, -22, 36, -36]){
      const x = p1.x + (p2.x - p1.x) * t + nx * off, y = p1.y + (p2.y - p1.y) * t + ny * off;
      const cost = overlap(x, y) + Math.abs(t - 0.5) * 40 + Math.abs(off) * 2
                 + (x < 48 || x > 512 || y < 12 || y > 388 ? 1e6 : 0); // stay inside the drawing
      if (cost < bestCost){ bestCost = cost; spot = [x, y]; }
    }
    tagEl.setAttribute("transform", `translate(${spot[0]} ${spot[1]})`);
    tagEl.querySelector("text").textContent = `${pct(best.score)}% match`;
    line.setAttribute("visibility", "visible"); tagEl.setAttribute("visibility", "visible");
    const tw = tagEl.querySelector("text").getComputedTextLength(), rect = tagEl.querySelector("rect");
    rect.setAttribute("width", tw + 18); rect.setAttribute("x", -(tw + 18) / 2);
    rect.setAttribute("height", tw > 80 ? 26 : 20); rect.setAttribute("y", tw > 80 ? -13 : -10);
    line.setAttribute("visibility", "visible"); tagEl.setAttribute("visibility", "visible");
  } else {
    line.setAttribute("visibility", "hidden"); tagEl.setAttribute("visibility", "hidden");
  }

  landingEl.querySelector("#lp-card").innerHTML = skillIds ? skillCard(lp.skill) : profileCard(shownId, best);
  landingEl.querySelectorAll("[data-lp-skill]").forEach(c => c.setAttribute("aria-pressed", String(!!lp.skill && key(c.dataset.lpSkill) === key(lp.skill))));
}

async function updatePreview(){
  const grid = landingEl.querySelector("#lp-preview-grid");
  if (!grid) return;
  const token = ++lp.token; // ignore results from older, slower calls
  landingEl.querySelectorAll("[data-lp-tab]").forEach(t => t.setAttribute("aria-selected", String(t.dataset.lpTab === lp.tab)));
  const me = db.students.find(s => s.id === lp.selected);
  landingEl.querySelector("#lp-viewer").innerHTML = me
    ? `Showing results for <strong>${esc(me.name)}</strong>. Select another student in the network above to change this.`
    : "";
  let html = "";
  try {
    if (lp.tab === "teammates"){
      const matches = await api.getMatches(lp.selected, 3);
      html = matches.map(m => `<article class="lp-pcard">
          <div class="lp-pcard__top"><div><p class="lp-pcard__name">${esc(m.student.name)}</p><p class="lp-pcard__meta">${esc(m.student.branch)}, year ${m.student.year}</p></div>
          <p class="lp-score">${pct(m.score)}%<small>match</small></p></div>
          <div class="lp-meter" aria-hidden="true"><span style="width:${pct(m.score)}%"></span></div>
          <ul class="lp-tags">${m.shared.map(t => `<li class="lp-tag is-shared">${esc(t)}</li>`).join("")}${m.onlyThem.map(t => `<li class="lp-tag">${esc(t)}</li>`).join("")}</ul>
          <p class="lp-pcard__foot">${m.shared.length} shared out of ${m.unionSize} skills and interests</p></article>`).join("");
      if (!html) html = `<p class="lp-empty">No overlapping skills with anyone yet.</p>`;
    } else if (lp.tab === "network"){
      const sugg = (await api.getSuggestions(lp.selected)).slice(0, 3);
      html = sugg.map(s => `<article class="lp-pcard">
          <p class="lp-pcard__name">${esc(s.student.name)}</p><p class="lp-pcard__meta">${esc(s.student.branch)}, year ${s.student.year}</p>
          <ul class="lp-tags">${tagsOf(s.student).slice(0, 4).map(t => `<li class="lp-tag">${esc(t)}</li>`).join("")}</ul>
          <p class="lp-pcard__foot">Two steps away, through ${s.mutual.map(esc).join(" and ")}</p></article>`).join("");
      if (!html) html = `<p class="lp-empty">No friends of friends to suggest yet.</p>`;
    } else {
      const mine = new Set(tagsOf(me || {}).map(key));
      const posts = (await api.listPosts("request")).filter(p => p.authorId !== lp.selected && p.tags.some(t => mine.has(key(t)))).slice(0, 3);
      html = posts.map(p => `<article class="lp-pcard">
          <p class="lp-kind lp-kind--request">Needs help</p><p class="lp-pcard__name">${esc(p.title)}</p>
          <p class="lp-pcard__body">${esc(p.body)}</p>
          <ul class="lp-tags">${p.tags.map(t => `<li class="lp-tag${mine.has(key(t)) ? " is-shared" : ""}">${esc(t)}</li>`).join("")}</ul>
          <p class="lp-pcard__foot">Posted by ${esc(p.author)}</p></article>`).join("");
      if (!html) html = `<p class="lp-empty">No open requests match ${esc(me ? me.name.split(" ")[0] : "this student")}'s skills right now.</p>`;
    }
  } catch (err) { html = `<p class="lp-empty">${esc(err.message)}</p>`; }
  if (token === lp.token) grid.innerHTML = html;
}

async function updateCommunity(){
  const grid = landingEl.querySelector("#lp-posts");
  if (!grid) return;
  landingEl.querySelectorAll("[data-lp-filter]").forEach(f => f.setAttribute("aria-pressed", String(f.dataset.lpFilter === lp.filter)));
  const posts = (await api.listPosts(lp.filter)).slice(0, 3);
  grid.innerHTML = posts.map(p => `<article class="lp-pcard">
      <p class="lp-kind lp-kind--${p.type}">${KIND[p.type]}</p><p class="lp-pcard__name">${esc(p.title)}</p>
      ${p.body ? `<p class="lp-pcard__body">${esc(p.body)}</p>` : ""}
      <ul class="lp-tags">${p.tags.map(t => `<li class="lp-tag">${esc(t)}</li>`).join("")}</ul>
      <p class="lp-pcard__foot">Posted by ${esc(p.author)}</p></article>`).join("") || `<p class="lp-empty">No sample posts of this type.</p>`;
}

function setupReveal(){
  if (lp.observer){ lp.observer.disconnect(); lp.observer = null; }
  landingEl.classList.remove("is-reveal");
  if (reduceMotion.matches || !("IntersectionObserver" in window)) return;
  landingEl.classList.add("is-reveal");
  lp.observer = new IntersectionObserver(entries => entries.forEach(en => {
    if (en.isIntersecting){ en.target.classList.add("is-in"); lp.observer.unobserve(en.target); }
  }), {rootMargin:"0px 0px -10% 0px"});
  landingEl.querySelectorAll("[data-reveal]").forEach(el => lp.observer.observe(el));
}

async function showLanding(){
  if (!db.students.some(s => s.id === lp.selected)) lp.selected = db.students[0]?.id ?? null;
  lp.preview = null;
  landingEl.innerHTML = landingHTML();
  updateNetwork();
  setupReveal();
  await Promise.all([updatePreview(), updateCommunity()]);
}

function selectStudent(id){
  lp.selected = id; lp.preview = null; lp.skill = null;
  updateNetwork(); updatePreview();
}
function setMenu(open){
  const nav = landingEl.querySelector(".lp-nav");
  if (!nav) return;
  nav.dataset.open = String(open);
  nav.querySelector("[data-lp-menu]").setAttribute("aria-expanded", String(open));
}

/* ---------- Landing events (attached ONCE) ---------- */
landingEl.addEventListener("click", e => {
  const t = e.target;
  const node = t.closest("[data-lp-node]");
  if (node){ selectStudent(Number(node.dataset.lpNode)); return; }
  const pick = t.closest("[data-lp-select]");
  if (pick){ selectStudent(Number(pick.dataset.lpSelect)); return; }
  const skill = t.closest("[data-lp-skill]");
  if (skill){
    const tag = skill.dataset.lpSkill;
    lp.skill = (lp.skill && key(lp.skill) === key(tag)) ? null : tag;
    lp.preview = null; updateNetwork(); return;
  }
  if (t.closest("[data-lp-clear]")){ lp.skill = null; updateNetwork(); return; }
  const tab = t.closest("[data-lp-tab]");
  if (tab){ lp.tab = tab.dataset.lpTab; updatePreview(); return; }
  const f = t.closest("[data-lp-filter]");
  if (f){ lp.filter = f.dataset.lpFilter; updateCommunity(); return; }
  if (t.closest("[data-lp-menu]")){ setMenu(landingEl.querySelector(".lp-nav").dataset.open !== "true"); return; }
  if (t.closest("[data-lp-close]")) setMenu(false);
});

// Hover and keyboard focus give a temporary preview; leaving restores the selected student
landingEl.addEventListener("mouseover", e => {
  const node = e.target.closest("[data-lp-node]");
  if (node && !lp.skill){ const id = Number(node.dataset.lpNode); if (lp.preview !== id){ lp.preview = id; updateNetwork(); } }
});
landingEl.addEventListener("mouseout", e => {
  const node = e.target.closest("[data-lp-node]");
  if (node && !node.contains(e.relatedTarget) && lp.preview !== null){ lp.preview = null; updateNetwork(); }
});
landingEl.addEventListener("focusin", e => {
  const node = e.target.closest("[data-lp-node]");
  if (node && !lp.skill){ lp.preview = Number(node.dataset.lpNode); updateNetwork(); }
});
landingEl.addEventListener("focusout", e => {
  if (e.target.closest("[data-lp-node]") && lp.preview !== null){ lp.preview = null; updateNetwork(); }
});
landingEl.addEventListener("keydown", e => {
  const node = e.target.closest("[data-lp-node]");
  if (node && (e.key === "Enter" || e.key === " ")){ e.preventDefault(); selectStudent(Number(node.dataset.lpNode)); }
  if (e.key === "Escape") setMenu(false);
});

render();
