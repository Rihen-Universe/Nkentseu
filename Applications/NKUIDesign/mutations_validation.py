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
     "\t\t\t\tif (NkGuiArchive::KindOf(ents[p].node) == NkGuiValueKind::Invalid) {",
     "\t\t\t\tif (false) {"),

    ("V2 toute section est reputee connue",
     "\t\t\t\tif (nom.Compare(kSections[i]) == 0) {",
     "\t\t\t\tif (true) {"),

    ("V3 la regle (d) ne protege plus une mineure plus recente",
     "\t\t\t\t\tif (!NkGSectionConnue(nom) && !plusRecent) {",
     "\t\t\t\t\tif (!NkGSectionConnue(nom)) {"),

    # V4 -- « une valeur mal formee satisfait n'importe quelle lettre » -- A SURVECU
    # le 2026-08-23, et le code qu'elle visait a ete RETIRE : voir la note sur
    # NkGValueMatches. Elle n'a plus d'ancre parce qu'elle n'a plus de cible.

    # ⚠️ L ANCRE DE V5 ETAIT FAUSSE AU PREMIER JET : six tabulations au lieu de
    #    sept. Le harnais a repondu « ANCRE INTROUVABLE », et c est la seule raison
    #    pour laquelle je l ai vu. Une mutation dont l ancre ne mord pas ne mesure
    #    rien et ne se plaint pas d elle-meme : le message « introuvable » est
    #    aussi important que le message « survit ».
    ("V5 le diagnostic d un BLOC perd sa ligne",
     "\t\t\t\t\t\t\tNkGValidateNode(*racines->array[k].object, NkString(\"widgets\"),\n\t\t\t\t\t\t\t\t\t\t\tracines->array[k].SourceLine(), out);",
     "\t\t\t\t\t\t\tNkGValidateNode(*racines->array[k].object, NkString(\"widgets\"), 0, out);"),

    ("V6 le diagnostic d une PROPRIETE perd sa ligne",
     "\t\t\t\t\tNkGPushDiag(out, k == NkGuiValueKind::Invalid ? \"E-VALEUR\" : \"E-TYPE\", m,\n\t\t\t\t\t\t\t\tents[p].node.SourceLine());",
     "\t\t\t\t\tNkGPushDiag(out, k == NkGuiValueKind::Invalid ? \"E-VALEUR\" : \"E-TYPE\", m, 0);"),
    # -------------------------------------------------------------------------
    # V7..V12 -- LE VOCABULAIRE D'APPARENCE (doc 9 §3), pose le 2026-08-23.
    #
    # ⚠️ ELLES EXISTENT PARCE QUE LE CORRECTIF EST PLUS DANGEREUX QUE LE DEFAUT.
    #    Les trois faux positifs disparaissaient tout aussi bien si
    #    `NkGValidateApparence` ne faisait RIEN : le controle 23a serait vert, et
    #    la validation aurait silencieusement cesse de juger tout un pan du
    #    format. C'est la forme exacte du 2026-08-22 -- une capacite de refuser
    #    qui s'en va sans laisser de trace. Ces six mutations demandent au
    #    harnais de le prouver.
    ("V7 `appearance` redevient un role de widget",
     "\t\t\treturn nom.Compare(\"appearance\") == 0;",
     "\t\t\treturn false;"),

    ("V8 les proprietes d un bloc `appearance` ne sont plus jugees",
     "\t\t\tNkGValidateProps(bloc, chemin, ap, an, nullptr, 0, NkString(\"appearance\"), out);",
     "\t\t\t(void)ap; (void)an;"),

    ("V9 les proprietes d un EFFET ne sont plus jugees",
     "\t\t\t\tNkGValidateProps(eff, cheminEff, def->props, def->count, nullptr, 0, nom, out);",
     "\t\t\t\t(void)def;"),

    ("V10 un effet hors de la liste fermee n est plus refuse",
     "\t\t\t\t\tNkGPushDiag(out, \"E-EFFET-INCONNU\", m, corps->array[c].SourceLine());",
     "\t\t\t\t\t(void)m;"),

    ("V11 un bloc DANS un effet n est plus refuse",
     "\t\t\t\t\tNkGPushDiag(out, \"E-EFFET-INCONNU\", m, dedans->array[k].SourceLine());",
     "\t\t\t\t\t(void)m;"),

    # ⚠️ CELLE-CI NE CASSE RIEN DE VISIBLE : elle RELACHE. Les capacites
    #    transversales du widget (`tooltip`, `enabled`) se mettent a passer dans
    #    un `fill`. Aucun fichier legal ne cesse d'etre accepte, aucun message ne
    #    change -- seul le controle 23d peut la voir. Un contournement de schema
    #    ne produit aucune sortie : c'est la mutation la plus proche de ce qui
    #    arrive vraiment quand on elargit une table « parce qu'un fichier reel ne
    #    passait pas ».
    ("V12 les capacites transversales du widget fuient dans l apparence",
     "\t\t\t\tNkGValidateProps(eff, cheminEff, def->props, def->count, nullptr, 0, nom, out);",
     "\t\t\t\tuint32 fuite = 0;\n"
     "\t\t\t\tconst NkGSchemaProp *fuites = NkGUniversalProps(fuite);\n"
     "\t\t\t\tNkGValidateProps(eff, cheminEff, def->props, def->count, fuites, fuite, nom, out);"),
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


# ⚠️ LE GARDE `__main__`, ET IL A FALLU QUE JE ME FASSE PRENDRE. Ce fichier
#    appelait `main()` a nu : un simple `import mutations` -- que j avais tape
#    pour VERIFIER les ancres sans rien lancer -- a demarre trente minutes de
#    mutation des sources. Rien n a ete perdu parce que le point de reprise
#    etait commite, mais un fichier qui MODIFIE des sources ne doit jamais
#    partir sur un import.
if __name__ == "__main__":
    main()
