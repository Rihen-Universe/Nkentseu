#!/usr/bin/env bash
# lance_runs.sh — les dix passages de la mesure (b1)
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# stdout et stderr de CHAQUE passage sont rediriges SEPAREMENT, et la sonde
# imprime sa CONDITION D'ESSAI (scale, ItemHeight, titleBarH) au moment meme de
# la capture : sans elle, un temoin muet passe pour un resultat.
#
# ⚠️ CE FICHIER A DEJA MENTI UNE FOIS. Une reecriture par `str.replace` n'avait
#    PAS pris (chaine cherchee inexacte), le script est reste sans
#    `NKPROBE_SCALE`, et les deux passages d'echelle ont rendu le meme nombre --
#    ce qui ressemblait exactement a « le kit ne met pas a l'echelle ». C'est le
#    script qui etait muet. D'ou l'echo de la commande complete ci-dessous.
set -u
EXE="$1"       # chemin de NkChromeProbe.exe
OUT="$2"       # dossier de sortie

run() {
  local nom="$1" titleh="$2" font="$3" cible="$4" echelle="${5:-0}"
  echo "=== $nom  titleH=$titleh font=$font cible=$cible scale=$echelle"
  env NKPROBE_TITLEH="$titleh" NKPROBE_FONT="$font" NKPROBE_CIBLE="$cible" \
      NKPROBE_SCALE="$echelle" NKPROBE_OUT="$OUT/$nom.png" NKPROBE_FRAME=6 \
      "$EXE" 1> "$OUT/$nom.out" 2> "$OUT/$nom.err"
  local code=$?
  echo "   exit=$code  out=$(wc -c < "$OUT/$nom.out") octets  err=$(wc -c < "$OUT/$nom.err") octets"
}

# ── ZERO DU COMPTEUR : la sonde entiere, la cible en moins ───────────────────
run zero            28  0  0

# ── LA MESURE : deux hauteurs imposees ───────────────────────────────────────
# NEGATIF : a28 et b56 doivent DIFFERER. S'ils sont egaux, je ne mesure pas le
#           reglage mais autre chose.
run a28             28  0  1
run b56             56  0  1

# ── TEMOIN POLICE : l'historique bouge avec la police, l'impose non ──────────
run c_hist_petit     0 12  1
run c_hist_grand     0 24  1
run c_impose_grand  28 24  1

# ── LE TEST DECISIF : l'echelle d'interface ──────────────────────────────────
# TEMOIN POSITIF : l'historique DOIT grandir de 1,00 a 1,25 (il passe par S()).
#                  S'il ne bouge pas, c'est mon levier qui est mort, pas le kit.
run d_hist_s100      0 16  1  1.00
run d_hist_s125      0 16  1  1.25
# LA MESURE      : la valeur IMPOSEE ne doit PAS bouger avec l'echelle.
run d_impose_s100   28 16  1  1.00
run d_impose_s125   28 16  1  1.25
