# -*- coding: utf-8 -*-
"""Harnais de MUTATION de la VALIDATION `.nkgui` (cote NKUIDesign).

Le harnais de la couche (`Sandbox/System/NKArchive/mutations.py`) ne rejoue que
`SandboxNKArchive` : tout ce qui vit dans `NkGuiValidate.h` lui echappe. Or c'est
precisement la qu'ont atterri, lors de la bascule du 2026-08-22, les quatre refus
que le lecteur syntaxique ne fait plus. **Un mecanisme qui change de domicile
doit changer de harnais avec lui**, sinon il devient le seul du systeme que
personne ne casse jamais pour voir.

Chaque mutation : on patche, ON RECONSTRUIT, on execute `--roundtrip-controles`,
on releve, on restaure, ON RECONSTRUIT.

Releve du 2026-08-23 : 4 mutations posees, 3 tuees, 1 SURVIVANTE -- et la
survivante a fait retirer un garde mort au lieu d'ajouter un controle.
"""
import io, os, re, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
V = os.path.join(ROOT, r"Applications\NKUIDesign\src\NKUIDesign\NkGuiValidate.h")
EXE = os.path.join(ROOT, r"Build\Bin\Debug-Windows\NKUIDesign\NKUIDesign.exe")
REP = os.path.join(ROOT, "nkuidesign_roundtrip_controles.txt")

MUTATIONS = [
    ("V1 la valeur mal formee n'est plus jugee AVANT le schema",
     "\t\t\t\t\tif (NkGuiArchive::KindOf(ents[p].node) == NkGuiValueKind::Invalid) {",
     "\t\t\t\t\tif (false) {"),

    ("V2 toute section est reputee connue",
     "\t\t\t\tif (nom.Compare(kSections[i]) == 0) {",
     "\t\t\t\tif (true) {"),

    ("V3 la regle (d) ne protege plus une mineure plus recente",
     "\t\t\t\t\tif (!NkGSectionConnue(nom) && !plusRecent) {",
     "\t\t\t\t\tif (!NkGSectionConnue(nom)) {"),

    # V4 -- « une valeur mal formee satisfait n'importe quelle lettre » -- A SURVECU
    # le 2026-08-23, et le code qu'elle visait a ete RETIRE : voir la note sur
    # NkGValueMatches. Elle n'a plus d'ancre parce qu'elle n'a plus de cible.
]


def build():
    r = subprocess.run(["jenga", "build", "--project", "NKUIDesign", "--config", "Debug"],
                       cwd=ROOT, capture_output=True, text=True, shell=True, errors="replace")
    return (r.stdout or "") + (r.stderr or "")


def run():
    subprocess.run([EXE, "--roundtrip-controles"], cwd=ROOT, capture_output=True,
                   text=True, errors="replace")
    txt = io.open(REP, encoding="utf-8", errors="replace").read()
    m = re.search(r"CONTROLES : (\d+) / (\d+)", txt)
    if not m:
        return None, None, txt
    return int(m.group(1)), int(m.group(2)), txt


def main():
    orig = io.open(V, encoding="utf-8").read()
    build()
    base, tot, txt = run()
    print("REFERENCE : %s / %s" % (base, tot))
    print("=" * 80)
    survivants = []
    for name, old, new in MUTATIONS:
        if old not in orig:
            print("%-64s  ANCRE INTROUVABLE" % name)
            continue
        io.open(V, "w", encoding="utf-8").write(orig.replace(old, new, 1))
        log = build()
        if "error:" in log:
            io.open(V, "w", encoding="utf-8").write(orig)
            print("%-64s  NE COMPILE PAS" % name)
            continue
        p, t, txt = run()
        io.open(V, "w", encoding="utf-8").write(orig)
        if p is None:
            print("%-64s  PLANTE" % name)
            survivants.append(name)
            continue
        tue = p < base
        if not tue:
            survivants.append(name)
        print("%-64s  %2d/%2d  %s" % (name, p, t, "TUEE" if tue else ">>> SURVIT <<<"))
        for l in txt.splitlines():
            if l.strip().startswith("[NON]"):
                print("        %s" % l.strip()[:120])
    io.open(V, "w", encoding="utf-8").write(orig)
    build()
    p, t, txt = run()
    print("=" * 80)
    print("APRES RESTAURATION : %s / %s" % (p, t))
    print("SURVIVANTES : %s" % (", ".join(survivants) if survivants else "aucune"))


main()
