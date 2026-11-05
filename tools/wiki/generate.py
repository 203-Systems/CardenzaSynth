#!/usr/bin/env python3
"""Draws what the Cardenza wiki (wiki/, Docusaurus) shows from the firmware, into wiki/generated/:

  data/manual.json     version and chapter order (docs/learn/manual.json)
  data/dump.json       pages, parameters, shortcuts and lists (CardenzaLearnDump)
  data/leds.json       the key lights of every screenshot (CardenzaSimShot)
  data/sections.json   every section id: its chapter and its titles (for links from the guide)
  data/synonyms.json   search synonyms (docs/learn/synonyms.json)
  data/icons.json      the icons (tools/learn/figures.py), data/scene-map.json the scene map's cards
  data/guide.json      the guide's lessons (docs/learn/guide.json, keys and demos resolved), and
  data/guide-leds.json the key lights of their pictures; the lessons run on the firmware first
  static/img/*.png     the screenshots, the guide's pictures
  static/reference.html  sends links of the one-page manual (reference.html#id) to the page of that id

  python tools/wiki/generate.py --host-build build/linux-groove
  python tools/wiki/generate.py --skip-shots          reuse the last screenshots (build/wiki)
  python tools/wiki/generate.py --lenient-lessons     warn instead of failing when a lesson does not
                                                      complete on this firmware (a stale host build)

The pictures are drawn by the same code as `make learn` (tools/learn/build.py); this only puts them
where the wiki reads them. A Windows host build (Debug/ or Release/ with .exe) works too.
"""
import argparse
import json
import os
import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
LEARN_TOOLS = ROOT / "tools" / "learn"
sys.path.insert(0, str(LEARN_TOOLS))

import build as learn  # noqa: E402  (tools/learn/build.py)
import guide as lessons  # noqa: E402  (tools/learn/guide.py)

WIKI = ROOT / "wiki"
GEN = WIKI / "generated"
WORK = ROOT / "build" / "wiki"
TOOLS = {"shot": ("Devices/CardenzaSim", "CardenzaSimShot"), "dump": ("tests", "CardenzaLearnDump")}


def host_build(path):
    """The host build as tools/learn/build.py expects it (tools without an extension). A Windows
    build keeps them as Debug/NAME.exe or Release/NAME.exe: those are copied into that layout."""
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


def sections(chapters):
    """id -> {chapter, title: {zh, en}} for every heading of the manual."""
    out = {}
    for lang in learn.LANGS:
        for chapter in chapters:
            text = (learn.LEARN / lang / f"{chapter}.md").read_text(encoding="utf-8")
            for m in learn.re.finditer(r"^(#{1,3})\s+(.*?)\s*(?:\{#([\w-]+)\})?$", text, learn.re.M):
                sid = m.group(3) or (chapter if len(m.group(1)) == 1 else learn.slug(m.group(2)))
                entry = out.setdefault(sid, {"chapter": chapter, "title": {}})
                entry["title"][lang] = learn.plain(m.group(2))
    return out


REDIRECT = """<!doctype html>
<meta charset="utf-8">
<title>Cardenza manual</title>
<meta name="robots" content="noindex">
<script>
// The guide links to the manual as reference.html#section (the one-page manual before the wiki).
// Each section now lives on its chapter's page, in the reader's language.
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
    each lesson's place in the course and its step pictures. The words keep the manual's inline
    markup ({{key:..}}, [[#id]]), which the pages render (src/guide/text.tsx)."""
    order = lessons.lesson_ids(guide)
    out = {"title": guide["title"], "order": order, "parts": guide["parts"], "lessons": {}}
    for part in guide["parts"]:
        for number, lesson_id in enumerate(part["lessons"], start=1):
            lesson = guide["lessons"][lesson_id]
            more = lesson.get("more")
            if more and more not in index:
                sys.exit(f"wiki: guide lesson {lesson_id} links to unknown manual section #{more}")
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
                for number_in, step in enumerate(lesson["steps"], start=1):
                    entry["steps"].append({
                        "say": step["say"],
                        "keys": lessons.key_indices(step.get("keys", "")),
                        "keySpec": step.get("keys", ""),
                        # Key presses are checked by KeyIndex on the page.
                        "done": [[c[0], lessons.key_indices(c[1])] + c[2:] if c[0] == "press" else c for c in step["done"]],
                        "demo": [list(action) for action in lessons.parse_demo(step["demo"])],
                        "shot": lessons.shot_name(lesson_id, number_in),
                        "marks": step.get("marks", []),
                        "lights": bool(step.get("lights")),
                        "keymarks": step.get("keymarks", []),
                    })
            out["lessons"][lesson_id] = entry
    return out


def generate(sim_build, skip_shots, lenient):
    manual = json.loads((learn.LEARN / "manual.json").read_text(encoding="utf-8"))
    chapters = manual["chapters"]
    data, static = GEN / "data", GEN / "static"
    data.mkdir(parents=True, exist_ok=True)
    static.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)

    sim = host_build(sim_build)
    dump = learn.run_dump(sim)
    if skip_shots and (WORK / "leds.json").exists() and (static / "img").exists():
        leds = json.loads((WORK / "leds.json").read_text())
    else:
        leds = learn.run_shots(sim, static, WORK)

    index = sections(chapters)
    (data / "manual.json").write_text(json.dumps(manual, ensure_ascii=False, indent=1), encoding="utf-8")
    (data / "dump.json").write_text(json.dumps(dump, ensure_ascii=False), encoding="utf-8")
    (data / "leds.json").write_text(json.dumps(leds), encoding="utf-8")
    (data / "sections.json").write_text(json.dumps(index, ensure_ascii=False), encoding="utf-8")
    shutil.copyfile(learn.LEARN / "synonyms.json", data / "synonyms.json")
    (data / "icons.json").write_text(json.dumps(learn.figures.ICONS), encoding="utf-8")
    (data / "scene-map.json").write_text(json.dumps(learn.SCENE_MAP, ensure_ascii=False), encoding="utf-8")

    # The guide: its lessons run on the firmware as `make learn` runs them (their pictures come from
    # that run, and a step that does not complete fails it); the wiki draws them from data/guide.json.
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
    for stale in ("index.html", "learn.css", "guide.css", "guide.js", "guide-anim.js", "learn-search.js"):
        (static / stale).unlink(missing_ok=True)  # the static guide page of before
    (static / "reference.html").write_text(REDIRECT.replace("__SECTIONS__", json.dumps(
        {sid: {"chapter": entry["chapter"]} for sid, entry in index.items()})), encoding="utf-8")

    commit, _date = learn.git_info()
    print(f"wiki: {GEN} v{manual['version']} ({commit or 'dev'}), {len(leds)} screenshots, {len(index)} sections, "
          f"{len(lessons.lesson_ids(guide))} guide lessons")
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
