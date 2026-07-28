# ============================================================================
# Extraction MECANIQUE des tables d'ordonnancement des bits (Annexe B) de
# 3GPP TS 26.101 (26101-i00.docx, livraison officielle du standard).
# Parse XML (ElementTree) du .docx : repere les legendes "Table B.x" et
# recupere UNIQUEMENT les nombres des tableaux, ligne par ligne, de gauche a
# droite (ordre de lecture normatif, cf. TS 26.101 4.2.1).
# AUCUNE logique/algorithme n'est extrait : donnees numeriques seulement.
# ============================================================================
import zipfile, re, io
import xml.etree.ElementTree as ET

NS = {"w": "http://schemas.openxmlformats.org/wordprocessingml/2006/main"}
W = NS["w"]

z = zipfile.ZipFile("spec101/26101-i00.docx")
root = ET.fromstring(z.read("word/document.xml"))
body = root.find("w:body", NS)

def text_of(el):
    return "".join(t.text or "" for t in el.iter("{%s}t" % W))

tokens = []
for child in body:
    tag = child.tag.split("}")[1]
    if tag in ("p", "tbl"):
        tokens.append((tag, child))

tables = {}
caption_re = re.compile(r"Table\s*B\.(\d)")
for idx, (kind, el) in enumerate(tokens):
    if kind != "p":
        continue
    m = caption_re.search(text_of(el))
    if not m:
        continue
    n = int(m.group(1))
    if n in tables:
        continue
    tbl = None
    for j in range(idx + 1, min(idx + 4, len(tokens))):
        if tokens[j][0] == "tbl":
            tbl = tokens[j][1]
            break
    if tbl is None:
        for j in range(idx - 1, max(idx - 4, -1), -1):
            if tokens[j][0] == "tbl":
                tbl = tokens[j][1]
                break
    if tbl is None:
        print("!! pas de tableau pour Table B.%d" % n)
        continue
    nums = []
    for row in tbl.findall("w:tr", NS):
        # ligne d'en-tete "j=0 j=1 ..." : pas des donnees
        if "j=" in text_of(row):
            continue
        for cell in row.findall("w:tc", NS):
            for tok in re.findall(r"-?\d+", text_of(cell)):
                nums.append(int(tok))
    tables[n] = nums

K = {1: 95, 2: 103, 3: 118, 4: 134, 5: 148, 6: 159, 7: 204, 8: 244}
ok = True
for n in sorted(tables):
    nums = tables[n]
    print("Table B.%d : %d nombres, min=%d max=%d" % (n, len(nums), min(nums), max(nums)))
    if len(nums) != K.get(n, -1):
        print("  !! attendu %d" % K.get(n, -1))
        ok = False
    elif sorted(nums) != list(range(len(nums))):
        print("  !! pas une permutation 0..K-1")
        ok = False
print("OK" if ok else "ECHEC")

if ok:
    with open("order_tables.txt", "w") as f:
        for n in sorted(tables):
            f.write("B%d\n" % n)
            f.write(",".join(str(x) for x in tables[n]))
            f.write("\n")
    print("-> order_tables.txt")
