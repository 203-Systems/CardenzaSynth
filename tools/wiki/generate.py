#!/usr/bin/env python3
"""Draws what the Cardenza wiki (wiki/, Docusaurus) shows from the firmware, into wiki/generated/
(not in git), and checks the manual's sections:

  data/manual.json     the manual's version (docs/learn/manual.json)
  data/dump.json       pages, parameters, shortcuts and lists (CardenzaLearnDump, tools/wiki/learn_dump.cpp)
  data/leds.json       the key lights of every screenshot (CardenzaSimShot, tools/wiki/shots.py)
  data/sections.json   every section id of the manual: its chapter and its titles (zh, en)
  data/synonyms.json   search synonyms (docs/learn/synonyms.json)
  data/guide.json      the guide's lessons (docs/learn/guide.json, keys and demos resolved), and
  data/guide-leds.json the key lights of their pictures; the lessons run on the firmware first
  static/img/*.png     the screenshots, the guide's pictures
  static/reference.html  sends links of the one-page manual of before (reference.html#id) to that section

  python3 tools/wiki/generate.py --host-build build/linux-groove     (make learn does this)
  python3 tools/wiki/generate.py --skip-shots          reuse the last screenshots (build/wiki)
  python3 tools/wiki/generate.py --lenient-lessons     warn instead of failing when a lesson does not
                                                       complete on this firmware (a stale host build)

The manual is wiki/docs/*.mdx and its translation wiki/i18n/zh-Hans/docusaurus-plugin-content-docs/
current/*.mdx: the same chapters with the same section ids ({#id} on every ## and ###), or this fails.
A Windows host build (Debug/ or Release/ with .exe) works too.
"""
import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))

import lessons  # noqa: E402  (tools/wiki/lessons.py)
import shots  # noqa: E402  (tools/wiki/shots.py)

LEARN = ROOT / "docs" / "learn"
WIKI = ROOT / "wiki"
GEN = WIKI / "generated"
WORK = ROOT / "build" / "wiki"
MANUAL = {"en": WIKI / "docs", "zh": WIKI / "i18n" / "zh-Hans" / "docusaurus-plugin-content-docs" / "current"}
TOOLS = {"shot": ("Devices/CardenzaSim", "CardenzaSimShot"), "dump": ("tests", "CardenzaLearnDump")}


def host_build(path):
    """The host build with its tools as shots.py runs them (no extension). A Windows build keeps them
    as Debug/NAME.exe or Release/NAME.exe: those are copied into that layout."""
    if all((path / folder / name).exists() for folder, name in TOOLS.values()):
        return path
    shim = WORK / "host"
    for folder, name in TOOLS.values():
        for candidate in (path / folder / f"{name}.exe", path / folder / "Release" / f"{name}.exe",
                          path / folder / "Debug" / f"{name}.exe"):
            if candidate.exists():
                (shim / folder).mkdir(parents=True, exist_ok=True)
                # Windows runs a program without an extension when it is the file named.
                shutil.copyfile(candidate, shim / folder / name)
                break
        else:
            sys.exit(f"wiki: {name} not in {path / folder} (build the host targets CardenzaSimShot CardenzaLearnDump)")
    return shim


HEADING = re.compile(r"^(#{1,3})\s+(.*?)\s*(?:\{#([\w-]+)\})?\s*$")


def plain(title):
    """A heading's words without markup."""
    title = re.sub(r"<(?:Key|Scene|Page|Track)\s+(?:k|name|n)=\"([^\"]*)\"\s*/>", r"\1", title)
    title = re.sub(r"<Led state=\"\w+\">(.*?)</Led>", r"\1", title)
    title = re.sub(r"<[^>]+>", "", title)
    title = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", title)
    title = re.sub(r"\\(.)", r"\1", title).replace("&lt;", "<").replace("&amp;", "&")
    title = re.sub(r"[*`>#|]", " ", title)
    return re.sub(r"\s+", " ", title).strip()


def sections():
    """id -> {chapter, title: {zh, en}} over the manual's pages; the two languages must have the same
    chapters and the same section ids, each id once."""
    out, ids = {}, {}
    for lang, folder in MANUAL.items():
        ids[lang] = []
        for page in sorted(folder.glob("*.mdx")):
            chapter = page.stem
            fenced = False
            for number, line in enumerate(page.read_text(encoding="utf-8").splitlines(), start=1):
                if line.strip().startswith("```"):
                    fenced = not fenced
                m = None if fenced else HEADING.match(line)
                if not m:
                    continue
                level, title, sid = len(m.group(1)), m.group(2), m.group(3)
                if level == 1:
                    sid = chapter
                elif not sid:
                    sys.exit(f"wiki: {page.relative_to(ROOT)}:{number}: a section needs its id ({{#id}}, the same in both languages)")
                ids[lang].append(sid)
                entry = out.setdefault(sid, {"chapter": chapter, "title": {}})
                if entry["chapter"] != chapter:
                    sys.exit(f"wiki: section #{sid} is in {entry['chapter']} and in {chapter}")
                entry["title"][lang] = plain(title)
    for lang, found in ids.items():
        twice = sorted({sid for sid in found if found.count(sid) > 1})
        if twice:
            sys.exit(f"wiki: [{lang}] section ids used more than once: {twice}")
    differ = sorted(set(ids["zh"]) ^ set(ids["en"]))
    if differ:
        sys.exit(f"wiki: section ids differ between zh and en: {differ}")
    return out


REDIRECT = """<!doctype html>
<meta charset="utf-8">
<title>Cardenza manual</title>
<meta name="robots" content="noindex">
<script>
// Links of the one-page manual of before (reference.html#section): each section now lives on its
// chapter's page, in the reader's language.
(function () {
  var sections = __SECTIONS__;
  var id = decodeURIComponent(location.hash.slice(1));
  var lang = null;
  try { lang = new URLSearchParams(location.search).get('lang') || localStorage.getItem('learn.lang'); } catch (e) {}
  if (!lang) lang = (navigator.language || '').toLowerCase().indexOf('zh') === 0 ? 'zh' : 'en';
  // reference.html, or /reference where a server drops the extension.
  var base = location.pathname.replace(/reference(\\.html)?$/, '');
  var root = base + (lang === 'zh' ? 'zh-Hans/' : '') + 'manual/';
  var entry = sections[id];
  location.replace(entry ? root + entry.chapter + '/' + (entry.chapter === id ? '' : '#' + id) : root + 'start/');
})();
</script>
"""


def guide_data(guide, index):
    """docs/learn/guide.json as the wiki's guide pages read it: keys as KeyIndex values, demos parsed,
    each lesson's place in the course and its step pictures. The words keep the inline markup
    ({{key:..}}, [[#id]]) that the pages render (wiki/src/guide/text.tsx)."""
    order = lessons.lesson_ids(guide)
    out = {"title": guide["title"], "order": order, "parts": guide["parts"], "lessons": {}}
    for part in guide["parts"]:
        for number, lesson_id in enumerate(part["lessons"], start=1):
            lesson = guide["lessons"][lesson_id]
            more = lesson.get("more")
            if more and more not in index:
                sys.exit(f"wiki: guide lesson {lesson_id} links to unknown manual section #{more}")
            for step in lesson["steps"]:
                for lang in ("zh", "en"):
                    for target in re.findall(r"\[\[#([\w-]+)", step["say"][lang]):
                        if target not in index:
                            sys.exit(f"wiki: guide lesson {lesson_id} links to unknown manual section #{target}")
            position = order.index(lesson_id)
            entry = {"id": lesson_id, "kind": "anim" if "anim" in lesson else "device", "part": part["id"],
                     "number": number, "of": len(part["lessons"]),
                     "next": order[position + 1] if position + 1 < len(order) else None,
                     "title": lesson["title"], "lead": lesson["lead"], "more": more, "steps": []}
            if "anim" in lesson:
                entry["anim"] = lesson["anim"]
                entry["steps"] = [{"say": step["say"]} for step in lesson["steps"]]
            else:
                entry["start"] = lesson["start"]
                for step_number, step in enumerate(lesson["steps"], start=1):
                    entry["steps"].append({
                        "say": step["say"],
                        "keys": lessons.key_indices(step.get("keys", "")),
                        "keySpec": step.get("keys", ""),
                        # Key presses are checked by KeyIndex on the page.
                        "done": [[c[0], lessons.key_indices(c[1])] + c[2:] if c[0] == "press" else c for c in step["done"]],
                        "demo": [list(action) for action in lessons.parse_demo(step["demo"])],
                        "shot": lessons.shot_name(lesson_id, step_number),
                        "marks": step.get("marks", []),
                        "lights": bool(step.get("lights")),
                        "keymarks": step.get("keymarks", []),
                    })
            out["lessons"][lesson_id] = entry
    return out


def commit():
    try:
        return subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return "dev"


def generate(sim_build, skip_shots, lenient):
    manual = json.loads((LEARN / "manual.json").read_text(encoding="utf-8"))
    data, static = GEN / "data", GEN / "static"
    data.mkdir(parents=True, exist_ok=True)
    static.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    index = sections()

    sim = host_build(sim_build)
    dump = shots.run_dump(sim)
    if skip_shots and (WORK / "leds.json").exists() and (static / "img").exists():
        leds = json.loads((WORK / "leds.json").read_text())
    else:
        leds = shots.run_shots(sim, static, WORK)

    (data / "manual.json").write_text(json.dumps(manual, ensure_ascii=False, indent=1), encoding="utf-8")
    (data / "dump.json").write_text(json.dumps(dump, ensure_ascii=False), encoding="utf-8")
    (data / "leds.json").write_text(json.dumps(leds), encoding="utf-8")
    (data / "sections.json").write_text(json.dumps(index, ensure_ascii=False), encoding="utf-8")
    shutil.copyfile(LEARN / "synonyms.json", data / "synonyms.json")

    # The guide: its lessons run on the firmware (their pictures come from that run, and a step that
    # does not complete fails it); the pages draw them from data/guide.json.
    failures = []
    if lenient:
        strict = lessons.check

        def check(guide, log_text):
            failures.extend(strict(guide, log_text))
            return []
        lessons.check = check
    guide = lessons.load()
    img = static / "img"
    if not (skip_shots and (WORK / "guide-leds.json").exists() and list(img.glob("guide-*.png"))):
        lessons.run(guide, sim, WORK, img)
    shutil.copyfile(WORK / "guide-leds.json", data / "guide-leds.json")
    (data / "guide.json").write_text(json.dumps(guide_data(guide, index), ensure_ascii=False), encoding="utf-8")
    (static / "reference.html").write_text(REDIRECT.replace("__SECTIONS__", json.dumps(
        {sid: {"chapter": entry["chapter"]} for sid, entry in index.items()})), encoding="utf-8")

    print(f"wiki: {GEN.relative_to(ROOT)} v{manual['version']} ({commit()}), {len(leds)} screenshots, "
          f"{len(index)} sections, {len(lessons.lesson_ids(guide))} guide lessons")
    if failures:
        print("wiki: WARNING lesson steps that do not complete on this host build (stale firmware?):\n  "
              + "\n  ".join(failures), file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--host-build", default=os.environ.get("LEARN_HOST_BUILD", str(ROOT / "build" / "linux-groove")))
    parser.add_argument("--skip-shots", action="store_true")
    parser.add_argument("--lenient-lessons", action="store_true")
    args = parser.parse_args()
    generate(pathlib.Path(args.host_build).resolve(), args.skip_shots, args.lenient_lessons)


if __name__ == "__main__":
    main()
