#!/bin/bash
# Courses de dimensionnement -- REFONTE UNIQUE contre l'etat final (2026-08-17).
# Etat final = accumulation reparee (#65), reserve branchee (defaut ON), NK_CPP11
# ouvert (#69), VramPic exact (deux pics), checkpoint v5.
# Chaque course : 8 pas, corpus REEL (prose_socle_dedup coupe a 5 M car.),
# tokenizer v5 mixte, --journal 1 pour dater chaque pas.
# Usage : bash courses.sh [liste de configs "d:L:B:accum" ...]
set -u
R=/d/Projets/Camrail/AI/IlyanaReel/dim2
EXE=$R/NKIlyana.exe
CORPUS=/d/Projets/Camrail/AI/IlyanaReel/socle/prose_socle_dedup.txt
BPE=/d/Projets/Camrail/AI/IlyanaReel/tokenizer/ilyana_v5_mixte.nkbpe
CONFS=${@:-"640:10:6:4 640:10:3:8 640:10:12:2 640:10:24:1 640:12:3:8 640:12:6:4"}
for c in $CONFS; do
  IFS=: read d L B A <<< "$c"
  nom="d${d}_L${L}_B${B}_a${A}"
  mkdir -p $R/$nom; cd $R/$nom
  rm -rf logs
  # GARDE VRAM (2026-08-18) : le 17/08 23:12-23:18, trois courses lancees avec le bureau a
  # 4,6 Go de VRAM ont donne 1 dispatch non calcule (filet ce_idx_fwd, evenement nvlddmkm 23:13:41)
  # puis un ecran bleu 0x119 VIDEO_SCHEDULER (nvlddmkm 23:18:00) pendant B12a2. Borne mesuree :
  # pic(B) ~ 1,3 Go + 0,95 Go x B a d640/L10/T256 (2 points, ordonnee = poids+Adam, coherente).
  used=$(nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits | tr -d ' ')
  besoin=$(( 1300 + 950 * B * L / 10 ))
  if [ $(( used + besoin )) -gt 7600 ]; then
    echo "=== $nom  $(date '+%H:%M:%S')  NON LANCEE : VRAM occupee ${used} MiB + besoin estime ${besoin} Mo > 7600 (carte 8192)" | tee -a $R/RESULTATS.txt
    continue
  fi
  echo "=== $nom  $(date '+%H:%M:%S')  VRAM avant: ${used} MiB  clang: $(tasklist | grep -c clang.exe)" | tee -a $R/RESULTATS.txt
  # --save DANS le dossier de la course : le 17/08 l'oubli a fait ecrire le checkpoint de fin de
  # course sur le chemin PAR DEFAUT D:/Projets/Camrail/AI/Ilyana/ilyana.nkgp (modele du 10/08,
  # sauve par la rotation a trois puis restaure a la main). Jetable, jamais le chemin par defaut.
  "$EXE" --train --save $R/$nom/ckpt_jetable.nkgp --llama --tying --d $d --layers $L --heads 8 --T 256 --B $B --accum $A \
     --corpus "$CORPUS" --bpe "$BPE" --maxchars 5000000 --steps 8 --warmup 2 --lr 3e-4 \
     --echantillons 0 --valfrac 0 --saveevery 0 --saveminutes 0 --journal 1 > stdout.txt 2>&1
  code=$?; echo "exit=$code" >> $R/RESULTATS.txt
  # extraction : pertes/pas datees, pics VRAM, temoin reserve
  grep -E "pas [0-9]+ : perte" logs/app.log | sed 's/^\[\([^]]*\)\].*-> */\1 /; s/ *(moy.*lr=/ lr=/' >> $R/RESULTATS.txt
  grep -E "VRAM suivie|VRAM pic physique|\[reserve\] servis|ARRET|Entrainement termine|Param.tres R.ELS|Defaut|defaut GPU" logs/app.log | sed 's/.*-> //' >> $R/RESULTATS.txt
  echo "VRAM apres: $(nvidia-smi --query-gpu=memory.used --format=csv,noheader)" >> $R/RESULTATS.txt
  echo "FIN $nom $(date '+%H:%M:%S')" >> $R/RESULTATS.txt   # une course sans FIN = coupee, ne compte pas
  # STOP SERIE au premier defaut GPU : on ne rempile pas sur un pilote qui vient de lacher.
  if [ $code -ne 0 ] || grep -q -e "ARRET au pas" -e "DEFAUT dans" logs/app.log; then
    echo "STOP SERIE apres $nom : defaut GPU ou sortie non nulle -- regarder nvidia-smi et l'observateur d'evenements (nvlddmkm)" | tee -a $R/RESULTATS.txt
    break
  fi
done
