#!/usr/bin/env python3
"""Render the SMS manual and native-wiki mirror using only Python's stdlib."""
from __future__ import annotations

import argparse
import html
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
SITE = REPO / "docs"
WIKI = REPO / "wiki"


class Node:
    def __init__(self, tag="", attrs=()):
        self.tag, self.attrs, self.children = tag, dict(attrs), []


class Document(HTMLParser):
    VOID = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "wbr"}

    def __init__(self, source):
        super().__init__(convert_charrefs=True)
        self.root = Node()
        self.stack = [self.root]
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        node = Node(tag, attrs)
        self.stack[-1].children.append(node)
        if tag not in self.VOID:
            self.stack.append(node)

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)
        if tag not in self.VOID:
            self.handle_endtag(tag)

    def handle_endtag(self, tag):
        for i in range(len(self.stack) - 1, 0, -1):
            if self.stack[i].tag == tag:
                del self.stack[i:]
                break

    def handle_data(self, text):
        self.stack[-1].children.append(text)


def walk(node):
    if isinstance(node, Node):
        yield node
        for child in node.children:
            yield from walk(child)


def plain(node):
    if isinstance(node, str):
        return node
    if node.tag in {"script", "style"}:
        return ""
    return " ".join(plain(child) for child in node.children)


def text(source):
    return " ".join(plain(Document(source).root).split())


def anchors(fragment):
    """Persist the IDs that the old browser TOC assigned at runtime."""
    used = set()

    def heading(match):
        attrs, title = match[2], match[3]
        explicit = re.search(r'\bid=["\']([^"\']+)["\']', attrs)
        if explicit:
            used.add(explicit[1])
            return match[0]
        slug = re.sub(r"[^a-z0-9]+", "-", text(title).lower()).strip("-")[:48] or "sec"
        while slug in used:
            slug += "-x"
        used.add(slug)
        return f'<h{match[1]}{attrs} id="{slug}">{title}</h{match[1]}>'

    return re.sub(r"<h([23])\b([^>]*)>(.*?)</h\1>", heading, fragment, flags=re.S | re.I)


def render(manifest, slug, fragment):
    site, page = manifest["site"], manifest["pages"][slug]
    esc = html.escape
    nav = ['<div class="sidebar-brand-sub">PLAYSTATION 2 // SIMPLE MEDIA SYSTEM</div>']
    for section in manifest["sections"]:
        nav.append(f'<h4>{esc(section["name"])}</h4>')
        for target in section["pages"]:
            meta = manifest["pages"][target]
            active = ' class="active" aria-current="page"' if target == slug else ""
            nav.append(f'<a href="{target}.html"{active}><span class="icon icon-{meta["icon"]}" aria-hidden="true"></span>{esc(meta["title"])}</a>')
    canonical = site["pages_url"] + ("" if slug == "index" else f"{slug}.html")
    title = esc(page["title"] + " · " + site["title"])
    desc = esc(page["description"], quote=True)
    return f'''<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>{title}</title><meta name="description" content="{desc}">
<link rel="canonical" href="{canonical}"><link rel="icon" type="image/svg+xml" href="assets/favicon.svg">
<meta property="og:type" content="website"><meta property="og:site_name" content="SMS Docs">
<meta property="og:title" content="{title}"><meta property="og:description" content="{desc}">
<meta property="og:image" content="{site["pages_url"]}assets/sms-icon.png"><meta name="twitter:card" content="summary">
<link rel="stylesheet" href="assets/style.css"><link rel="stylesheet" href="assets/sms.css">
</head><body data-base="">
<a class="skip-link" href="#main">Skip to content</a>
<header class="topbar">
 <button class="menu-btn" type="button" aria-label="Open navigation" aria-controls="site-nav" aria-expanded="false">☰</button>
 <a class="brand sms-brand" href="index.html"><img src="assets/sms-icon.png" alt="" width="38" height="38"><span>SMS <small>DOCS</small></span></a>
 <div class="topbar-actions"><button id="orb-toggle" class="topbar-btn orb-toggle" type="button" aria-pressed="false" aria-label="Enable cursor orbs"><span class="orb-toggle-symbol" aria-hidden="true">✧</span><span class="orb-toggle-label">Orbs off</span></button>
 <a class="topbar-btn primary" href="releases.html" aria-label="Downloads"><span class="icon icon-releases" aria-hidden="true"></span><span>Downloads</span></a>
 <a class="topbar-btn" href="{site["repo"]}" aria-label="GitHub"><span class="icon icon-github" aria-hidden="true"></span><span>GitHub</span></a></div>
 <div class="search" role="search"><span class="ico" aria-hidden="true"></span><label class="sr-only" for="q">Search SMS documentation</label>
 <input id="q" type="search" placeholder="Search playback, devices, settings…" autocomplete="off" aria-controls="results" aria-expanded="false"><kbd aria-hidden="true">/</kbd>
 <div id="results" aria-label="Search results"></div><span id="search-status" class="sr-only" role="status" aria-live="polite"></span></div>
</header>
<div class="layout"><nav class="sidebar" id="site-nav" aria-label="Documentation">{"".join(nav)}</nav>
<main class="content" id="main" tabindex="-1">
{fragment}
</main><aside class="toc" aria-label="On this page"><div class="toc-h">On this page</div><nav id="toc-nav" aria-label="Page sections"></nav></aside></div>
<footer><p><b>SMS Documentation</b> — Simple Media System for PlayStation 2. Created by Eugene Plotnikov; this community fork is maintained by NathanNeurotic and contributors. <a href="credits.html">Credits &amp; licenses</a>.</p>
<p class="prov-inline">Documentation reviewed {site["reviewed"]}. See <a href="known-issues.html">known issues and validation limits</a>. Sister projects: <a href="https://nathanneurotic.github.io/Launchstation/">Launchstation</a> · <a href="https://nathanneurotic.github.io/POPSLoader/">POPSLoader</a> · <a href="https://nathanneurotic.github.io/Open-PS2-Loader/">RiptOPL</a>.</p></footer>
<script src="data/search-index.js"></script><script src="assets/app.js"></script><script src="assets/orbs.js"></script>
</body></html>
'''


def absolute_link(url, manifest):
    if urlsplit(url).scheme or url.startswith("//"):
        return url
    return manifest["site"]["pages_url"] + url


def markdown(node, manifest, pre=False):
    """Convert the small HTML vocabulary of our articles to native-wiki GFM."""
    if isinstance(node, str):
        return node if pre else re.sub(r"\s+", " ", node)
    tag = node.tag
    if tag in {"script", "style"} or (tag == "span" and node.attrs.get("aria-hidden") == "true"):
        return ""
    if node.attrs.get("class") == "eyebrow":
        return ""
    if node.attrs.get("class") in {"hero-tags", "hero-actions"}:
        items = [markdown(child, manifest).strip() for child in node.children if isinstance(child, Node)]
        return "\n\n" + " · ".join(filter(None, items)) + "\n\n"
    if tag == "div" and "grid" in node.attrs.get("class", "").split():
        items = [markdown(child, manifest).strip() for child in node.children if isinstance(child, Node)]
        return "\n\n" + "\n".join("- " + item for item in items if item) + "\n\n"
    if tag in {"ul", "ol"}:
        items = []
        for child in node.children:
            if not isinstance(child, Node) or child.tag != "li":
                continue
            item = "".join(markdown(part, manifest) for part in child.children).strip()
            prefix = f"{len(items) + 1}. " if tag == "ol" else "- "
            items.append(prefix + item.replace("\n", "\n" + " " * len(prefix)))
        return "\n\n" + "\n".join(items) + "\n\n"
    body = "".join(markdown(child, manifest, pre or tag == "pre") for child in node.children)
    if re.fullmatch(r"h[1-6]", tag):
        return "\n\n" + "#" * int(tag[1]) + " " + body.strip() + "\n\n"
    if tag == "pre":
        literal = "".join(plain(child) if not isinstance(child, str) else child for child in node.children)
        fence = "`" * max(3, max((len(x) + 1 for x in re.findall(r"`+", literal)), default=3))
        return f"\n\n{fence}\n{literal.strip()}\n{fence}\n\n"
    if tag in {"code", "kbd"} and not pre:
        fence = "`" * max(1, max((len(x) + 1 for x in re.findall(r"`+", body)), default=1))
        return fence + body.strip() + fence
    if tag in {"b", "strong"}:
        return "**" + body.strip() + "**"
    if tag in {"i", "em"}:
        return "*" + body.strip() + "*"
    if tag == "a":
        label = " ".join(plain(node).split()).replace("[", "\\[").replace("]", "\\]")
        href = node.attrs.get("href", "")
        # Fragment-only links refer to the source page's stable IDs.
        if href.startswith("#"):
            href = manifest["_current_slug"] + ".html" + href
        return f"[{label}]({absolute_link(href, manifest)})" if label and href else body
    if tag == "img":
        return f'![{node.attrs.get("alt", "").replace("]", "")}]({absolute_link(node.attrs.get("src", ""), manifest)})'
    if tag == "table":
        rows = []
        for row in walk(node):
            if row.tag == "tr":
                cells = [markdown(c, manifest).strip().replace("|", "\\|").replace("\n", "<br>")
                         for c in row.children if isinstance(c, Node) and c.tag in {"th", "td"}]
                if cells:
                    rows.append(cells)
        if not rows:
            return ""
        width = max(map(len, rows))
        lines = ["| " + " | ".join(r + [""] * (width - len(r))) + " |" for r in rows]
        lines.insert(1, "| " + " | ".join(["---"] * width) + " |")
        return "\n\n" + "\n".join(lines) + "\n\n"
    if tag == "br":
        return "<br>"
    if tag == "hr":
        return "\n\n---\n\n"
    if tag == "summary":
        return "<summary>" + html.escape(plain(node).strip()) + "</summary>\n\n"
    if tag == "details":
        return "\n\n<details>" + body.strip() + "\n\n</details>\n\n"
    if tag in {"p", "div", "section", "figure", "figcaption", "blockquote", "dl", "dt", "dd"}:
        return "\n\n" + body.strip() + "\n\n"
    return body


def wiki_name(slug):
    return "Home" if slug == "index" else "-".join(word.capitalize() for word in slug.split("-"))


def products(manifest):
    result, search = {}, []
    for slug, meta in manifest["pages"].items():
        path = ROOT / "content" / f"{slug}.html"
        fragment = anchors(path.read_text(encoding="utf-8").strip())
        result[SITE / f"{slug}.html"] = render(manifest, slug, fragment).encode("utf-8")
        parsed = Document(fragment)
        headings = [n for n in walk(parsed.root) if n.tag == "h1"]
        if len(headings) != 1:
            raise ValueError(f"{path}: expected one h1")
        search.append({"title": " ".join(plain(headings[0]).split()), "cat": meta["section"], "url": f"{slug}.html", "text": meta["description"]})
        pattern = re.compile(r'<h[23]\b[^>]*id="([^"]+)"[^>]*>(.*?)</h[23]>', re.S | re.I)
        matches = list(pattern.finditer(fragment))
        for i, match in enumerate(matches):
            end = matches[i + 1].start() if i + 1 < len(matches) else len(fragment)
            search.append({"title": text(match[2]), "cat": meta["section"], "url": f"{slug}.html#{match[1]}", "text": text(fragment[match.end():end])[:2200] or meta["description"]})
        wiki_manifest = dict(manifest, _current_slug=slug)
        article = markdown(parsed.root, wiki_manifest).strip()
        article = re.sub(r"(?m)[ \t]+$", "", article)
        article = re.sub(r"\n{3,}", "\n\n", article)
        note = f'\n\n---\n[Searchable manual]({manifest["site"]["pages_url"]}{slug}.html) · [Edit the maintained source]({manifest["site"]["repo"]}/blob/master/docs-site/content/{slug}.html)\n'
        result[WIKI / f"{wiki_name(slug)}.md"] = (article + note).encode("utf-8")
    payload = json.dumps(search, ensure_ascii=False, separators=(",", ":"))
    result[SITE / "data/search-index.json"] = (payload + "\n").encode("utf-8")
    result[SITE / "data/search-index.js"] = ("window.SMS_DOC_INDEX=" + payload.replace("<", "\\u003c") + ";\n").encode("utf-8")
    result[SITE / ".nojekyll"] = b""
    for path in sorted((ROOT / "static").rglob("*")):
        if path.is_file():
            result[SITE / path.relative_to(ROOT / "static")] = path.read_bytes()
    sidebar = ["[**SMS documentation**](Home)", ""]
    for section in manifest["sections"]:
        sidebar.extend(["### " + section["name"], ""])
        sidebar.extend(f'- [{manifest["pages"][slug]["title"]}]({wiki_name(slug)})' for slug in section["pages"])
        sidebar.append("")
    result[WIKI / "_Sidebar.md"] = ("\n".join(sidebar).rstrip() + "\n").encode("utf-8")
    result[WIKI / "_Footer.md"] = f'[SMS manual]({manifest["site"]["pages_url"]}) · [Downloads]({manifest["site"]["pages_url"]}releases.html) · [Credits]({manifest["site"]["pages_url"]}credits.html)\n'.encode("utf-8")
    urls = ET.Element("urlset", xmlns="http://www.sitemaps.org/schemas/sitemap/0.9")
    for slug in manifest["pages"]:
        item = ET.SubElement(urls, "url")
        ET.SubElement(item, "loc").text = manifest["site"]["pages_url"] + ("" if slug == "index" else slug + ".html")
    result[SITE / "sitemap.xml"] = ET.tostring(urls, encoding="utf-8", xml_declaration=True) + b"\n"
    return result, search


def validate(result, search, manifest):
    errors, documents, ids = [], {}, {}
    for path, data in result.items():
        if path.parent == SITE and path.suffix == ".html":
            document = Document(data.decode("utf-8"))
            documents[path] = document
            found = [node.attrs["id"] for node in walk(document.root) if "id" in node.attrs]
            if len(found) != len(set(found)):
                errors.append(f"{path.name}: duplicate IDs")
            ids[path] = set(found)
    for path, doc in documents.items():
        for node in walk(doc.root):
            attr = "src" if node.tag in {"img", "script"} else "href" if node.tag in {"a", "link"} else None
            if not attr or not node.attrs.get(attr):
                continue
            link = urlsplit(node.attrs[attr])
            if link.scheme or link.netloc:
                continue
            target = (path.parent / unquote(link.path)).resolve() if link.path else path
            if not target.is_relative_to(SITE):
                errors.append(f"{path.name}: link escapes published docs: {node.attrs[attr]}")
            elif target not in result and not target.is_file():
                errors.append(f"{path.name}: missing link: {node.attrs[attr]}")
            elif link.fragment and target in ids and unquote(link.fragment) not in ids[target]:
                errors.append(f"{path.name}: missing anchor: {node.attrs[attr]}")
    for row in search:
        url = urlsplit(row["url"])
        target = SITE / url.path
        if target not in documents or (url.fragment and url.fragment not in ids[target]):
            errors.append("Invalid search target: " + row["url"])
    style = result[SITE / "assets/style.css"].decode("utf-8")
    for meta in manifest["pages"].values():
        if ".icon-" + meta["icon"] not in style:
            errors.append("Missing icon class: " + meta["icon"])
    if errors:
        raise ValueError("\n".join(sorted(set(errors))))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Validate and reject stale generated files without writing")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "manifest.json").read_text(encoding="utf-8"))
    result, search = products(manifest)
    validate(result, search, manifest)
    stale = []
    for path, data in result.items():
        if args.check:
            if not path.is_file() or path.read_bytes() != data:
                stale.append(str(path.relative_to(REPO)))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    if stale:
        raise ValueError("Generated files are stale; run python docs-site/build.py:\n" + "\n".join(stale))
    print(f'{"Checked" if args.check else "Generated"} {len(manifest["pages"])} pages, {len(search)} search entries and {len(manifest["pages"])+2} wiki files; local links and anchors validated')


if __name__ == "__main__":
    try:
        main()
    except (ValueError, FileNotFoundError) as exc:
        print(exc, file=sys.stderr)
        sys.exit(1)
