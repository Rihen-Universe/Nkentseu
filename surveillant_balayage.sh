#!/usr/bin/env bash
# =============================================================================
# surveillant_balayage.sh — attend que la carte se libere, PUIS lance le balayage.
#
# POURQUOI IL EXISTE. La carte se libere vers 12h00 (estimation), et les taches
# d'arriere-plan de l'agent meurent vers une heure, EN SILENCE. Si j'attends
# moi-meme, je peux etre tue avant de lancer et personne ne le saura. Ce
# surveillant part DETACHE : il appartient au systeme, pas a l'agent.
#
# ⚠️ IL NE FAIT PAS CONFIANCE A L'ESTIMATION D'HEURE. Il ne lance que quand
# `carte_libre()` du script de balayage est vrai — trois verifications
# independantes (processus, nvidia-smi, VRAM libre). L'heure sert a savoir
# quand commencer a regarder, jamais a decider.
#
# Il ecrit sa TRACE des le demarrage : un PID rendu par Start-Process prouve le
# lancement, pas la survie.
# =============================================================================
set -u
RACINE="D:/Projets/2026/Nkentseu/Nkentseu-gpudebit"
cd "$RACINE" || exit 1
TRACE="$RACINE/mesures_budget/surveillant.trace"
mkdir -p "$RACINE/mesures_budget"

echo "surveillant DEMARRE pid=$$ le $(date '+%Y-%m-%d %H:%M:%S')" > "$TRACE"

# On reutilise EXACTEMENT le garde du script de balayage : deux definitions
# divergeraient, et c'est toujours celle qu'on ne relit pas qui se trompe.
source <(sed -n '/^BESOIN_MO/,/^}/p' "$RACINE/balayage_budget.sh")

LIMITE=$(( $(date +%s) + 4*3600 ))   # abandon apres 4 h : un surveillant eternel
                                     # qui a rate sa fenetre est un menteur silencieux
while true; do
	if [ "$(date +%s)" -gt "$LIMITE" ]; then
		echo "ABANDON : 4 h sans fenetre libre, $(date '+%H:%M:%S')" >> "$TRACE"
		exit 2
	fi
	if carte_libre >> "$TRACE" 2>&1; then
		echo "CARTE LIBRE a $(date '+%H:%M:%S') -> lancement du balayage" >> "$TRACE"
		bash "$RACINE/balayage_budget.sh" >> "$RACINE/mesures_budget/balayage_sortie.log" 2>&1
		echo "BALAYAGE TERMINE a $(date '+%H:%M:%S'), code=$?" >> "$TRACE"
		exit 0
	fi
	echo "  ... encore pris, $(date '+%H:%M:%S')" >> "$TRACE"
	sleep 60
done
