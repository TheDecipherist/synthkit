#!/usr/bin/env python3
"""Instantiate a new machine from synthkit.

    python3 tools/new_synth.py Jupiter8 --kind synth --brand Roland --model "Jupiter-8"
    python3 tools/new_synth.py Space1 --kind effect --brand Roland --model "RE-201"

It copies the skeleton, renames everything consistently, writes the docs, makes
the first commit, and prints what to do next. The result builds and passes its
own gates before a single byte of the machine has been decoded, which is the
whole point: the clock gate, the differentiation probe and the control audit are
already wired, so today's failures are structurally impossible rather than
merely documented.

It never copies synthshell, dspblocks or synthpanel-creator. Those are
DEPENDENCIES, found by walking up from the new repo.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
KIT = os.path.dirname(HERE)
PROJECTS = os.path.dirname(KIT)

TEMPLATE = {
    "synth":  {"dir": "synth",  "cls": "ExampleSynth", "ns": "examplesynth",
               "code": "Exs1", "macro": "EXAMPLESYNTH"},
    "effect": {"dir": "effect", "cls": "ExampleFx",    "ns": "examplefx",
               "code": "Exf1", "macro": "EXAMPLEFX"},
}

# Copied from synthkit's own docs so a unit carries them, rather than pointing at
# a file in another repo that a reader may not have open.
SHARED_DOCS = ["before-you-call-it-finished.md", "provenance.md", "the-gates.md",
               "new-machine.md"]

SKIP_DIRS = {"build", "node_modules", "__pycache__", ".git"}
BINARY_EXT = {".png", ".jpg", ".pdf", ".syx", ".bin", ".rom", ".ico"}


def die(msg):
    print(f"new_synth: {msg}", file=sys.stderr)
    sys.exit(1)


def plugin_code(name, override):
    """JUCE's four-character plugin code: one uppercase, three lowercase.

    It has to be UNIQUE across every plugin on the machine - two plugins sharing
    a code are the same plugin as far as a host's cache is concerned, and the
    symptom is one unit opening with the other's editor. Check it against the
    siblings you already have."""
    if override:
        if len(override) != 4:
            die("--code must be exactly four characters")
        return override
    letters = re.sub(r"[^A-Za-z0-9]", "", name)
    if len(letters) < 4:
        letters = (letters + "xxxx")[:4]
    return letters[0].upper() + letters[1:4].lower()


def substitute(text, sub):
    for a, b in sub:
        text = text.replace(a, b)
    return text


def copy_tree(src, dst, sub, written):
    for root, dirs, files in os.walk(src):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        rel = os.path.relpath(root, src)
        out_dir = os.path.join(dst, rel) if rel != "." else dst
        os.makedirs(out_dir, exist_ok=True)
        for f in files:
            if f == ".DS_Store":
                continue
            src_f = os.path.join(root, f)
            out_f = os.path.join(out_dir, substitute(f, sub))
            if os.path.splitext(f)[1].lower() in BINARY_EXT:
                shutil.copy2(src_f, out_f)
            else:
                with open(src_f, errors="ignore") as fh:
                    body = fh.read()
                with open(out_f, "w") as fh:
                    fh.write(substitute(body, sub))
                if os.access(src_f, os.X_OK):
                    os.chmod(out_f, 0o755)
            written.append(os.path.relpath(out_f, dst))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("name", help="the unit's name in PascalCase, e.g. Jupiter8")
    ap.add_argument("--kind", choices=sorted(TEMPLATE), default="synth",
                    help="synth (keyboard row, CHORDS/ARP/CHOPPER, three clock states) "
                         "or effect (AUDITION tab, no keyboard, two clock states)")
    ap.add_argument("--brand", default="Unknown", help='e.g. "Roland"')
    ap.add_argument("--model", default="", help='e.g. "Jupiter-8"')
    ap.add_argument("--display", default="", help="the product name shown on screen "
                                                  "(defaults to the name)")
    ap.add_argument("--code", default="", help="the four-character JUCE plugin code")
    ap.add_argument("--dir", default="", help="where to create it "
                                              "(defaults to ~/projects/<lowercase name>)")
    ap.add_argument("--force", action="store_true", help="overwrite an existing directory")
    ap.add_argument("--no-git", action="store_true", help="do not init a repo or commit")
    a = ap.parse_args()

    if not re.match(r"^[A-Za-z][A-Za-z0-9_]*$", a.name):
        die("the name must start with a letter and hold only letters, digits and _")

    t = TEMPLATE[a.kind]
    ns = a.name.lower()
    display = a.display or a.name
    model = a.model or a.name
    code = plugin_code(a.name, a.code)
    dest = a.dir or os.path.join(PROJECTS, ns)

    if os.path.exists(dest):
        if not a.force:
            die(f"{dest} already exists (pass --force to overwrite)")
        shutil.rmtree(dest)

    # Order matters: the longest tokens first, or a shorter one eats a prefix of
    # a longer one and the result compiles into something subtly wrong.
    sub = [
        (t["macro"], a.name.upper()),
        (t["cls"], a.name),
        (t["ns"], ns),
        (t["code"], code),
        ("{{DISPLAY}}", display),
        ("{{BRAND}}", a.brand),
        ("{{MODEL}}", model),
        ("{{NAME}}", a.name),
        ("{{name}}", ns),
    ]

    written = []
    copy_tree(os.path.join(KIT, "template", "common"), dest, sub, written)
    copy_tree(os.path.join(KIT, "template", t["dir"]), dest, sub, written)

    # The cmake helpers travel with the unit, so `../../cmake` is not needed and
    # the repo stands alone.
    os.makedirs(os.path.join(dest, "cmake"), exist_ok=True)
    for f in os.listdir(os.path.join(KIT, "cmake")):
        shutil.copy2(os.path.join(KIT, "cmake", f), os.path.join(dest, "cmake", f))
        written.append(f"cmake/{f}")

    os.makedirs(os.path.join(dest, "docs"), exist_ok=True)
    for f in SHARED_DOCS:
        shutil.copy2(os.path.join(KIT, "docs", f), os.path.join(dest, "docs", f))
        written.append(f"docs/{f}")

    shutil.copy2(os.path.join(KIT, "LICENSE"), os.path.join(dest, "LICENSE"))
    written.append("LICENSE")

    # The generated headers carry the unit's name in their FILENAME as well as
    # their contents, and the generator writes that name. Re-run it rather than
    # trusting the copy, so the very first `ctest -R generated_headers_are_current`
    # is a real check and not a tautology.
    gen = os.path.join(dest, "tools", "gen_headers.py")
    r = subprocess.run([sys.executable, gen], cwd=dest, capture_output=True, text=True)
    if r.returncode != 0:
        die(f"the generator failed in the new repo:\n{r.stdout}\n{r.stderr}")

    if not a.no_git:
        subprocess.run(["git", "init", "-q"], cwd=dest, check=True)
        subprocess.run(["git", "add", "-A"], cwd=dest, check=True)
        # the commit is the caller's own: their git user.name and user.email, never one written here
        subprocess.run(["git", "commit", "-q", "-m",
                        f"{display}: skeleton from synthkit\n\n"
                        f"{a.brand} {model}, {a.kind} variant. Builds, sounds, and every gate is\n"
                        f"wired: the clock probe, the differentiation probe, the control audit, the\n"
                        f"stored-values-reach-the-engine assertion, and a negative control for each.\n"
                        f"Nothing in the engine is this machine's yet - every law is tagged [REF]."],
                       cwd=dest, check=True)

    rel = os.path.relpath(dest, os.path.expanduser("~"))
    print(f"{display}  ({a.brand} {model}, {a.kind})")
    print(f"  created  ~/{rel}   {len(written)} files")
    print(f"  code     {code}   (JUCE plugin code - it MUST be unique across your plugins;")
    print(f"           two sharing a code are the same plugin to a host's cache)")
    print()
    print("NEXT")
    print(f"  cd ~/{rel}")
    print("  cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo")
    print("  cmake --build build")
    print("  ctest --test-dir build --output-on-failure")
    print()
    print("  That passes as it stands. It is a generic engine with generic laws, and")
    print("  every stand-in is marked TODO(new machine) with the playbook section that")
    print("  says how to find the real answer.")
    print()
    print("THEN, in this order (docs/new-machine.md)")
    print("  1. gather:   firmware image, manuals, sysex spec, factory banks, schematics")
    print("               (~/projects/eproms/reverse-engineer.md section 4)")
    print("  2. decode:   the container format, then the corpus profile, then the")
    print("               coefficient builders. Fill data/params.csv and data/factory.csv")
    print("               and re-run tools/gen_headers.py.")
    print("  3. prove:    the loader touches EVERY byte the machine stores and exports the")
    print("               same set back out, BEFORE writing any engine code.")
    print("  4. engine:   replace src/engine/, one law at a time, each with a provenance tag")
    print("  5. panel:    edit panel/panel.json, npm run panel:check, npm run panel:native")
    print("  6. finish:   docs/before-you-call-it-finished.md, all of it")
    print()
    print("  Write findings into docs/research.md, bugs into bugs.md and debts into")
    print("  todo.md AS THEY HAPPEN. Fill CLAUDE.md's non-negotiables the day you find")
    print("  the first one.")
    print()
    print("  synthshell, dspblocks and synthpanel-creator are DEPENDENCIES. If you find")
    print("  yourself copying a file out of any of them, stop.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
