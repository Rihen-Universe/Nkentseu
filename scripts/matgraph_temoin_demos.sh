#!/usr/bin/env bash
# Temoin de NON-REGRESSION des demos materiaux Demo4..Demo8 (indices --demo=4..8, apres le remappage de main.cpp:513-544).
#
# POURQUOI PAS UNE COMPARAISON DE PIXELS : mesure le 2026-08-21, trois executions
# du MEME binaire non modifie rendent trois PNG d'empreintes DIFFERENTES
# (e7a5dab3 / f3790858 / a6d0223c sur Demo4). La scene est animee et le pas de
# temps suit l'horloge murale : le plancher de bruit d'une comparaison d'image
# n'est pas nul, donc l'egalite d'octets ne prouve rien et l'inegalite non plus.
#
# CE QU'ON COMPARE A LA PLACE : la SIGNATURE du journal, normalisee (horodatages,
# nombres flottants et pointeurs retires), triee et dediupliquee. Mesuree stable
# sur deux executions consecutives (65 lignes, seul le nom du fichier de capture
# differe). Elle couvre : gabarits enregistres, pipelines compiles, shaders
# charges, systemes initialises, erreurs.
#
# REGIME COUVERT : backend par defaut (OpenGL), Debug-Windows, 30 frames, capture
# a la frame 20, CACHE DE SHADERS CHAUD (une passe de chauffe precede la mesure).
# NE COUVRE PAS : Vulkan/DX11/DX12, Release, les frames tardives, le cache froid.
#
# Usage : scripts/matgraph_temoin_demos.sh <dossier-de-sortie>
set -u
OUT="${1:-mesures_matgraph/courant}"
EXE="./Build/Bin/Debug-Windows/renderdemo/renderdemo.exe"
mkdir -p "$OUT"
rc_global=0
for d in 4 5 6 7 8; do
    n=$d
    # PASSE DE CHAUFFE, jetee. Mesure le 2026-08-21 : sans elle, Demo8 rendait
    # DEUX signatures differentes d'une execution a l'autre -- la premiere course
    # ecrivait « [ShaderCache] Warning: saving non-SPIRV data », la seconde non,
    # parce que le cache de shaders PERSISTE SUR DISQUE entre les executions.
    # L'ecart n'avait rien a voir avec le code teste : il opposait un cache froid
    # a un cache chaud. On chauffe donc, et le regime devient explicite.
    NK_MAXFRAMES=30 "$EXE" --demo=$d > /dev/null 2>&1
    NK_MAXFRAMES=30 NK_CAPTURE=20 NK_CAPTURE_PATH="$OUT/demo$n.png" \
        "$EXE" --demo=$d > "$OUT/demo$n.log" 2>&1
    rc=$?
    errs=$(grep -cE "\[ERR\]|\[ERROR\]" "$OUT/demo$n.log")
    png=0; [ -s "$OUT/demo$n.png" ] && png=1
    # Signature normalisee : on retire l'horodatage, les flottants, les pointeurs
    # et le nom du fichier de capture (il change a chaque dossier de sortie).
    #
    # EXCLUSION DECLAREE : les lignes « [ShaderCache] ». Elles rapportent l'etat du
    # cache de shaders SUR DISQUE, pas le comportement du moteur -- et cet etat
    # converge sur PLUSIEURS executions, pas une seule. Mesure du 2026-08-21,
    # cache efface : la passe de chauffe ne suffisait pas pour Demo4 (4 lignes
    # « saving non-SPIRV data » restaient a la 2e course, zero a la 3e). Les
    # garder ferait dependre le temoin du nombre de fois qu'on l'a deja lance.
    # Ce que l'exclusion NE cache PAS : un shader qui ne compile plus ressort en
    # [ERR] (compte a part) et par les lignes pipeline/shader qu'on garde.
    grep -oE "\[[A-Za-z_0-9]+\] [A-Za-z].*" "$OUT/demo$n.log" \
        | grep -v "^\[ShaderCache\]" \
        | sed -E 's/[0-9]+\.[0-9]+//g; s/0x[0-9a-f]+//g; s#NK_CAPTURE frame [0-9]+ -> .*#NK_CAPTURE fait#' \
        | sort | uniq -c > "$OUT/demo$n.sig"
    lignes=$(wc -l < "$OUT/demo$n.sig")
    echo "demo$n  exit=$rc  erreurs=$errs  capture=$png  signature=$lignes lignes"
    [ "$rc" -ne 0 ] && rc_global=1
    [ "$errs" -ne 0 ] && rc_global=1
    [ "$png" -ne 1 ] && rc_global=1
done
exit $rc_global
