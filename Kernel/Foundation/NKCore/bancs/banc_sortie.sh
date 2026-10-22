#!/usr/bin/env bash
# =============================================================================
# NKCore/bancs/banc_sortie.sh : NkSortie contre la libc, en temps et en octets.
#
#   bash banc_sortie.sh <banc_sortie_libc.exe> <dossier-de-travail> [n]
#
# Pour chaque opération (fputs, puts, printf, fputc, fwrite) et chaque
# destination (fichier, NUL, tube) : la libc, NkSortie, et la contre-épreuve
# NkSortie SANS tampon (nk0). Meilleur de 3 essais (nk0 : 1 essai). Vers un
# fichier, les octets de NkSortie et de la libc doivent être IDENTIQUES.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# DATE : 2026-10-22
# =============================================================================
set -u
EXE="$1"
W="$2"
N="${3:-1000000}"
mkdir -p "$W"
fautes=0

mesurer() { # mode op n destination -> « microsecondes appels »
	local mode="$1" op="$2" n="$3" dest="$4" m="$W/mesure.txt"
	rm -f "$m"
	case "$dest" in
		fichier) "$EXE" "$mode" "$op" "$n" "$m" > "$W/sortie_${mode}_${op}.txt" ;;
		nul) "$EXE" "$mode" "$op" "$n" "$m" > /dev/null ;;
		tube) "$EXE" "$mode" "$op" "$n" "$m" | cat > /dev/null ;;
	esac
	# Pas de mesure = le programme n'a pas tourné (DLL manquante, plantage) :
	# on s'arrête, au lieu de comparer deux fichiers vides et de dire « identiques ».
	if [ ! -s "$m" ]; then
		echo "ARRET : aucune mesure pour $mode $op $dest (le banc n'a pas tourné)" >&2
		exit 90
	fi
	cat "$m"
}

meilleur() { # mode op n destination essais -> « microsecondes appels » du plus rapide
	local best="" ligne
	for _ in $(seq 1 "$5"); do
		ligne=$(mesurer "$1" "$2" "$3" "$4")
		if [ -z "$best" ] || [ "${ligne%% *}" -lt "${best%% *}" ]; then best="$ligne"; fi
	done
	echo "$best"
}

printf '%-7s %-8s %12s %12s %10s %12s %10s %s\n' operation dest "libc(us)" "nk(us)" "nk/libc" "nk0(us)" "appels nk" octets
for op in fputs puts printf fputc fwrite; do
	n="$N"
	[ "$op" = fputc ] && n=$((N * 4))
	for dest in fichier nul tube; do
		l=$(meilleur libc "$op" "$n" "$dest" 3)
		k=$(meilleur nk "$op" "$n" "$dest" 3)
		z=$(meilleur nk0 "$op" "$n" "$dest" 1)
		octets="-"
		if [ "$dest" = fichier ]; then
			if [ -s "$W/sortie_libc_${op}.txt" ] && cmp -s "$W/sortie_libc_${op}.txt" "$W/sortie_nk_${op}.txt" &&
				cmp -s "$W/sortie_libc_${op}.txt" "$W/sortie_nk0_${op}.txt"; then
				octets="IDENTIQUES ($(stat -c %s "$W/sortie_nk_${op}.txt"))"
			else
				octets="DIFFERENTS"
				fautes=$((fautes + 1))
			fi
		fi
		rapport=$(awk -v a="${k%% *}" -v b="${l%% *}" 'BEGIN { printf "%.2f", a / b }')
		printf '%-7s %-8s %12s %12s %10s %12s %10s %s\n' "$op" "$dest" "${l%% *}" "${k%% *}" "$rapport" "${z%% *}" "${k##* }" "$octets"
	done
done
echo "fautes d'octets : $fautes"
exit "$fautes"
