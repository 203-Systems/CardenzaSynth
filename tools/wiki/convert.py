#!/usr/bin/env python3
"""Moves the Learn manual's sources (docs/learn/{zh,en}/*.md, the markdown subset of
tools/learn/build.py) to MDX for the wiki (wiki/, Docusaurus), one page per chapter:

  en  -> wiki/docs/<chapter>.mdx
  zh  -> wiki/i18n/zh-Hans/docusaurus-plugin-content-docs/current/<chapter>.mdx

  python tools/wiki/convert.py               every chapter of docs/learn/manual.json
  python tools/wiki/convert.py keys seq      only these (links to the others still resolve)

Meant to run once, when the wiki takes over: from then on the MDX is the source. Until then it
can be run again; it overwrites the pages it writes. Every {{...}} of the old syntax becomes a
component of wiki/src/components/cardenza (registered for every page in src/theme/MDXComponents):

  {{key:FN+6}} {{hold:ESC}}  <Key k="FN+6" />  <Key k="hold ESC" />
  {{icon:rec}}               <Icon name="rec" />
  {{led:half|半亮}}           <Led state="half">半亮</Led>
  {{scene:SEQ}} {{track:2}} {{page:ROLL}}   <Scene name="SEQ" />  <Track n="2" />  <Page name="ROLL" />
  {{shot:NAME|caption ...}}  <Shot name="NAME"> <Mark at=".."> <KeyMark keys=".."> <Caption> </Shot>
  {{strip ...}}              <Strip> <Step keys=".." shot=".." at=".."> </Strip>
  {{keymap:caption ...}}     <Keymap> <Group keys=".."> <Caption> </Keymap>
  {{ref:engine:JUNO}} ...    <Ref engine="JUNO" />  <Ref kind="fn" />
  {{map:device}} {{map:scenes}}   <DeviceMap />  <SceneMap />
  [[#id]] [[#id|text]]       [title](./chapter.mdx#id)  (the section's title in that language)
  <!-- keywords: a, b -->    <Keywords k="a, b" />
  > [!tip] / [!warn] / [!note]   :::tip / :::warning / :::note
"""
import json
import pathlib
import re
import sys
import unicodedata

ROOT = pathlib.Path(__file__).resolve().parents[2]
LEARN = ROOT / "docs" / "learn"
WIKI = ROOT / "wiki"
OUT = {"en": WIKI / "docs", "zh": WIKI / "i18n" / "zh-Hans" / "docusaurus-plugin-content-docs" / "current"}
LANGS = ("zh", "en")
GUIDE_HREF = "/"  # the guide is the site's home (src/pages/index.tsx)
ADMONITIONS = {"tip": "tip", "warn": "warning", "note": "note", "try": "info", "keys": "note"}
CHIPS = "key|hold|icon|led|scene|track|page"


def slug(text):
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")


def plain(text):
    """A title without markup (tools/learn/build.py plain)."""
    text = re.sub(r"\{\{icon:[^}]*\}\}", "", text)
    text = re.sub(r"\{\{led:\w+\|?([^}]*)\}\}", r"\1", text)
    text = re.sub(r"\{\{(?:key|hold|scene|page|track):([^}]+)\}\}", r"\1", text)
    text = re.sub(r"\[\[#[\w-]+\|([^\]]+)\]\]", r"\1", text)
    text = re.sub(r"\[\[#([\w-]+)\]\]", r"\1", text)
    text = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", text)
    text = re.sub(r"[*`>#|]", " ", text)
    return re.sub(r"\s+", " ", text).strip()


HEADING = re.compile(r"^(#{1,3})\s+(.*?)\s*(?:\{#([\w-]+)\})?$")


def section_index(chapters):
    """id -> (chapter, {lang: title}) over the whole manual, for links between pages."""
    index = {}
    for lang in LANGS:
        for chapter in chapters:
            for line in (LEARN / lang / f"{chapter}.md").read_text(encoding="utf-8").splitlines():
                m = HEADING.match(line.strip())
                if m:
                    sid = m.group(3) or (chapter if len(m.group(1)) == 1 else slug(m.group(2)))
                    index.setdefault(sid, (chapter, {}))[1][lang] = plain(m.group(2))
    return index


class Page:
    def __init__(self, lang, chapter, index):
        self.lang, self.chapter, self.index = lang, chapter, index

    def fail(self, message):
        sys.exit(f"convert: [{self.lang}] {self.chapter}: {message}")


def attr(value):
    """A JSX attribute value: a plain string, or a JS string when it has a double quote."""
    return json.dumps(value, ensure_ascii=False) if '"' in value else f'"{value}"'


def attr_expr(value):
    return "{" + json.dumps(value, ensure_ascii=False) + "}" if '"' in value else f'"{value}"'


def text(value):
    """Plain text for MDX: no JSX, expressions, entities or markdown it did not have before."""
    value = value.replace("\\", "\\\\").replace("&", "&amp;").replace("<", "&lt;")
    return re.sub(r"([{}*_\[\]~])", r"\\\1", value)


def chip(kind, arg, page):
    arg = arg.strip()
    if kind == "key":
        return f"<Key k={attr_expr(arg)} />"
    if kind == "hold":
        return f"<Key k={attr_expr('hold ' + arg)} />"
    if kind == "icon":
        return f"<Icon name={attr_expr(arg)} />"
    if kind == "led":
        state, _, label = arg.partition("|")
        if state not in ("off", "half", "full", "blink"):
            page.fail(f"unknown light {state}")
        return f'<Led state="{state}">{text(label)}</Led>' if label else f'<Led state="{state}" />'
    if kind == "scene":
        return f"<Scene name={attr_expr(arg.upper())} />"
    if kind == "track":
        return f"<Track n={attr_expr(arg)} />"
    if kind == "page":
        return f"<Page name={attr_expr(arg)} />"
    page.fail(f"unknown chip {kind}")


def link(target, label, page):
    if target not in page.index:
        page.fail(f"link to unknown section #{target}")
    chapter, titles = page.index[target]
    if label is None:
        label = text(titles.get(page.lang) or titles.get("en") or target)
    if chapter == page.chapter:
        href = f"#{target}"
    else:
        href = f"./{chapter}.mdx" + ("" if target == chapter else f"#{target}")
    return f"[{label}]({href})"


INLINE = re.compile(r"`([^`]+)`|\[\[#([\w-]+)(?:\|([^\]]+))?\]\]|\{\{(" + CHIPS + r"):([^}]+)\}\}"
                    r"|\*\*(.+?)\*\*|\*([^*\s][^*]*?)\*|\[([^\]]+)\]\(([^)]+)\)")


def punctuation(ch):
    return unicodedata.category(ch)[0] in "PS"


def emphasis(marker, tag, inner, before, after):
    """**inner** where CommonMark reads it as emphasis; <strong> / <em> where it would not (Chinese
    text with no spaces round a run that starts or ends with punctuation or a key chip)."""
    space = lambda ch: ch == "" or ch.isspace()
    first, last = inner[:1], inner[-1:]
    opens = not space(first) and (not punctuation(first) or space(before) or punctuation(before))
    closes = not space(last) and (not punctuation(last) or space(after) or punctuation(after))
    return f"{marker}{inner}{marker}" if opens and closes else f"<{tag}>{inner}</{tag}>"


def inline(value, page):
    out, pos = [], 0
    for m in INLINE.finditer(value):
        out.append(text(value[pos:m.start()]))
        before, after = value[m.start() - 1:m.start()], value[m.end():m.end() + 1]
        if m.group(1) is not None:
            out.append(f"`{m.group(1)}`")
        elif m.group(2) is not None:
            out.append(link(m.group(2), inline(m.group(3), page) if m.group(3) else None, page))
        elif m.group(4) is not None:
            out.append(chip(m.group(4), m.group(5), page))
        elif m.group(6) is not None:
            out.append(emphasis("**", "strong", inline(m.group(6), page), before, after))
        elif m.group(7) is not None:
            out.append(emphasis("*", "em", inline(m.group(7), page), before, after))
        else:
            href = m.group(9)
            out.append(f"[{inline(m.group(8), page)}]({GUIDE_HREF if href == './' else href})")
        pos = m.end()
    out.append(text(value[pos:]))
    return "".join(out)


def body_lines(body):
    for raw in body:
        line = raw.strip()
        if line and not line.startswith("#"):
            yield line


def shot(arg, body, page):
    name, _, caption = arg.partition("|")
    out = [f"<Shot name={attr(name.strip())}>"]
    for line in body_lines(body):
        m = re.match(r"(mark|key)\s+(.+?)\s*(?:=\s*(.*))?$", line)
        if not m:
            page.fail(f"cannot read mark {line!r}")
        kind, target, label = m.group(1), m.group(2), (m.group(3) or "").strip()
        tag, key = ("Mark", "at") if kind == "mark" else ("KeyMark", "keys")
        out.append(f"  <{tag} {key}={attr(target)}>{inline(label, page)}</{tag}>" if label else f"  <{tag} {key}={attr(target)} />")
    if caption.strip():
        out.append(f"  <Caption>{inline(caption.strip(), page)}</Caption>")
    out.append("</Shot>")
    return out


def strip(body, page):
    out = ["<Strip>"]
    for line in body_lines(body):
        m = re.match(r"^(.*?)\s+>\s+(.*?)\s*\|\s*(.*)$", line)
        if not m:
            page.fail(f"strip step needs KEYS > PICTURE | caption: {line!r}")
        keys, picture, caption = m.groups()
        props = []
        if keys.strip() != "-":
            props.append(f"keys={attr(keys.strip())}")
        if picture.strip() != "-":
            name, *targets = picture.split()
            props.append(f"shot={attr(name)}")
            at = " ".join(t.lstrip("@") for t in targets if t.startswith("@"))
            if at:
                props.append(f"at={attr(at)}")
        out.append(f"  <Step {' '.join(props)}>{inline(caption, page)}</Step>")
    if len(out) == 1:
        page.fail("empty strip")
    out.append("</Strip>")
    return out


def keymap(arg, body, page):
    out = ["<Keymap>"]
    for line in body_lines(body):
        spec, _, label = line.partition("=")
        label = re.sub(r"\s*@(left|right|bottom|top)\b", "", label).strip()
        out.append(f"  <Group keys={attr(spec.strip())}>{inline(label, page)}</Group>")
    if arg.strip():
        out.append(f"  <Caption>{inline(arg.strip(), page)}</Caption>")
    out.append("</Keymap>")
    return out


def reference(arg, page):
    kind, _, rest = arg.partition(":")
    if kind == "engine":
        return [f"<Ref engine={attr(rest)} />"]
    if kind in ("fn", "settings", "fn-quick", "lists", "pages"):
        return [f'<Ref kind="{"fn" if kind == "settings" else kind}" />']
    page.fail(f"unknown reference {arg}")


def split_cells(row):
    cells, depth, current = [], 0, ""
    for index, ch in enumerate(row):
        pair = row[index:index + 2]
        if pair in ("{{", "[["):
            depth += 1
        elif pair in ("}}", "]]") and depth:
            depth -= 1
        if ch == "|" and depth == 0:
            cells.append(current)
            current = ""
        else:
            current += ch
    cells.append(current)
    return cells


def convert(markdown, page, position):
    lines = markdown.splitlines()
    out = ["---", f"sidebar_position: {position}", "---", ""]
    paragraph = []
    i = 0

    def emit(*block):
        if out[-1] != "":
            out.append("")
        out.extend(block)
        out.append("")

    def flush():
        if paragraph:
            emit(inline(" ".join(paragraph), page))
            paragraph.clear()

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        if not stripped:
            flush()
            i += 1
            continue
        kw = re.match(r"<!--\s*keywords:(.*)-->", stripped)
        if kw:
            flush()
            words = ", ".join(k.strip() for k in kw.group(1).split(",") if k.strip())
            emit(f"<Keywords k={attr_expr(words)} />")
            i += 1
            continue
        if stripped.startswith("<!--"):
            page.fail(f"comment MDX cannot hold: {stripped}")
        heading = HEADING.match(stripped)
        if heading:
            flush()
            level = len(heading.group(1))
            title = heading.group(2)
            if level == 1:
                emit(f"# {inline(title, page)}")
            else:
                sid = heading.group(3) or slug(title)
                emit(f"{'#' * level} {inline(title, page)} {{#{sid}}}")
            i += 1
            continue
        if stripped.startswith("```"):
            flush()
            code = [line]
            i += 1
            while i < len(lines) and not lines[i].strip().startswith("```"):
                code.append(lines[i])
                i += 1
            code.append(lines[i] if i < len(lines) else "```")
            i += 1
            emit(*code)
            continue
        block = re.match(r"^\{\{(shot|ref|map):([^{}]+)\}\}$", stripped)
        if block:
            flush()
            kind, arg = block.groups()
            if kind == "shot":
                emit(*shot(arg, [], page))
            elif kind == "ref":
                emit(*reference(arg, page))
            else:
                emit("<DeviceMap />" if arg == "device" else "<SceneMap />")
            i += 1
            continue
        opening = re.match(r"^\{\{(shot|strip|keymap)(?::(.*))?$", stripped)
        if opening:
            flush()
            body = []
            i += 1
            while i < len(lines) and lines[i].strip() != "}}":
                body.append(lines[i])
                i += 1
            if i >= len(lines):
                page.fail(f"{{{{{opening.group(1)} is not closed with }}}}")
            i += 1
            kind, arg = opening.group(1), opening.group(2) or ""
            emit(*(shot(arg, body, page) if kind == "shot" else strip(body, page) if kind == "strip" else keymap(arg, body, page)))
            continue
        if stripped.startswith("|"):
            flush()
            rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                rows.append([c.strip() for c in split_cells(lines[i].strip().strip("|"))])
                i += 1
            table = ["| " + " | ".join(inline(c, page) for c in rows[0]) + " |",
                     "|" + "|".join("---" for _ in rows[0]) + "|"]
            table += ["| " + " | ".join(inline(c, page) for c in row) + " |" for row in rows[2:]]
            emit(*table)
            continue
        if stripped.startswith(">"):
            flush()
            quote = []
            while i < len(lines) and lines[i].strip().startswith(">"):
                quote.append(lines[i].strip()[1:].strip())
                i += 1
            kind = "note"
            m = re.match(r"\[!(\w+)\]\s*", quote[0]) if quote else None
            if m:
                kind = m.group(1).lower()
                quote[0] = quote[0][m.end():]
            emit(f":::{ADMONITIONS.get(kind, 'note')}", "", inline(" ".join(q for q in quote if q), page), "", ":::")
            continue
        list_match = re.match(r"^(-|\d+\.)\s+(.*)$", stripped)
        if list_match:
            flush()
            ordered = list_match.group(1) != "-"
            items = []
            while i < len(lines):
                m = re.match(r"^(-|\d+\.)\s+(.*)$", lines[i].strip())
                if m and (m.group(1) != "-") == ordered:
                    items.append(m.group(2))
                    i += 1
                elif lines[i].startswith("  ") and lines[i].strip() and items:
                    items[-1] += " " + lines[i].strip()
                    i += 1
                else:
                    break
            emit(*[f"{'1.' if ordered else '-'} {inline(item, page)}" for item in items])
            continue
        if stripped.startswith("<"):
            page.fail(f"raw HTML MDX would read as JSX: {stripped[:60]}")
        paragraph.append(stripped)
        i += 1
    flush()
    while out and out[-1] == "":
        out.pop()
    return "\n".join(out) + "\n"


def main():
    manual = json.loads((LEARN / "manual.json").read_text(encoding="utf-8"))
    chapters = manual["chapters"]
    wanted = sys.argv[1:] or chapters
    unknown = [c for c in wanted if c not in chapters]
    if unknown:
        sys.exit(f"convert: not chapters of the manual: {unknown}")
    index = section_index(chapters)
    for lang in LANGS:
        OUT[lang].mkdir(parents=True, exist_ok=True)
        for chapter in wanted:
            page = Page(lang, chapter, index)
            source = (LEARN / lang / f"{chapter}.md").read_text(encoding="utf-8")
            target = OUT[lang] / f"{chapter}.mdx"
            target.write_text(convert(source, page, chapters.index(chapter) + 1), encoding="utf-8", newline="\n")
    print(f"convert: {len(wanted)} chapters x {len(LANGS)} languages -> {OUT['en'].relative_to(ROOT)}, {OUT['zh'].relative_to(ROOT)}")


if __name__ == "__main__":
    main()
