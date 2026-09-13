#!/usr/bin/env bash
#
# contre_epreuve_verif.sh — LE VERIFICATEUR VU EN ROUGE (2026-08-22)
# =============================================================================
# POURQUOI CE SCRIPT EXISTE
#
#   « Un verificateur qui n'a jamais echoue ne prouve que son existence. »
#
#   `verif_bancs.sh` au vert ne dit pas qu'il sait dire non. Ce script casse un
#   banc VOLONTAIREMENT, exige que le verificateur le dise et sorte non nul,
#   puis restaure et exige qu'il repasse au vert.
#
# TROIS EPREUVES, ET CE QUE CHACUNE REFUTERAIT
#
#   A. CONSTRUCTION CASSEE (exigence 1)
#      On injecte une erreur de compilation. Attendu : verdict ECHEC,
#      « construit = NON », code de sortie non nul.
#      ⚠️ ET SURTOUT : le banc ne doit PAS avoir ete EXECUTE. C'est le defaut
#      exact qui a motive ce chantier — un agent a compte une mutation
#      « survivante » alors que la compilation avait echoue et que L'ANCIEN
#      BINAIRE avait tourne. On le prouve en horodatant le journal d'execution
#      avant l'epreuve : s'il a bouge, le verificateur a lance l'ancien exe, et
#      cette contre-epreuve echoue.
#
#   B. EXECUTION EN ECHEC (exigences 1 et 5)
#      Le banc compile mais rend un code non nul. Attendu : ECHEC, code
#      rapporte, et une premiere ligne significative qui dit POURQUOI.
#
#   D. UN BANC NEUF NON DECLARE (exigence 2)
#      On cree un projet dans Applications/ sans ligne de classement, avec un
#      nom que AUCUNE heuristique ne rattraperait (`NkBancFantome` — ni
#      « Test », ni « Check », ni « Bench »). Attendu : il est NOMME, la cause
#      est dite, et le code de sortie vaut 3.
#
#   C. RETOUR AU VERT
#      Apres restauration, le meme banc doit repasser OK et le verificateur
#      sortir 0. Sans C, A et B ne prouveraient qu'un outil casse en permanence.
#
# CONTROLE NEGATIF DE LA CONTRE-EPREUVE ELLE-MEME
#   Avant de casser quoi que ce soit, on exige que le banc soit DEJA au vert.
#   Un banc deja rouge rendrait les epreuves A et B vraies pour rien.
#
# LE FICHIER TOUCHE EST RESTAURE OCTET POUR OCTET, meme en cas d'interruption
# (trap EXIT). L'etat git est verifie a la fin.
#
# USAGE
#   ./contre_epreuve_verif.sh [--banc NKSmoothMeshTest]
# =============================================================================
set -uo pipefail

dire()  { printf '%s\n' "$*"; }
dire2() { printf '%s\n' "$*" >&2; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || { dire2 "[contre] cd $ROOT impossible"; exit 2; }

BANC="NKSmoothMeshTest"
[ "${1:-}" = "--banc" ] && BANC="${2:-$BANC}"

SRC="Applications/$BANC/src/main.cpp"
[ -f "$SRC" ] || { dire2 "[contre] source introuvable : $SRC"; exit 2; }

SAUVE="$(mktemp)"
cp -p "$SRC" "$SAUVE" || { dire2 "[contre] sauvegarde impossible"; exit 2; }

# ⚠️ RESTAURER SANS `cp -p`, PUIS `touch` — DEFAUT MESURE LE 2026-08-22.
# La premiere version restaurait par `cp -p`, qui PRESERVE LA DATE d'origine.
# Le fichier redevenait juste, avec une date ANTERIEURE aux objets compiles
# pendant l'epreuve ; le cache de Jenga, qui compare les dates, ne voyait aucun
# changement, ne recompilait rien, et l'epreuve C relancait LE BINAIRE CASSE.
# Le verificateur avait raison — le binaire rendait bien 3 — c'est la
# contre-epreuve qui mentait sur ce qu'elle avait restaure.
# C'est LA MEME FAMILLE que le defaut qui a motive tout ce chantier : un
# ancien binaire qui tourne sans que personne ne s'en apercoive.
restaurer() {
  cat "$SAUVE" > "$SRC" 2>/dev/null
  touch "$SRC" 2>/dev/null
  rm -f "$SAUVE" 2>/dev/null
}
trap restaurer EXIT INT TERM

ECHECS=0
sep() { dire ""; dire "======================================================================"; dire " $*"; dire "======================================================================"; }
attendu() {  # $1 = ce qui etait attendu, $2 = ok/ko, $3 = ce qu'on a vu
  if [ "$2" = "ok" ]; then dire "   [vu] $1"
  else dire2 "   [MANQUE] attendu : $1"; dire2 "            observe : $3"; ECHECS=$((ECHECS + 1)); fi
}

JR="Build/Verif/$BANC.run.log"

# =============================================================================
sep "CONTROLE NEGATIF — le banc doit etre DEJA au vert avant qu'on le casse"
# =============================================================================
SORTIE0=$(./verif_bancs.sh --banc "$BANC" --mode complet 2>&1); RC0=$?
dire "$SORTIE0" | tail -12
if [ "$RC0" -ne 0 ]; then
  dire2 ""
  dire2 "[contre] ABANDON : $BANC est deja en echec (code $RC0)."
  dire2 "[contre] Casser un banc deja rouge rendrait les epreuves A et B vraies"
  dire2 "[contre] pour rien. Repare le banc d'abord, ou choisis-en un autre :"
  dire2 "[contre]   ./contre_epreuve_verif.sh --banc <AutreBanc>"
  exit 2
fi
dire ""
dire "   [vu] $BANC au vert, code 0 — la suite mesure bien ce qu'on casse."

# =============================================================================
sep "EPREUVE A — CONSTRUCTION CASSEE (exigence 1)"
# =============================================================================
# Empreinte du journal d'EXECUTION avant l'epreuve. S'il bouge, le verificateur
# a lance l'ancien binaire : c'est exactement le faux « survivant » a interdire.
AVANT_RUN="absent"
[ -f "$JR" ] && AVANT_RUN=$(md5sum "$JR" | cut -d' ' -f1)
dire "   journal d'execution avant l'epreuve : $AVANT_RUN"

# Injection : un identificateur qui n'existe nulle part, en tete de main().
# `printf '%s\n'` avec le texte en ARGUMENT — aucune donnee dans une chaine de
# format, y compris ici (regle des accolades, transposee au shell).
python - "$SRC" <<'PYFIN'
import io, sys
p = sys.argv[1]
s = io.open(p, encoding="utf-8", newline="").read()
marque = "int main("
i = s.index(marque)
j = s.index("{", i) + 1
s = s[:j] + "\n\tcontre_epreuve_identificateur_inexistant = 1; // INJECTE\n" + s[j:]
io.open(p, "w", encoding="utf-8", newline="").write(s)
PYFIN
[ $? -eq 0 ] || { dire2 "[contre] injection impossible"; exit 2; }
dire "   injecte dans $SRC : un identificateur non declare"

SORTIE_A=$(./verif_bancs.sh --banc "$BANC" --mode complet 2>&1); RC_A=$?
dire ""
dire "$SORTIE_A" | sed -n '/-- Bancs ---/,$p'
dire ""

if [ "$RC_A" -ne 0 ]; then attendu "code de sortie non nul (obtenu $RC_A)" ok ""
else attendu "code de sortie non nul" ko "0"; fi

if printf '%s' "$SORTIE_A" | grep -q "ECHEC"; then attendu "verdict ECHEC" ok ""
else attendu "verdict ECHEC" ko "aucun ECHEC dans la sortie"; fi

if printf '%s' "$SORTIE_A" | grep -q "construction echouee"; then
  attendu "la CAUSE nommee : « construction echouee »" ok ""
else
  attendu "la CAUSE nommee : « construction echouee »" ko "cause absente du rapport"
fi

LIGNE=$(printf '%s' "$SORTIE_A" | grep -A1 "^  $BANC :" | tail -1)
if printf '%s' "$LIGNE" | grep -qi "error\|C[0-9]\{4\}"; then
  attendu "la premiere ligne significative du compilateur (exigence 5)" ok ""
  dire "        -> $LIGNE"
else
  attendu "la premiere ligne significative du compilateur (exigence 5)" ko "$LIGNE"
fi

APRES_RUN="absent"
[ -f "$JR" ] && APRES_RUN=$(md5sum "$JR" | cut -d' ' -f1)
if [ "$APRES_RUN" = "$AVANT_RUN" ]; then
  attendu "L'ANCIEN BINAIRE N'A PAS TOURNE (journal d'execution inchange)" ok ""
else
  attendu "L'ANCIEN BINAIRE N'A PAS TOURNE" ko "le journal d'execution a change ($AVANT_RUN -> $APRES_RUN)"
  dire2 "            C'est LE defaut que ce chantier existe pour supprimer."
fi

# =============================================================================
sep "EPREUVE B — EXECUTION EN ECHEC (exigences 1 et 5)"
# =============================================================================
cat "$SAUVE" > "$SRC"; touch "$SRC"      # date rafraichie : voir la note sur `cp -p`
# Le banc compile, mais rend 3. Aucun doute possible sur la provenance du code.
python - "$SRC" <<'PYFIN'
import io, sys
p = sys.argv[1]
s = io.open(p, encoding="utf-8", newline="").read()
marque = "int main("
i = s.index(marque)
j = s.index("{", i) + 1
s = s[:j] + '\n\tprintf("ECHEC INJECTE PAR LA CONTRE-EPREUVE\\n"); return 3; // INJECTE\n' + s[j:]
io.open(p, "w", encoding="utf-8", newline="").write(s)
PYFIN
dire "   injecte dans $SRC : return 3 en tete de main()"

SORTIE_B=$(./verif_bancs.sh --banc "$BANC" --mode complet 2>&1); RC_B=$?
dire ""
dire "$SORTIE_B" | sed -n '/-- Bancs ---/,$p'
dire ""

if [ "$RC_B" -ne 0 ]; then attendu "code de sortie non nul (obtenu $RC_B)" ok ""
else attendu "code de sortie non nul" ko "0"; fi
if printf '%s' "$SORTIE_B" | grep -q "code de sortie 3"; then
  attendu "le CODE DU BANC rapporte tel quel : 3" ok ""
else
  attendu "le CODE DU BANC rapporte tel quel : 3" ko "code 3 absent du rapport"
fi
if printf '%s' "$SORTIE_B" | grep -q "ECHEC INJECTE PAR LA CONTRE-EPREUVE"; then
  attendu "la premiere ligne significative de la SORTIE DU BANC (exigence 5)" ok ""
else
  attendu "la premiere ligne significative de la SORTIE DU BANC" ko "ligne du banc absente"
fi

# =============================================================================
sep "EPREUVE C — RETOUR AU VERT"
# =============================================================================
cat "$SAUVE" > "$SRC"; touch "$SRC"      # date rafraichie : voir la note sur `cp -p`
dire "   $SRC restaure (date rafraichie, sinon le cache de Jenga ne le voit pas)"
SORTIE_C=$(./verif_bancs.sh --banc "$BANC" --mode complet 2>&1); RC_C=$?
dire ""
dire "$SORTIE_C" | sed -n '/-- Tableau ---/,$p'
dire ""
if [ "$RC_C" -eq 0 ]; then attendu "retour au code 0" ok ""
else attendu "retour au code 0" ko "$RC_C"; fi
if printf '%s' "$SORTIE_C" | grep -q "VERDICT : OK"; then attendu "verdict OK" ok ""
else attendu "verdict OK" ko "verdict non OK"; fi

# =============================================================================
sep "EPREUVE D — UN BANC NEUF QUE PERSONNE N'A DECLARE (exigence 2)"
# =============================================================================
# On cree un projet dans Applications/ SANS ligne dans config/bancs.list, et on
# exige que le verificateur le nomme et sorte 3.
#
# ⚠️ LE NOM EST CHOISI EXPRES : `NkBancFantome` ne finit pas par « Test », ne
# contient ni « Check » ni « Bench ». Une heuristique sur le nom ne le verrait
# pas — c'est precisement ce que cette epreuve refute. Elle ne passe que si le
# marquage est une DONNEE.
FANTOME="Applications/NkBancFantome"
nettoyer_fantome() { rm -rf "$FANTOME" 2>/dev/null; }
trap 'restaurer; nettoyer_fantome' EXIT INT TERM

mkdir -p "$FANTOME"
{
  printf '%s
' 'from Jenga import *'
  printf '%s
' 'from jengaconfig import *'
  printf '%s
' ''
  printf '%s
' 'with project("NkBancFantome"):'
  printf '%s
' '    consoleapp()'
} > "$FANTOME/NkBancFantome.jenga"
dire "   cree : $FANTOME/NkBancFantome.jenga (aucune ligne dans config/bancs.list)"

SORTIE_D=$(./verif_bancs.sh --liste 2>&1); RC_D=$?
dire ""
dire "$SORTIE_D" | sed -n '/-- Inventaire et garde/,/^$/p'
dire ""
if [ "$RC_D" -eq 3 ]; then attendu "code de sortie 3 (echec de classement)" ok ""
else attendu "code de sortie 3 (echec de classement)" ko "$RC_D"; fi
if printf '%s' "$SORTIE_D" | grep -q "NkBancFantome"; then
  attendu "le projet NOMME dans le rapport" ok ""
else
  attendu "le projet NOMME dans le rapport" ko "nom absent"
fi
if printf '%s' "$SORTIE_D" | grep -q "NON CLASSE"; then
  attendu "la cause nommee : NON CLASSE" ok ""
else
  attendu "la cause nommee : NON CLASSE" ko "cause absente"
fi

nettoyer_fantome
SORTIE_D2=$(./verif_bancs.sh --liste 2>&1); RC_D2=$?
if [ "$RC_D2" -eq 0 ]; then attendu "retour au code 0 apres retrait du fantome" ok ""
else attendu "retour au code 0 apres retrait du fantome" ko "$RC_D2"; fi

# =============================================================================
sep "ETAT DE L'ARBRE"
# =============================================================================
SALE=$(git status --porcelain -- "$SRC" 2>/dev/null)
if [ -z "$SALE" ]; then
  attendu "$SRC identique a l'original (rien laisse derriere)" ok ""
else
  attendu "$SRC identique a l'original" ko "$SALE"
fi

# =============================================================================
dire ""
dire "======================================================================"
if [ "$ECHECS" -eq 0 ]; then
  dire " CONTRE-EPREUVE REUSSIE — le verificateur sait dire NON."
  dire "   A : construction cassee  -> ECHEC, non nul, ancien binaire NON lance"
  dire "   B : execution en echec   -> ECHEC, code 3 rapporte, cause citee"
  dire "   C : apres restauration   -> OK, code 0"
  dire "   D : banc neuf non declare -> nomme, cause dite, code 3"
  dire "======================================================================"
  exit 0
fi
dire2 " CONTRE-EPREUVE ECHOUEE — $ECHECS attente(s) non satisfaite(s)."
dire2 " Le verificateur ne prouve PAS ce qu'il pretend. Ne pas s'y fier."
dire2 "======================================================================"
exit 1
