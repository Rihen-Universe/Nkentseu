# ============================================================================
# Extraction MECANIQUE des tables de donnees normatives de 3GPP TS 26.073
# (livraison ANSI-C officielle du standard, fichiers *.tab UNIQUEMENT).
# Le script recupere, par regex, les initialisateurs de tableaux constants :
# nom, dimensions declarees, et la liste des nombres. RIEN d'autre n'est lu :
# aucun fichier .c d'algorithme n'est ouvert, aucune fonction/logique extraite.
# ============================================================================
import re, os, sys, json

SRC = "ref/c-code"
OUT = "tabs_dump.json"

files = sorted(f for f in os.listdir(SRC) if f.endswith(".tab"))
arrays = []
decl_re = re.compile(
    r"(?:static\s+)?(?:const\s+)?(?:Word16|Word32|Float32|int|short|long)\s+"
    r"(?:const\s+)?([A-Za-z_]\w*)\s*((?:\[[^\]]*\])+)\s*=\s*\{",
    re.M)

for fn in files:
    raw = open(os.path.join(SRC, fn), "r", errors="replace").read()
    # retirer les commentaires (pour ne garder que les nombres des donnees)
    txt = re.sub(r"/\*.*?\*/", " ", raw, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    for m in decl_re.finditer(txt):
        name = m.group(1)
        dims = [d.strip() for d in re.findall(r"\[([^\]]*)\]", m.group(2))]
        # trouver l'accolade fermante correspondante
        i = m.end() - 1
        depth = 0
        start = i
        while True:
            c = txt[i]
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        blob = txt[start:i + 1]
        nums = [int(x, 0) for x in re.findall(r"-?(?:0[xX][0-9a-fA-F]+|\d+)", blob)]
        arrays.append({"file": fn, "name": name, "dims": dims, "count": len(nums), "data": nums})

with open(OUT, "w") as f:
    json.dump(arrays, f)

for a in arrays:
    print("%-14s %-24s dims=%-12s n=%d" % (a["file"], a["name"], "x".join(a["dims"]), a["count"]))
print("total arrays:", len(arrays))
