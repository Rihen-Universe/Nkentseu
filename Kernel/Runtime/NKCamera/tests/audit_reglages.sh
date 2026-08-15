#!/usr/bin/env bash
# =============================================================================
# audit_reglages.sh — Pour chaque champ public de configuration : QUI LE LIT ?
#
# POURQUOI : NKCamera a livré, en deux jours, quatre réglages déclarés mais non
# câblés — `flipHorizontal`, `outputFormat`, `autoExposure`, `autoWhiteBalance`.
# Chacun révélé par un utilisateur qui butait dessus, jamais par le module.
# Ce n'est pas une série d'incidents : c'est que RIEN NE SIGNALE un champ que
# personne ne lit. Ce script est la chose la moins chère qui transforme cet
# oubli silencieux en bruit.
#
# CE QU'IL ATTRAPE : un champ public sans aucun lecteur hors de sa déclaration.
# CE QU'IL N'ATTRAPE PAS, et il faut le savoir avant de s'y fier :
#   - un champ LU mais mal utilisé — la plage de couleur était lue, elle était
#     seulement DÉDUITE du format ; ce script l'aurait déclarée saine ;
#   - un champ honoré sur une plateforme et ignoré sur une autre — `autoFocus`
#     a deux lecteurs (Android) et zéro effet sur Windows ;
#   - un champ lu par un consommateur hors périmètre de recherche.
# Un outil qui annonce ses angles morts se relit ; un outil silencieux se croit.
#
# Usage : bash Kernel/Runtime/NKCamera/tests/audit_reglages.sh
# =============================================================================

DECL="Kernel/Runtime/NKCamera/src/NKCamera/NkCameraTypes.h"
PORTEE="Kernel/ Engine/ Applications/"

# Le périmètre s'IMPRIME avec le résultat, il ne se retient pas de tête. Trois
# fois le 2026-08-15, une recherche exacte a produit une conclusion fausse parce
# que son exclusion n'était écrite nulle part — dont `preset`, donné à 0 lecteur
# alors qu'il est lu par `Resolve()`, dans le fichier même qui l'exclut.
echo "PERIMETRE  : $PORTEE  (extensions .h/.cpp/.mm)"
echo "EXCLUSION  : $DECL — le fichier de declaration lui-meme"
echo "REMARQUE   : les lectures internes a ce fichier (ex. Resolve()) ne sont donc PAS comptees ;"
echo "             un champ a 0 lecteur doit etre verifie A LA MAIN dans ce fichier avant conclusion."
echo

# Champs publics de NkCameraConfig, extraits de la declaration.
CHAMPS=$(sed -n '/struct NkCameraConfig/,/^	};/p' "$DECL" \
	| grep -oE '^\s+[A-Za-z_:<>0-9]+ [a-z][A-Za-z0-9]* =' \
	| awk '{print $(NF-1)}')

# SEPARER LIRE ET ECRIRE, et c'est tout l'interet du script.
# La premiere version comptait les deux ensemble : `outputFormat` sortait a
# « 3 lecteurs, ok » alors que ces trois occurrences sont trois ECRITURES par
# trois applications qui croient demander du RGBA8. Le pire cas du module
# passait donc au vert. Un champ ECRIT mais JAMAIS LU est le mensonge le plus
# couteux : il a deja des victimes, contrairement a un champ mort que personne
# ne touche.
# CE QUE CE SCRIPT VERIFIE, ET CE QU'IL NE VERIFIE PAS — imprime a CHAQUE
# execution, pas seulement dans l'en-tete. Un en-tete se lit une fois ; une
# sortie se lit chaque fois. Sans cette ligne, quelqu'un verra « audit vert » et
# conclura « les reglages du module sont honnetes », ce qui est faux.
echo "PORTEE     : verifie que chaque champ public a UN LECTEUR."
echo "             NE verifie PAS que ce lecteur en fait le BON USAGE."
echo "             La plage de couleur, deduite du format au lieu d'etre declaree,"
echo "             avait des lecteurs : cet audit l'aurait donnee pour saine."
echo "             Il attrape le NON-CABLE, pas le MAL-CABLE."
echo

# Faux positifs connus : champ + raison. Une exception SANS raison ecrite est une
# porte ouverte ; celle-ci se relit et se conteste.
EXCEPTIONS="preset:lu par Resolve() dans le fichier de declaration, que le perimetre exclut"

anomalies=0

echo "champ                  ecrit   lu   verdict"
echo "--------------------  ------  ---   -------"
for c in $CHAMPS; do
	occ=$(grep -rhn --include=*.h --include=*.cpp --include=*.mm "\.${c}\b" $PORTEE 2>/dev/null)
	tout=$(printf '%s\n' "$occ" | grep -c . )
	ecrit=$(printf '%s\n' "$occ" | grep -cE "\.${c}\b\s*=[^=]" )
	lu=$((tout - ecrit))
	raison=$(printf '%s\n' "$EXCEPTIONS" | grep "^${c}:" | cut -d: -f2-)
	if [ "$lu" -eq 0 ] && [ -n "$raison" ]; then
		v="(exception connue : $raison)"
	elif [ "$lu" -eq 0 ] && [ "$ecrit" -gt 0 ]; then
		v="!!! ECRIT MAIS JAMAIS LU — mensonge d'API AVEC victimes"
		anomalies=$((anomalies + 1))
	elif [ "$lu" -eq 0 ]; then
		v="!!  aucun lecteur — reglage mort"
		anomalies=$((anomalies + 1))
	elif [ "$lu" -le 2 ]; then
		v="~   peu de lecteurs : couvrent-ils toutes les plateformes ?"
	else
		v="ok"
	fi
	printf "%-20s  %6d  %3d   %s\n" "$c" "$ecrit" "$lu" "$v"
done

echo
echo "LIMITE CONNUE : un nom de champ generique (width, height) est compte sur"
echo "TOUTES les structures du perimetre, pas seulement NkCameraConfig. Les gros"
echo "chiffres ne veulent donc rien dire ; ce sont les ZEROS qui portent le signal."
echo

# Un garde-fou qui n'echoue pas ne garde rien : il faut un code de sortie, sinon
# la chaine d'integration passe au vert sur une anomalie affichee en clair.
if [ "$anomalies" -gt 0 ]; then
	echo "ECHEC : $anomalies champ(s) sans lecteur. Cabler, retirer, ou ajouter une"
	echo "        EXCEPTION AVEC SA RAISON dans ce script si le lecteur est hors perimetre."
	exit 1
fi
echo "OK : tout champ public a au moins un lecteur (dans le perimetre ci-dessus)."
exit 0
