/**
 * Campus Connect - Interactive Cat Companion
 * - Wanders gently along the bottom of the page (idle + walk cycles)
 * - Click anywhere on the page and the cat sprints over to you (run cycle)
 * - Click directly on the cat to pet it (meow & heart reaction)
 */

(function() {
  // Sprite sizing: 6 columns x 3 rows in cat_normalized.png
  // Displayed at 2.5x pixel scale (60px x 50px)
  const FRAME_WIDTH = 60;
  const FRAME_HEIGHT = 50;

  // Animation metadata:
  // Row 0: Idle (5 frames: 0, 1, 2, 3, 4)
  // Row 1: Walk (6 frames: 0, 1, 2, 3, 4, 5)
  // Row 2: Run  (6 frames: 0, 1, 2, 3, 4, 5)
  const ANIMATIONS = {
    idle: { row: 0, frames: 5, fps: 3.5 },
    walk: { row: 1, frames: 6, fps: 8.5 },
    run:  { row: 2, frames: 6, fps: 14 }
  };

  const WALK_SPEED = 65;  // px / sec
  const RUN_SPEED  = 240; // px / sec

  // Create DOM Elements
  const wrapper = document.createElement('div');
  wrapper.className = 'cat-companion-wrapper';
  wrapper.setAttribute('title', 'Click me to pet, or click anywhere on the page and I will run to you! 🐾');

  const sprite = document.createElement('div');
  sprite.className = 'cat-sprite';

  const bubble = document.createElement('div');
  bubble.className = 'cat-speech-bubble';
  bubble.innerText = 'Meow! 🐾';

  wrapper.appendChild(bubble);
  wrapper.appendChild(sprite);
  document.body.appendChild(wrapper);

  let targetIndicator = null;
  let bubbleTimeout = null;

  // Initial State
  let x = Math.max(50, Math.min(window.innerWidth - 100, window.innerWidth * 0.72));
  let targetX = x;
  let currentState = 'idle';
  let facing = 'left'; // original sprite faces left
  let currentFrame = 0;
  let animTimer = 0;
  let lastTime = performance.now();
  let wanderCountdown = 2.5;

  function setFacing(newFacing) {
    facing = newFacing;
    if (facing === 'right') {
      sprite.classList.add('facing-right');
    } else {
      sprite.classList.remove('facing-right');
    }
  }

  function showBubble(text, duration = 2200) {
    bubble.innerText = text;
    bubble.classList.add('visible');
    if (bubbleTimeout) clearTimeout(bubbleTimeout);
    bubbleTimeout = setTimeout(() => {
      bubble.classList.remove('visible');
    }, duration);
  }

  function spawnReaction(emoji) {
    const reaction = document.createElement('div');
    reaction.className = 'cat-floating-reaction';
    reaction.innerText = emoji;
    reaction.style.left = (x + 22) + 'px';
    reaction.style.bottom = '54px';
    document.body.appendChild(reaction);
    setTimeout(() => reaction.remove(), 1000);
  }

  function setTarget(newTargetX, isRun = false) {
    targetX = Math.max(10, Math.min(window.innerWidth - 70, newTargetX));
    const dist = targetX - x;

    if (Math.abs(dist) > 10) {
      setFacing(dist > 0 ? 'right' : 'left');
      currentState = isRun ? 'run' : 'walk';
      currentFrame = 0;
      animTimer = 0;

      if (isRun) {
        if (!targetIndicator) {
          targetIndicator = document.createElement('div');
          targetIndicator.className = 'cat-target-indicator';
          document.body.appendChild(targetIndicator);
        }
        targetIndicator.style.left = (targetX + 30) + 'px';
        targetIndicator.style.display = 'block';
      }
    } else {
      currentState = 'idle';
      if (targetIndicator) targetIndicator.style.display = 'none';
    }
  }

  // Click on the cat directly to pet it
  wrapper.addEventListener('click', (e) => {
    e.stopPropagation();
    const meows = [
      'Purrrr... ❤️',
      'Meow! 🐾',
      'Prrrrt~ ✨',
      'ฅ^•ﻌ•^ฅ',
      'Happy cat! 💕',
      'Nyah! 🧶'
    ];
    const text = meows[Math.floor(Math.random() * meows.length)];
    showBubble(text);
    spawnReaction(Math.random() > 0.4 ? '❤️' : '🐾');
    currentState = 'idle';
    currentFrame = 3; // sit/curl frame
    wanderCountdown = 4.0;
    if (targetIndicator) targetIndicator.style.display = 'none';
  });

  // Click anywhere on the webpage -> run over!
  document.addEventListener('click', (e) => {
    // If user clicked inside a form input/button, allow the click normally and summon the cat
    setTarget(e.clientX - 30, true);
    wanderCountdown = 5.0; // Don't wander while responding to user
  });

  // Main animation / physics loop
  function loop(now) {
    const dt = Math.min((now - lastTime) / 1000, 0.1);
    lastTime = now;

    const anim = ANIMATIONS[currentState];

    // Advance frame
    animTimer += dt;
    const frameInterval = 1 / anim.fps;
    if (animTimer >= frameInterval) {
      animTimer -= frameInterval;
      currentFrame = (currentFrame + 1) % anim.frames;
    }

    // Movement logic
    if (currentState === 'walk' || currentState === 'run') {
      const speed = currentState === 'run' ? RUN_SPEED : WALK_SPEED;
      const dist = targetX - x;
      const step = speed * dt;

      if (Math.abs(dist) <= step) {
        // Reached destination!
        x = targetX;
        currentState = 'idle';
        currentFrame = 0;
        animTimer = 0;
        if (targetIndicator) targetIndicator.style.display = 'none';
        
        // Brief happy arrival purr if user called
        wanderCountdown = 3.0 + Math.random() * 2.5;
      } else {
        x += Math.sign(dist) * step;
      }
    } else {
      // Idle / wander decision timer
      wanderCountdown -= dt;
      if (wanderCountdown <= 0) {
        wanderCountdown = 3.0 + Math.random() * 4.0;
        if (Math.random() < 0.65) {
          // Wander to a random nearby location along bottom
          const wanderDistance = (Math.random() * 320 - 160);
          setTarget(x + wanderDistance, false);
        } else {
          // Stay idle, change pose
          currentState = 'idle';
          currentFrame = Math.floor(Math.random() * anim.frames);
        }
      }
    }

    // Clamp inside viewport
    x = Math.max(10, Math.min(window.innerWidth - 70, x));

    // Update DOM transform and background position
    wrapper.style.transform = `translate3d(${x}px, 0, 0)`;
    sprite.style.backgroundPosition = `-${currentFrame * FRAME_WIDTH}px -${anim.row * FRAME_HEIGHT}px`;

    requestAnimationFrame(loop);
  }

  // Handle screen resize
  window.addEventListener('resize', () => {
    x = Math.max(10, Math.min(window.innerWidth - 70, x));
  });

  // Start loop
  requestAnimationFrame(loop);
})();

