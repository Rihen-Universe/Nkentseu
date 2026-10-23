#!/usr/bin/env bash
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# banc_h264_vitesse.sh : L'ENCODEUR H.264 TIENT LE TEMPS REEL SUR UNE IMAGE QUI CHANGE EN ENTIER (23/10)
# =============================================================================
# Le coordinateur, avant les cameras du direct : « Une camera change TOUTE l'image a chaque trame. [...]
# fais-lui tenir le temps reel, sans perdre en qualite de facon mesurable. Prouve-le avec un banc et sa
# contre-epreuve. »
#
# NKMediaTest --mesure-h264 (src/NkMesureH264.h) encode 45 images 1280x720 a 15 i/s d'une source de
# synthese ou AUCUN macrobloc ne se repete (defilement, disque, scintillement, bruit de capteur) : une fois
# comme le direct (recherche rapide, tranches en parallele), une fois comme AVANT le 23/10 (recherche
# exhaustive +-16, une seule tranche : la reference de qualite). Le juge de la qualite n'est pas le notre :
# ffmpeg decode les deux flux et mesure le PSNR contre la source.
#   (t1) le direct tient le temps reel avec de la marge : moyenne <= 40 ms par image (60 % de 66,7 ms) ;
#   (t2) et sans a-coup : 95e centile <= 66,7 ms ;
#   (c1) ffmpeg relit les 45 images du direct, IDENTIQUES au bit pres a la reconstruction de l'encodeur ;
#   (q1) PSNR (RGB, ffmpeg) du direct >= celui de la reference - 0,05 dB (moyenne), - 0,25 dB (pire image) ;
#   (q2) taille du direct <= 1,02 x celle de la reference.
#
#   ./Applications/NKMediaTest/tools/banc_h264_vitesse.sh                 (VERT attendu : 5 temoins)
#   ./Applications/NKMediaTest/tools/banc_h264_vitesse.sh --contre-essai  chaque mutation ROUGIT :
#     h264-une-tranche           (pas de parallele)                         -> t1, t2
#     h264-recherche-exhaustive  (la recherche d'avant, en tranches)        -> q2 (et t1)
#     h264-recherche-zero        (le vecteur nul seul)                      -> q2
#     h264-plans-arrondi         (le plan demi-pel « j » arrondi de travers) -> c1
# Le temps depend de la charge de la machine : le banc dit la charge processeur avant de mesurer.
# CODES : 0 vert (ou contre-essai reussi), 1 rouge, 2 instrument casse.
# =============================================================================
set -u
RACINE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
EXE="${NK_BANC_EXE:-$RACINE/Build/Bin/Release-Windows/NKMediaTest/NKMediaTest.exe}"
FF="${NK_FFMPEG:-C:/Program Files/Softdeluxe/Free Download Manager/ffmpeg.exe}"
[ -x "$EXE" ] || { echo "[banc-h264] ECHEC D'INSTRUMENT : $EXE absent (construire NKMediaTest Release)."; exit 2; }
[ -x "$FF" ] || { echo "[banc-h264] ECHEC D'INSTRUMENT : ffmpeg absent ($FF ; NK_FFMPEG=<chemin>)."; exit 2; }
TMP="$(mktemp -d)" || { echo "[banc-h264] ECHEC D'INSTRUMENT : mktemp"; exit 2; }
sortie_banc() {
	if [ "${NK_GARDER_BANC:-}" = 1 ]; then echo "[banc-h264] dossier jetable garde : $TMP"; else rm -rf "$TMP"; fi
}
trap sortie_banc EXIT
MUTATIONS="${NK_BANC_MUTATIONS:-h264-une-tranche h264-recherche-exhaustive h264-recherche-zero h264-plans-arrondi}"

dire() { # dire <temoin> <0|1>
	if [ "$2" = 1 ]; then echo "[banc-h264] $1  OK"; else echo "[banc-h264] $1  ECHEC"; fi
}
psnr() { # psnr <source.rgba> <flux.h264> -> « moyenne pire »
	"$FF" -hide_banner -f rawvideo -pix_fmt rgba -s 1280x720 -r 15 -i "$1" -r 15 -i "$2" \
		-lavfi "[1:v]format=gbrp[d];[0:v]format=gbrp[s];[d][s]psnr" -f null - 2>&1 |
		sed -n 's/.*average:\([0-9.]*\) min:\([0-9.]*\).*/\1 \2/p' | tail -1
}
vrai() { awk "BEGIN { exit !($1) }"; } # vrai "<expression awk>"

lancer() { # lancer <mutation>
	local m="$1" D="$TMP/$1" s moy p95 oR oE pR pE decN n=0 ok
	mkdir -p "$D"
	command -v powershell.exe > /dev/null 2>&1 &&
		echo "[banc-h264] charge processeur avant la mesure : $(powershell.exe -NoProfile -Command \
			"[int](Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average" 2>/dev/null | tr -d '\r') %"
	s="$(cd "$RACINE" && NK_NKMEDIA_MUTATION="$m" timeout 900 "$EXE" --mesure-h264 "$(cygpath -m "$D")" 45 26 1 2>&1)"
	echo "$s" | grep -a "\[mesure-h264\]" | cut -c1-230
	moy=$(echo "$s" | sed -n 's/.*direct \([0-9.]*\) ms en moyenne.*/\1/p')
	p95=$(echo "$s" | sed -n 's/.*en moyenne ([0-9]* %), 95e centile \([0-9.]*\) ms.*/\1/p')
	oR=$(echo "$s" | sed -n 's/.*taille : direct \([0-9]*\) octets, exhaustive.*/\1/p')
	oE=$(echo "$s" | sed -n 's/.*exhaustive \([0-9]*\) octets (rapport.*/\1/p')
	if [ -z "$moy" ] || [ -z "$p95" ] || [ -z "$oR" ] || [ -z "$oE" ]; then
		echo "[banc-h264] ECHEC D'INSTRUMENT : la mesure n'a pas rendu ses nombres"; echo "[banc-h264] VERDICT : ROUGE"; return
	fi
	ok=0; vrai "$moy <= 40.0" && ok=1; dire "(t1) temps reel avec marge : $moy ms par image en moyenne <= 40 ms" $ok; n=$((n + ok))
	ok=0; vrai "$p95 <= 66.7" && ok=1; dire "(t2) sans a-coup : 95e centile $p95 ms <= 66,7 ms" $ok; n=$((n + ok))
	"$FF" -v error -i "$D/rapide.h264" -f rawvideo -pix_fmt yuv420p -y "$D/dec.yuv" > /dev/null 2>&1
	decN=$(stat -c %s "$D/dec.yuv" 2>/dev/null || echo 0)
	ok=0; [ "$decN" = $((45 * 1382400)) ] && cmp -s "$D/dec.yuv" "$D/rapide_recon.yuv" && ok=1
	dire "(c1) ffmpeg relit $((decN / 1382400)) image(s), identiques au bit pres a la reconstruction de l'encodeur" $ok; n=$((n + ok))
	pR=$(psnr "$D/source.rgba" "$D/rapide.h264"); pE=$(psnr "$D/source.rgba" "$D/exhaustive.h264")
	echo "[banc-h264] PSNR RGB (ffmpeg), moyenne et pire image : direct $pR ; reference $pE"
	ok=0
	if [ -n "$pR" ] && [ -n "$pE" ]; then
		set -- $pR $pE
		vrai "$1 >= $3 - 0.05 && $2 >= $4 - 0.25" && ok=1
	fi
	dire "(q1) pas de perte de qualite mesurable : PSNR >= reference - 0,05 dB (pire image - 0,25 dB)" $ok; n=$((n + ok))
	ok=0; vrai "$oR <= $oE * 1.02" && ok=1
	dire "(q2) pas de flux plus lourd : $oR octets <= 1,02 x $oE" $ok; n=$((n + ok))
	if [ $n = 5 ]; then echo "[banc-h264] $n temoin(s) sur 5 -- VERDICT : VERT"; else echo "[banc-h264] $n temoin(s) sur 5 -- VERDICT : ROUGE"; fi
}

if [ "${1:-}" = "--contre-essai" ]; then
	rc=0
	for m in $MUTATIONS; do
		echo "=== contre-essai : mutation « $m » (le banc DOIT rougir) ==="
		r="$(lancer "$m")"
		echo "$r" | grep -a "ECHEC\|VERDICT"
		if echo "$r" | grep -q "VERDICT : ROUGE"; then echo "[banc-h264] contre-essai « $m » : a rougi -- REUSSI"
		else echo "[banc-h264] contre-essai « $m » : N'A PAS ROUGI -- ECHOUE"; rc=1; fi
	done
	exit $rc
fi
r="$(lancer aucune)"
echo "$r"
echo "$r" | grep -q "VERDICT : VERT" && exit 0
echo "$r" | grep -q "ECHEC D'INSTRUMENT" && exit 2
exit 1
