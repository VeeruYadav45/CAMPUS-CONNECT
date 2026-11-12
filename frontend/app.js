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
  chatPollTimer: null
};

// --- API Helpers ---
const API = {
  async get(endpoint) {
    try {
      const res = await fetch(endpoint);
      const json = await res.json();
      if (!res.ok || !json.ok) throw new Error(json.error || 'Server error');
      return json.data;
    } catch (err) {
      console.error(`GET ${endpoint} failed:`, err);
      showToast(err.message, 'error');
      throw err;
    }
  },

  async post(endpoint, body) {
    try {
      const res = await fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
      });
      const json = await res.json();
      if (!res.ok || !json.ok) throw new Error(json.error || 'Request failed');
      return json.data;
    } catch (err) {
      console.error(`POST ${endpoint} failed:`, err);
      showToast(err.message, 'error');
      throw err;
    }
  }
};

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
    // Default acting user to the first student if available
    if (State.students.length > 0) {
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

    updateCounters();
    populateSelectDropdowns();
    renderStudentsGrid();
    renderProjectPostsList();
    renderGroupsGrid();
    renderFeedTimeline();

    if (!State.activeStudentId && State.students.length > 0) {
      State.activeStudentId = State.students[0].id;
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
    `<option value="${s.id}">#${s.id} ${escapeHtml(s.name)} (${s.department} Y${s.year})</option>`
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
          <div class="student-avatar">${escapeHtml(s.name.charAt(0))}</div>
          <div class="student-info">
            <div class="student-name">${escapeHtml(s.name)} ${isActive ? '<span class="badge-pill">You</span>' : ''}</div>
            <div class="student-meta">${s.department} &bull; Year ${s.year} &bull; ID: #${s.id}</div>
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
            ${!isActive ? `
              <button class="btn btn-secondary btn-sm" onclick="connectWithStudent(${s.id})">
                + Connect
              </button>
            ` : `<span class="text-muted small">Current active user</span>`}
          </div>
        </div>
      </div>
    `;
  }).join('');
}

window.connectWithStudent = async function (targetId) {
  if (!State.activeStudentId) return;
  try {
    await API.post('/api/connections', {
      a: State.activeStudentId,
      b: targetId
    });
    showToast(`Connected with Student #${targetId}!`, 'success');
    await refreshAllData();
  } catch (e) {
    // Error handled in API
  }
};

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
      activeFriendsContainer.innerHTML = friends.map(f => `
        <div class="friend-item-row">
          <span>#${f.id} <strong>${escapeHtml(f.name)}</strong> (${f.department})</span>
          <span class="badge-pill">Direct</span>
        </div>
      `).join('');
    }
  } catch (e) {
    // Error handled
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
          Posted by <strong>${escapeHtml(author ? author.name : (project.authorName || 'Student #' + aId))}</strong> &bull;
          Needed Team Size: <strong>${project.teamSizeNeeded || 3}</strong>
        </div>
      </div>
      <div>
        <button class="btn btn-primary" id="btn-create-project-group" onclick="createGroupFromTopK(${projectId})">
          🚀 Assemble Project Group
        </button>
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
              <div class="student-name">${escapeHtml(sName)}</div>
              <div class="student-meta">${sDept} &bull; Year ${sYear}</div>
              
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
                <label style="font-size:0.75rem; color:var(--text-muted); cursor:pointer;">
                  <input type="checkbox" class="candidate-checkbox" value="${sId}" checked>
                  Include in Team Group
                </label>
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
          ${isMember ? `
            <button class="btn btn-secondary btn-sm" style="color:var(--danger);" onclick="leaveGroup(${g.id})">
              Leave Group
            </button>
          ` : `
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

    postsList.innerHTML = posts.map(p => {
      const author = State.students.find(s => s.id === p.studentId);
      return `
        <div class="feed-post-card" style="padding:0.85rem;">
          <div class="post-header">
            <div class="user-avatar-sm" style="width:26px; height:26px; font-size:0.75rem;">${author ? author.name.charAt(0) : 'U'}</div>
            <span class="post-author">${escapeHtml(author ? author.name : 'Student #' + p.studentId)}</span>
            <span class="post-timestamp">${p.timestamp || ''}</span>
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

  timeline.innerHTML = State.feedPosts.map(p => {
    const author = State.students.find(s => s.id === p.studentId);
    return `
      <div class="feed-post-card">
        <div class="post-header">
          <div class="user-avatar-sm">${author ? author.name.charAt(0) : 'U'}</div>
          <div>
            <div class="post-author">${escapeHtml(author ? author.name : 'Student #' + p.studentId)}</div>
            <div class="post-timestamp">${p.timestamp || ''}</div>
          </div>
        </div>
        <div class="post-body">${escapeHtml(p.content)}</div>
      </div>
    `;
  }).join('');
}

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
      await API.post('/api/connections', { a, b });
      showToast("Connection established!", "success");
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
    const leaderId = parseInt(document.getElementById('proj-leader').value);
    const skills = document.getElementById('proj-skills').value.split(',').map(s => s.trim()).filter(Boolean);
    const size = parseInt(document.getElementById('proj-size').value);

    try {
      const post = await API.post('/api/project-posts', {
        studentId: leaderId,
        title: title,
        requiredSkills: skills,
        teamSize: size
      });
      showToast(`Project "${title}" posted!`, "success");
      document.getElementById('modal-create-project').classList.add('hidden');
      e.target.reset();
      await refreshAllData();
      selectProjectPost(post.id);
    } catch (err) { }
  });

  // Form: Create Group
  document.getElementById('form-create-group')?.addEventListener('submit', async (e) => {
    e.preventDefault();
    const name = document.getElementById('grp-name').value;
    const desc = document.getElementById('grp-desc').value;
    const adminId = parseInt(document.getElementById('grp-admin').value);

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
  document.getElementById('btn-submit-feed-post')?.addEventListener('click', async () => {
    const input = document.getElementById('feed-post-content');
    const content = input?.value.trim();
    if (!content || !State.activeStudentId) return;

    try {
      await API.post('/api/posts', {
        studentId: State.activeStudentId,
        content
      });
      input.value = '';
      showToast("Post shared to campus feed!", "success");
      const posts = await API.get('/api/posts');
      State.feedPosts = posts || [];
      renderFeedTimeline();
    } catch (e) { }
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

function initChatTab() {
  renderChatContacts();

  // Default to first contact if none selected
  const contacts = State.students.filter(s => s.id !== State.activeStudentId);
  if (contacts.length > 0 && !State.selectedChatPartnerId) {
    selectChatPartner(contacts[0].id);
  } else if (State.selectedChatPartnerId) {
    selectChatPartner(State.selectedChatPartnerId);
  }
}

function renderChatContacts() {
  const container = document.getElementById('chat-partners-list');
  if (!container) return;

  const contacts = State.students.filter(s => s.id !== State.activeStudentId);

  if (contacts.length === 0) {
    container.innerHTML = '<p class="text-muted small" style="padding:1rem;">No other students available</p>';
    return;
  }

  container.innerHTML = contacts.map(c => `
    <div class="chat-partner-item ${c.id === State.selectedChatPartnerId ? 'active' : ''}" 
         onclick="selectChatPartner(${c.id})">
      <div class="avatar">${escapeHtml(c.name.charAt(0))}</div>
      <div class="details">
        <div class="name">${escapeHtml(c.name)}</div>
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

  // Check if a student with this name already exists in DataStore
  let matchedStudent = State.students.find(s =>
    s.name.toLowerCase() === name.toLowerCase()
  );

  if (matchedStudent) {
    setActiveStudent(matchedStudent.id);
  } else {
    // If first time logging in, auto-create a verified student profile
    try {
      const newStudent = await API.post("/api/students", {
        name: name,
        department: "CSE",
        year: 2,
        skills: ["C++", "Python", "DSA"],
        interests: ["Web Dev", "AI"]
      });
      await refreshAllData();
      setActiveStudent(newStudent.id);
      showToast(`Verified student profile created for ${name}!`, "info");
    } catch (e) {
      console.warn("Could not auto-register student profile:", e);
    }
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
  sessionStorage.removeItem("geu_student_email");
  sessionStorage.removeItem("geu_student_name");

  const btnLogin = document.getElementById("btn-ms-login");
  const profileDiv = document.getElementById("ms-user-profile");

  if (btnLogin) btnLogin.style.display = "inline-flex";
  if (profileDiv) profileDiv.style.display = "none";

  showToast("Signed out of Graphic Era Outlook.", "info");
}