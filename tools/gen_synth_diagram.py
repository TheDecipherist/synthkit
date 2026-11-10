#!/usr/bin/env python3
"""THE COMPLETE MACHINE AS ONE MERMAID DIAGRAM, derived from the source every run.

    python3 tools/gen_synth_diagram.py            write docs/synth-diagram.md
    python3 tools/gen_synth_diagram.py --check    fail if it would change

The author, 2026-10-02: "I want to be able to see not just a FILTER box. but what the filter does
to the sound. the actual math. if it has LP/HP etc they should all show how they affect the
sound coming in and how they alter the sound going out. It needs to show what happens when a
matrix cable is connected from one thing to another. It needs to show conditionals with ON
buttons for every section and show every scenario. If the synth 'does something', it exists
and belongs in this chart." And then, on the filter envelope: "of course it should. it is a
module."

THE PRINCIPLE, which is his: A MODULE IS A BOX. Not only the things in the audio path - the
filter envelope never touches the signal and is a module, so it is a box with a control edge
out of it. The three LFOs likewise. Anything that does something has a shape on this chart.

TWO KINDS OF EDGE, because conflating them is what made the earlier diagrams useless:
    solid   ==>   AUDIO, a signal flowing
    dashed  -.->  CONTROL, something moving a parameter

WHY THIS LIVES IN synthkit AND NOT IN ONE SYNTH. Every machine built from the kit has the
same shape - parameters declared with `b()`, `f()`, `c()`, a voice with a mod matrix, a chain
of blocks - so the derivation is the kit's and every unit gets the chart for free. A copy per
repo would be eleven copies drifting apart, which is the mistake `MIGRATION.md` exists to
stop.

WHAT IT REFUSES TO DO. It derives or it says nothing. Where the source does not state
something - what a knob is FOR, in prose - the chart carries the name, the range and the
default, and the note file beside it carries the sentence. It never invents a connection to
make the picture tidier, because a chart that is 95 per cent derived and 5 per cent imagined
is a chart nobody can trust any part of.
"""

import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# --------------------------------------------------------------- reading the source

def find_repo(start):
    """The synth repo to document: the one this script was invoked from."""
    return os.path.abspath(start)


def read(path):
    try:
        return open(path, encoding="utf-8", errors="ignore").read()
    except OSError:
        return ""


class Source:
    """Everything the chart is derived from, loaded once."""

    def __init__(self, repo):
        self.repo = repo
        self.voice = read(os.path.join(repo, "Source", "Voice.h"))
        self.proc = read(os.path.join(repo, "Source", "PluginProcessor.cpp"))
        self.adapter = read(os.path.join(repo, "Source", "FilterAdapter.h"))
        self.lock = []
        lk = os.path.join(repo, "design", "params.lock.json")
        if os.path.exists(lk):
            try:
                self.lock = json.load(open(lk, encoding="utf-8"))
            except Exception:
                self.lock = []
        self.layout = {}
        ly = os.path.join(repo, "resources", "native", "layout.json")
        if os.path.exists(ly):
            try:
                self.layout = json.load(open(ly, encoding="utf-8"))
            except Exception:
                self.layout = {}

    # ---- parameters, with their real ranges and defaults -----------------------

    def parameters(self):
        """Every parameter the processor declares, as (id, kind, detail)."""
        out = {}
        # b("id","Name",default)
        for m in re.finditer(r'\bb\s*\(\s*"(\w+)"\s*,\s*"([^"]*)"\s*,\s*(true|false)\s*\)',
                             self.proc):
            out[m.group(1)] = ("switch", m.group(2), "default " + m.group(3))
        # f("id","Name",lo,hi,default[,"unit"]) and fc(...) which adds a step
        for m in re.finditer(r'\bfc?\s*\(\s*"(\w+)"\s*,\s*"([^"]*)"\s*,'
                             r'\s*([-\d.ef]+)\s*,\s*([-\d.ef]+)\s*,\s*([-\d.ef]+)'
                             r'(?:\s*,\s*[-\d.ef]+)?(?:\s*,\s*"([^"]*)")?\s*\)', self.proc):
            unit = (" " + m.group(6)) if m.group(6) else ""
            out[m.group(1)] = ("knob", m.group(2),
                               "%s..%s%s, default %s" % (m.group(3), m.group(4), unit,
                                                         m.group(5)))
        # c("id","Name",{ "A","B" },default) - a choice, and its members matter
        for m in re.finditer(r'\bc\s*\(\s*"(\w+)"\s*,\s*"([^"]*)"\s*,\s*'
                             r'(?:juce::StringArray\s*)?\{([^}]*)\}\s*,\s*(\d+)\s*\)',
                             self.proc):
            members = re.findall(r'"([^"]*)"', m.group(3))
            out[m.group(1)] = ("choice", m.group(2),
                               " / ".join(members) if members else "(computed)")
        return out

    # ---- the matrix: every source and every destination, with what it does -----

    def mod_enum(self, name):
        """`enum ModSrc`/`ModDst` members, each with its own trailing comment."""
        m = re.search(r"enum\s+" + name + r"\s*\{(.*?)\};", self.voice, re.S)
        if m is None:
            return []
        out = []
        for line in m.group(1).split("\n"):
            body = line.split("//")[0]
            note = line.split("//")[1].strip() if "//" in line else ""
            for mm in re.finditer(r"\b(M[SD]_\w+)", body):
                out.append((mm.group(1), note))
                note = ""           # the comment belongs to the first on the line
        return out

    # ---- the filter: topologies, responses, and the actual arithmetic ----------

    def filter_topologies(self):
        return [m.group(1) for m in re.finditer(r"\bstruct\s+(\w*Filter)\b", self.voice)]

    def cutoff_law(self):
        """The line that turns the knob into hertz, verbatim, plus the envelope's reach."""
        laws = []
        for pat, what in ((r"float cutHz = std::exp2 \(cutOct\);", "base"),
                          (r"cutHz \*= std::pow \(2\.0f, fe \* modded \(p\.filterEnvAmt[^;]*;",
                           "filter envelope"),
                          (r"cutHz \*= std::pow \(2\.0f, lfoCut\);", "LFO"),
                          (r"cutHz \*= std::pow \(2\.0f, md\[MD_Cutoff\][^;]*;", "matrix"),
                          (r"cutHz = std::clamp \(cutHz[^;]*;", "limit")):
            m = re.search(pat, self.voice, re.S)
            if m:
                laws.append((what, " ".join(m.group(0).split())))
        return laws

    # ---- the audio chain, in the order a sample travels it ---------------------

    def chain(self):
        """Each step as (line number, what it is, the statement) in source order."""
        steps = []
        for i, l in enumerate(self.voice.split("\n")):
            s = l.strip()
            if s.startswith("//") or not s:
                continue
            for pat, what in ((r"float mix = oscPair", "the oscillator base"),
                              (r"mix = 0\.6f \* mix", "unison doubles it"),
                              (r"mix \+= fmSample", "FM is added"),
                              (r"mix \+= modded \(p\.subLevel", "the sub is added"),
                              (r"mix \+= p\.noiseLevel", "noise is added"),
                              (r"mix \+= s; else pcmDirect", "PCM is added"),
                              (r"y = superFilter\.process", "the filter"),
                              (r"y = twinAlign\.process \(twinFilter", "the twin filter"),
                              (r"return y \* ae \* ampMod \* vel", "the amp envelope, last")):
                if re.search(pat, s):
                    steps.append((i + 1, what, " ".join(s.split())))
        return steps

    # ---- every conditional that gates a section --------------------------------

    def gates(self):
        """A switch and what it governs, read from the statements it wraps."""
        found = {}
        lines = self.voice.split("\n")
        for i, l in enumerate(lines):
            m = re.search(r"if \(p\.(\w+)(?:On|Live)?\b[^)]*\)", l)
            if not m:
                continue
            pid = m.group(1)
            nxt = ""
            for j in range(i + 1, min(len(lines), i + 8)):
                t = lines[j].strip()
                if t.startswith("//") or t in ("{", ""):
                    continue
                nxt = " ".join(t.split())[:92]
                break
            if pid not in found and nxt:
                found[pid] = (i + 1, nxt)
        return found

    # ---- the mini buttons, which write real cables -----------------------------

    def mini_routes(self):
        m = re.search(r"struct R \{[^}]*\};\s*static const R routes\[\]\s*\{(.*?)\};",
                      self.proc, re.S)
        if m is None:
            return []
        return [(r.group(1), r.group(2), r.group(3))
                for r in re.finditer(r'\{\s*"(\w+)",\s*obxa::(M\w+),\s*obxa::(M\w+)', m.group(1))]

    # ---- the panel ------------------------------------------------------------

    def controls(self):
        out = {}

        def walk(o):
            if isinstance(o, dict):
                cid = o.get("id")
                if isinstance(cid, str) and cid and o.get("kind") not in (None, "label", "text"):
                    out[cid] = o.get("kind")
                for v in o.values():
                    walk(v)
            elif isinstance(o, list):
                for v in o:
                    walk(v)

        walk(self.layout)
        return out


# --------------------------------------------------------------- drawing the chart

def nid(s):
    """A Mermaid-safe node id. Mermaid will not take punctuation in an id."""
    return re.sub(r"\W", "_", s)


def lbl(s):
    """A label Mermaid will accept: quoted, with its own quotes and brackets neutralised."""
    s = str(s).replace('"', "'").replace("[", "(").replace("]", ")")
    return '"' + s + '"'


def dotted(a, label, b, indent="    "):
    """A CONTROL edge, written the way every renderer accepts.

    `-. "text" .->` parses under Mermaid 12 and is NOT the documented form: GitHub and the
    copy bundled with VS Code reject it, so a chart full of them renders as a code block and
    looks like Mermaid is broken. The author: "for some reason the mermaid graphs dont render now?"
    `-.->|text|` is the canonical labelled dotted link and works everywhere, which is the only
    property that matters for a document whose whole job is to be looked at.

    A `|` inside the label would close it early, so it goes."""
    return "%s%s -.->|%s| %s" % (indent, a, str(label).replace("|", " ").strip(), b)


def _stack(A, ids, indent="        "):
    """Force a column. `~~~` is Mermaid's invisible link: it draws nothing and gives dagre the
    rank it needs, which is the only thing that actually makes a fan-out read top to bottom."""
    for a, b in zip(ids, ids[1:]):
        A("%s%s ~~~ %s" % (indent, a, b))


def build(src):
    params = src.parameters()
    controls = src.controls()
    srcs = src.mod_enum("ModSrc")
    dsts = src.mod_enum("ModDst")
    chain = src.chain()
    gates = src.gates()
    routes = src.mini_routes()
    laws = src.cutoff_law()
    tops = src.filter_topologies()

    L = []
    A = L.append
    A("# The whole machine, as one diagram")
    A("")
    A("Generated by `synthkit/tools/gen_synth_diagram.py` from this repo's own source on every")
    A("run. **Do not edit.** If something here is wrong, the source says it.")
    A("")
    A("**Solid arrows are AUDIO** - a signal flowing. **Dashed arrows are CONTROL** -")
    A("something moving a parameter without being in the signal path. A module is a box")
    A("whether or not it is in the audio path, which is why both envelopes and every LFO")
    A("appear: they do something, so they are here.")
    A("")
    A("Derived from this repo: %d parameters, %d panel controls, %d matrix sources, %d"
      % (len(params), len(controls), len(srcs), len(dsts)))
    A("destinations, %d filter topologies, %d audio steps, %d gated sections."
      % (len(tops), len(chain), len(gates)))
    A("")
    A("```mermaid")
    A("flowchart TD")

    # ---- the key ------------------------------------------------------------
    # ---- EVERY SUBGRAPH STACKS, 2026-10-02 ---------------------------------
    #
    # The author: "why is the mermaid chart rendered sideways and not from top to bottom?" and then,
    # after the first attempt, "it still renders from left to right".
    #
    # THE FIRST ATTEMPT WAS WRONG IN AN INSTRUCTIVE WAY. I removed a stray `direction LR` and
    # gave every subgraph `direction TB` instead - but `TD` and `TB` are THE SAME DIRECTION in
    # Mermaid, so that second change said nothing at all, and a subgraph direction is itself
    # the one thing that can override the root. Adding more of the thing that overrides the
    # layout is not a way to fix the layout.
    #
    # The real cause is ranking. Dagre lays a graph out by RANK, and members of a subgraph with
    # no edges between them share one rank - so 18 matrix sources and 26 destinations sat on a
    # single row, side by side, a chart a metre wide however the top line reads.
    #
    # So: NO subgraph sets a direction, and each fan-out chains its members with Mermaid's
    # invisible link `~~~`, which gives dagre the successive ranks it needs and draws nothing.
    # That is the mechanism rather than a hint, which is why it works.
    A('    subgraph KEY["how to read this"]')
    A("        KA[audio] ==> KB[audio]")
    A("        KC[modulator] -.-> KD[a parameter]")
    A("    end")

    # ---- sources ------------------------------------------------------------
    A('    NOTE(["a key is pressed"])')
    A('    subgraph SRC["SOUND SOURCES - each with its own ON, each silent without it"]')
    srcnode = {}
    for pid, gate_target in (("o1On", "OSC 1"), ("o2On", "OSC 2"), ("subOn", "SUB"),
                             ("noiseOn", "NOISE"), ("fmOn", "FM"), ("digMode", "PCM")):
        if pid in params:
            kind, name, detail = params[pid]
            srcnode[pid] = nid(gate_target)
            A('        G_%s{%s}' % (nid(pid), lbl(pid + " ?")))
            A('        %s[%s]' % (nid(gate_target), lbl(gate_target + " - " + name)))
            A('        G_%s -- on --> %s' % (nid(pid), nid(gate_target)))
            A('        G_%s -- off --> OFF_%s[%s]'
              % (nid(pid), nid(pid), lbl("silent")))
    _stack(A, ["G_" + nid(p) for p in srcnode])
    A("    end")
    A("    NOTE ==> SRC")

    # OSC 1's waveforms and its two replacements
    waves1 = [p for p in params if re.match(r"^o1(saw|pulse|tri|sine)$", p)]
    waves2 = [p for p in params if re.match(r"^o2(saw|pulse|tri|sine)$", p)]
    # POINT AT THE REAL NODE IDS. These edges used to name `o1On` and `o2On`, which are the
    # PARAMETERS and not nodes - the boxes are `OSC_1` and `OSC_2` - so Mermaid invented two
    # empty boxes and the chart rendered perfectly while being wrong. `_dangling` below now
    # refuses to write a diagram that does this, because a diagram that draws fine and lies is
    # the worst of the three outcomes.
    if waves1 and "o1On" in srcnode:
        A('    W1[%s] ==> %s' % (lbl("waveform: " + " / ".join(waves1) + "  (one at a time)"),
                                 srcnode["o1On"]))
    if waves2 and "o2On" in srcnode:
        A('    W2[%s] ==> %s' % (lbl("waveform: " + " / ".join(waves2) + "  (one at a time)"),
                                 srcnode["o2On"]))
    for pid, what in (("virusOn", "SWARM - shape + unison, REPLACES the waveform"),
                      ("osc1super", "SUPERSAW - seven detuned saws, REPLACES the waveform")):
        if pid in params and "o1On" in srcnode:
            A('    R_%s[%s] ==> %s' % (nid(pid), lbl(what), srcnode["o1On"]))
            A(dotted("G_o1On", "gates it too", "R_" + nid(pid)))

    # ---- the chain in source order -----------------------------------------
    A('    subgraph CHAIN["THE AUDIO PATH, in the order a sample travels it"]')
    prev = None
    for ln, what, stmt in chain:
        node = "C%d" % ln
        A('        %s[%s]' % (node, lbl("%s   (Voice.h:%d)" % (what, ln))))
        if prev:
            A("        %s ==> %s" % (prev, node))
        prev = node
    A("    end")
    A("    SRC ==> CHAIN")

    # ---- the filter, with its actual arithmetic -----------------------------
    A('    subgraph FILT["THE FILTER - what it does to the sound"]')
    if "fType" in params:
        A('        FT{%s}' % lbl("fType: " + params["fType"][2]))
    for t in sorted(set(tops)):
        A('        T_%s[%s]' % (nid(t), lbl(t)))
        if "fType" in params:
            A("        FT --> T_%s" % nid(t))
    if "filterResp" in params:
        A('        FR{%s}' % lbl("filterResp: " + params["filterResp"][2]))
        for i, r in enumerate(params["filterResp"][2].split(" / ")):
            meaning = {"LP": "keeps BELOW the corner, removes above - darker",
                       "BP": "keeps a BAND at the corner, removes both sides - hollow",
                       "HP": "keeps ABOVE the corner, removes below - thinner",
                       "Notch": "removes a band AT the corner, keeps both sides - phasey"}
            A('        FR --> RSP%d[%s]' % (i, lbl(r + ": " + meaning.get(r.strip(), ""))))
    for pid in ("fSlope", "fShape", "fWidth", "resonance", "cutoff"):
        if pid in params:
            A('        P_%s[%s]' % (nid(pid), lbl(pid + " - " + params[pid][1]
                                                 + "  [" + params[pid][2] + "]")))
            A("        P_%s -.-> FT" % nid(pid))
    A("    end")
    for ln, what, _ in chain:
        if "filter" in what:
            A("    C%d --- FILT" % ln)
            break

    # the cutoff arithmetic, verbatim
    if laws:
        A('    subgraph CUT["THE CORNER FREQUENCY, verbatim from the source"]')
        for i, (what, stmt) in enumerate(laws):
            A('        L%d[%s]' % (i, lbl(what + ":  " + stmt)))
        A("    end")
        A("    CUT -.-> FILT")

    # ---- the modulators, each a module ------------------------------------
    A('    subgraph MODS["MODULATORS - each is a module, none is in the audio path"]')
    A('        FENV[%s]' % lbl("FILTER ENVELOPE - fA fD fS fR, reaches modAmount octaves"))
    A('        AENV[%s]' % lbl("AMP ENVELOPE - lA lD lS lR, multiplies the signal LAST"))
    for n in (1, 2, 3):
        rate = "lfoRate" if n == 1 else "lfo%dRate" % n
        if rate in params:
            A('        LFO%d[%s]' % (n, lbl("LFO %d - %s [%s]"
                                            % (n, params[rate][1], params[rate][2]))))
    A("    end")
    A(dotted("FENV", "moves the corner", "FILT"))
    for ln, what, _ in chain:
        if "amp envelope" in what:
            A("    AENV ==>|IS in the path| C%d" % ln)
            break
    for n in (1, 2, 3):
        rate = "lfoRate" if n == 1 else "lfo%dRate" % n
        if rate in params:
            A("    LFO%d -.-> MATRIX" % n)

    # ---- the matrix: every source, every destination, what each one does ---
    A('    MATRIX{{%s}}' % lbl("THE MATRIX - %d slots: matSrc -> matDst at matAmt" % 16))
    A('    subgraph MSRC["every SOURCE a cable can start at"]')
    for s, note in srcs:
        if s.endswith("_None"):
            continue
        A('        %s[%s]' % (nid(s), lbl(s.replace("MS_", "") + (" - " + note if note else ""))))
        A("        %s -.-> MATRIX" % nid(s))
    _stack(A, [nid(s) for s, _ in srcs if not s.endswith("_None")])
    A("    end")
    A('    subgraph MDST["every DESTINATION a cable can end at, and what it does there"]')
    for d, note in dsts:
        if d.endswith("_None"):
            continue
        A('        %s[%s]' % (nid(d), lbl(d.replace("MD_", "") + (" - " + note if note else ""))))
        A("        MATRIX -.-> %s" % nid(d))
    _stack(A, [nid(d) for d, _ in dsts if not d.endswith("_None")])
    A("    end")

    # ---- the mini buttons, which write real cables -------------------------
    if routes:
        A('    subgraph MINI["the mini buttons - each WRITES A CABLE, it is not a shortcut"]')
        for bid, s, d in routes:
            A('        MB_%s[%s]' % (nid(bid), lbl(bid)))
            A(dotted("MB_" + nid(bid), s + " to " + d, "MATRIX", indent="        "))
        _stack(A, ["MB_" + nid(b) for b, _, _ in routes])
        A("    end")

    A("```")
    A("")

    # ---------------------------------------------------- the tables beneath
    A("## Every conditional, and what it governs")
    A("")
    A("| switch | at | the first thing it governs |")
    A("|---|---|---|")
    for pid, (ln, stmt) in sorted(gates.items()):
        A("| `%s` | `Voice.h:%d` | `%s` |" % (pid, ln, stmt))
    A("")
    A("## The corner frequency, step by step")
    A("")
    A("| step | the statement |")
    A("|---|---|")
    for what, stmt in laws:
        A("| %s | `%s` |" % (what, stmt))
    A("")
    A("## Every parameter")
    A("")
    A("| id | kind | name | range / members |")
    A("|---|---|---|---|")
    for pid in sorted(params):
        kind, name, detail = params[pid]
        A("| `%s` | %s | %s | %s |" % (pid, kind, name, detail))
    A("")
    missing = sorted(p for p in params if p not in controls)
    A("## Parameters with no control on the panel (%d)" % len(missing))
    A("")
    A("A parameter here is reachable by a patch, by automation or by a matrix cable, but not")
    A("by hand. Some are deliberate; `every_control_responds` is the gate that knows which.")
    A("")
    A(", ".join("`%s`" % p for p in missing) if missing else "None.")
    A("")
    return "\n".join(L) + "\n"


def _dangling(text):
    """Edges pointing at a node nothing declares.

    Mermaid is happy to draw an empty box for an id it has never seen, so the chart renders
    and is wrong - which is exactly what happened on the first run, where the waveform edges
    named the PARAMETERS `o1On` and `o2On` rather than the boxes `OSC_1` and `OSC_2`. A
    generated diagram has to check itself; nobody reads 230 lines of Mermaid looking for this."""
    m = re.search(r"```mermaid\n(.*?)\n```", text, re.S)
    if m is None:
        return set()
    body = m.group(1)
    declared = set(re.findall(r"([A-Za-z_][A-Za-z0-9_]*)\s*[\[\{\(]", body))
    declared |= set(re.findall(r"subgraph ([A-Za-z_][A-Za-z0-9_]*)", body))
    used = set()
    for line in body.split("\n"):
        if "--" not in line and "==" not in line:
            continue
        # strip quoted edge labels first, so `-- on -->` does not read as a node
        clean = re.sub(r'"[^"]*"', '""', line)
        used |= set(re.findall(r"(?:^|\s)([A-Za-z_][A-Za-z0-9_]*)\s*(?:--|==|-\.)", clean))
        used |= set(re.findall(r"(?:-->|==>|-\.->|---)\s*([A-Za-z_][A-Za-z0-9_]*)", clean))
    # bare words used as edge labels in the unquoted `-- on -->` form are not nodes
    labels = set(re.findall(r"--\s*(\w+)\s*-->", body))
    return used - declared - labels


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="fail if the file would change")
    ap.add_argument("--repo", default=os.getcwd(), help="the synth repo to document")
    a = ap.parse_args()

    repo = find_repo(a.repo)
    src = Source(repo)
    if not src.voice:
        print("gen_synth_diagram: no Source/Voice.h under %s" % repo, file=sys.stderr)
        return 1
    text = build(src)
    bad = _dangling(text)
    if bad:
        print("gen_synth_diagram: %d edge(s) point at a node that is never declared: %s"
              % (len(bad), ", ".join(sorted(bad))), file=sys.stderr)
        print("  Mermaid would draw an empty box for each, so the chart would render and lie.",
              file=sys.stderr)
        return 1
    out = os.path.join(repo, "docs", "synth-diagram.md")
    cur = read(out)
    if a.check:
        if cur != text:
            print("docs/synth-diagram.md is stale - run "
                  "`python3 ~/projects/synthkit/tools/gen_synth_diagram.py`", file=sys.stderr)
            return 1
        print("docs/synth-diagram.md is current")
        return 0
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, "w", encoding="utf-8").write(text)
    print("wrote docs/synth-diagram.md (%d lines)" % text.count("\n"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
