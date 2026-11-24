/* ==========================================================================
   Campus Connect - Interactive JavaScript Application
   Connects to C++ httplib backend on http://localhost:8080
   ========================================================================== */

// --- Global State ---
const State = {
  activeStudentId: null,
  students: [],
  groups: [],
  connections: [],
  projectPosts: [],
  feedPosts: [],
  selectedProjectId: null,
  d3Simulation: null,
  selectedChatPartnerId: null,
  chatPollTimer: null,
  requests: { incoming: [], outgoing: [] },
  teamInvites: []
};

// --- API Helpers ---
// When signed in, every request carries the Microsoft ID token
// ("Authorization: Bearer ...") so the server can verify who you are.
// On 401 (e.g. token expired after ~1 hour) we refresh it once and retry.
async function getIdToken(forceRefresh = false) {
  if (!msalInstance || !sessionStorage.getItem("geu_student_email")) return null;
  const account = msalInstance.getAllAccounts()[0];
  if (!account) return null;
  try {
    // Same scope as sign-in, so MSAL answers from its cache; every result
    // also carries the ID token, which is what our server verifies.
    const result = await msalInstance.acquireTokenSilent({
      scopes: ["User.Read"],
      account,
      forceRefresh
    });
    return result.idToken || null;
  } catch (e) {
    console.warn("Could not get sign-in token:", e);
    return null;
  }
}

async function apiRequest(method, endpoint, body) {
  const send = async (forceRefresh) => {
    const headers = {};
    if (body !== undefined) headers['Content-Type'] = 'application/json';
    const token = await getIdToken(forceRefresh);
    if (token) headers['Authorization'] = `Bearer ${token}`;
    const res = await fetch(endpoint, {
      method,
      headers,
      body: body !== undefined ? JSON.stringify(body) : undefined
    });
    return { res, hadToken: !!token };
  };

  try {
    let { res, hadToken } = await send(false);
    if (res.status === 401 && hadToken) {
      ({ res } = await send(true));   // token expired -> refresh once
    }
    const json = await res.json();
    if (!res.ok || !json.ok) {
      const msg = (json.data && json.data.error) || json.error || 'Request failed';
      throw new Error(msg);
    }
    return json.data;
  } catch (err) {
    console.error(`${method} ${endpoint} failed:`, err);
    showToast(err.message, 'error');
    throw err;
  }
}

// Identical writes already on their way (double click, Enter + click...) are
// not sent twice: the second caller just waits for the first request.
const pendingWrites = new Map();

const API = {
  get(endpoint) { return apiRequest('GET', endpoint); },
  post(endpoint, body) {
    const key = endpoint + '|' + JSON.stringify(body || {});
    if (pendingWrites.has(key)) return pendingWrites.get(key);
    const request = apiRequest('POST', endpoint, body || {})
      .finally(() => pendingWrites.delete(key));
    pendingWrites.set(key, request);
    return request;
  }
};

// --- Founders: special name style + moderation (role comes from the server) ---
function isFounderId(id) {
  const st = State.students.find(s => s.id === id);
  return !!(st && st.role === 'founder');
}
function amFounder() {
  const me = signedInStudentId();
  return !!(me && isFounderId(me));
}
// Escaped name; founders get the gold crown style everywhere
function nameHtml(name, id) {
  const safe = escapeHtml(name);
  return isFounderId(id)
    ? `<span class="founder-name" title="Founder of Campus Connect">👑 ${safe}</span>`
    : safe;
}
function avatarClass(id) {
  return isFounderId(id) ? ' founder-avatar' : '';
}

// The signed-in student's ID (saved for this browser tab), or null
function signedInStudentId() {
  const id = parseInt(sessionStorage.getItem("geu_student_id"));
  return Number.isInteger(id) && State.students.some(s => s.id === id) ? id : null;
}

// --- Toast Notifications ---
function showToast(message, type = 'info') {
  const container = document.getElementById('toast-container');
  const toast = document.createElement('div');
  toast.className = `toast ${type}`;
  toast.innerText = message;
  container.appendChild(toast);
  setTimeout(() => {
    toast.remove();
  }, 4000);
}

document.addEventListener('DOMContentLoaded', async () => {
  setupNavigation();
  setupModals();
  setupEventListeners();
  initMicrosoftAuth();

  try {
    await refreshAllData();
    // Signed in before this update (no saved profile ID yet)? Ask the server once.
    if (sessionStorage.getItem("geu_student_email") && !signedInStudentId() && await getIdToken()) {
      try {
        const profile = await API.post("/api/students", {});
        sessionStorage.setItem("geu_student_id", String(profile.id));
        await refreshAllData();
      } catch (e) { /* not signed in any more; browse anonymously */ }
    }
    // Signed in? Show the site as yourself. Otherwise browse as the first student.
    const me = signedInStudentId();
    if (me) {
      setActiveStudent(me);
    } else if (State.students.length > 0) {
      setActiveStudent(State.students[0].id);
    }
  } catch (e) {
    console.warn("Backend not yet connected or empty. Attempting demo load...", e);
  }
});

// --- Tab Switching ---
function setupNavigation() {
  const tabs = document.querySelectorAll('.nav-item');
  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      const targetTab = tab.getAttribute('data-tab');
      tabs.forEach(t => t.classList.remove('active'));
      tab.classList.add('active');

      document.querySelectorAll('.tab-pane').forEach(pane => {
        pane.classList.remove('active');
      });
      const pane = document.getElementById(`tab-${targetTab}`);
      if (pane) pane.classList.add('active');

      if (targetTab === 'network') {
        renderNetworkGraph();
        updateMutualsPanel();
      }
      if (targetTab === 'messages') {
        initChatTab();
      } else {
        if (State.chatPollTimer) {
          clearInterval(State.chatPollTimer);
          State.chatPollTimer = null;
        }
      }
    });
  });
}

// --- Active User Switching ---
function setActiveStudent(id) {
  State.activeStudentId = parseInt(id);
  const sel = document.getElementById('current-user-select');
  if (sel) sel.value = State.activeStudentId;

  const current = State.students.find(s => s.id === State.activeStudentId);
  if (current) {
    const avatar = document.getElementById('feed-creator-avatar');
    const nameEl = document.getElementById('feed-creator-name');
    if (avatar) avatar.innerText = current.name.charAt(0);
    if (nameEl) nameEl.innerText = `${current.name} (${current.department})`;
  }
  if (document.getElementById('tab-messages')?.classList.contains('active')) {
    initChatTab();
  }
  // Update dependent views
  renderStudentsGrid();
  if (document.getElementById('tab-network').classList.contains('active')) {
    renderNetworkGraph();
    updateMutualsPanel();
  }
}

// --- Data Refresh Master ---
async function refreshAllData() {
  try {
    const [students, connections, groups, feedPosts, projectPosts] = await Promise.all([
      API.get('/api/students'),
      API.get('/api/connections'),
      API.get('/api/groups'),
      API.get('/api/posts'),
      API.get('/api/project-posts')
    ]);

    State.students = students || [];
    State.connections = connections || [];
    State.groups = groups || [];
    State.feedPosts = feedPosts || [];
    State.projectPosts = projectPosts || [];

    await loadConnectionRequests();
    await loadTeamInvites();
    updateCounters();
    populateSelectDropdowns();
    renderStudentsGrid();
    renderProjectPostsList();
    renderGroupsGrid();
    renderFeedTimeline();

    if (!State.activeStudentId && State.students.length > 0) {
      State.activeStudentId = signedInStudentId() || State.students[0].id;
    }
    setActiveStudent(State.activeStudentId || (State.students[0] ? State.students[0].id : null));

    const statusDot = document.getElementById('server-status-dot');
    const statusText = document.getElementById('server-status-text');
    if (statusDot) statusDot.style.backgroundColor = '#22c55e';
    if (statusText) statusText.innerText = 'Cloud Database Connected';

  } catch (err) {
    const statusDot = document.getElementById('server-status-dot');
    const statusText = document.getElementById('server-status-text');
    if (statusDot) statusDot.style.backgroundColor = '#ef4444';
    if (statusText) statusText.innerText = 'Backend Offline';
  }
}

function updateCounters() {
  document.getElementById('stat-students-count').innerText = State.students.length;
  document.getElementById('stat-edges-count').innerText = State.connections.length;
  document.getElementById('stat-groups-count').innerText = State.groups.length;
  document.getElementById('stat-projects-count').innerText = State.projectPosts.length;
}

function populateSelectDropdowns() {
  const currentSelect = document.getElementById('current-user-select');
  const pathTarget = document.getElementById('select-path-target');
  const connA = document.getElementById('conn-student-a');
  const connB = document.getElementById('conn-student-b');
  const projLeader = document.getElementById('proj-leader');
  const grpAdmin = document.getElementById('grp-admin');

  const optionsHtml = State.students.map(s =>
    `<option value="${s.id}">#${s.id} ${escapeHtml(s.name)} (${escapeHtml(s.department)} Y${s.year})</option>`
  ).join('');

  if (currentSelect) currentSelect.innerHTML = optionsHtml;
  if (pathTarget) pathTarget.innerHTML = optionsHtml;
  if (connA) connA.innerHTML = optionsHtml;
  if (connB) connB.innerHTML = optionsHtml;
  if (projLeader) projLeader.innerHTML = optionsHtml;
  if (grpAdmin) grpAdmin.innerHTML = optionsHtml;

  if (State.activeStudentId && currentSelect) {
    currentSelect.value = State.activeStudentId;
  }
}

// --- TAB 1: Students Directory & Similarities ---
async function renderStudentsGrid() {
  const grid = document.getElementById('students-grid');
  if (!grid) return;

  const searchTerm = (document.getElementById('input-student-search')?.value || '').toLowerCase();
  const deptFilter = document.getElementById('select-filter-dept')?.value || '';

  // Calculate similarity scores against active student if present
  let similarityMap = {};
  if (State.activeStudentId) {
    try {
      const simList = await API.get(`/api/students/${State.activeStudentId}/similar`);
      simList.forEach(item => {
        similarityMap[item.student.id] = (item.similarity * 100).toFixed(0);
      });
    } catch (e) {
      // Non-blocking
    }
  }

  const filtered = State.students.filter(s => {
    const matchesDept = !deptFilter || s.department === deptFilter;
    const skillsText = (s.skills || []).join(' ').toLowerCase();
    const interestsText = (s.interests || []).join(' ').toLowerCase();
    const matchesSearch = !searchTerm ||
      s.name.toLowerCase().includes(searchTerm) ||
      s.department.toLowerCase().includes(searchTerm) ||
      skillsText.includes(searchTerm) ||
      interestsText.includes(searchTerm);
    return matchesDept && matchesSearch;
  });

  if (filtered.length === 0) {
    grid.innerHTML = `<div class="empty-state-card w-full" style="grid-column: 1/-1;">No students found matching current filters.</div>`;
    return;
  }

  grid.innerHTML = filtered.map(s => {
    const isActive = s.id === State.activeStudentId;
    const simScore = similarityMap[s.id] !== undefined ? `${similarityMap[s.id]}% Match` : null;

    return `
      <div class="student-card ${isActive ? 'active-card' : ''}">
        <div class="student-card-header">
          <div class="student-avatar${avatarClass(s.id)}">${escapeHtml(s.name.charAt(0))}</div>
          <div class="student-info">
            <div class="student-name">${nameHtml(s.name, s.id)} ${isActive ? '<span class="badge-pill">You</span>' : ''}</div>
            <div class="student-meta">${escapeHtml(s.department)} &bull; Year ${s.year} &bull; ID: #${s.id}</div>
          </div>
        </div>

        <div class="tags-section">
          <span class="tags-label">Skills</span>
          <div class="tags-list">
            ${(s.skills || []).map(sk => `<span class="tag-badge skill">${escapeHtml(sk)}</span>`).join('')}
          </div>
        </div>

        <div class="tags-section">
          <span class="tags-label">Interests</span>
          <div class="tags-list">
            ${(s.interests || []).map(it => `<span class="tag-badge interest">${escapeHtml(it)}</span>`).join('')}
          </div>
        </div>

        <div class="card-actions-row">
          <div>
            ${simScore && !isActive ? `<span class="similarity-score-badge" title="Jaccard Skill/Interest Similarity">${simScore}</span>` : ''}
          </div>
          <div>
            ${!isActive ? connectButtonHtml(s.id) : `<span class="text-muted small">Current active user</span>`}
          </div>
        </div>
      </div>
    `;
  }).join('');
}

// --- Connections need the other student's consent (request -> accept) ---
function isConnected(a, b) {
  return State.connections.some(c => (c.a === a && c.b === b) || (c.a === b && c.b === a));
}

// 'connected' | 'outgoing' | 'incoming' | 'none'  (only meaningful when signed in)
function connectionStatus(otherId) {
  const me = signedInStudentId();
  if (!me) return 'none';
  if (isConnected(me, otherId)) return 'connected';
  if (State.requests.outgoing.some(r => r.student.id === otherId)) return 'outgoing';
  if (State.requests.incoming.some(r => r.student.id === otherId)) return 'incoming';
  return 'none';
}

function connectButtonHtml(otherId) {
  switch (connectionStatus(otherId)) {
    case 'connected':
      return `<span class="badge-pill">✓ Connected</span>`;
    case 'outgoing': {
      const r = State.requests.outgoing.find(x => x.student.id === otherId);
      return `<button class="btn btn-secondary btn-sm" title="Cancel request" onclick="answerRequest(${r.id}, 'cancel')">Requested ✕</button>`;
    }
    case 'incoming': {
      const r = State.requests.incoming.find(x => x.student.id === otherId);
      return `<button class="btn btn-primary btn-sm" onclick="answerRequest(${r.id}, 'accept')">Accept request</button>`;
    }
    default:
      return `<button class="btn btn-secondary btn-sm" onclick="connectWithStudent(${otherId})">+ Connect</button>`;
  }
}

window.connectWithStudent = async function (targetId) {
  if (!State.activeStudentId) return;
  try {
    if (signedInStudentId()) {
      const res = await API.post('/api/connection-requests', { toId: targetId });
      showToast(res.status === 'connected' ? 'You are now connected!' : 'Connection request sent. They need to accept it.', 'success');
    } else {
      // Local C++ demo (no sign-in): connects directly
      await API.post('/api/connections', { a: State.activeStudentId, b: targetId });
      showToast(`Connected with Student #${targetId}!`, 'success');
    }
    await refreshAllData();
  } catch (e) {
    // Error handled in API
  }
};

window.answerRequest = async function (requestId, action) {
  try {
    await API.post(`/api/connection-requests/${requestId}/${action}`, {});
    showToast({ accept: 'Request accepted. You are now connected!', decline: 'Request declined.', cancel: 'Request cancelled.' }[action], 'success');
    await refreshAllData();
  } catch (e) { }
};

window.removeConnection = async function (otherId, name) {
  if (!confirm(`Remove ${name} from your connections?`)) return;
  try {
    await API.post('/api/connections/remove', { studentId: otherId });
    showToast('Connection removed.', 'success');
    await refreshAllData();
  } catch (e) { }
};

// --- Team invitations ---
async function loadTeamInvites() {
  const badge = document.getElementById('invites-badge');
  const card = document.getElementById('team-invites-card');
  const list = document.getElementById('team-invites-list');
  if (!signedInStudentId()) {
    State.teamInvites = [];
    badge?.classList.add('hidden');
    card?.classList.add('hidden');
    return;
  }
  try {
    State.teamInvites = await API.get('/api/team-invites');
  } catch (e) {
    State.teamInvites = [];
  }
  const n = State.teamInvites.length;
  if (badge) { badge.textContent = n; badge.classList.toggle('hidden', n === 0); }
  if (card) card.classList.toggle('hidden', n === 0);
  if (list) {
    list.innerHTML = State.teamInvites.map(i => `
      <div class="friend-item-row pymk-row">
        <div class="pymk-info">
          <span><strong>${escapeHtml(i.group.name)}</strong> ${i.group.isProjectGroup ? '(project team)' : ''}</span>
          <span class="pymk-via">invited by ${escapeHtml(i.invitedBy.name || 'the admin')}</span>
        </div>
        <span class="post-actions">
          <button class="btn btn-primary btn-sm" onclick="answerInvite(${i.id}, 'accept')">Accept</button>
          <button class="post-action-btn" onclick="answerInvite(${i.id}, 'decline')">Decline</button>
        </span>
      </div>`).join('');
  }
}

window.answerInvite = async function (inviteId, action) {
  try {
    await API.post(`/api/team-invites/${inviteId}/${action}`, {});
    showToast(action === 'accept' ? 'You joined the team!' : 'Invitation declined.', 'success');
    await refreshAllData();
  } catch (e) { }
};

window.deleteGroup = async function (groupId, name) {
  if (!confirm(`Delete the group "${name}"? Its members, discussion and invitations are removed. This cannot be undone.`)) return;
  try {
    await API.post(`/api/groups/${groupId}/delete`, {});
    showToast('Group deleted.', 'success');
    await refreshAllData();
  } catch (e) { }
};

async function loadConnectionRequests() {
  const badge = document.getElementById('requests-badge');
  const card = document.getElementById('requests-card');
  if (!signedInStudentId()) {
    State.requests = { incoming: [], outgoing: [] };
    badge?.classList.add('hidden');
    card?.classList.add('hidden');
    return;
  }
  try {
    State.requests = await API.get('/api/connection-requests');
  } catch (e) {
    State.requests = { incoming: [], outgoing: [] };
  }
  const n = State.requests.incoming.length;
  if (badge) { badge.textContent = n; badge.classList.toggle('hidden', n === 0); }
  renderRequestsPanel();
}

function renderRequestsPanel() {
  const card = document.getElementById('requests-card');
  const list = document.getElementById('requests-list');
  if (!card || !list) return;
  const { incoming, outgoing } = State.requests;
  card.classList.toggle('hidden', !signedInStudentId());
  if (incoming.length === 0 && outgoing.length === 0) {
    list.innerHTML = `<span class="text-muted small">No pending requests.</span>`;
    return;
  }
  list.innerHTML =
    incoming.map(r => `
      <div class="friend-item-row pymk-row">
        <div class="pymk-info">
          <span><strong>${escapeHtml(r.student.name)}</strong> (${escapeHtml(r.student.department)})</span>
          <span class="pymk-via">wants to connect</span>
        </div>
        <span class="post-actions">
          <button class="btn btn-primary btn-sm" onclick="answerRequest(${r.id}, 'accept')">Accept</button>
          <button class="post-action-btn" onclick="answerRequest(${r.id}, 'decline')">Decline</button>
        </span>
      </div>`).join('') +
    outgoing.map(r => `
      <div class="friend-item-row pymk-row">
        <div class="pymk-info">
          <span><strong>${escapeHtml(r.student.name)}</strong> (${escapeHtml(r.student.department)})</span>
          <span class="pymk-via">request sent, waiting</span>
        </div>
        <button class="post-action-btn" onclick="answerRequest(${r.id}, 'cancel')">Cancel</button>
      </div>`).join('');
}

// --- TAB 2: D3 Social Network Graph ---
function renderNetworkGraph() {
  const container = document.getElementById('graph-container');
  const svgEl = document.getElementById('network-svg');
  if (!svgEl || !container) return;

  const width = container.clientWidth;
  const height = container.clientHeight;

  // Clear previous SVG
  d3.select(svgEl).selectAll("*").remove();

  const svg = d3.select(svgEl)
    .attr("viewBox", [0, 0, width, height]);

  const g = svg.append("g");

  // Zoom behavior
  const zoom = d3.zoom()
    .scaleExtent([0.3, 3])
    .on("zoom", (event) => g.attr("transform", event.transform));
  svg.call(zoom);

  document.getElementById('btn-fit-graph')?.addEventListener('click', () => {
    svg.transition().duration(750).call(zoom.transform, d3.zoomIdentity);
  });

  // Nodes & Edges
  const nodes = State.students.map(s => ({
    id: s.id,
    name: s.name,
    department: s.department,
    isActive: s.id === State.activeStudentId
  }));

  const links = State.connections.map(c => ({
    source: c.a,
    target: c.b
  }));

  // Force simulation
  const simulation = d3.forceSimulation(nodes)
    .force("link", d3.forceLink(links).id(d => d.id).distance(90))
    .force("charge", d3.forceManyBody().strength(-240))
    .force("center", d3.forceCenter(width / 2, height / 2))
    .force("collision", d3.forceCollide().radius(35));

  State.d3Simulation = simulation;

  // Draw links
  const link = g.append("g")
    .attr("stroke", "rgba(255, 255, 255, 0.2)")
    .attr("stroke-width", 1.8)
    .selectAll("line")
    .data(links)
    .join("line");

  // Draw node groups
  const node = g.append("g")
    .selectAll("g")
    .data(nodes)
    .join("g")
    .call(d3.drag()
      .on("start", (event, d) => {
        if (!event.active) simulation.alphaTarget(0.3).restart();
        d.fx = d.x;
        d.fy = d.y;
      })
      .on("drag", (event, d) => {
        d.fx = event.x;
        d.fy = event.y;
      })
      .on("end", (event, d) => {
        if (!event.active) simulation.alphaTarget(0);
        d.fx = null;
        d.fy = null;
      })
    );

  // Node circles
  node.append("circle")
    .attr("r", d => d.isActive ? 22 : 16)
    .attr("fill", d => {
      if (d.isActive) return "#6366f1";
      return "#38bdf8";
    })
    .attr("stroke", "#ffffff")
    .attr("stroke-width", d => d.isActive ? 3 : 1.5)
    .style("filter", d => d.isActive ? "drop-shadow(0 0 8px #6366f1)" : "none")
    .style("cursor", "pointer")
    .on("click", (event, d) => {
      setActiveStudent(d.id);
    });

  // Node text labels
  node.append("text")
    .text(d => d.name.split(' ')[0])
    .attr("x", 0)
    .attr("y", d => d.isActive ? 34 : 28)
    .attr("text-anchor", "middle")
    .attr("fill", "#cbd5e1")
    .attr("font-size", "11px")
    .attr("font-weight", "600")
    .attr("pointer-events", "none");

  simulation.on("tick", () => {
    link
      .attr("x1", d => d.source.x)
      .attr("y1", d => d.source.y)
      .attr("x2", d => d.target.x)
      .attr("y2", d => d.target.y);

    node
      .attr("transform", d => `translate(${d.x},${d.y})`);
  });
}

async function updateMutualsPanel() {
  const activeFriendsContainer = document.getElementById('active-user-friends-list');
  if (!activeFriendsContainer || !State.activeStudentId) return;

  try {
    const friends = await API.get(`/api/students/${State.activeStudentId}/friends`);
    if (friends.length === 0) {
      activeFriendsContainer.innerHTML = `<span class="text-muted small">No direct connections yet.</span>`;
    } else {
      const mine = signedInStudentId() && State.activeStudentId === signedInStudentId();
      activeFriendsContainer.innerHTML = friends.map(f => `
        <div class="friend-item-row">
          <span>#${f.id} <strong>${nameHtml(f.name, f.id)}</strong> (${escapeHtml(f.department)})</span>
          ${mine ? `<button class="post-action-btn danger" onclick='removeConnection(${f.id}, ${JSON.stringify(escapeHtml(f.name))})'>Remove</button>`
                 : `<span class="badge-pill">Direct</span>`}
        </div>
      `).join('');
    }
  } catch (e) {
    // Error handled
  }
  renderPeopleYouMayKnow();
}

// "People You May Know": friends of friends ranked by mutual connections
// (GET /api/students/:id/suggestions -> Graph::suggestFriends / BFS 2 steps)
async function renderPeopleYouMayKnow() {
  const list = document.getElementById('people-you-may-know-list');
  if (!list || !State.activeStudentId) return;
  const forStudent = State.activeStudentId;

  try {
    const suggestions = await API.get(`/api/students/${forStudent}/suggestions?limit=8`);
    if (forStudent !== State.activeStudentId) return;   // user switched while loading
    if (suggestions.length === 0) {
      list.innerHTML = `<span class="text-muted small">No suggestions yet. Connect with a few students and their friends will show up here.</span>`;
      return;
    }
    list.innerHTML = suggestions.map(item => {
      const s = item.student;
      const names = item.mutualFriends.map(m => escapeHtml(m.name));
      const via = names.length <= 2 ? names.join(', ') : `${names.slice(0, 2).join(', ')} +${names.length - 2} more`;
      const label = `${item.mutualCount} mutual friend${item.mutualCount === 1 ? '' : 's'}`;
      return `
        <div class="friend-item-row pymk-row">
          <div class="pymk-info">
            <span>#${s.id} <strong>${nameHtml(s.name, s.id)}</strong> (${escapeHtml(s.department)})</span>
            <span class="pymk-via" title="${label}: ${names.join(', ')}">${label} &middot; via ${via}</span>
          </div>
          ${connectButtonHtml(s.id)}
        </div>
      `;
    }).join('');
  } catch (e) {
    list.innerHTML = `<span class="text-muted small">Suggestions are unavailable right now.</span>`;
  }
}

// --- TAB 3: Projects & Top-K Teammate Matcher ---
function renderProjectPostsList() {
  const list = document.getElementById('project-posts-list');
  if (!list) return;

  if (State.projectPosts.length === 0) {
    list.innerHTML = `<div class="empty-state-card">No active project posts yet.</div>`;
    return;
  }

  list.innerHTML = State.projectPosts.map(p => {
    const isSelected = p.id === State.selectedProjectId;
    const aId = p.authorId || p.studentId;
    const author = State.students.find(s => s.id === aId);
    return `
      <div class="project-card-item ${isSelected ? 'selected' : ''}" onclick="selectProjectPost(${p.id})">
        <div class="project-item-title">${escapeHtml(p.projectTitle || 'Project #' + p.id)}</div>
        <div class="project-item-meta">
          <span>Lead: ${escapeHtml(author ? author.name : (p.authorName || 'ID ' + aId))}</span>
          <span>Team Size: ${p.teamSizeNeeded || 2}</span>
        </div>
        <div class="tags-list">
          ${(p.requiredSkills || []).map(sk => `<span class="tag-badge skill">${escapeHtml(sk)}</span>`).join('')}
        </div>
      </div>
    `;
  }).join('');
}

window.selectProjectPost = async function (projectId) {
  State.selectedProjectId = projectId;
  renderProjectPostsList();

  const emptyState = document.getElementById('project-empty-state');
  const detailsPane = document.getElementById('project-active-details');
  if (emptyState) emptyState.classList.add('hidden');
  if (detailsPane) detailsPane.classList.remove('hidden');

  const project = State.projectPosts.find(p => p.id === projectId);
  if (!project) return;

  const aId = project.authorId || project.studentId;
  const author = State.students.find(s => s.id === aId);
  const me = signedInStudentId();
  const isAuthor = !!me && aId === me;

  // One team per project; selected students get invitations (signed-in site).
  // The local C++ demo (no sign-in) keeps the old direct "assemble" behaviour.
  let team = null;
  if (me) {
    try { team = await API.get(`/api/project-posts/${projectId}/team`); } catch (e) { }
  }
  const hasTeam = !!(team && team.groupId);
  const canPick = !me || isAuthor;
  const teamButtons = !me
    ? `<button class="btn btn-primary" id="btn-create-project-group" onclick="createGroupFromTopK(${projectId})">🚀 Assemble Project Group</button>`
    : isAuthor
      ? (hasTeam
          ? `<button class="btn btn-primary" id="btn-create-project-group" onclick="createGroupFromTopK(${projectId})">📨 Invite selected</button>
             <button class="btn btn-secondary" onclick="openGroupDiscussion(${team.groupId})">Open team</button>`
          : `<button class="btn btn-primary" id="btn-create-project-group" onclick="createGroupFromTopK(${projectId})">🚀 Assemble Project Team</button>`)
      : (hasTeam ? `<button class="btn btn-secondary" onclick="openGroupDiscussion(${team.groupId})">View team</button>` : '');
  const candidateStatus = (sId) => {
    if (sId === aId) return `<span class="badge-pill">Project author</span>`;
    if (hasTeam && team.memberIds.includes(sId)) return `<span class="badge-pill">✓ In team</span>`;
    if (hasTeam && team.invitedIds.includes(sId)) return `<span class="text-muted small">Invited, waiting for reply</span>`;
    if (!canPick) return '';
    return `<label style="font-size:0.75rem; color:var(--text-muted); cursor:pointer;">
              <input type="checkbox" class="candidate-checkbox" value="${sId}" ${me ? '' : 'checked'}>
              ${me ? 'Invite to team' : 'Include in Team Group'}
            </label>`;
  };

  // Fetch Top-K recommendations from C++ backend
  let topKResults = [];
  try {
    const topkData = await API.get(`/api/project-posts/${projectId}/top-k?mode=all&k=6`);
    topKResults = topkData.results || [];
  } catch (e) {
    console.error("Top-K fetch failed:", e);
  }

  detailsPane.innerHTML = `
    <div class="project-detail-header">
      <div>
        <div class="project-title-large">${escapeHtml(project.projectTitle || 'Project #' + project.id)}</div>
        <div class="text-muted small mt-2">
          Posted by <strong>${nameHtml(author ? author.name : (project.authorName || 'Student #' + aId), aId)}</strong> &bull;
          Needed Team Size: <strong>${project.teamSizeNeeded || 3}</strong>
        </div>
      </div>
      <div class="project-detail-actions">
        ${signedInStudentId() && aId === signedInStudentId() ? `
          <button class="post-action-btn" onclick="editProjectPost(${projectId})">Edit</button>
          <button class="post-action-btn danger" onclick="deleteProjectPost(${projectId})">Delete</button>
        ` : amFounder() ? `
          <button class="post-action-btn danger" title="Remove as founder" onclick="deleteProjectPost(${projectId})">Remove</button>
        ` : ''}
        ${teamButtons}
      </div>
    </div>

    <div>
      <h4 class="tags-label">Required Skills Profile</h4>
      <div class="tags-list mt-2">
        ${(project.requiredSkills || []).map(sk => `<span class="tag-badge skill" style="font-size:0.85rem;">${escapeHtml(sk)}</span>`).join('')}
      </div>
    </div>

    <div class="topk-filter-bar">
      <div>
        <strong style="font-size:0.9rem;">C++ Top-K Ranked Candidates:</strong>
        <span class="text-muted small" style="margin-left:8px;">Calculated using Jaccard skill similarity & max-heap ranking</span>
      </div>
      <div>
        <span class="badge-pill">${topKResults.length} Matches Found</span>
      </div>
    </div>

    <div class="candidate-cards-grid" id="candidates-grid">
      ${topKResults.length === 0 ? `<div class="empty-state-card w-full">No candidates found matching skills.</div>` :
      topKResults.map((c, idx) => {
        const sId = c.studentId || (c.student ? c.student.id : 0);
        const studentObj = c.student || State.students.find(s => s.id === sId) || {};
        const sName = c.name || studentObj.name || ('Student #' + sId);
        const sDept = c.department || studentObj.department || 'General';
        const sYear = c.year || studentObj.year || 1;
        const sSkills = (studentObj.skills && studentObj.skills.length) ? studentObj.skills : (c.matchedSkills || []);
        const matchPercent = (c.score * 100).toFixed(0);
        return `
            <div class="candidate-card selected-for-team" id="candidate-card-${sId}" data-id="${sId}">
              <div class="candidate-rank-badge">Rank #${idx + 1}</div>
              <div class="student-name">${nameHtml(sName, sId)}</div>
              <div class="student-meta">${escapeHtml(sDept)} &bull; Year ${escapeHtml(sYear)}</div>
              
              <div>
                <div class="stat-row">
                  <span class="small text-muted">Jaccard Score</span>
                  <span class="small" style="font-weight:700; color:var(--accent);">${matchPercent}%</span>
                </div>
                <div class="candidate-match-bar">
                  <div class="candidate-match-fill" style="width: ${matchPercent}%"></div>
                </div>
              </div>

              <div class="tags-list mt-2">
                ${sSkills.map(sk => `<span class="tag-badge">${escapeHtml(sk)}</span>`).join('')}
              </div>

              <div class="mt-2">
                ${candidateStatus(sId)}
              </div>
            </div>
          `;
      }).join('')
    }
    </div>
  `;
};

window.createGroupFromTopK = async function (projectId) {
  const project = State.projectPosts.find(p => p.id === projectId);
  if (!project) return;

  const checkboxes = document.querySelectorAll('.candidate-checkbox:checked');
  const memberIds = Array.from(checkboxes).map(cb => parseInt(cb.value));

  if (signedInStudentId()) {
    // Signed-in site: builds the project's single team and INVITES the selected students
    try {
      const res = await API.post(`/api/project-posts/${projectId}/create-group`, { memberIds });
      const n = (res.invited || []).length;
      const invitedText = n ? `Invited ${n} student${n === 1 ? '' : 's'}. They join if they accept.` : 'No new invitations (already invited or in the team).';
      showToast(res.created ? `Team "${res.name}" created. ${invitedText}` : invitedText, 'success');
      await refreshAllData();
      selectProjectPost(projectId);
    } catch (e) { }
    return;
  }

  try {
    const leaderId = project.authorId || project.studentId;
    const group = await API.post(`/api/project-posts/${projectId}/create-group`, {
      leaderId: leaderId,
      groupName: `${project.projectTitle || 'Project'} Team`,
      memberIds: memberIds
    });

    showToast(`Project Group "${group.name}" created with ${group.memberCount} members!`, 'success');
    await refreshAllData();

    // Switch to groups tab
    document.getElementById('tab-btn-groups')?.click();
  } catch (e) {
    // Error handled
  }
};

// --- TAB 4: Groups View ---
function renderGroupsGrid() {
  const grid = document.getElementById('groups-grid');
  if (!grid) return;

  if (State.groups.length === 0) {
    grid.innerHTML = `<div class="empty-state-card w-full" style="grid-column: 1/-1;">No campus groups formed yet.</div>`;
    return;
  }

  grid.innerHTML = State.groups.map(g => {
    const isMember = State.activeStudentId && (g.memberIds || []).includes(State.activeStudentId);
    return `
      <div class="group-card">
        <div style="display:flex; justify-content:space-between; align-items:flex-start;">
          <h3 style="font-size:1.1rem; font-weight:700;">${escapeHtml(g.name)}</h3>
          <span class="${g.isProjectGroup ? 'group-badge-project' : 'group-badge-general'}">
            ${g.isProjectGroup ? 'Project Team' : 'General Club'}
          </span>
        </div>

        <p class="text-muted small">${escapeHtml(g.description || 'No description provided.')}</p>

        <div class="stat-row">
          <span class="small text-muted">Members:</span>
          <span class="small" style="font-weight:700;">${g.memberCount || (g.memberIds || []).length} students</span>
        </div>

        <div class="card-actions-row">
          <button class="btn btn-secondary btn-sm" onclick="openGroupDiscussion(${g.id})">
            💬 Discussions
          </button>
          ${(signedInStudentId() && g.leaderId === signedInStudentId()) || amFounder() ? `
            <button class="btn btn-secondary btn-sm" style="color:var(--danger);" onclick='deleteGroup(${g.id}, ${JSON.stringify(escapeHtml(g.name))})'>
              Delete Group
            </button>
          ` : isMember ? `
            <button class="btn btn-secondary btn-sm" style="color:var(--danger);" onclick="leaveGroup(${g.id})">
              Leave Group
            </button>
          ` : g.isProjectGroup ? `<span class="text-muted small" title="Members are chosen by the project author">Invite only</span>` : `
            <button class="btn btn-primary btn-sm" onclick="joinGroup(${g.id})">
              Join Group
            </button>
          `}
        </div>
      </div>
    `;
  }).join('');
}

window.joinGroup = async function (groupId) {
  if (!State.activeStudentId) return;
  try {
    await API.post(`/api/groups/${groupId}/join`, { studentId: State.activeStudentId });
    showToast("Successfully joined group!", "success");
    await refreshAllData();
  } catch (e) { }
};

window.leaveGroup = async function (groupId) {
  if (!State.activeStudentId) return;
  try {
    await API.post(`/api/groups/${groupId}/leave`, { studentId: State.activeStudentId });
    showToast("Left group.", "info");
    await refreshAllData();
  } catch (e) { }
};

let currentModalGroupId = null;
window.openGroupDiscussion = async function (groupId) {
  currentModalGroupId = groupId;
  const group = State.groups.find(g => g.id === groupId);
  if (!group) return;

  document.getElementById('modal-group-title').innerText = group.name;
  document.getElementById('modal-group-subtitle').innerText = `${group.memberCount} members &bull; ${group.isProjectGroup ? 'Project Group' : 'Interest Club'}`;

  // Member chips
  const membersBar = document.getElementById('modal-group-members-list');
  const memberStudents = State.students.filter(s => (group.memberIds || []).includes(s.id));
  membersBar.innerHTML = memberStudents.map(m => `
    <span class="tag-badge" style="background:rgba(255,255,255,0.08);">#${m.id} ${escapeHtml(m.name)}</span>
  `).join('');

  await refreshGroupPosts(groupId);
  document.getElementById('modal-group-discussion').classList.remove('hidden');
};

async function refreshGroupPosts(groupId) {
  const postsList = document.getElementById('modal-group-posts-list');
  try {
    const posts = await API.get(`/api/groups/${groupId}/posts`);
    if (posts.length === 0) {
      postsList.innerHTML = `<div class="empty-state-card">No messages yet in this group thread. Be the first to share an update!</div>`;
      return;
    }

    State.groupPosts = posts;
    const me = signedInStudentId();
    const group = State.groups.find(g => g.id === groupId);
    const iAmAdmin = !!(me && group && group.leaderId === me) || amFounder();
    postsList.innerHTML = posts.map(p => {
      const author = State.students.find(s => s.id === p.studentId);
      const mine = me && p.studentId === me;
      return `
        <div class="feed-post-card" style="padding:0.85rem;">
          <div class="post-header">
            <div class="user-avatar-sm${avatarClass(p.studentId)}" style="width:26px; height:26px; font-size:0.75rem;">${escapeHtml(author ? author.name.charAt(0) : 'U')}</div>
            <span class="post-author">${nameHtml(author ? author.name : (p.authorName || 'Student #' + p.studentId), p.studentId)}</span>
            <span class="post-timestamp">${escapeHtml(p.timestamp || '')}</span>
            ${(mine || iAmAdmin) ? `<span class="post-actions">
              ${mine ? `<button class="post-action-btn" onclick="editGroupPost(${groupId}, ${p.id})">Edit</button>` : ''}
              <button class="post-action-btn danger" onclick="deleteGroupPost(${groupId}, ${p.id})">Delete</button>
            </span>` : ''}
          </div>
          <div class="post-body" style="font-size:0.88rem;">${escapeHtml(p.content)}</div>
        </div>
      `;
    }).join('');
  } catch (e) { }
}

// --- TAB 5: Campus Feed ---
function renderFeedTimeline() {
  const timeline = document.getElementById('feed-timeline');
  if (!timeline) return;

  if (State.feedPosts.length === 0) {
    timeline.innerHTML = `<div class="empty-state-card">No campus updates posted yet.</div>`;
    return;
  }

  const me = signedInStudentId();
  timeline.innerHTML = State.feedPosts.map(p => {
    const authorId = p.authorId ?? p.studentId;   // Python API: authorId, C++: both
    const author = State.students.find(s => s.id === authorId);
    const authorName = author ? author.name : (p.authorName || `Student #${authorId}`);
    return `
      <div class="feed-post-card">
        <div class="post-header">
          <div class="user-avatar-sm${avatarClass(authorId)}">${escapeHtml(authorName.charAt(0) || 'U')}</div>
          <div>
            <div class="post-author">${nameHtml(authorName, authorId)}</div>
            <div class="post-timestamp">${escapeHtml(p.timestamp || '')}</div>
          </div>
          ${me && authorId === me ? `<span class="post-actions">
            <button class="post-action-btn" onclick="editFeedPost(${p.id})">Edit</button>
            <button class="post-action-btn danger" onclick="deleteFeedPost(${p.id})">Delete</button>
          </span>` : amFounder() ? `<span class="post-actions">
            <button class="post-action-btn danger" title="Remove as founder" onclick="deleteFeedPost(${p.id})">Remove</button>
          </span>` : ''}
        </div>
        <div class="post-body">${escapeHtml(p.content)}</div>
      </div>
    `;
  }).join('');
}

// --- Edit / delete your own posts (group admins may also delete group posts) ---
window.editFeedPost = async function (postId) {
  const post = State.feedPosts.find(p => p.id === postId);
  if (!post) return;
  const text = prompt("Edit your post:", post.content);
  if (text === null || !text.trim() || text.trim() === post.content) return;
  try {
    await API.post(`/api/posts/${postId}/edit`, { content: text.trim() });
    showToast("Post updated.", "success");
    State.feedPosts = await API.get('/api/posts') || [];
    renderFeedTimeline();
  } catch (e) { }
};

window.deleteFeedPost = async function (postId) {
  if (!confirm("Delete this post? This cannot be undone.")) return;
  try {
    await API.post(`/api/posts/${postId}/delete`, {});
    showToast("Post deleted.", "success");
    State.feedPosts = await API.get('/api/posts') || [];
    renderFeedTimeline();
  } catch (e) { }
};

window.editGroupPost = async function (groupId, postId) {
  const post = (State.groupPosts || []).find(p => p.id === postId);
  if (!post) return;
  const text = prompt("Edit your post:", post.content);
  if (text === null || !text.trim() || text.trim() === post.content) return;
  try {
    await API.post(`/api/groups/${groupId}/posts/${postId}/edit`, { content: text.trim() });
    showToast("Post updated.", "success");
    await refreshGroupPosts(groupId);
  } catch (e) { }
};

window.deleteGroupPost = async function (groupId, postId) {
  if (!confirm("Delete this post? This cannot be undone.")) return;
  try {
    await API.post(`/api/groups/${groupId}/posts/${postId}/delete`, {});
    showToast("Post deleted.", "success");
    await refreshGroupPosts(groupId);
  } catch (e) { }
};

// --- Edit / delete your own project posts ---
// The "Post New Project" modal doubles as the edit form.
function setProjectModalMode(projectId) {
  State.editingProjectId = projectId;
  const modal = document.getElementById('modal-create-project');
  const heading = modal?.querySelector('h2');
  const submit = modal?.querySelector('button[type="submit"]');
  const owner = document.getElementById('proj-leader')?.closest('.form-group');
  if (heading) heading.textContent = projectId ? 'Edit Project' : 'Post New Project';
  if (submit) {
    if (!submit.dataset.createLabel) submit.dataset.createLabel = submit.textContent;
    submit.textContent = projectId ? 'Save Changes' : submit.dataset.createLabel;
  }
  if (owner) owner.style.display = projectId ? 'none' : '';   // the author can't change
  const leader = document.getElementById('proj-leader');
  if (leader) leader.required = !projectId;
}

window.editProjectPost = function (projectId) {
  const project = State.projectPosts.find(p => p.id === projectId);
  if (!project) return;
  setProjectModalMode(projectId);
  document.getElementById('proj-title').value = project.projectTitle || '';
  document.getElementById('proj-skills').value = (project.requiredSkills || []).join(', ');
  document.getElementById('proj-size').value = project.teamSizeNeeded || 3;
  document.getElementById('modal-create-project').classList.remove('hidden');
};

window.deleteProjectPost = async function (projectId) {
  if (!confirm("Delete this project post? Teams already created from it stay.")) return;
  try {
    await API.post(`/api/project-posts/${projectId}/delete`, {});
    showToast("Project post deleted.", "success");
    State.selectedProjectId = null;
    document.getElementById('project-active-details')?.classList.add('hidden');
    document.getElementById('project-empty-state')?.classList.remove('hidden');
    await refreshAllData();
  } catch (e) { }
};

// --- Modals & Event Listeners ---
function setupModals() {
  document.querySelectorAll('.btn-close-modal').forEach(btn => {
    btn.addEventListener('click', () => {
      const modalId = btn.getAttribute('data-modal');
      const modal = document.getElementById(modalId);
      if (modal) modal.classList.add('hidden');
    });
  });

  // Modal Triggers
  document.getElementById('btn-open-register-student')?.addEventListener('click', () => {
    document.getElementById('modal-register-student').classList.remove('hidden');
  });

  document.getElementById('btn-open-connect-modal')?.addEventListener('click', () => {
    document.getElementById('modal-connect').classList.remove('hidden');
  });

  document.getElementById('btn-open-create-project')?.addEventListener('click', () => {
    setProjectModalMode(null);
    document.getElementById('form-create-project')?.reset();
    document.getElementById('modal-create-project').classList.remove('hidden');
  });

  document.getElementById('btn-open-create-group')?.addEventListener('click', () => {
    document.getElementById('modal-create-group').classList.remove('hidden');
  });

  // Edit Profile / Interests Modal Triggers
  document.getElementById('btn-open-edit-profile')?.addEventListener('click', () => {
    const cur = State.students.find(s => s.id === State.activeStudentId);
    if (!cur) {
      showToast("Select a student first or sign in.", "warning");
      return;
    }
    document.getElementById('edit-department').value = cur.department || 'CSE';
    document.getElementById('edit-year').value = cur.year || 2;
    document.getElementById('edit-skills').value = (cur.skills || []).join(', ');
    document.getElementById('edit-interests').value = (cur.interests || []).join(', ');
    document.getElementById('modal-edit-profile').classList.remove('hidden');
  });

  document.getElementById('btn-close-edit-profile-modal')?.addEventListener('click', () => {
    document.getElementById('modal-edit-profile').classList.add('hidden');
  });

  document.getElementById('btn-cancel-edit-profile')?.addEventListener('click', () => {
    document.getElementById('modal-edit-profile').classList.add('hidden');
  });
}

function setupEventListeners() {
  // Current user dropdown switch
  document.getElementById('current-user-select')?.addEventListener('change', (e) => {
    setActiveStudent(e.target.value);
  });

  // Reset / Load Demo button
  document.getElementById('btn-load-demo')?.addEventListener('click', async () => {
    try {
      await API.post('/api/demo/load', {});
      showToast("Demo dataset loaded successfully!", "success");
      await refreshAllData();
    } catch (e) { }
  });

  // Search & filter directory
  document.getElementById('input-student-search')?.addEventListener('input', renderStudentsGrid);
  document.getElementById('select-filter-dept')?.addEventListener('change', renderStudentsGrid);
  document.getElementById('btn-refresh-students')?.addEventListener('click', refreshAllData);

  // Check mutuals button
  document.getElementById('btn-find-mutuals')?.addEventListener('click', async () => {
    const targetId = document.getElementById('select-path-target')?.value;
    const box = document.getElementById('path-results-box');
    if (!targetId || !State.activeStudentId || !box) return;

    try {
      const mutuals = await API.get(`/api/connections/mutual?a=${State.activeStudentId}&b=${targetId}`);
      if (mutuals.length === 0) {
        box.innerHTML = `<p class="text-muted small">No mutual connections found between active student and Student #${targetId}.</p>`;
      } else {
        box.innerHTML = `
          <strong style="color:var(--accent); font-size:0.85rem;">${mutuals.length} Mutual Connections:</strong>
          <div class="tags-list mt-2">
            ${mutuals.map(m => `<span class="tag-badge">#${m.id} ${escapeHtml(m.name)}</span>`).join('')}
          </div>
        `;
      }
    } catch (e) { }
  });

  // Form: Register Student
  document.getElementById('form-register-student')?.addEventListener('submit', async (e) => {
    e.preventDefault();
    const name = document.getElementById('reg-name').value;
    const dept = document.getElementById('reg-dept').value;
    const year = parseInt(document.getElementById('reg-year').value);
    const skills = document.getElementById('reg-skills').value.split(',').map(s => s.trim()).filter(Boolean);
    const interests = document.getElementById('reg-interests').value.split(',').map(s => s.trim()).filter(Boolean);

    try {
      const newStudent = await API.post('/api/students', {
        name,
        department: dept,
        year,
        skills,
        interests
      });
      showToast(`Student #${newStudent.id} registered!`, 'success');
      document.getElementById('modal-register-student').classList.add('hidden');
      e.target.reset();
      await refreshAllData();
      setActiveStudent(newStudent.id);
    } catch (err) { }
  });

  // Form: Edit Profile & Interests
  document.getElementById('form-edit-profile')?.addEventListener('submit', async (e) => {
    e.preventDefault();
    if (!State.activeStudentId) return;

    const dept = document.getElementById('edit-department').value.trim();
    const year = parseInt(document.getElementById('edit-year').value) || 2;
    const skills = document.getElementById('edit-skills').value.split(',').map(s => s.trim()).filter(Boolean);
    const interests = document.getElementById('edit-interests').value.split(',').map(i => i.trim()).filter(Boolean);

    try {
      await API.post(`/api/students/${State.activeStudentId}/update`, {
        department: dept,
        year: year,
        skills: skills,
        interests: interests
      });
      document.getElementById('modal-edit-profile').classList.add('hidden');
      showToast('Profile and interests updated in Neon Cloud!', 'success');
      await refreshAllData();
    } catch (err) {
      showToast('Failed to update profile: ' + (err.message || err), 'error');
    }
  });

  // Form: Add Connection
  document.getElementById('form-connect')?.addEventListener('submit', async (e) => {
    e.preventDefault();
    const a = parseInt(document.getElementById('conn-student-a').value);
    const b = parseInt(document.getElementById('conn-student-b').value);
    if (a === b) {
      showToast("Cannot connect student with themselves.", "error");
      return;
    }

    try {
      const res = await API.post('/api/connections', { a, b });
      showToast(res && res.status === 'pending' ? "Connection request sent. They need to accept it." : "Connection established!", "success");
      document.getElementById('modal-connect').classList.add('hidden');
      await refreshAllData();
      if (document.getElementById('tab-network').classList.contains('active')) {
        renderNetworkGraph();
      }
    } catch (err) { }
  });

  // Form: Create Project
  document.getElementById('form-create-project')?.addEventListener('submit', async (e) => {
    e.preventDefault();
    const title = document.getElementById('proj-title').value;
    const rawLeader = document.getElementById('proj-leader').value;
    const leaderId = parseInt(rawLeader) || State.activeStudentId || (State.students[0] ? State.students[0].id : 12);
    const skills = document.getElementById('proj-skills').value.split(',').map(s => s.trim()).filter(Boolean);
    const size = parseInt(document.getElementById('proj-size').value) || 3;

    try {
      const editingId = State.editingProjectId;
      const post = editingId
        ? await API.post(`/api/project-posts/${editingId}/edit`, {
            projectTitle: title,
            requiredSkills: skills,
            teamSizeNeeded: size,
            content: title
          })
        : await API.post('/api/project-posts', {
            authorId: leaderId,
            projectTitle: title,
            requiredSkills: skills,
            teamSizeNeeded: size,
            content: title
          });
      showToast(editingId ? `Project "${title}" updated!` : `Project "${title}" posted!`, "success");
      setProjectModalMode(null);
      document.getElementById('modal-create-project').classList.add('hidden');
      e.target.reset();
      await refreshAllData();
      if (post && post.id) {
        selectProjectPost(post.id);
      }
    } catch (err) { }
  });

  // Form: Create Group
  document.getElementById('form-create-group')?.addEventListener('submit', async (e) => {
    e.preventDefault();
    const name = document.getElementById('grp-name').value;
    const desc = document.getElementById('grp-desc').value;
    const rawAdmin = document.getElementById('grp-admin').value;
    const adminId = parseInt(rawAdmin) || State.activeStudentId || (State.students[0] ? State.students[0].id : 12);

    try {
      await API.post('/api/groups', {
        name,
        description: desc,
        adminId
      });
      showToast(`Group "${name}" created!`, "success");
      document.getElementById('modal-create-group').classList.add('hidden');
      e.target.reset();
      await refreshAllData();
    } catch (err) { }
  });

  // Feed: Submit post
  document.getElementById('btn-submit-feed-post')?.addEventListener('click', async (ev) => {
    const btn = ev.currentTarget;
    const input = document.getElementById('feed-post-content');
    const content = input?.value.trim();
    if (!content || btn.disabled) return;   // ignore extra clicks while sending

    const authorId = State.activeStudentId || (State.students[0] ? State.students[0].id : 12);

    btn.disabled = true;
    try {
      await API.post('/api/posts', {
        authorId: authorId,
        content
      });
      input.value = '';
      showToast("Post shared to campus feed!", "success");
      const posts = await API.get('/api/posts');
      State.feedPosts = posts || [];
      renderFeedTimeline();
    } catch (e) {
    } finally {
      btn.disabled = false;
    }
  });


  // Group discussion submit post
  document.getElementById('btn-submit-group-post')?.addEventListener('click', async () => {
    const input = document.getElementById('input-group-post-text');
    const text = input?.value.trim();
    if (!text || !currentModalGroupId || !State.activeStudentId) return;

    try {
      await API.post(`/api/groups/${currentModalGroupId}/posts`, {
        studentId: State.activeStudentId,
        content: text
      });
      input.value = '';
      await refreshGroupPosts(currentModalGroupId);
    } catch (e) { }
  });
  // Direct Message form submit
  document.getElementById('chat-form')?.addEventListener('submit', async (e) => {
    e.preventDefault();
    await sendChatMessage();
  });
}

// Utility: HTML Escaper
function escapeHtml(str) {
  if (!str) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#039;');
}
// ==========================================================================
// Direct Messaging / Chat Logic
// ==========================================================================

// Signed in: you can only message your connections. Local demo: everyone.
function chatContacts() {
  const me = signedInStudentId();
  return State.students.filter(s => s.id !== State.activeStudentId && (!me || isConnected(me, s.id)));
}

function initChatTab() {
  renderChatContacts();

  // Default to first contact if none selected
  const contacts = chatContacts();
  if (contacts.length > 0 && !State.selectedChatPartnerId) {
    selectChatPartner(contacts[0].id);
  } else if (State.selectedChatPartnerId) {
    selectChatPartner(State.selectedChatPartnerId);
  }
}

function renderChatContacts() {
  const container = document.getElementById('chat-partners-list');
  if (!container) return;

  const contacts = chatContacts();

  if (contacts.length === 0) {
    container.innerHTML = signedInStudentId()
      ? '<p class="text-muted small" style="padding:1rem;">Connect with students to message them.</p>'
      : '<p class="text-muted small" style="padding:1rem;">No other students available</p>';
    return;
  }

  container.innerHTML = contacts.map(c => `
    <div class="chat-partner-item ${c.id === State.selectedChatPartnerId ? 'active' : ''}" 
         onclick="selectChatPartner(${c.id})">
      <div class="avatar">${escapeHtml(c.name.charAt(0))}</div>
      <div class="details">
        <div class="name">${nameHtml(c.name, c.id)}</div>
        <div class="dept">${escapeHtml(c.department)} &bull; Year ${c.year}</div>
      </div>
    </div>
  `).join('');
}

async function selectChatPartner(partnerId) {
  State.selectedChatPartnerId = parseInt(partnerId);
  renderChatContacts();

  const partner = State.students.find(s => s.id === State.selectedChatPartnerId);
  if (!partner) return;

  // Update header
  const nameEl = document.getElementById('chat-partner-name');
  const subEl = document.getElementById('chat-partner-sub');
  const avatarEl = document.getElementById('chat-partner-avatar');

  if (nameEl) nameEl.innerText = partner.name;
  if (subEl) subEl.innerText = `${partner.department} (Year ${partner.year}) &bull; ${partner.skills.join(', ')}`;
  if (avatarEl) avatarEl.innerText = partner.name.charAt(0);

  // Load messages
  await loadConversation();

  // Poll for new messages every 3 seconds while on chat
  if (State.chatPollTimer) clearInterval(State.chatPollTimer);
  State.chatPollTimer = setInterval(loadConversation, 3000);
}

async function loadConversation() {
  if (!State.activeStudentId || !State.selectedChatPartnerId) return;

  const thread = document.getElementById('chat-messages-thread');
  if (!thread) return;

  try {
    const messages = await API.get(`/api/messages/conversation?user1=${State.activeStudentId}&user2=${State.selectedChatPartnerId}`);

    if (!messages || messages.length === 0) {
      thread.innerHTML = `
        <div class="chat-empty-state">
          <p>No messages yet. Say hello to start the conversation! 👋</p>
        </div>
      `;
      return;
    }

    const wasAtBottom = thread.scrollHeight - thread.clientHeight <= thread.scrollTop + 40;

    thread.innerHTML = messages.map(m => {
      const isSent = m.senderId === State.activeStudentId;
      return `
        <div class="message-bubble ${isSent ? 'sent' : 'received'}">
          <div class="msg-content">${escapeHtml(m.content)}</div>
          <div class="msg-meta">${escapeHtml(m.timestamp)}</div>
        </div>
      `;
    }).join('');

    // Auto-scroll to bottom on first load or if already at bottom
    if (wasAtBottom || messages.length > 0) {
      thread.scrollTop = thread.scrollHeight;
    }
  } catch (err) {
    console.error("Failed to load conversation:", err);
  }
}

async function sendChatMessage() {
  const input = document.getElementById('chat-input');
  const content = input?.value.trim();

  if (!content) return;
  if (!State.activeStudentId || !State.selectedChatPartnerId) {
    showToast("Select a contact to chat with first", "warning");
    return;
  }

  try {
    await API.post('/api/messages', {
      senderId: State.activeStudentId,
      receiverId: State.selectedChatPartnerId,
      content
    });

    input.value = '';
    await loadConversation();
  } catch (err) {
    showToast("Failed to send message", "error");
  }
}
/* ==========================================================================
   Microsoft Outlook (@geu.ac.in) Authentication Logic
   ========================================================================== */
const MSAL_CONFIG = {
  auth: {
    clientId: "690b6eff-568c-4b04-8f26-3e27c1089010",
    authority: "https://login.microsoftonline.com/1490b17d-5dc9-4cbf-aeba-a2e854f521b8",
    redirectUri: window.location.origin
  },
  cache: {
    cacheLocation: "sessionStorage",
    storeAuthStateInCookie: false
  }
};

let msalInstance = null;

function initMicrosoftAuth() {
  if (typeof msal === "undefined") {
    console.warn("MSAL library not loaded yet.");
    return;
  }

  try {
    msalInstance = new msal.PublicClientApplication(MSAL_CONFIG);
  } catch (e) {
    console.error("Failed to initialize MSAL:", e);
    return;
  }

  const btnLogin = document.getElementById("btn-ms-login");
  const btnLogout = document.getElementById("btn-ms-logout");

  if (btnLogin) {
    btnLogin.addEventListener("click", handleMicrosoftLogin);
  }
  if (btnLogout) {
    btnLogout.addEventListener("click", handleMicrosoftLogout);
  }

  checkExistingMsSession();
}

async function handleMicrosoftLogin() {
  if (!msalInstance) return;

  try {
    const loginResponse = await msalInstance.loginPopup({
      scopes: ["user.read", "openid", "profile", "email"],
      prompt: "select_account"
    });

    const account = loginResponse.account;
    const email = (account.username || "").toLowerCase();
    const name = account.name || "Student";

    // Verify official Graphic Era University student domain
    if (!email.endsWith("@geu.ac.in") && !email.endsWith("@graphicera.ac.in")) {
      showToast("Access Restricted: Please sign in with your official @geu.ac.in student email.", "error");
      await msalInstance.logoutPopup();
      return;
    }

    onMicrosoftAuthSuccess(name, email);
  } catch (err) {
    if (err.name !== "BrowserAuthError" || !err.message.includes("user_cancelled")) {
      console.error("Microsoft login error:", err);
      showToast("Microsoft login failed: " + (err.message || err), "error");
    }
  }
}

async function onMicrosoftAuthSuccess(name, email) {
  sessionStorage.setItem("geu_student_email", email);
  sessionStorage.setItem("geu_student_name", name);

  updateMsAuthUI(name, email);
  showToast(`Welcome, ${name}! Signed in via Graphic Era Outlook 🎓`, "success");

  // Ask the server who we are. It verifies the Microsoft token and returns
  // our profile, creating it on the first sign-in. (Emails are no longer in
  // the public student list, so we can't match by email in the browser.)
  try {
    const me = await API.post("/api/students", {
      name: name,
      email: email,
      department: "CSE",
      year: 2,
      skills: ["C++", "Python", "DSA"],
      interests: ["Web Dev", "AI"]
    });
    sessionStorage.setItem("geu_student_id", String(me.id));
    await refreshAllData();
    setActiveStudent(me.id);
  } catch (e) {
    console.warn("Could not load your student profile:", e);
  }
}

function checkExistingMsSession() {
  const savedEmail = sessionStorage.getItem("geu_student_email");
  const savedName = sessionStorage.getItem("geu_student_name");
  if (savedEmail && savedName) {
    updateMsAuthUI(savedName, savedEmail);
  }
}

function updateMsAuthUI(name, email) {
  const btnLogin = document.getElementById("btn-ms-login");
  const profileDiv = document.getElementById("ms-user-profile");
  const nameSpan = document.getElementById("ms-user-name");

  if (btnLogin) btnLogin.style.display = "none";
  if (profileDiv) profileDiv.style.display = "inline-flex";
  if (nameSpan) nameSpan.textContent = name;
}

function handleMicrosoftLogout() {
  // Removing these also stops getIdToken() from attaching the token
  sessionStorage.removeItem("geu_student_email");
  sessionStorage.removeItem("geu_student_name");
  sessionStorage.removeItem("geu_student_id");

  const btnLogin = document.getElementById("btn-ms-login");
  const profileDiv = document.getElementById("ms-user-profile");

  if (btnLogin) btnLogin.style.display = "inline-flex";
  if (profileDiv) profileDiv.style.display = "none";

  showToast("Signed out of Graphic Era Outlook.", "info");
}