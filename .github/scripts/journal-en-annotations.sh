#!/usr/bin/env bash
#
# journal-en-annotations.sh — LE JOURNAL DE CI, LISIBLE SANS COMPTE (2026-09-30)
# =============================================================================
# POURQUOI IL EXISTE
#
#   Le journal d'un run GitHub Actions ne se lit qu'authentifie, MEME sur un
#   depot public. Les ANNOTATIONS, elles, sont publiques (API check-runs).
#   Du 26/09 au 30/09, tous les runs de main ont echoue et la seule chose
#   lisible etait « Process completed with exit code 1 » : l'erreur etait
#   ecrite, mais personne ne pouvait la lire sans `gh auth login`.
#
#   Ce script relit un journal de construction, en tire les premieres erreurs
#   du compilateur, du lieur ou de Jenga, et les publie en annotations
#   ::error::, suivies de la fin du journal. Le journal complet reste en
#   artefact pour qui est connecte.
#
# USAGE
#   journal-en-annotations.sh <journal> <titre>
#
#   Il ne fait JAMAIS echouer l'etape qui l'appelle (sortie 0) : c'est un
#   rapporteur, le verdict appartient a la construction.
#
# LIMITES ASSUMEES
#   GitHub garde 10 annotations d'erreur par etape et 50 par job : on en emet
#   au plus 8 + 1 (la fin du journal). Chaque extrait est borne en lignes, pour
#   rester sous la taille maximale d'une annotation.
# =============================================================================

set -u

JOURNAL="${1:?journal manquant}"
TITRE="${2:-construction}"
MAX_ERREURS=8
LIGNES_CONTEXTE=6
LIGNES_FIN=60

if [ ! -f "$JOURNAL" ]; then
    echo "::error title=${TITRE}::journal introuvable : ${JOURNAL}"
    exit 0
fi

# Les couleurs ANSI de Jenga rendent une annotation illisible. perl plutot que
# sed : le sed de macOS (BSD) ne comprend pas \x1b dans une expression.
PROPRE="${JOURNAL%.log}.propre.log"
perl -pe 's/\e\[[0-9;?]*[A-Za-z]//g; s/\r$//' "$JOURNAL" > "$PROPRE"

# Une annotation tient sur UNE ligne de commande : les fins de ligne et le
# signe pourcent s'echappent, sinon le message est coupe a la premiere ligne.
Echapper() {
    perl -pe 's/%/%25/g; s/\r/%0D/g; s/\n/%0A/g'
}

# ── Les erreurs : compilateur, lieur, Python (chargement du workspace) ──
# « : error: » et non « error » : NkError.cpp, -Werror et les noms de fichiers
# contenant le mot feraient sinon des faux positifs par dizaines.
MOTIF=': (fatal )?error:|^Undefined symbols|^ld: |linker command failed|Traceback \(most recent call last\)|^[A-Za-z]*(Error|Exception): |[Ee]rreur :|No such file or directory'

N=0
for NUM in $(grep -n -E "$MOTIF" "$PROPRE" | cut -d: -f1); do
    if [ "$N" -ge "$MAX_ERREURS" ]; then
        break
    fi
    FIN=$((NUM + LIGNES_CONTEXTE))
    EXTRAIT=$(sed -n "${NUM},${FIN}p" "$PROPRE" | cut -c1-400)
    # Une meme erreur repetee (un en-tete fautif inclus par vingt fichiers)
    # ne doit pas consommer les huit places : on saute les doublons exacts.
    PREMIERE=$(sed -n "${NUM}p" "$PROPRE")
    if [ -n "${DEJA_VU:-}" ] && printf '%s\n' "$DEJA_VU" | grep -Fxq -- "$PREMIERE"; then
        continue
    fi
    DEJA_VU="${DEJA_VU:-}${PREMIERE}"$'\n'
    N=$((N + 1))
    # Pas de virgule ni de deux-points dans le titre : ce sont les separateurs
    # des proprietes d'une commande de workflow.
    printf '::error title=%s erreur %d ligne %d::%s\n' "$TITRE" "$N" "$NUM" \
        "$(printf '%s' "$EXTRAIT" | Echapper)"
done

if [ "$N" -eq 0 ]; then
    echo "::warning title=${TITRE}::aucune ligne d'erreur reconnue ; voir la fin du journal"
fi

# ── La fin du journal : ce que Jenga dit en dernier (resume, cible en echec) ──
printf '::error title=%s fin du journal::%s\n' "$TITRE" \
    "$(tail -n "$LIGNES_FIN" "$PROPRE" | cut -c1-300 | Echapper)"

exit 0
