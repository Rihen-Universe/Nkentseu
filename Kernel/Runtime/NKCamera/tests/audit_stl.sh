#!/usr/bin/env bash
# audit_stl.sh — Combien de STL reste-t-il, et combien de bibliothèque C ?
#
# ⚠️ POURQUOI CE SCRIPT REMPLACE `grep -c "std::"` :
#   Ce chantier vise **zéro STL**, PAS **zéro `std::`**. Les deux ne sont pas la
#   même chose, et les confondre fait lire « chantier inachevé » sur un chantier
#   fini.
#
#   `std::snprintf`, `std::time`, `std::fwrite`, `std::size_t` sont des fonctions
#   et des types de la bibliothèque standard **C**, simplement exposés dans
#   l'espace de noms `std` par les en-têtes `<cXXX>`. Le dépôt les accepte
#   explicitement — `NkChrono.cpp` revendique en tête de fichier « aucune
#   dépendance STL : uniquement <cstdio> pour snprintf ».
#
#   Ce que le projet refuse, c'est ce qui **alloue et gère des durées de vie à
#   votre place** : conteneurs, chaînes, algorithmes, fils, verrous,
#   `std::function`, pointeurs intelligents.
#
# Usage : bash Kernel/Runtime/NKCamera/tests/audit_stl.sh
# =============================================================================

PORTEE="Kernel/Runtime/NKCamera/src"

# ── Les deux populations, énumérées en clair ────────────────────────────────
# À RETIRER : gestion de durée de vie, conteneurs, synchronisation.
STL="mutex|lock_guard|unique_lock|shared_lock|condition_variable|thread|jthread\
|string|wstring|string_view|vector|queue|deque|list|map|unordered_map|set|array\
|function|bind|move|forward|make_unique|make_shared|unique_ptr|shared_ptr|weak_ptr\
|atomic|sort|copy|find_if|accumulate|transform|optional|variant|tuple|pair\n|filesystem|ifstream|ofstream|fstream|stringstream|error_code|error_condition"

# TOLÉRÉES : bibliothèque C, exposée dans `std` par les en-têtes <cXXX>.
LIBC="snprintf|sprintf|printf|fprintf|fopen|fclose|fwrite|fread|fflush\
|time|time_t|clock|clock_t|strftime|localtime|gmtime|mktime\
|size_t|ptrdiff_t|memcpy|memmove|memset|memcmp|strlen|strcmp|strncmp|strcpy\
|abs|fabs|floor|ceil|round|sqrt|pow|min|max|malloc|free|qsort|isnan|isinf\n|ferror|feof|fgets|fputs|fseek|ftell|signal|strstr|strchr|tolower|toupper|system|atoi|strtol"

echo "PERIMETRE : $PORTEE   —   $(git rev-parse --short HEAD), $(date +%Y-%m-%d)"
echo "CIBLE     : ZERO STL. Pas zero 'std::' — la bibliotheque C reste autorisee."
echo

printf "%-42s %6s %6s\n" "fichier" "STL" "libC"
printf "%-42s %6s %6s\n" "------------------------------------------" "------" "------"
ttlStl=0
ttlC=0
for f in $(grep -rl "std::" $PORTEE 2>/dev/null | sort); do
	# Les COMMENTAIRES ne comptent pas. Un fichier converti qui explique ce
	# qu'il a remplacé cite forcément le nom de la construction retirée, et
	# paraîtrait donc non converti : l'instrument mesurerait sa propre
	# documentation. Constaté sur le backend Win32, à 0 STL mais compté 1.
	corps=$(sed -E 's://.*::' "$f" | grep -vE '^[[:space:]]*\*')
	nStl=$(printf '%s\n' "$corps" | grep -oE "std::($STL)\b" | wc -l)
	nC=$(printf '%s\n' "$corps" | grep -oE "std::($LIBC)\b" | wc -l)
	ttlStl=$((ttlStl + nStl))
	ttlC=$((ttlC + nC))
	printf "%-42s %6d %6d\n" "${f#$PORTEE/NKCamera/}" "$nStl" "$nC"
done
printf "%-42s %6s %6s\n" "------------------------------------------" "------" "------"
printf "%-42s %6d %6d\n" "TOTAL" "$ttlStl" "$ttlC"
echo
echo "Non classe (ni STL ni libC connue) — a examiner, la liste ci-dessus est"
echo "peut-etre incomplete :"
grep -rhoE "std::[a-z_]+" $PORTEE 2>/dev/null | sort -u \
	| grep -vE "^std::($STL)$" | grep -vE "^std::($LIBC)$" | sed 's/^/  /'

if [ "$ttlStl" -gt 0 ]; then
	echo
	echo "ECHEC : $ttlStl construction(s) STL restante(s)."
	exit 1
fi
echo
echo "OK : zero STL dans $PORTEE."
exit 0
