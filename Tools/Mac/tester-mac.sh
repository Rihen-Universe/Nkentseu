#!/usr/bin/env bash
#
# tester-mac.sh — LE KIT DE TEST MAC DE NKENTSEU (2026-09-30)
# =============================================================================
# POURQUOI IL EXISTE
#
#   Aucune machine de l'equipe n'est un Mac : la CI (un Mac virtuel, sans
#   ecran Retina) prouve que tout se construit et que Metal dessine. Ce qu'elle
#   ne peut pas voir, c'est un VRAI ecran : nettete du texte en Retina, taille
#   des fenetres, passage d'un ecran a l'autre. Ce script fait tout ce qui
#   s'automatise, puis montre chaque programme a la personne qui teste et note
#   ses reponses. Mode d'emploi pas a pas : docs/TEST_MAC_ETUDIANT.md.
#
# USAGE (depuis le dossier du depot Nkentseu)
#   Tools/Mac/tester-mac.sh            construire, capturer, puis regarder
#   Tools/Mac/tester-mac.sh --vite     sans la partie « regarder »
#   Puis : Tools/Mac/rassembler-journaux.sh  (fabrique le zip a renvoyer)
#
#   Tout ce qui est produit va dans test-mac/ (journaux, images, reponses).
# =============================================================================

set -u

RACINE=$(cd "$(dirname "$0")/../.." && pwd)
cd "$RACINE" || exit 1
SORTIE="$RACINE/test-mac"
mkdir -p "$SORTIE/captures" "$SORTIE/journaux"
REPONSES="$SORTIE/reponses.txt"
BILAN="$SORTIE/bilan.txt"
VITE=0
[ "${1:-}" = "--vite" ] && VITE=1

: > "$BILAN"
note() { echo "$*" | tee -a "$BILAN"; }
titre() { echo; echo "=================================================================="; echo "  $*"; echo "=================================================================="; }

# Lance un programme au plus <secondes>, sortie dans <journal>. Rend son code.
lancer_borne() {
    local secondes="$1" journal="$2"
    shift 2
    "$@" > "$journal" 2>&1 &
    local pid=$! t=0
    while [ "$t" -lt "$secondes" ] && kill -0 "$pid" 2>/dev/null; do
        sleep 1
        t=$((t + 1))
    done
    if kill -0 "$pid" 2>/dev/null; then
        kill -TERM "$pid" 2>/dev/null
        sleep 2
        kill -KILL "$pid" 2>/dev/null
        wait "$pid" 2>/dev/null
        return 124
    fi
    wait "$pid"
}

binaire() {
    find Build/Bin -type f -name "$1" -perm -u+x -not -path "*.app/*" 2>/dev/null | head -1
}

# ── 1. Les outils ────────────────────────────────────────────────────────────
titre "1/5  Verifier les outils"
if ! xcode-select -p > /dev/null 2>&1; then
    echo "Les outils de ligne de commande Xcode manquent. Tapez :  xcode-select --install"
    echo "puis relancez ce script une fois l'installation terminee."
    exit 1
fi
{
    echo "date      : $(date)"
    echo "macOS     : $(sw_vers -productVersion) ($(sw_vers -buildVersion))"
    echo "processeur: $(uname -m) -- $(sysctl -n machdep.cpu.brand_string 2>/dev/null)"
    echo "clang     : $(clang --version | head -1)"
    echo "python    : $(python3 --version 2>&1)"
    echo "depot     : $(git rev-parse --abbrev-ref HEAD 2>/dev/null) $(git rev-parse --short HEAD 2>/dev/null)"
} | tee "$SORTIE/machine.txt"
system_profiler SPDisplaysDataType > "$SORTIE/ecrans.txt" 2>&1
grep -E "Resolution|Retina|UI Looks like|Display Type|Main Display|Mirror" "$SORTIE/ecrans.txt" | sed 's/^ */  ecran : /'
if ! python3 -c "import Jenga" > /dev/null 2>&1; then
    echo
    echo "Jenga est introuvable pour python3. Voir docs/TEST_MAC_ETUDIANT.md, etape 2 :"
    echo "  export PYTHONPATH=\"\$HOME/nk/jenga\""
    exit 1
fi
python3 -c "import Jenga; print('jenga     :', getattr(Jenga, '__version__', '?'), Jenga.__file__)" | tee -a "$SORTIE/machine.txt"

# ── 2. Construire ────────────────────────────────────────────────────────────
titre "2/5  Construire (la premiere fois : 10 a 30 minutes)"
CIBLES="Tuto01Fenetre Tuto02Renderer Tuto03Scene Tuto04Camera Tuto05Meshes NkDames"
for C in $CIBLES; do
    printf '  %-16s ' "$C"
    if python3 -m Jenga build --target "$C" > "$SORTIE/journaux/build-$C.log" 2>&1 \
        && ! grep -q -E 'BUILD FAILED|\] Build failed\.' "$SORTIE/journaux/build-$C.log" \
        && [ -n "$(binaire "$C")" ]; then
        echo "construit"
        note "construction $C : OK"
    else
        echo "ECHEC (voir test-mac/journaux/build-$C.log)"
        note "construction $C : ECHEC"
    fi
done

# ── 3. Captures automatiques ─────────────────────────────────────────────────
# Une image rendue hors ecran, ecrite en PNG : la meme que la CI, mais sur CE
# Mac et en Retina. Metal d'abord, rendu logiciel ensuite pour comparer.
titre "3/5  Captures automatiques (une fenetre s'ouvre et se ferme seule)"
if B=$(binaire Tuto03Scene) && [ -n "$B" ]; then
    for API in metal software; do
        NK_GFX_BACKEND=$API NK_CAPTURE=40 NK_CAPTURE_PATH="$SORTIE/captures/tuto03-$API.png" \
            lancer_borne 120 "$SORTIE/journaux/capture-tuto03-$API.log" "$B"
        if [ -s "$SORTIE/captures/tuto03-$API.png" ]; then note "capture Tuto03 $API : OK"; else note "capture Tuto03 $API : ECHEC"; fi
    done
fi
if B=$(binaire NkDames) && [ -n "$B" ]; then
    for API in metal software; do
        lancer_borne 120 "$SORTIE/journaux/capture-nkdames-$API.log" "$B" --backend=$API \
            --capture="$SORTIE/captures/nkdames-$API.png" --capture-frame=40
        if [ -s "$SORTIE/captures/nkdames-$API.png" ]; then note "capture NkDames $API : OK"; else note "capture NkDames $API : ECHEC"; fi
    done
fi
# Tuto03 : compare a l'image DX11 de reference (meme scene, meme PBR) ;
# NkDames : compare a son propre rendu logiciel.
if [ -f .github/scripts/comparer-captures.py ]; then
    if [ -s "$SORTIE/captures/tuto03-metal.png" ]; then
        python3 .github/scripts/comparer-captures.py --image "$SORTIE/captures/tuto03-metal.png" \
            --reference docs/captures-reference/tuto03-dx11-windows.png --regles tuto03 --tolerance 60 \
            --rapport "$SORTIE/captures/tuto03-comparaison.txt" > /dev/null 2>&1
        note "comparaison tuto03 : $(tail -1 "$SORTIE/captures/tuto03-comparaison.txt" 2>/dev/null)"
    fi
    if [ -s "$SORTIE/captures/nkdames-metal.png" ]; then
        python3 .github/scripts/comparer-captures.py --image "$SORTIE/captures/nkdames-metal.png" \
            --reference "$SORTIE/captures/nkdames-software.png" \
            --rapport "$SORTIE/captures/nkdames-comparaison.txt" > /dev/null 2>&1
        note "comparaison nkdames : $(tail -1 "$SORTIE/captures/nkdames-comparaison.txt" 2>/dev/null)"
    fi
fi

# ── 4. Regarder ──────────────────────────────────────────────────────────────
if [ "$VITE" -eq 1 ]; then
    titre "4/5  (--vite : partie « regarder » sautee)"
else
    titre "4/5  A vous de regarder"
    echo "Chaque programme s'ouvre a son tour. Regardez, faites ce qui est demande,"
    echo "puis FERMEZ LA FENETRE (croix rouge, ou touche Echap) pour passer au suivant."
    echo "Apres chaque fenetre, repondez par o (oui) ou n (non), puis un commentaire."
    : > "$REPONSES"
    question() {
        local id="$1" texte="$2" rep com
        printf '\n  %s\n  -> (o/n) ' "$texte"
        read -r rep
        printf '  -> commentaire (Entree si rien) : '
        read -r com
        echo "$id | $rep | $texte | $com" >> "$REPONSES"
    }
    regarder() {
        local cible="$1" consigne="$2"
        shift 2
        local B
        B=$(binaire "$cible")
        if [ -z "$B" ]; then
            echo "  ($cible n'a pas ete construit : saute)"
            echo "$cible | absent" >> "$REPONSES"
            return
        fi
        echo
        echo "  ---- $cible ----"
        echo "$consigne" | sed 's/^/  /'
        printf '  Entree pour ouvrir...'
        read -r _
        "$B" "$@" > "$SORTIE/journaux/regarder-$cible$*.log" 2>&1
        echo "  (code de sortie : $?)" | tee -a "$REPONSES"
    }

    regarder Tuto01Fenetre "Une fenetre grise vide doit s'ouvrir au MILIEU de l'ecran.
Deplacez-la, redimensionnez-la par un coin, puis fermez-la par la croix rouge."
    question T01-taille "La fenetre avait-elle une taille normale (ni minuscule, ni plus grande que l'ecran) ?"
    question T01-croix "La croix rouge l'a-t-elle fermee tout de suite ?"

    regarder Tuto02Renderer "Un panneau de texte en haut a gauche : « Backend : Metal » et un compteur FPS.
Approchez-vous de l'ecran : le texte doit etre NET, pas flou ni baveux.
Redimensionnez la fenetre, puis fermez-la."
    question T02-metal "Le panneau disait-il « Backend : Metal » ?"
    question T02-net "Le texte etait-il net (pas flou) ?"
    question T02-resize "Apres redimensionnement, le panneau restait-il net et a la bonne taille ?"

    regarder Tuto03Scene "Un cube DORE qui tourne, une sphere blanche a sa droite, un sol gris et leurs OMBRES.
Si vous avez DEUX ecrans : glissez la fenetre de l'un a l'autre (Retina <-> non-Retina)."
    question T03-scene "Voyiez-vous le cube dore, la sphere et le sol ?"
    question T03-ombres "Les ombres etaient-elles sous les objets (pas a l'envers, pas absentes) ?"
    question T03-ecrans "Deux ecrans : l'image restait-elle nette et a la bonne taille apres le passage ? (n si un seul ecran : dites-le en commentaire)"

    regarder Tuto04Camera "La meme scene, avec une camera : bouton du MILIEU de la souris (ou deux doigts sur le trackpad) pour tourner,
molette pour zoomer, WASD pour se deplacer."
    question T04-camera "La camera suivait-elle la souris / le trackpad dans le bon sens ?"

    regarder Tuto05Meshes "Des objets que l'on selectionne d'un clic ; E extrude, C subdivise, R remet a zero."
    question T05-clic "Le clic selectionnait-il bien l'objet SOUS le curseur (pas a cote) ?"

    regarder NkDames "Un jeu de dames (NKCanvas, en Metal). Le plateau et le texte doivent etre nets.
Jouez un ou deux coups a la souris, redimensionnez la fenetre, puis fermez-la." --backend=metal
    question D-net "Plateau et textes etaient-ils nets ?"
    question D-clic "Les clics tombaient-ils sur la bonne case ?"
    question D-resize "Apres redimensionnement, tout suivait-il sans flou ni deformation ?"
fi

# ── 5. Fin ───────────────────────────────────────────────────────────────────
titre "5/5  Termine"
cp -R logs "$SORTIE/" 2>/dev/null || true
cat "$BILAN"
echo
echo "Il reste a fabriquer le zip a renvoyer :   Tools/Mac/rassembler-journaux.sh"
