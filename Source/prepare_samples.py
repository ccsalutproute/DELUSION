#!/usr/bin/env python3
"""
Télécharge des enregistrements d'instruments libres de droits, choisit les notes
utiles et prépare la banque de sons du plugin (avec l'outil SamplePrep).

Sources (toutes gratuites) :
  - VSCO 2 Community Edition (Versilian Studios)    : CC0 (domaine public)
  - VCSL (Versilian Community Sample Library)       : CC0 (domaine public)
  - Salamander Grand Piano V3 (Alexander Holm)      : CC-BY 3.0
  - Tone.js instruments (Nicholas Brosowsky et al.) : CC-BY 3.0

Usage : prepare_samples.py --prep chemin/SamplePrep --out dossier_sortie [--work dossier_travail]
"""
import argparse, json, os, re, subprocess, sys

REPOS = {
    "vsco": ("https://github.com/sgossner/VSCO-2-CE", "SFZ", [
        "/*.sfz",
        "/Brass/Trumpet/susvib/", "/Brass/Tenor Trombone/sus/", "/Brass/F Horn/sus/",
        "/Strings/Violin Section/susVib/", "/Strings/Viola Section/susvib/", "/Strings/Cello Section/susvib/",
        "/Strings/Solo Contrabass/SusVib/",
        "/Strings/Violin Section/Trem/", "/Strings/Viola Section/trem/", "/Strings/Cello Section/trem/",
        "/Strings/Violin Section/Pizz/", "/Strings/Viola Section/pizz/", "/Strings/Cello Section/pizzT/",
        "/Strings/Solo Contrabass/Pizz/", "/Strings/Solo Violin/Arco Vib/",
        "/Woodwinds/Flute/susvib/", "/Woodwinds/Oboe/Vib/", "/Woodwinds/Clarinet/susLong/",
        "/Strings/Harp/", "/Keys/Upright Piano/"]),
    "vcsl": ("https://github.com/sgossner/VCSL", None, [
        "/Idiophones/Plucked Idiophones/Kalimba, Kenya/",
        "/Aerophones/Reed Aerophones/Tenor Saxophone/Vibrato/"]),
    "sal": ("https://github.com/sfzinstruments/SalamanderGrandPiano", None, [
        "/Samples/*v6.flac", "/Samples/*v11.flac", "/Samples/*v16.flac", "/README.md", "/LICENSE"]),
    "tj": ("https://github.com/nbrosowsky/tonejs-instruments", None, [
        "/samples/guitar-nylon/*.wav", "/samples/guitar-acoustic/*.wav", "/README.md"]),
}

NOTE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def note_to_midi(s):
    m = re.match(r"^([A-Ga-g])(#|s|b)?(-?\d)$", s)
    if not m:
        return None
    pc = NOTE[m.group(1).upper()] + {"#": 1, "s": 1, "b": -1, None: 0}[m.group(2)]
    return 12 * (int(m.group(3)) + 1) + pc


def run(cmd, cwd=None):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=cwd, check=True)


def fetch(work):
    for name, (url, branch, paths) in REPOS.items():
        d = os.path.join(work, name)
        if not os.path.isdir(os.path.join(d, ".git")):
            cmd = ["git", "clone", "-q", "--filter=blob:none", "--no-checkout", "--depth", "1"]
            if branch:
                cmd += ["-b", branch]
            run(cmd + [url, d])
        subprocess.run(["git", "sparse-checkout", "set", "--no-cone", "--stdin"], cwd=d, check=True,
                       input="\n".join(paths).encode())
        run(["git", "-c", "core.quotepath=off", "read-tree", "-mu", "HEAD"], cwd=d)


# ---------------------------------------------------------------- SFZ (lecteur minimal)
def parse_sfz(path):
    text = open(path, encoding="latin-1").read().replace("\r", "\n")
    text = re.sub(r"//[^\n]*", "", text)
    default_path = ""
    levels = {"global": {}, "group": {}, "region": None}
    regions = []
    tokens = re.split(r"(<\w+>)", text)
    current = None
    for tok in tokens:
        tok = tok.strip()
        if not tok:
            continue
        if tok.startswith("<"):
            header = tok[1:-1]
            if current is not None:
                regions.append(current)
                current = None
            if header == "global":
                levels["global"] = {}
                levels["group"] = {}
                target = levels["global"]
            elif header in ("group", "master"):
                levels["group"] = {}
                target = levels["group"]
            elif header == "region":
                current = {}
                target = current
            else:
                target = {}
            levels["_t"] = target
            continue
        # opcodes : nom=valeur (la valeur peut contenir des espaces pour sample=)
        for m in re.finditer(r"(\w+)=(.*?)(?=\s+\w+=|$)", tok, re.S):
            k, v = m.group(1), m.group(2).strip()
            if k == "default_path":
                default_path = v.replace("\\", "/")
            else:
                levels["_t"][k] = v
        if current is not None and levels["_t"] is current:
            pass
    if current is not None:
        regions.append(current)
    # héritage global -> group -> region (approximation suffisante pour ces fichiers)
    out = []
    # on relit pour associer chaque région à son groupe
    cur_global, cur_group = {}, {}
    for tok in tokens:
        tok = tok.strip()
        if not tok:
            continue
        if tok.startswith("<"):
            hdr = tok[1:-1]
            mode = hdr
            if hdr == "global":
                cur_global, cur_group = {}, {}
            elif hdr in ("group", "master"):
                cur_group = {}
            elif hdr == "region":
                out.append({})
            continue
        ops = {m.group(1): m.group(2).strip() for m in re.finditer(r"(\w+)=(.*?)(?=\s+\w+=|$)", tok, re.S)}
        ops.pop("default_path", None)
        if mode == "global":
            cur_global.update(ops)
        elif mode in ("group", "master"):
            cur_group.update(ops)
        elif mode == "region" and out:
            merged = dict(cur_global); merged.update(cur_group); merged.update(out[-1]); merged.update(ops)
            out[-1] = merged

    def num(v):
        n = note_to_midi(v) if not re.match(r"^-?\d+(\.\d+)?$", v) else None
        return n if n is not None else float(v)

    base = os.path.dirname(path)
    res = []
    for r in out:
        if "sample" not in r:
            continue
        key = num(r.get("pitch_keycenter", r.get("key", "60")))
        lo = num(r.get("lokey", r.get("key", str(key))))
        hi = num(r.get("hikey", r.get("key", str(key))))
        res.append({
            "src": os.path.join(base, default_path, r["sample"].replace("\\", "/")),
            "lokey": int(lo), "hikey": int(hi), "key": int(key),
            "lovel": int(num(r.get("lovel", "0"))), "hivel": int(num(r.get("hivel", "127"))),
            "gainDb": float(r.get("volume", "0")), "tune": float(r.get("tune", "0")),
        })
    return res


def band(regions, lo, hi):
    """Garde seulement la partie du clavier [lo, hi] (pour assembler un ensemble)."""
    out = []
    for r in regions:
        a, b = max(r["lokey"], lo), min(r["hikey"], hi)
        if a <= b:
            r = dict(r); r["lokey"], r["hikey"] = a, b
            out.append(r)
    return out


def from_names(files, pattern, vel_of=lambda m: (0, 127)):
    """Instruments nommés par note (A2.wav, C4v6.flac...) : on calcule les zones du clavier."""
    items = []
    for f in files:
        m = re.search(pattern, os.path.basename(f))
        if not m:
            continue
        k = note_to_midi(m.group("note"))
        if k is None:
            continue
        lv, hv = vel_of(m)
        items.append((k, lv, hv, f))
    out = []
    for (lv, hv) in sorted({(i[1], i[2]) for i in items}):
        layer = sorted([i for i in items if i[1] == lv and i[2] == hv])
        keys = sorted({i[0] for i in layer})
        for i in layer:
            k = i[0]
            idx = keys.index(k)
            lo = 0 if idx == 0 else (keys[idx - 1] + k) // 2 + 1
            hi = 127 if idx == len(keys) - 1 else (k + keys[idx + 1]) // 2
            out.append({"src": i[3], "lokey": lo, "hikey": hi, "key": k, "lovel": lv, "hivel": hv, "gainDb": 0.0, "tune": 0.0})
    return out


def ls(d, ext):
    if not os.path.isdir(d):
        print("  ! dossier manquant :", d)
        return []
    return [os.path.join(d, f) for f in sorted(os.listdir(d)) if f.lower().endswith(ext)]


def build_job(work):
    v = lambda f: parse_sfz(os.path.join(work, "vsco", f))
    sal_vel = {"6": (0, 58), "11": (59, 98), "16": (99, 127)}
    insts = [
        {"id": "piano_grand", "loop": False, "maxLen": 8.0,
         "regions": from_names(ls(os.path.join(work, "sal", "Samples"), ".flac"),
                               r"^(?P<note>[A-G]#?\d)v(?P<v>\d+)\.flac$", lambda m: sal_vel[m.group("v")])},
        {"id": "piano_upright", "loop": False, "maxLen": 8.0, "regions": v("UprightPiano.sfz")},
        {"id": "trumpet", "loop": True, "maxLen": 5.0, "regions": v("TrumpetSusVib.sfz")},
        {"id": "trombone", "loop": True, "maxLen": 5.0, "regions": v("TromboneSus.sfz")},
        {"id": "horn", "loop": True, "maxLen": 5.0, "regions": v("FHornSus.sfz")},
        {"id": "strings_ens", "loop": True, "maxLen": 5.0,
         "regions": band(v("ViolinEnsSusVib.sfz"), 67, 127) + band(v("ViolaEnsSusVib.sfz"), 55, 66)
                    + band(v("CelloEnsSusVib.sfz"), 36, 54) + band(v("ContrabassSusVB.sfz"), 0, 35)},
        {"id": "strings_trem", "loop": True, "maxLen": 5.0,
         "regions": band(v("ViolinEnsTrem.sfz"), 62, 127) + band(v("ViolaEnsTrem.sfz"), 50, 61)
                    + band(v("CelloEnsTrem.sfz"), 0, 49)},
        {"id": "strings_pizz", "loop": False, "maxLen": 3.0,
         "regions": band(v("ViolinEnsPizz.sfz"), 67, 127) + band(v("ViolaEnsPizz.sfz"), 55, 66)
                    + band(v("CelloEnsPizz.sfz"), 36, 54) + band(v("ContrabassPizz.sfz"), 0, 35)},
        {"id": "violin_solo", "loop": True, "maxLen": 5.0, "regions": v("SViolinVib.sfz")},
        {"id": "flute", "loop": True, "maxLen": 5.0, "regions": v("FluteSusVib.sfz")},
        {"id": "oboe", "loop": True, "maxLen": 5.0, "regions": v("OboeSusVib.sfz")},
        {"id": "clarinet", "loop": True, "maxLen": 5.0, "regions": v("ClarinetSus.sfz")},
        {"id": "sax", "loop": True, "maxLen": 5.0,
         "regions": from_names(ls(os.path.join(work, "vcsl", "Aerophones/Reed Aerophones/Tenor Saxophone/Vibrato"), ".wav"),
                               r"_(?P<note>[A-G]#?\d)_var\d+\.wav$")},
        {"id": "guitar_nylon", "loop": False, "maxLen": 6.0,
         "regions": from_names(ls(os.path.join(work, "tj", "samples/guitar-nylon"), ".wav"), r"^(?P<note>[A-G]s?\d)\.wav$")},
        {"id": "guitar_steel", "loop": False, "maxLen": 6.0,
         "regions": from_names(ls(os.path.join(work, "tj", "samples/guitar-acoustic"), ".wav"), r"^(?P<note>[A-G]s?\d)\.wav$")},
        {"id": "harp", "loop": False, "maxLen": 6.0, "regions": v("Harp.sfz")},
        {"id": "kalimba", "loop": False, "maxLen": 5.0,
         "regions": from_names(ls(os.path.join(work, "vcsl", "Idiophones/Plucked Idiophones/Kalimba, Kenya"), ".wav"),
                               r"_(?P<note>[A-G]#?\d)_k\d+_vl\d_rr\d\.wav$")},
    ]
    for i in insts:
        missing = [r["src"] for r in i["regions"] if not os.path.isfile(r["src"])]
        i["regions"] = [r for r in i["regions"] if os.path.isfile(r["src"])]
        print(f"{i['id']:14s} {len(i['regions']):3d} zones" + (f"  ({len(missing)} fichiers manquants)" if missing else ""))
    return {"instruments": [i for i in insts if i["regions"]]}


CREDITS = """Delusion - banque de sons
VSCO 2 Community Edition & VCSL - Versilian Studios LLC - CC0 1.0 (domaine public)
Salamander Grand Piano V3 - Alexander Holm - CC-BY 3.0 - https://creativecommons.org/licenses/by/3.0/
Tone.js instruments (guitares) - Nicholas Brosowsky ; sources : University of Iowa EMS, Freesound (quartertone) - CC-BY 3.0
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--prep", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--work", default="sample_sources")
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    fetch(a.work)
    job = build_job(a.work)
    jp = os.path.join(a.work, "job.json")
    json.dump(job, open(jp, "w"), indent=1)
    os.makedirs(a.out, exist_ok=True)
    run([os.path.abspath(a.prep), os.path.abspath(jp), os.path.abspath(a.out)])
    open(os.path.join(a.out, "CREDITS.txt"), "w").write(CREDITS)


if __name__ == "__main__":
    main()
