#!/usr/bin/env bash
#
# rassembler-journaux.sh — UN ZIP A RENVOYER, AVEC TOUT CE QUI SERT (2026-09-30)
# =============================================================================
# Rassemble, depuis le dossier du depot Nkentseu :
#   - test-mac/        (tester-mac.sh : bilan, reponses, captures PNG, journaux)
#   - logs/            (journaux du moteur, un fichier par lancement)
#   - les rapports de plantage macOS des programmes du depot
#     (~/Library/Logs/DiagnosticReports, dernieres 48 h)
#   - la machine : macOS, processeur, ecrans (Retina ou non), branche et commit
# et fabrique ~/Desktop/nkentseu-test-mac-<date>.zip.
#
# USAGE (depuis le dossier du depot Nkentseu)
#   Tools/Mac/rassembler-journaux.sh
# =============================================================================

set -u

RACINE=$(cd "$(dirname "$0")/../.." && pwd)
cd "$RACINE" || exit 1
QUAND=$(date +%Y-%m-%d_%H%M)
TMP=$(mktemp -d)
DOSSIER="$TMP/nkentseu-test-mac-$QUAND"
mkdir -p "$DOSSIER/plantages"

cp -R test-mac "$DOSSIER/" 2>/dev/null || echo "(pas de test-mac/ : tester-mac.sh n'a pas encore tourne)"
cp -R logs "$DOSSIER/" 2>/dev/null || true

{
    echo "date    : $(date)"
    echo "macOS   : $(sw_vers -productVersion) ($(sw_vers -buildVersion))"
    echo "machine : $(uname -m) -- $(sysctl -n machdep.cpu.brand_string 2>/dev/null) -- $(sysctl -n hw.model 2>/dev/null)"
    echo "depot   : $(git rev-parse --abbrev-ref HEAD 2>/dev/null) $(git rev-parse HEAD 2>/dev/null)"
    echo "modifs  :"
    git status --short 2>/dev/null | head -n 30
    echo "python  : $(python3 --version 2>&1)"
    python3 -c "import Jenga; print('jenga   :', getattr(Jenga, '__version__', '?'), Jenga.__file__)" 2>&1
} > "$DOSSIER/machine.txt"
system_profiler SPDisplaysDataType > "$DOSSIER/ecrans.txt" 2>&1

# Rapports de plantage des programmes du depot (noms des binaires sous Build/Bin).
NOMS=$(find Build/Bin -type f -perm -u+x -not -path "*.app/*" 2>/dev/null | xargs -n1 basename 2>/dev/null | sort -u)
for N in $NOMS; do
    find "$HOME/Library/Logs/DiagnosticReports" -maxdepth 1 -name "$N*" -mtime -2 -exec cp {} "$DOSSIER/plantages/" \; 2>/dev/null
done

ZIP="$HOME/Desktop/nkentseu-test-mac-$QUAND.zip"
(cd "$TMP" && zip -qr "$ZIP" "nkentseu-test-mac-$QUAND")
rm -rf "$TMP"
echo "Fichier a renvoyer :"
echo "  $ZIP"
du -h "$ZIP" | cut -f1 | sed 's/^/  taille : /'
