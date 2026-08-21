#!/usr/bin/env bash
# =============================================================================
# palier_adaptatif.sh — RATTRAPE le plus grand budget qui TIENT, apres le balayage.
#
# ⚠️ POURQUOI. 512 / 1 024 / 2 048 MiB sont ARBITRAIRES. Ce qu'on cherche est la
# FORME de la courbe « evictions par pas en fonction du budget », pas trois
# valeurs canoniques. Trois points quelconques bien espaces repondent aussi bien.
#
#   Un palier saute perd la tendance. Un palier deplace la preserve.
#
# ⚠️ ET IL ECRIT LA VALEUR REELLEMENT UTILISEE, jamais celle qu'on visait.
# Un budget « 2 GiB » annonce alors que 1,7 GiB a tourne serait un chiffre faux
# avec l'apparence d'un protocole.
#
# ⚠️ Il RE-MESURE la VRAM juste avant : Rodolf ferme des applications pendant
# qu'on tourne, donc la fenetre s'ELARGIT. Un palier refuse a 14h05 peut passer
# a 14h20 — on reessaie a la fin plutot que de le declarer perdu.
# =============================================================================
set -u
RACINE="D:/Projets/2026/Nkentseu/Nkentseu-gpudebit"
cd "$RACINE" || exit 1
SORTIE="$RACINE/mesures_budget"
TRACE="$SORTIE/adaptatif.trace"
echo "adaptatif DEMARRE pid=$$ le $(date '+%H:%M:%S')" > "$TRACE"

source <(sed -n '/^besoin_du_palier/,/^}$/p' "$RACINE/balayage_budget.sh")

# 1) attendre la fin du balayage : plus aucun NKIlyana
LIMITE=$(( $(date +%s) + 3*3600 ))
while true; do
	n=$(powershell -NoProfile -Command "@(Get-Process -Name NKIlyana -EA SilentlyContinue).Count" 2>/dev/null | tr -d '\r ')
	[ -z "$n" ] && n=0
	[ "$n" -eq 0 ] && break
	[ "$(date +%s)" -gt "$LIMITE" ] && { echo "ABANDON : balayage toujours en cours apres 3 h" >> "$TRACE"; exit 2; }
	sleep 30
done
echo "balayage termine a $(date '+%H:%M:%S')" >> "$TRACE"

# 2) le palier 2 GiB a-t-il ete saute ?
if [ ! -s "$SORTIE/paliers_sautes.txt" ]; then
	echo "aucun palier saute : rien a rattraper." >> "$TRACE"
	exit 0
fi
echo "paliers sautes :" >> "$TRACE"; cat "$SORTIE/paliers_sautes.txt" >> "$TRACE"

# 3) quel est le PLUS GRAND budget qui tient MAINTENANT ?
sleep 20 # laisser la VRAM se rendre vraiment
LIBRE=$(nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits | head -1 | tr -d '\r ')
MARGE=250
# 3815 + budget_MiB * 1.048576 <= LIBRE - MARGE
MAXB=$(( (LIBRE - MARGE - 3815) * 1000000 / 1048576 ))
echo "VRAM libre ${LIBRE} Mo -> budget maximal tenable ${MAXB} MiB" >> "$TRACE"

if [ "$MAXB" -le 1024 ]; then
	echo "RIEN A RATTRAPER : ${MAXB} MiB ne depasse pas le palier 1 GiB deja mesure." >> "$TRACE"
	echo "3e palier NON MESURE : VRAM libre ${LIBRE} Mo n'autorise que ${MAXB} MiB" >> "$SORTIE/paliers_sautes.txt"
	exit 0
fi
# arrondi a 64 MiB pres, vers le BAS — jamais vers le haut, on ne force pas
BUDGET=$(( (MAXB / 64) * 64 ))
[ "$BUDGET" -gt 2048 ] && BUDGET=2048

echo "PALIER ADAPTATIF RETENU : ${BUDGET} MiB (vise 2048)" >> "$TRACE"

EXE="$RACINE/Build/Bin/Release-Windows/NKIlyana/NKIlyana.exe"
DATA="D:/Projets/Camrail/AI/IlyanaReel"
MODELE="--train --llama --tying --d 640 --layers 10 --heads 8 --T 256 --B 3 --accum 8"
DONNEES="--corpus $DATA/socle/prose_socle_dedup.txt --bpe $DATA/tokenizer/ilyana_v5_mixte.nkbpe --maxchars 200000000"
COURSE="--lr 3e-4 --clip 1.0 --graine 7919 --echantillons 0 --journal 25 --steps 60 --horizon 60"

"$EXE" $MODELE $DONNEES $COURSE --reserve-budget-mo "$BUDGET" > "$SORTIE/palier_${BUDGET}mib_ADAPTATIF.log" 2>&1
echo "termine a $(date '+%H:%M:%S'), code=$?  -> palier_${BUDGET}mib_ADAPTATIF.log" >> "$TRACE"
echo "3e palier MESURE a ${BUDGET} MiB (et NON 2048 : VRAM libre ${LIBRE} Mo)" >> "$SORTIE/paliers_sautes.txt"
