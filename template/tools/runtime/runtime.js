/* Litt Play Runtime - turns generated worlds into playable games.
 * Consumes exactly what the generators emit:
 *   /world_state.json            -> palette, gameplay physics, identity, environment
 *   /assets/scenes/world.lscn.json -> nodes (position, quaternion, tags incl model:<name>)
 *   /assets/models/*.obj|.mtl    -> triangles + Kd colors
 * Camera/movement mode derives from state.identity; physics constants come
 * verbatim from state.gameplay.physics when present (gravity, jump_velocity,
 * run_speed, coyote_time_s, jump_buffer_s). */
(function () {
  "use strict";

  // ------------------------------------------------------------ obj/mtl
  function parseMTL(text) {
    var mats = {}; var cur = null;
    text.split(/\r?\n/).forEach(function (line) {
      line = line.trim();
      if (line.indexOf("newmtl ") === 0) { cur = line.slice(7); mats[cur] = [0.7, 0.7, 0.7]; }
      else if (cur && line.indexOf("Kd ") === 0) {
        var p = line.slice(3).trim().split(/\s+/);
        mats[cur] = [parseFloat(p[0]), parseFloat(p[1]), parseFloat(p[2])];
      }
    });
    return mats;
  }

  function parseOBJ(text) {
    var vs = [], ns = [], groups = [], cur = null;
    function begin(mat) { cur = { mat: mat, pos: [], nor: [] }; groups.push(cur); }
    begin("default");
    text.split(/\r?\n/).forEach(function (line) {
      var t = line.trim();
      if (t.charAt(0) === "#") return;
      var p = t.split(/\s+/);
      if (p[0] === "v") { vs.push(parseFloat(p[1]), parseFloat(p[2]), parseFloat(p[3])); }
      else if (p[0] === "vn") { ns.push(parseFloat(p[1]), parseFloat(p[2]), parseFloat(p[3])); }
      else if (p[0] === "usemtl") { begin(p[1]); }
      else if (p[0] === "f") {
        var idx = [];
        for (var i = 1; i < p.length; i++) {
          var parts = p[i].split("//");
          idx.push([parseInt(parts[0], 10) - 1, parseInt(parts[1], 10) - 1]);
        }
        for (var k = 1; k < idx.length - 1; k++) {
          var tri = [idx[0], idx[k], idx[k + 1]];
          for (var j = 0; j < 3; j++) {
            var vi = tri[j][0] * 3, ni = tri[j][1] * 3;
            cur.pos.push(vs[vi], vs[vi + 1], vs[vi + 2]);
            cur.nor.push(ns[ni], ns[ni + 1], ns[ni + 2]);
          }
        }
      }
    });
    return groups.filter(function (g) { return g.pos.length > 0; });
  }

  var matCache = {};
  function objToGroup(url, mtlMats) {
    var group = new THREE.Group();
    return fetch(url).then(function (r) { return r.text(); }).then(function (txt) {
      parseOBJ(txt).forEach(function (g) {
        var geo = new THREE.BufferGeometry();
        geo.setAttribute("position", new THREE.Float32BufferAttribute(g.pos, 3));
        geo.setAttribute("normal", new THREE.Float32BufferAttribute(g.nor, 3));
        var kd = mtlMats[g.mat] || [0.7, 0.7, 0.7];
        var key = kd.join(",");
        var mesh = new THREE.Mesh(geo, matCache[key] ||
          (matCache[key] = new THREE.MeshLambertMaterial({ color: new THREE.Color(kd[0], kd[1], kd[2]) })));
        group.add(mesh);
      });
      return group;
    });
  }

  function quatYaw(q) {
    return Math.atan2(2 * (q[3] * q[1] + q[0] * q[2]), 1 - 2 * (q[1] * q[1] + q[2] * q[2]));
  }

  // ------------------------------------------------------------- helpers
  function has(list, word) { return (list || []).indexOf(word) !== -1; }
  function hasSub(str, sub) { return (str || "").toLowerCase().indexOf(sub) !== -1; }

  window.LittPlay = { init: init, parseOBJ: parseOBJ, parseMTL: parseMTL };

  function init() {
    Promise.all([
      fetch("../world_state.json").then(function (r) { return r.json(); }),
      fetch("../assets/scenes/world.lscn.json").then(function (r) { return r.json(); }),
      fetch("../assets/models/materials.mtl").then(function (r) { return r.text(); })
    ]).then(function (all) { boot(all[0], all[1], parseMTL(all[2])); });
  }

  function boot(state, scene, mtlMats) {
    var id = state.identity || {};
    var gp = state.gameplay || {};
    var env = state.environment || {};
    var phys = gp.physics || {};
    var G = phys.gravity || 22, JUMPV = phys.jump_velocity || 8,
        RUN = phys.run_speed || 7, COYOTE = phys.coyote_time_s || 0.1;

    var mode = "3D";
    if (hasSub(id.movement, "platformer") || hasSub(id.camera, "side") || gp.genre === "platformer_2_5d") mode = "2D5";
    else if (hasSub(id.camera, "top_down") || hasSub(id.camera, "isometric")) mode = "TOP";

    // renderer + scene
    var renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setSize(window.innerWidth, window.innerHeight);
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    document.body.appendChild(renderer.domElement);
    var sc = new THREE.Scene();
    var skyTop = env.sky ? env.sky.top_color : null;
    var bgCol = skyTop ? new THREE.Color(skyTop[0], skyTop[1], skyTop[2]) : new THREE.Color(0x87b7d4);
    sc.background = bgCol;
    if (env.fog && env.fog.density) {
      var fc = env.fog.color || [0.6, 0.65, 0.7];
      sc.fog = new THREE.FogExp2(new THREE.Color(fc[0], fc[1], fc[2]).getHex(), env.fog.density);
    }
    var amb = new THREE.HemisphereLight(0xffffff, 0x445544, 0.9);
    sc.add(amb);
    var sunEl = ((env.sun && env.sun.elevation_deg) || 50) * Math.PI / 180;
    var sunAz = ((env.sun && env.sun.azimuth_deg) || 135) * Math.PI / 180;
    var sun = new THREE.DirectionalLight(0xfff2dd, (env.lighting && env.lighting.global_light_intensity) || 1.0);
    sun.position.set(Math.cos(sunAz) * Math.cos(sunEl), Math.sin(sunEl), Math.sin(sunAz) * Math.cos(sunEl)).multiplyScalar(60);
    sc.add(sun);

    var cam = new THREE.PerspectiveCamera(mode === "TOP" ? 50 : 62, window.innerWidth / window.innerHeight, 0.1, 600);

    // world assembly
    var interactives = [];   // {node, mesh, tags, box}
    var solids = [];         // THREE.Box3 list for platforming
    var pending = [];
    function register(node, obj) {
      obj.position.fromArray(node.position || [0, 0, 0]);
      obj.rotation.y = quatYaw(node.rotation || [0, 0, 0, 1]);
      sc.add(obj);
      var box = new THREE.Box3().setFromObject(obj);
      var tags = node.tags || [];
      if (has(tags, "floor") || has(tags, "level") || has(tags, "board") ||
          has(tags, "track") || has(tags, "hub") || has(tags, "terrain")) solids.push(box);
      if (has(tags, "platform")) solids.push(box);
      if (tags.some(function (t) { return ["pickup", "score", "goal", "hazard", "enemy", "checkpoint", "poi", "objective", "dice", "token", "player", "start", "win"].indexOf(t) !== -1; }))
        interactives.push({ name: node.name, tags: tags, obj: obj, box: box, alive: true });
    }

    (state.chunks || []).forEach(function (c) {
      pending.push(objToGroup("../assets/" + c.path.replace(/^assets\//, ""), mtlMats).then(function (g) {
        g.position.fromArray(c.position || [0, 0, 0]); sc.add(g);
        solids.push(new THREE.Box3().setFromObject(g));
      }).catch(function () {}));
    });
    (scene.nodes || []).forEach(function (node) {
      if (node.id === 0) return;
      var mt = (node.tags || []).filter(function (t) { return t.indexOf("model:") === 0; })[0];
      if (!mt) return;
      var url = "../assets/models/" + mt.slice(6) + ".obj";
      pending.push(objToGroup(url, mtlMats).then(function (g) { register(node, g); }).catch(function () {}));
    });

    // player
    var spawn = new THREE.Vector3(0, 1.2, 4);
    var playerMesh = new THREE.Mesh(
      new THREE.CapsuleGeometry ? new THREE.CapsuleGeometry(0.45, 0.9, 4, 8) : new THREE.CylinderGeometry(0.45, 0.45, 1.6, 10),
      new THREE.MeshLambertMaterial({ color: 0xffd97a }));
    sc.add(playerMesh);
    var vel = new THREE.Vector3(); var pos = spawn.clone();
    var grounded = false, coyote = 0, buffer = 0, camYaw = Math.PI, score = 0, deadUntil = 0, won = false;
    var keys = {};
    addEventListener("keydown", function (e) { keys[e.code] = true; if (e.code === "Space") buffer = COYOTE + 0.02; });
    addEventListener("keyup", function (e) { keys[e.code] = false; });
    addEventListener("mousemove", function (e) { if (document.pointerLockElement) camYaw -= e.movementX * 0.003; });
    addEventListener("click", function () { if (mode === "3D") renderer.domElement.requestPointerLock(); });
    addEventListener("resize", function () {
      cam.aspect = window.innerWidth / window.innerHeight; cam.updateProjectionMatrix();
      renderer.setSize(window.innerWidth, window.innerHeight);
    });

    // hud
    var hud = document.getElementById("hud");
    var objective = gp.objective || "explore the world";
    hud.innerHTML = "<b>" + (gp.genre || state.theme || "litt") + "</b> · " + mode +
      "<br>" + objective +
      "<br>score: <span id=\"sc\">0</span>" +
      "<br><span style=\"opacity:.7\">WASD move · Space jump · click to look</span>";
    function toast(msg, bad) {
      var t = document.getElementById("toast");
      t.textContent = msg; t.style.color = bad ? "#ff9a8a" : "#bfe3f2";
      t.style.opacity = 1; clearTimeout(toast._t);
      toast._t = setTimeout(function () { t.style.opacity = 0; }, 1600);
    }

    function respawn(msg) {
      pos.copy(spawn); vel.set(0, 0, 0);
      if (msg) toast(msg, true);
    }

    function groundAt(x, z, y) {
      var best = -Infinity;
      for (var i = 0; i < solids.length; i++) {
        var b = solids[i];
        if (x >= b.min.x - 0.3 && x <= b.max.x + 0.3 && z >= b.min.z - 0.3 && z <= b.max.z + 0.3) {
          if (b.max.y <= y + 0.6 && b.max.y > best) best = b.max.y;
        }
      }
      return best;
    }

    var clock = new THREE.Clock();
    Promise.all(pending).then(function () { loop(); });

    function loop() {
      requestAnimationFrame(loop);
      var dt = Math.min(clock.getDelta(), 0.05);
      var now = performance.now() / 1000;
      if (now < deadUntil) { renderer.render(sc, cam); return; }

      // input -> planar velocity
      var f = (keys.KeyW ? 1 : 0) - (keys.KeyS ? 1 : 0);
      var s = (keys.KeyD ? 1 : 0) - (keys.KeyA ? 1 : 0);
      var dirX = 0, dirZ = 0;
      if (mode === "2D5") { dirX = f !== 0 || s !== 0 ? (f - s) : 0; dirZ = 0; }
      else if (mode === "TOP") { dirX = s; dirZ = f; }
      else {
        var fx = -Math.sin(camYaw), fz = -Math.cos(camYaw);
        dirX = fx * f - fz * s; dirZ = fz * f + fx * s;
      }
      var len = Math.hypot(dirX, dirZ) || 1;
      vel.x = dirX / len * RUN * (len > 0 ? 1 : 0);
      vel.z = dirZ / len * RUN * (len > 0 ? 1 : 0);

      // vertical
      coyote = grounded ? COYOTE : Math.max(0, coyote - dt);
      buffer = Math.max(0, buffer - dt);
      if (buffer > 0 && coyote > 0) { vel.y = JUMPV; coyote = 0; buffer = 0; }
      vel.y -= G * dt;
      pos.x += vel.x * dt; pos.z += vel.z * dt; pos.y += vel.y * dt;

      var gy = groundAt(pos.x, pos.z, pos.y);
      grounded = false;
      if (gy > -Infinity && pos.y <= gy + 0.05 && vel.y <= 0) { pos.y = gy; vel.y = 0; grounded = true; }
      if (pos.y < -14) respawn("fell into the dark - back to checkpoint");

      // interactions
      for (var i = 0; i < interactives.length; i++) {
        var it = interactives[i];
        if (!it.alive) continue;
        var c = it.box.getCenter(new THREE.Vector3());
        if (has(it.tags, "enemy")) {
          var d = c.distanceTo(pos);
          var aggro = (gp.enemy_aggro_m || 6);
          if (d < aggro && d > 0.1) {
            var push = c.clone().sub(pos).normalize().multiplyScalar(-3.2 * dt);
            it.obj.position.add(push); it.box.setFromObject(it.obj);
          }
          if (d < 1.1) { deadUntil = now + 0.7; respawn(gp.corpse_run ? "you died - corpse run begins" : "caught - respawning"); break; }
        } else if (c.distanceTo(pos) < 1.6) {
          if (has(it.tags, "pickup") || has(it.tags, "score")) {
            it.alive = false; it.obj.visible = false;
            var pts = (gp.scoring && gp.scoring.coins) ? 25 : 10;
            score += pts; document.getElementById("sc").textContent = score;
            toast("+" + pts);
          } else if (has(it.tags, "goal") || has(it.tags, "win")) {
            won = true;
            document.getElementById("win").style.display = "flex";
          } else if (has(it.tags, "checkpoint")) {
            spawn.copy(c); spawn.y += 1.2; toast("checkpoint lit"); it.alive = false;
          } else if (has(it.tags, "poi")) {
            toast(it.name); it.alive = false;
          }
        }
      }

      playerMesh.position.copy(pos);

      // camera
      if (mode === "TOP") {
        cam.position.set(pos.x, 34, pos.z + 12); cam.lookAt(pos.x, pos.y, pos.z);
      } else if (mode === "2D5") {
        cam.position.set(pos.x + 2, pos.y + 6, 16); cam.lookAt(pos.x, pos.y + 1, 0);
      } else {
        var cd = 9;
        cam.position.set(pos.x + Math.sin(camYaw) * cd, pos.y + 4.5, pos.z + Math.cos(camYaw) * cd);
        cam.lookAt(pos.x, pos.y + 1.4, pos.z);
      }
      renderer.render(sc, cam);
    }
  }
})();