/* Optional PS2-style cursor orbs. Reduced-motion and touch defaults are off. */
(function () {
  "use strict";
  var button = document.getElementById("orb-toggle");
  var reduced = window.matchMedia("(prefers-reduced-motion: reduce)");
  var coarse = window.matchMedia("(pointer: coarse)");
  var key = "sms.docs.cursor-orbs", saved = null;
  try { saved = localStorage.getItem(key); } catch (_) { /* Storage is optional. */ }
  var enabled = !reduced.matches && !coarse.matches && saved !== "off";
  var canvas = document.createElement("canvas");
  canvas.className = "ps2-orbs-canvas"; canvas.setAttribute("aria-hidden", "true");
  document.body.appendChild(canvas);
  var context = canvas.getContext("2d"), frame = null, width, height, scale, x = -300, y = -300;
  if (!context) { button.hidden = true; return; }
  function resize() {
    width = innerWidth; height = innerHeight; scale = Math.min(devicePixelRatio || 1, 2);
    canvas.width = width * scale; canvas.height = height * scale;
    canvas.style.width = width + "px"; canvas.style.height = height + "px";
    context.setTransform(scale, 0, 0, scale, 0, 0);
  }
  function draw(time) {
    context.clearRect(0, 0, width, height);
    for (var i = 0; i < 7; i++) {
      var angle = time / (1200 + i * 180) + i * 0.9;
      var radius = 20 + i * 7;
      var px = x + Math.cos(angle) * radius, py = y + Math.sin(angle) * radius * 0.45;
      var glow = context.createRadialGradient(px, py, 0, px, py, 9);
      glow.addColorStop(0, "rgba(180,240,255,.9)"); glow.addColorStop(.25, "rgba(0,170,255,.6)"); glow.addColorStop(1, "rgba(0,80,240,0)");
      context.fillStyle = glow; context.beginPath(); context.arc(px, py, 9, 0, Math.PI * 2); context.fill();
    }
    frame = requestAnimationFrame(draw);
  }
  function update() {
    if (frame !== null) cancelAnimationFrame(frame);
    frame = null; canvas.hidden = !enabled || document.hidden;
    button.setAttribute("aria-pressed", String(enabled));
    button.setAttribute("aria-label", enabled ? "Disable cursor orbs" : "Enable cursor orbs");
    button.querySelector(".orb-toggle-label").textContent = enabled ? "Orbs on" : "Orbs off";
    if (!canvas.hidden) frame = requestAnimationFrame(draw);
    else context.clearRect(0, 0, width, height);
  }
  button.addEventListener("click", function () {
    enabled = !enabled;
    try { localStorage.setItem(key, enabled ? "on" : "off"); } catch (_) { /* Storage is optional. */ }
    update();
  });
  window.addEventListener("pointermove", function (event) { if (event.pointerType !== "touch") { x = event.clientX; y = event.clientY; } }, {passive:true});
  window.addEventListener("resize", resize);
  document.addEventListener("visibilitychange", update);
  if (reduced.addEventListener) reduced.addEventListener("change", function () { if (reduced.matches) { enabled = false; update(); } });
  resize(); update();
})();
