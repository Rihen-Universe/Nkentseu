#!/usr/bin/env bash
#
# epreuve_msaa_contract.sh — LE BANC SAIT-IL TOMBER ? (2026-08-23)
# =============================================================================
# Un banc vert ne prouve rien tant qu'il n'a pas montre qu'il sait rougir.
# Cette epreuve injecte deux defauts REELS dans le CONTRAT (NkDeviceCaps), pas
# un `return 3` artificiel dans le main du banc, et exige que
# NkMsaaContractCheck les voie.
#
# ⚠️ LES DEUX DEFAUTS TOUCHENT DES REGLES DISTINCTES, ET C'EST LE POINT.
#   A  `case 4: return msaa4x;` -> `return true`   : le drapeau est ignore
#   B  `default: return false;` -> `return true`   : la borne saute
# Ils doivent faire tomber des ENSEMBLES DIFFERENTS de cas. Si les deux
# faisaient tomber les memes, les cas mesureraient la meme chose trois fois.
#
# ⚠️ LE PREMIER DEFAUT A ESSAYE N'EN ETAIT PAS UN. Il retirait la garde
# « puissance de deux » de SupportsSamples — qui etait REDONDANTE avec le
# `default:`. Aucun cas ne tombait, et j'ai d'abord cru que le banc tournait sur
# un ancien binaire. Mesure faite : `jenga build` avait recompile 33 fichiers, et
# un `jenga rebuild` table rase donnait le meme vert. Le banc voyait tres bien —
# c'est le defaut qui n'existait pas.
#   => Un defaut injecte qui n'en est pas un fait croire que le banc est aveugle.
#      Avant de conclure qu'un controle ne voit rien, prouver que ce qu'on lui a
#      montre etait visible.
#   => Et deux protections pour le meme cas rendent le banc incapable de dire
#      LAQUELLE tient. La garde redondante a ete supprimee.
#
# CODES DE SORTIE
#   0  les deux defauts sont vus, et l'arbre revient a l'identique
#   2  refus de demarrer (arbre sale), ou ancre d'injection introuvable
# =============================================================================
set -uo pipefail
cd /d/Projets/2026/Nkentseu/Nkentseu-verif || exit 2
# AUCUN CHEMIN SOUS /tmp -- voir la note de epreuve_ignore_gpu.sh : un chemin
# partage entre agents concurrents est une variable globale deguisee en fichier
# temporaire, et deux agents qui y ecrivent echangent leurs fichiers SANS QUE
# RIEN N ECHOUE. Build/ est ignore par git et propre a CET arbre de travail.
mkdir -p Build/Verif/epreuve_msaa
JOURNALS="Build/Verif/epreuve_msaa"

H="Kernel/Runtime/NKRHI/src/NKRHI/Core/NkIDevice.h"
EXE="./Build/Bin/Debug-Windows/NkMsaaContractCheck/NkMsaaContractCheck.exe"

restaurer() { git checkout -- "$H" 2>/dev/null; touch "$H"; }

# ⚠️ ORDRE CRITIQUE, PAYE CETTE NUIT : le controle negatif D'ABORD, le piege APRES.
# La premiere version armait `trap restaurer EXIT` AVANT de verifier que l'arbre
# etait propre. Son `exit 2` de refus a donc declenche la restauration et DETRUIT
# la modification non commitee qu'il refusait justement d'ecraser.
# Un filet arme avant le controle detruit exactement ce que le controle protege.
sale=$(git status --porcelain -- "$H")
if [ -n "$sale" ]; then
  printf '%s\n' "REFUS : $H n'est pas propre — RIEN n'a ete touche."
  printf '%s\n' "$sale"
  exit 2
fi
trap 'restaurer' EXIT INT TERM HUP

injecter() {
  python - "$H" "$1" <<'PY'
import io, sys
p, quoi = sys.argv[1], sys.argv[2]
s = io.open(p, encoding='utf-8', newline='').read()
if quoi == 'A':
    # DEFAUT A, version 2 : le premier essai retirait la garde
    # « puissance de deux », qui etait REDONDANTE avec le default — aucun cas ne
    # tombait. Un defaut injecte qui n'en est pas un fait croire que le banc est
    # aveugle. Celui-ci fait ignorer un DRAPEAU : une regle distincte.
    a = "case 4: return msaa4x;"
    assert a in s, "ancre A"
    s = s.replace(a, "case 4: return true; // [DEFAUT INJECTE A] le drapeau msaa4x est ignore", 1)
elif quoi == 'B':
    a = "default: return false;"
    assert a in s, "ancre B"
    s = s.replace(a, "default: return true; // [DEFAUT INJECTE B]", 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
PY
  rc=$?
  touch "$H"
  return $rc
}

for d in A B; do
  printf '%s\n' "=============================================================="
  printf '%s\n' " EPREUVE $d"
  printf '%s\n' "=============================================================="
  if ! injecter "$d"; then
    printf '%s\n' "  ANCRE INTROUVABLE — l'epreuve ne mesure rien, on s'arrete."
    exit 2
  fi
  jenga build --target NkMsaaContractCheck --config Debug > "$JOURNALS/build_$d.log" 2>&1
  rcb=$?
  if [ "$rcb" -ne 0 ]; then
    printf '%s\n' "  construction echouee (code $rcb) — voir $JOURNALS/build_$d.log"
    restaurer
    continue
  fi
  "$EXE" > "$JOURNALS/run_$d.log" 2>&1
  rc=$?
  printf '%s\n' "  code de sortie du banc : $rc   (attendu : non nul)"
  grep -aE '^\s+\[FAIL\]|^=== Resultat' "$JOURNALS/run_$d.log"
  restaurer
done

printf '%s\n' "=============================================================="
printf '%s\n' " RESTAURATION"
printf '%s\n' "=============================================================="
if git diff --quiet -- "$H"; then
  printf '%s\n' "  $H identique a l'original"
else
  printf '%s\n' "  ATTENTION : $H differe encore"
fi
jenga build --target NkMsaaContractCheck --config Debug > $JOURNALS/build_C.log 2>&1
"$EXE" > $JOURNALS/run_C.log 2>&1
printf '%s\n' "  code apres restauration : $?   (attendu : 0)"
grep -aE '^=== Resultat' $JOURNALS/run_C.log
