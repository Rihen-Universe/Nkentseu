#!/usr/bin/env bash
#
# lancer-fenetre.sh — UNE APPLICATION FENETREE DEMARRE-T-ELLE SUR macOS ? (2026-09-30)
# =============================================================================
# POURQUOI IL EXISTE
#
#   Un binaire qui se construit n'est pas un binaire qui tourne. Sur macOS,
#   l'essentiel se joue APRES l'edition de liens : NSApplication, la premiere
#   NSWindow, le calque Metal. Aucune machine de Rihen n'est un Mac ; ce runner
#   l'est, et il a une session graphique.
#
#   Le script lance l'application, la laisse vivre quelques secondes, puis
#   repond a trois questions, dans cet ordre :
#     1. est-elle encore en vie ?          (sinon : code de sortie + rapport
#                                           de plantage macOS en annotation)
#     2. a-t-elle une fenetre a l'ecran ?  (CoreGraphics, par PID)
#     3. a quoi ressemble l'ecran ?        (capture, si l'autorisation existe)
#   puis il l'arrete.
#
# USAGE
#   lancer-fenetre.sh <binaire> <secondes> <dossier-sortie> [arguments...]
#
#   Sortie 0 : l'application tenait encore debout au bout du delai.
#   Sortie 1 : elle est morte avant (plantage, ou sortie en erreur).
#   L'absence de fenetre est un AVERTISSEMENT, pas un echec : le runner peut
#   refuser la lecture de la liste des fenetres sans que l'application y soit
#   pour rien.
# =============================================================================

set -u

BIN="${1:?binaire manquant}"
SECONDES="${2:?delai manquant}"
SORTIE="${3:?dossier de sortie manquant}"
shift 3

NOM=$(basename "$BIN")
ICI=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$SORTIE"
JOURNAL="$SORTIE/$NOM-execution.log"

Echapper() {
    perl -pe 's/%/%25/g; s/\r/%0D/g; s/\n/%0A/g'
}

# ── Le rapport de plantage macOS, en clair ──
# Les .ips sont du JSON (en-tete d'une ligne, puis le corps) : on en tire
# l'exception, le message applicatif (NSException non rattrapee, abort) et la
# pile du fil fautif — c'est tout ce qu'il faut pour trouver la ligne.
RapportDePlantage() {
    python3 - "$NOM" <<'PY'
import glob, json, os, sys
nom = sys.argv[1]
motifs = [os.path.expanduser(f"~/Library/Logs/DiagnosticReports/{nom}*.ips"),
          f"/Library/Logs/DiagnosticReports/{nom}*.ips"]
fichiers = sorted([f for m in motifs for f in glob.glob(m)], key=os.path.getmtime)
if not fichiers:
    print("(aucun rapport de plantage macOS trouve)")
    sys.exit(0)
texte = open(fichiers[-1], encoding="utf-8", errors="replace").read()
_, _, corps = texte.partition("\n")
try:
    d = json.loads(corps)
except Exception as e:
    print(f"rapport illisible ({e}) : {fichiers[-1]}")
    print(texte[:3000])
    sys.exit(0)
print(f"rapport : {os.path.basename(fichiers[-1])}")
print("exception :", json.dumps(d.get("exception", {}), ensure_ascii=False))
if d.get("termination"):
    print("terminaison :", json.dumps(d.get("termination"), ensure_ascii=False)[:600])
asi = d.get("asi")
if asi:
    print("message applicatif :", json.dumps(asi, ensure_ascii=False)[:1500])
images = d.get("usedImages", [])
fil = d.get("faultingThread", 0)
fils = d.get("threads", [])
if fil < len(fils):
    for i, f in enumerate(fils[fil].get("frames", [])[:30]):
        img = "?"
        idx = f.get("imageIndex")
        if idx is not None and idx < len(images):
            img = images[idx].get("name", "?")
        sym = f.get("symbol", "?")
        pos = f.get("symbolLocation", f.get("imageOffset", 0))
        src = ""
        if f.get("sourceFile"):
            src = f"  ({f.get('sourceFile')}:{f.get('sourceLine', '?')})"
        print(f"#{i:02d} {img:<24} {sym} + {pos}{src}")
PY
}

echo "== lancement : $BIN $* (delai ${SECONDES} s)"
"$BIN" "$@" > "$JOURNAL" 2>&1 &
PID=$!

# Attente par tranches d'une seconde : une mort precoce se voit tout de suite,
# et son code de sortie reste recuperable par `wait`.
ECOULE=0
while [ "$ECOULE" -lt "$SECONDES" ]; do
    if ! kill -0 "$PID" 2>/dev/null; then
        break
    fi
    sleep 1
    ECOULE=$((ECOULE + 1))
done

if ! kill -0 "$PID" 2>/dev/null; then
    wait "$PID"
    CODE=$?
    # Le serveur de rapports ecrit le .ips une ou deux secondes apres la mort.
    sleep 3
    RAPPORT=$(RapportDePlantage)
    echo "$RAPPORT" > "$SORTIE/$NOM-plantage.txt"
    echo "== $NOM est MORT apres ${ECOULE} s, code $CODE"
    echo "--- sortie du programme (fin) ---"
    tail -n 60 "$JOURNAL"
    echo "--- rapport de plantage ---"
    echo "$RAPPORT"
    printf '::error title=%s mort apres %s s code %s::%s\n' "$NOM" "$ECOULE" "$CODE" \
        "$( { tail -n 40 "$JOURNAL" | cut -c1-300; echo '--- plantage ---'; echo "$RAPPORT"; } | Echapper)"
    cp -R logs "$SORTIE/" 2>/dev/null || true
    exit 1
fi

echo "== $NOM tient debout apres ${SECONDES} s (pid $PID)"

# ── La fenetre existe-t-elle vraiment ? ──
FENETRES=$(swift "$ICI/fenetres-du-processus.swift" "$PID" 2>&1)
FEN_CODE=$?
echo "$FENETRES" | tee "$SORTIE/$NOM-fenetres.txt"

# ── La capture : meilleur effort ──
if screencapture -x "$SORTIE/$NOM-ecran.png" 2>"$SORTIE/$NOM-capture.err"; then
    echo "capture : $SORTIE/$NOM-ecran.png"
else
    echo "capture refusee : $(cat "$SORTIE/$NOM-capture.err")"
fi

kill -TERM "$PID" 2>/dev/null
sleep 2
kill -KILL "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true

echo "--- sortie du programme (fin) ---"
tail -n 40 "$JOURNAL"
cp -R logs "$SORTIE/" 2>/dev/null || true

if [ "$FEN_CODE" -eq 0 ]; then
    printf '::notice title=%s demarre::%s\n' "$NOM" \
        "$( { echo "vivant apres ${SECONDES} s"; echo "$FENETRES"; tail -n 15 "$JOURNAL" | cut -c1-300; } | Echapper)"
else
    printf '::warning title=%s sans fenetre visible::%s\n' "$NOM" \
        "$( { echo "vivant apres ${SECONDES} s mais aucune fenetre d'application listee"; echo "$FENETRES"; tail -n 15 "$JOURNAL" | cut -c1-300; } | Echapper)"
fi
exit 0
