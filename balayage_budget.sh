#!/usr/bin/env bash
# =============================================================================
# balayage_budget.sh — BALAYAGE DU BUDGET DE RESERVE (agent debit GPU, 2026-08-19)
#
# Repond a UNE question : les 6 376 allocations pilote reelles par pas viennent-
# elles d'un BUDGET trop petit (taille deja vue, tampon evince) ou de TAILLES
# JAMAIS REVUES (qu'aucun budget ne servirait) ?
#
# ⚠️ CE SCRIPT NE CHANGE QU'UNE SEULE CHOSE : `--reserve-budget-mo`.
#    Le prototype de recyclage des tampons de commandes reste INACTIF
#    (NK_GPU_RECYCLE_CMD non definie). Verifie : `NkString(nullptr)` est garde
#    par `if (str)`, donc chaine vide -> `Empty()` vrai -> recyclage = 0.
#    Monter le budget ET comparer un bras recycle ne dirait pas lequel a agi.
#
# ⚠️ IL NE PEUT PAS TOURNER A COTE DE LA COURSE 3. Calcul, pas supposition :
#    pic physique de la course = 4 352 Mo ; mon pic au meme modele = 4 352 Mo a
#    512 MiB, 5 962 Mo a 2 GiB. Somme = 8 704 a 10 314 Mo sur une carte de
#    8 192 Mo. AUCUN palier ne tient. Le script REFUSE de demarrer tant que
#    NKIlyana.exe occupe la carte — l'attente se decide sur `nvidia-smi`, pas
#    sur l'horloge.
#
# ⚠️ IL N'ECRIT AUCUN CHECKPOINT dans le dossier de la course (`clip_g7919`).
#    Pas de `--save` du tout : ecraser le checkpoint d'Ilyana serait pire que
#    n'importe quel gain de debit.
#
# Lancement DETACHE obligatoire (les taches d'arriere-plan de l'agent meurent
# vers une heure, en silence) :
#   powershell -NoProfile -Command "Start-Process -FilePath bash \
#       -ArgumentList './balayage_budget.sh' -WorkingDirectory <ici> -PassThru"
# =============================================================================
set -u

RACINE="D:/Projets/2026/Nkentseu/Nkentseu-gpudebit"
EXE="$RACINE/Build/Bin/Release-Windows/NKIlyana/NKIlyana.exe"
DATA="D:/Projets/Camrail/AI/IlyanaReel"
SORTIE="$RACINE/mesures_budget"
mkdir -p "$SORTIE"

# --- Le modele est EXACTEMENT celui de la course en cours ---------------------
# Releve sur la ligne de commande reelle du PID 492. Le motif d'allocation
# depend de ces parametres : les changer changerait la question posee.
MODELE="--train --llama --tying --d 640 --layers 10 --heads 8 --T 256 --B 3 --accum 8"
DONNEES="--corpus $DATA/socle/prose_socle_dedup.txt --bpe $DATA/tokenizer/ilyana_v5_mixte.nkbpe --maxchars 200000000"
# ⚠️ `--steps` pilote le schema de LR (regle du 18/08) : une course de 60 pas
#    n'est PAS une course de 4 000 tronquee. Sans effet ici — je mesure des
#    COMPTEURS D'ALLOCATION et un debit, pas une trajectoire de perte. Ecrit
#    pour que personne ne lise ces pertes comme comparables a la campagne.
COURSE="--lr 3e-4 --clip 1.0 --graine 7919 --echantillons 0 --journal 25 --steps 60 --horizon 60"
# Pas de --save : aucun checkpoint. Pas de --stop : la course doit finir
# NORMALEMENT, sinon le `return` de NkGptTrainer.cpp mange le profil (dette Q24)
# et le balayage ne rendrait aucune distribution de tailles.

# ⚠️ TROIS verifications INDEPENDANTES. Une seule source peut mentir :
#  - `nvidia-smi --query-compute-apps` rend « [Insufficient Permissions] » pour
#    certains processus, donc un nom peut manquer de sa liste ;
#  - la liste des processus ne dit rien de la VRAM reellement libre ;
#  - un autre agent (NkUIDesign) peut prendre la carte quelques secondes pour une
#    capture, sans etre NKIlyana.
# Il faut les TROIS d'accord pour demarrer. Un refus coute une attente ; un faux
# « libre » coute la course 3 de Rodolf.
# ⚠️ LE BESOIN EST PAR PALIER, PAS GLOBAL (corrige le 2026-08-19 13:45).
# La premiere version exigeait 6 200 Mo pour TOUTE la serie — le pic du palier
# 2 GiB. Resultat : avec 5 603 Mo libres (les 2 429 Mo restants sont Windows et
# ne se libereront jamais), le garde bloquait AUSSI les paliers 512 MiB et 1 GiB,
# qui tiennent tres largement.
#
#   Une precondition globale dimensionnee sur le PIRE cas bloque les cas qui
#   tiendraient.
#
# ⚠️ Et la correction n'est PAS d'abaisser le seuil : il est calcule sur un pic
# reel, l'abaisser ferait ECHOUER une mesure en cours au lieu de la REFUSER
# proprement. La correction est de le rendre exact pour chaque palier.
#
# Pic attendu = (pic calcul seul mesure : 4 352 - 537) + budget retenu.
besoin_du_palier() { # $1 = budget en MiB
	echo $(( 3815 + $1 * 1024 * 1024 / 1000000 ))
}

# $1 = Mo requis pour CE palier. Trois verifications independantes, toutes
# requises : une seule source peut mentir.
carte_libre() {
	local besoin="$1"
	# (1) le PROCESSUS existe-t-il ? (independant du pilote)
	local nproc
	nproc=$(powershell -NoProfile -Command "@(Get-Process -Name NKIlyana -EA SilentlyContinue).Count" 2>/dev/null | tr -d ' ')
	[ -z "$nproc" ] && nproc=0
	if [ "$nproc" -ne 0 ]; then
		echo "  [garde] NKIlyana : $nproc processus vivant(s)"
		return 1
	fi
	# (2) la carte le voit-elle encore ? (le processus meurt avant la VRAM)
	local occ
	occ=$(nvidia-smi --query-compute-apps=process_name --format=csv,noheader 2>/dev/null | grep -c 'NKIlyana' || true)
	if [ "$occ" -ne 0 ]; then
		echo "  [garde] nvidia-smi voit encore NKIlyana"
		return 1
	fi
	# (3) assez de VRAM libre POUR CE PALIER ? (attrape un tiers : NkUIDesign...)
	local libre
	libre=$(nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits 2>/dev/null | head -1 | tr -d ' ')
	[ -z "$libre" ] && libre=0
	if [ "$libre" -lt "$besoin" ]; then
		echo "  [garde] VRAM libre ${libre} Mo < ${besoin} Mo requis pour ce palier"
		return 1
	fi
	echo "  [garde] OK : ${libre} Mo libres, ${besoin} Mo requis"
	return 0
}


echo "=== BALAYAGE DU BUDGET DE RESERVE — $(date '+%Y-%m-%d %H:%M:%S') ==="
echo "prototype de recyclage : ${NK_GPU_RECYCLE_CMD:-<non definie, donc INACTIF>}"

if [ ! -x "$EXE" ]; then
	echo "ARRET : $EXE absent. Construire NKIlyana en Release d'abord."
	exit 1
fi

for BUDGET in 512 1024 2048; do
	echo
	echo "----------------------------------------------------------------"
	echo "PALIER $BUDGET MiB — $(date '+%H:%M:%S')"

	# ⚠️ nvidia-smi AVANT CHAQUE PALIER, pas une seule fois au debut : la course 3
	# peut se relancer entre deux paliers, et une mesure prise dans un etat qu'on
	# n'a pas verifie n'accuse que celui qui la publie.
	BESOIN=$(besoin_du_palier "$BUDGET")
	if ! carte_libre "$BESOIN"; then
		# ⚠️ UN PALIER SAUTE SE DIT. Une serie qui n'a jamais demarre ne se
		# distingue pas d'une serie sans effet ; un palier saute ET ANNONCE se
		# distingue des deux.
		LIBRE=$(nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits | head -1 | tr -d ' ')
		echo "PALIER $BUDGET MiB **NON MESURE** — VRAM insuffisante : ${LIBRE} Mo libres pour ${BESOIN} Mo requis."
		echo "$BUDGET MiB : NON MESURE (VRAM ${LIBRE} < ${BESOIN})" >> "$SORTIE/paliers_sautes.txt"
		continue
	fi
	nvidia-smi --query-gpu=utilization.gpu,memory.used --format=csv,noheader

	JOURNAL="$SORTIE/palier_${BUDGET}mib.log"
	"$EXE" $MODELE $DONNEES $COURSE --reserve-budget-mo "$BUDGET" >"$JOURNAL" 2>&1
	echo "code de sortie : $?  ->  $JOURNAL"

	# Les quatre chiffres attendus, extraits tout de suite : un journal qu'on
	# depouille plus tard est un journal qu'on depouille mal.
	grep -E 'RESERVE DE TAMPONS|\[reserve\]|VRAM suivie|PROFIL PAR NOYAU|DISTRIBUTION DES TAILLES|taille DEJA VUE|taille JAMAIS VUE|servis par la reserve|Entra.nement termin' "$JOURNAL" || echo "  (aucune ligne cle — la course a-t-elle atteint la fin ?)"
done

echo
echo "=== FIN — $(date '+%Y-%m-%d %H:%M:%S') ==="
echo "Rappel de lecture :"
echo "  - 'taille DEJA VUE' dominante  -> le budget suffit, sous-allocateur de second ordre"
echo "  - 'taille JAMAIS VUE' dominante -> aucun budget n'y fera rien, changer la CLE ou sous-allouer"
echo "  - evictions/pas qui chutent SANS que le debit bouge -> le goulot est ailleurs (information, pas echec)"
