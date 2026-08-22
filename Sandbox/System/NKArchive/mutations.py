# -*- coding: utf-8 -*-
"""Harnais de MUTATION de la couche `.nkgui` <-> NkArchive.

Un controle vert ne prouve rien tant qu'on n'a pas montre ce qu'il tue. Ce
harnais casse le code EXPRES, une chose a la fois, et verifie que le banc
`SandboxNKArchive` le voit. Une mutation qui SURVIT est le vrai resultat : elle
dit qu'un mecanisme n'est pas mesure -- ou, deux fois ici, qu'il ne servait a
rien.

Chaque mutation : on patche la source, ON RECONSTRUIT, on execute, on releve, on
restaure, ON RECONSTRUIT. La reconstruction avant la mesure n'est PAS
optionnelle : un rapport sur disque survit au binaire qui l'a ecrit, et une
source restauree n'est pas une mesure restauree.

Usage :  python Sandbox/System/NKArchive/mutations.py     (depuis la racine)
Releve du 2026-08-22 : 23 mutations, 23 tuees, 0 survivante, reference 260/260.
"""
import io, os, re, subprocess, sys

ROOT = r"D:\Projets\2026\Nkentseu\Nkentseu-noge"
CPP = os.path.join(ROOT, r"Kernel\System\NKSerialization\src\NKSerialization\NkGui\NkGuiArchive.cpp")
EXE = os.path.join(ROOT, r"Build\Bin\Debug-Windows\SandboxNKArchive\SandboxNKArchive.exe")

MUTATIONS = [
    ("M1  IsReservedKey rend toujours false",
     "return key.Size() > 0 && key.Data() && key.Data()[0] == '$';",
     "return false;"),

    ("M2  l'ecrivain CONCATENE au lieu de fusionner par rang",
     "takeEntry = RankOf(ents[ei].node) <= RankOf(body->array[bi]);",
     "takeEntry = true;"),

    ("M3  la trivia de tete n'est pas reemise",
     "\t\t\t\t\tAppendView(mOut, lead);\n\t\t\t\t\tif (suite) {",
     "\t\t\t\t\tif (suite) {"),

    ("M4  la disposition `$layout` est ignoree",
     "\t\t\t\t\tconst NkString &t = n->value.text;\n\t\t\t\t\tif (t.Compare(\"block\") == 0) {",
     "\t\t\t\t\tconst NkString &t = n->value.text;\n\t\t\t\t\treturn Layout::Defaut;\n\t\t\t\t\tif (t.Compare(\"block\") == 0) {"),

    ("M5  `$` devient un caractere d'identifiant (le canari de T10)",
     "return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';\n\t\t}\n\n\t\tclass Lexer",
     "return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';\n\t\t}\n\n\t\tclass Lexer"),

    ("M6  une chaine sans litteral valable n'est PAS remise entre guillemets",
     "\t\t\t\t\tif (node.value.type == NkArchiveValueType::NK_VALUE_STRING\n\t\t\t\t\t\t&& !node.HasUsableLiteral()) {\n\t\t\t\t\t\tQuoted(node.value.text);\n\t\t\t\t\t\treturn;\n\t\t\t\t\t}",
     "\t\t\t\t\tif (false) {\n\t\t\t\t\t\tQuoted(node.value.text);\n\t\t\t\t\t\treturn;\n\t\t\t\t\t}"),

    ("M7  la tranche brute garde le TEXTE DU JETON, pas la tranche de source",
     "NkArchiveNode node(NkArchiveValue::FromString(NkStringView(Slice(i, e))));",
     "NkArchiveNode node(NkArchiveValue::FromString(NkStringView(T[i].text)));"),

    ("M8  SpanEnd s'arrete au premier jeton",
     "\t\tnk_size SpanEnd(const NkVector<Tok> &toks, nk_size start, bool stopAtMember) {\n\t\t\tnk_int32 depth = 0;",
     "\t\tnk_size SpanEnd(const NkVector<Tok> &toks, nk_size start, bool stopAtMember) {\n\t\t\treturn start;\n\t\t\tnk_int32 depth = 0;"),

    ("M9  la majeure trop recente n'est plus refusee",
     "if (major > kMajor) {",
     "if (major > 99) {"),

    ("M10 la propriete en DOUBLE ecrase la premiere",
     "&& !ar.Has(NkStringView(T[i].text))",
     ""),

    ("M11 aucun litteral sur une CHAINE lue",
     "\t\t\t\t\t\t\tar.SetString(k, NkStringView(t.text));\n\t\t\t\t\t\t\tar.SetLiteral(k, NkStringView(raw));",
     "\t\t\t\t\t\t\tar.SetString(k, NkStringView(t.text));"),

    ("M12 aucun litteral sur un NOMBRE lu",
     "\t\t\t\t\t\t\t\tar.SetFloat64(k, ParseFloat(t.text));\n\t\t\t\t\t\t\t\tar.SetLiteral(k, NkStringView(raw));",
     "\t\t\t\t\t\t\t\tar.SetFloat64(k, ParseFloat(t.text));"),

    ("M13 `$layout` n'est jamais pose par le lecteur",
     "\t\t\t\t\tif (sameLine && !vide) {",
     "\t\t\t\t\tif (false) {"),

    ("M14 la trivia de FIN de ligne n'est pas reemise",
     "\t\t\t\t\t\t\tAppendView(mOut, e.node.TrailingTrivia());\n\t\t\t\t\t\t\tEndLine();\n\t\t\t\t\t\t\tcontinue;",
     "\t\t\t\t\t\t\tEndLine();\n\t\t\t\t\t\t\tcontinue;"),

    ("M15 SetToken ne pose pas le litteral (le jeton nu devient une chaine)",
     "\t\tif (!ar.SetString(key, token)) {\n\t\t\treturn false;\n\t\t}\n\t\treturn ar.SetLiteral(key, token) != 0;",
     "\t\tif (!ar.SetString(key, token)) {\n\t\t\treturn false;\n\t\t}\n\t\treturn true;"),

    ("M16 le SEPARATEUR retenu (la virgule) est ignore",
     "\t\t\t\t\tif (mSepStart != 0 && T[i].begin > mSepStart) {",
     "\t\t\t\t\tif (false) {"),

    ("M17 SpanEnd ne s'arrete plus a la virgule",
     "\t\t\t\tif (IsPunct(toks[nx], ',')) {\n\t\t\t\t\tbreak;\n\t\t\t\t}",
     ""),

    ("M18 SpanEnd ne s'arrete plus au membre suivant (`Ident =`)",
     "\t\t\t\tif (stopAtMember && StartsMember(toks, nx)) {\n\t\t\t\t\tbreak;\n\t\t\t\t}",
     ""),

    ("M19 un membre ne POURSUIT jamais la ligne du precedent",
     "\t\t\t\t\tconst bool suite = (lead.Size() > 0) && !HasNewLine(lead);",
     "\t\t\t\t\tconst bool suite = false;"),

    ("M20 l'intervalle SANS saut de ligne est jete (ancien comportement)",
     "\t\t\t\t\t\ttoks[i].lead = NkString(g, n);\n\t\t\t\t\t\tcontinue;",
     "\t\t\t\t\t\tcontinue;"),

    ("M21 la FERMETURE ne poursuit jamais la ligne du dernier membre",
     "\t\t\t\t\tconst bool suite = (foot.Size() > 0) && !HasNewLine(foot);",
     "\t\t\t\t\tconst bool suite = false;"),

    ("M22 le premier jeton n'est plus un cas a part dans la trivia",
     "\t\t\t\tconst nk_uint32 leadBegin = premier ? 0u : (firstNL + 1);",
     "\t\t\t\tconst nk_uint32 leadBegin = firstNL + 1;"),

    ("M23 le pied de bloc n'est jamais pose",
     "\t\t\t\t\tif (!sameLine && !T[k].lead.Empty()) {",
     "\t\t\t\t\tif (false) {"),
]


def build():
    r = subprocess.run(["jenga", "build", "--project", "SandboxNKArchive", "--config", "Debug"],
                       cwd=ROOT, capture_output=True, text=True, shell=True, errors="replace")
    out = (r.stdout or "") + (r.stderr or "")
    return out


def run():
    r = subprocess.run([EXE], cwd=ROOT, capture_output=True, text=True, errors="replace")
    out = (r.stdout or "") + (r.stderr or "")
    m = re.search(r"RESULTAT : (\d+) / (\d+)", out)
    if not m:
        return None, None, out
    return int(m.group(1)), int(m.group(2)), out


def failing(out):
    return [l.strip() for l in out.splitlines() if l.strip().startswith("FAIL")]


def main():
    orig = io.open(CPP, encoding="utf-8").read()
    build()
    base, tot, out = run()
    print("REFERENCE : %s / %s" % (base, tot))
    print("=" * 88)
    survivants = []
    for name, old, new in MUTATIONS:
        if old not in orig:
            print("%-72s  ANCRE INTROUVABLE" % name)
            continue
        io.open(CPP, "w", encoding="utf-8").write(orig.replace(old, new, 1))
        blog = build()
        if "error C" in blog or "error LNK" in blog:
            io.open(CPP, "w", encoding="utf-8").write(orig)
            print("%-72s  NE COMPILE PAS" % name)
            continue
        p, t, out = run()
        io.open(CPP, "w", encoding="utf-8").write(orig)
        if p is None:
            print("%-72s  PLANTE" % name)
            survivants.append(name)
            continue
        tue = p < base
        if not tue:
            survivants.append(name)
        print("%-72s  %3d/%3d  %s" % (name, p, t, "TUEE" if tue else ">>> SURVIT <<<"))
        for l in failing(out)[:3]:
            print("        %s" % l[:140])
    io.open(CPP, "w", encoding="utf-8").write(orig)
    build()
    p, t, out = run()
    print("=" * 88)
    print("APRES RESTAURATION : %s / %s" % (p, t))
    print("SURVIVANTES : %s" % (", ".join(survivants) if survivants else "aucune"))


main()
