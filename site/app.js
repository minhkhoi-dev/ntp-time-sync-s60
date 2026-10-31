// NTP Time Sync's introduction page: the latest release and the sync demo. Shared by both languages.
(function () {
  "use strict";
  var REPO = "minhkhoi-dev/ntp-time-sync-s60";

  // The newest release: its version and a direct link to its .sisx.
  if (window.fetch) {
    fetch("https://api.github.com/repos/" + REPO + "/releases/latest", { headers: { Accept: "application/vnd.github+json" } })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (rel) {
        if (!rel || !rel.tag_name) return;
        var version = rel.tag_name.replace(/^v/, "");
        var sisx = (rel.assets || []).filter(function (a) { return /\.sisx$/i.test(a.name); })[0];
        document.querySelectorAll(".js-ver").forEach(function (el) { el.textContent = (el.getAttribute("data-fmt") || "%s").replace("%s", version); });
        if (sisx) {
          document.querySelectorAll(".js-dl").forEach(function (el) { el.href = sisx.browser_download_url; });
          document.querySelectorAll(".js-file").forEach(function (el) { el.textContent = sisx.name; });
        }
      })
      .catch(function () {});
  }

  // The demo: a phone whose clock is off, and the app putting it right.
  var clock = document.getElementById("clock");
  var capClock = document.getElementById("capClock");
  var capState = document.getElementById("capState");
  var cap = document.getElementById("cap");
  var rowState = document.getElementById("rowState");
  var rowLast = document.getElementById("rowLast");
  var dpad = document.getElementById("dpad");
  if (!clock || !capClock || !capState || !cap || !rowState || !rowLast) return;

  var T = document.documentElement.lang === "en"
    ? { running: "Running", syncing: "Syncing…", corrected: "%t, corrected by %o", right: "%t, clock was right (%o)" }
    : { running: "Đang chạy", syncing: "Đang đồng bộ…", corrected: "%t, đã chỉnh %o", right: "%t, giờ đã đúng (%o)" };

  function two(n) { return ("0" + n).slice(-2); }
  function hm(d) { return two(d.getHours()) + ":" + two(d.getMinutes()); }
  // The time as the app shows it: HH:MM today, else DD/MM HH:MM.
  function fmtTime(d) {
    var now = new Date();
    if (d.toDateString() === now.toDateString()) return hm(d);
    return two(d.getDate()) + "/" + two(d.getMonth() + 1) + " " + hm(d);
  }
  // The offset as the app shows it: a sign, seconds with 3 decimals, " s".
  function fmtOffset(s) { return (s < 0 ? "-" : "+") + Math.abs(s).toFixed(3) + " s"; }
  function row(tpl, d, off) { return tpl.replace("%t", fmtTime(d)).replace("%o", fmtOffset(off)); }

  var drift = 0; // phone time = real time + drift, in seconds
  function phoneNow() { return new Date(Date.now() + drift * 1000); }
  function randomDrift() {
    var s = 90 + Math.random() * 210;
    return (Math.random() < .5 ? -s : s) + Math.round(Math.random() * 1000) / 1000;
  }

  function tick() {
    var t = phoneNow();
    clock.textContent = hm(t);
    capClock.textContent = hm(t) + ":" + two(t.getSeconds());
  }
  function showDrift() {
    var tpl = cap.getAttribute(drift > 0 ? "data-fast" : "data-slow") || "";
    capState.textContent = tpl.replace("%s", Math.round(Math.abs(drift)));
    capState.classList.remove("ok");
  }
  function showOk() {
    capState.textContent = cap.getAttribute("data-ok") || "";
    capState.classList.add("ok");
  }
  function flash(el) {
    el.classList.remove("flash");
    void el.offsetWidth;
    el.classList.add("flash");
    setTimeout(function () { el.classList.remove("flash"); }, 650);
  }
  // The clock is put right; old is the drift it had.
  function correct(old, withFlash) {
    drift = 0;
    rowState.textContent = T.running;
    rowLast.textContent = row(T.corrected, new Date(), -old);
    showOk();
    tick();
    if (withFlash) { flash(clock); flash(rowLast); }
  }

  var reduced = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  setInterval(tick, 1000);

  if (reduced) {
    correct(randomDrift(), false);
    return;
  }

  drift = randomDrift();
  rowState.textContent = T.running;
  rowLast.textContent = row(T.right, new Date(Date.now() - 6 * 3600 * 1000), -0.391);
  showDrift();
  tick();

  var visible = true;
  if ("IntersectionObserver" in window) {
    new IntersectionObserver(function (e) { visible = e[0].isIntersecting; }).observe(clock);
  }
  function wait(ms) {
    return new Promise(function (done) {
      (function check() {
        setTimeout(function () { (visible && !document.hidden) ? done() : check(); }, ms);
        ms = 250;
      })();
    });
  }

  async function play() {
    for (;;) {
      await wait(2500);
      if (dpad) {
        dpad.classList.add("on");
        setTimeout(function () { dpad.classList.remove("on"); }, 200);
      }
      rowState.textContent = T.syncing;
      await wait(1800);
      correct(drift, true);
      await wait(6000);
      // as if something else moved the clock
      drift = randomDrift();
      tick();
      showDrift();
    }
  }
  play();
})();
