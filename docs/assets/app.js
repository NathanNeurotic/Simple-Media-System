/* SMS manual: local search, keyboard navigation, mobile menu and page contents.
 * No analytics, external scripts, fonts or runtime service dependencies. */
(function () {
  "use strict";
  document.documentElement.classList.add("js");
  var input = document.getElementById("q"), results = document.getElementById("results");
  var status = document.getElementById("search-status"), selected = -1;
  var index = Array.isArray(window.SMS_DOC_INDEX) ? window.SMS_DOC_INDEX : null;
  var menu = document.querySelector(".menu-btn"), sidebar = document.getElementById("site-nav");
  var header = document.querySelector(".topbar");
  function sizeHeader() {
    document.documentElement.style.setProperty("--sms-header-height", Math.ceil(header.getBoundingClientRect().height) + "px");
  }
  sizeHeader();
  window.addEventListener("resize", sizeHeader);
  if (window.ResizeObserver) new ResizeObserver(sizeHeader).observe(header);

  function escape(text) {
    return String(text).replace(/[&<>"']/g, function (c) {
      return {"&":"&amp;", "<":"&lt;", ">":"&gt;", '"':"&quot;", "'":"&#39;"}[c];
    });
  }
  function closeSearch() {
    results.classList.remove("open");
    input.setAttribute("aria-expanded", "false");
    selected = -1;
  }
  function query() {
    var terms = input.value.toLowerCase().trim().split(/\s+/).filter(Boolean);
    selected = -1;
    if (!terms.length) { closeSearch(); results.textContent = ""; status.textContent = ""; return; }
    results.classList.add("open"); input.setAttribute("aria-expanded", "true");
    if (!index) {
      results.innerHTML = '<p class="res-empty">Search data is unavailable. Use the navigation or <a href="wiki.html">guide index</a>.</p>';
      status.textContent = "Search data is unavailable";
      return;
    }
    var hits = [];
    index.forEach(function (row) {
      var title = row.title.toLowerCase();
      var content = (title + " " + row.cat + " " + row.text).toLowerCase();
      if (!terms.every(function (term) { return content.indexOf(term) !== -1; })) return;
      var score = terms.reduce(function (sum, term) {
        return sum + (title === term ? 100 : title.indexOf(term) === 0 ? 50 : title.indexOf(term) !== -1 ? 25 : 2);
      }, row.url.indexOf("#") === -1 ? 0 : 3);
      hits.push({row: row, score: score});
    });
    hits.sort(function (a, b) { return b.score - a.score || a.row.title.localeCompare(b.row.title); });
    hits = hits.slice(0, 30);
    status.textContent = hits.length ? hits.length + " search results" : "No search results";
    if (!hits.length) { results.innerHTML = '<p class="res-empty">No matches for “' + escape(input.value) + '”.</p>'; return; }
    results.innerHTML = hits.map(function (hit) {
      var row = hit.row, content = row.text || "", pos = content.toLowerCase().indexOf(terms[0]);
      var start = Math.max(0, pos - 45), excerpt = content.slice(start, start + 180);
      return '<a class="res" href="' + escape(row.url) + '"><span class="t">' + escape(row.title) +
        '</span><span class="c">' + escape(row.cat) + '</span><div class="s">' + (start ? "…" : "") + escape(excerpt) + '</div></a>';
    }).join("");
  }
  input.addEventListener("input", query);
  input.addEventListener("focus", function () { if (input.value.trim()) query(); });
  input.addEventListener("keydown", function (event) {
    var links = results.querySelectorAll("a.res");
    if ((event.key === "ArrowDown" || event.key === "ArrowUp") && links.length) {
      event.preventDefault();
      selected = (selected + (event.key === "ArrowDown" ? 1 : selected < 0 ? 0 : -1) + links.length) % links.length;
      links.forEach(function (link, i) { link.classList.toggle("sel", i === selected); });
      links[selected].scrollIntoView({block:"nearest"});
      status.textContent = links[selected].querySelector(".t").textContent;
    } else if (event.key === "Enter" && links.length && results.classList.contains("open")) {
      event.preventDefault(); links[Math.max(0, selected)].click();
    } else if (event.key === "Escape") closeSearch();
  });
  function closeMenu(focus) {
    sidebar.classList.remove("open");
    menu.setAttribute("aria-expanded", "false"); menu.setAttribute("aria-label", "Open navigation");
    if (focus) menu.focus();
  }
  menu.addEventListener("click", function () {
    var open = sidebar.classList.toggle("open");
    menu.setAttribute("aria-expanded", String(open)); menu.setAttribute("aria-label", open ? "Close navigation" : "Open navigation");
  });
  sidebar.addEventListener("click", function (event) { if (event.target.closest("a")) closeMenu(false); });
  document.addEventListener("click", function (event) {
    if (!event.target.closest(".search")) closeSearch();
    if (!sidebar.contains(event.target) && !menu.contains(event.target)) closeMenu(false);
  });
  document.addEventListener("keydown", function (event) {
    var target = event.target, editing = /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName) || target.isContentEditable;
    if (event.key === "/" && !editing && !event.ctrlKey && !event.altKey && !event.metaKey) { event.preventDefault(); input.focus(); }
    if (event.key === "Escape" && sidebar.classList.contains("open")) closeMenu(true);
  });

  var toc = document.getElementById("toc-nav"), links = {};
  var headings = Array.prototype.slice.call(document.querySelectorAll("main h2, main h3"))
    .filter(function (h) { return !h.closest(".card, .step, details"); });
  if (headings.length < 2) document.querySelector(".toc").classList.add("hide");
  headings.forEach(function (heading) {
    var link = document.createElement("a"); link.href = "#" + heading.id;
    link.textContent = heading.textContent; link.className = heading.tagName === "H3" ? "lv3" : "lv2";
    toc.appendChild(link); links[heading.id] = link;
  });
  if (window.IntersectionObserver) {
    var observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (!entry.isIntersecting) return;
        Object.keys(links).forEach(function (id) { links[id].classList.toggle("active", id === entry.target.id); });
      });
    }, {rootMargin:"-10% 0px -70% 0px"});
    headings.forEach(function (heading) { observer.observe(heading); });
  }
  document.querySelectorAll("table").forEach(function (table) {
    var wrapper = document.createElement("div"); wrapper.className = "table-scroll";
    wrapper.tabIndex = 0; wrapper.setAttribute("role", "region");
    var caption = table.querySelector("caption"), previous = table.previousElementSibling;
    wrapper.setAttribute("aria-label", caption ? caption.textContent : previous && /^H[23]$/.test(previous.tagName) ? previous.textContent + " table" : "Scrollable reference table");
    table.parentNode.insertBefore(wrapper, table); wrapper.appendChild(table);
  });
})();

// Color-scheme attribution in the shared documentation footer.
(function () {
  "use strict";
  var footer = document.querySelector("footer");
  if (!footer || footer.querySelector(".color-scheme-credit")) return;
  var paragraph = document.createElement("p");
  paragraph.className = "prov-inline color-scheme-credit";
  paragraph.appendChild(document.createTextNode("Color scheme by "));
  var link = document.createElement("a");
  link.href = "https://www.psx-place.com/members/gamerman-5000.192149/";
  link.textContent = "gamerman 5000";
  paragraph.appendChild(link);
  footer.appendChild(paragraph);
})();

/* project-links:start (generated, see https://github.com/NathanNeurotic/NathanNeurotic/tree/main/project-links) */
/* Small bar at the top of every page: links to the six project sites and a
   language picker that reopens THIS page through Google Translate. */
(function () {
  if (window.__nnLangbar) return;
  window.__nnLangbar = true;
  var SITES = [
    ["RiptOPL", "RiptOPL"], ["Launchstation", "Launchstation"],
    ["PS2-Servers", "PS2-Servers"], ["launcHER", "launcHER"],
    ["Simple-Media-System", "Simple-Media-System"], ["POPSLoader", "POPSLoader"]
  ];
  var LANGS = [
    ["en", "English"], ["es", "Español"], ["pt", "Português (Brasil)"],
    ["pt-PT", "Português"], ["fr", "Français"], ["de", "Deutsch"],
    ["it", "Italiano"], ["nl", "Nederlands"], ["da", "Dansk"],
    ["sv", "Svenska"], ["pl", "Polski"], ["cs", "Čeština"],
    ["hr", "Hrvatski"], ["sr", "Српски"], ["bg", "Български"],
    ["ru", "Русский"], ["el", "Ελληνικά"], ["hu", "Magyar"],
    ["ro", "Română"], ["gl", "Galego"], ["sq", "Shqip"],
    ["tr", "Türkçe"], ["ar", "العربية"], ["fa", "فارسی"],
    ["ja", "日本語"], ["ko", "한국어"], ["zh-CN", "简体中文"],
    ["zh-TW", "繁體中文"], ["vi", "Tiếng Việt"], ["lo", "ລາວ"],
    ["id", "Bahasa Indonesia"], ["tl", "Filipino"], ["ceb", "Cebuano"]
  ];
  var HOME = "https://nathanneurotic.github.io";
  var PROXY = "https://nathanneurotic-github-io.translate.goog";

  function run() {
    if (!document.body || document.getElementById("nn-langbar")) return;
    var path = location.pathname || "/";
    var proxied = /\.translate\.goog$/.test(location.hostname);
    var params = new URLSearchParams(location.search);
    var current = proxied ? (params.get("_x_tr_tl") || "en") : "en";
    ["_x_tr_sl", "_x_tr_tl", "_x_tr_hl", "_x_tr_pto", "_x_tr_hist"].forEach(function (k) { params.delete(k); });
    var rest = params.toString();
    var here = (path.split("/")[1] || "").toLowerCase();

    var bar = document.createElement("div");
    bar.id = "nn-langbar";
    bar.setAttribute("translate", "no");
    bar.className = "notranslate";
    bar.style.cssText = "font-size:12px;line-height:1.6;text-align:center;padding:4px 12px;" +
      "opacity:.85;position:relative;z-index:50;";

    SITES.forEach(function (s, i) {
      if (i) bar.appendChild(document.createTextNode(" · "));
      var el;
      if (s[1].toLowerCase() === here) {
        el = document.createElement("b");
      } else {
        el = document.createElement("a");
        el.href = HOME + "/" + s[1] + "/";
        el.style.color = "inherit";
      }
      el.textContent = s[0];
      bar.appendChild(el);
    });

    bar.appendChild(document.createTextNode("  ·  "));
    var sel = document.createElement("select");
    sel.setAttribute("aria-label", "Language");
    sel.style.cssText = "font-size:12px;background:transparent;color:inherit;border:1px solid currentColor;" +
      "border-radius:4px;padding:0 2px;";
    LANGS.forEach(function (l) {
      var o = document.createElement("option");
      o.value = l[0];
      o.textContent = l[1];
      o.style.color = "#000";
      if (l[0] === current) o.selected = true;
      sel.appendChild(o);
    });
    sel.addEventListener("change", function () {
      var code = sel.value;
      var q = rest ? rest + "&" : "";
      location.href = code === "en"
        ? HOME + path + (rest ? "?" + rest : "") + location.hash
        : PROXY + path + "?" + q + "_x_tr_sl=en&_x_tr_tl=" + code + "&_x_tr_hl=" + code + location.hash;
    });
    bar.appendChild(sel);
    var note = document.createElement("i");
    note.textContent = " automatic translation by Google";
    note.style.opacity = ".7";
    bar.appendChild(note);
    document.body.insertBefore(bar, document.body.firstChild);
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", run);
  else run();
})();
/* project-links:end */
