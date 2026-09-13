# -*- coding: utf-8 -*-
"""CE QUE RODOLF PEUT VOIR — et non ce que nous avons écrit.

=============================================================================
 POURQUOI CET OUTIL EXISTE, ET C'EST LA CINQUIÈME FOIS
=============================================================================
 Le 2026-09-03 au matin, Rodolf signale que TROIS gestes livrés, prouvés par
 bancs et mutations, « ne fonctionnent pas ». Mesure faite avant de toucher
 une ligne de code :

     dernière exécution de l'INTERFACE  : 2026-09-01 22:28
     liste déroulante des catégories    : 2026-09-02 23:06
     rotation ouverte aux nœuds à enfants : 2026-09-03 01:15
     glisser-déposer depuis la palette  : 2026-09-03 02:08

 **Aucun des trois n'existait dans le binaire qu'il a lancé.** Il ne
 rapportait pas des défauts : il rapportait, exactement, ce qu'il voyait.

 C'était la cinquième fois. Les quatre premières ont été cherchées dans le
 code, où il n'y avait rien à trouver.

 🔑 CE QUE `verif_binaire.py` NE POUVAIT PAS VOIR. Il répond à « le binaire
    est-il plus récent que les sources ? » — une question sur NOTRE poste. La
    question qui manquait est autre : **« l'application a-t-elle été RELANCÉE
    depuis que ce geste existe ? »** Un binaire peut être parfaitement à jour
    et n'avoir jamais été ouvert. *Compiler n'est pas livrer ; livrer, c'est
    qu'il l'ait sous les yeux.*

=============================================================================
 CE QU'IL FAIT
=============================================================================
 Il compare la date du dernier JOURNAL D'EXÉCUTION de l'interface (les
 `logs/app_*.log` écrits à côté du binaire, donc par un lancement réel) à la
 date des commits qui touchent les sources de l'application, et nomme un par
 un les commits qu'aucune exécution n'a jamais portés à l'écran.

⚠️ CE QU'IL NE PEUT PAS FAIRE, et il faut le savoir pour ne pas s'y fier de
   travers :
   - il ne sait pas QUEL exécutable Rodolf double-clique ; il regarde tous
     les dossiers `*/NKUIDesign/logs` de tous les arbres de travail et prend
     le plus récent. Si Rodolf lance depuis un endroit qui n'écrit pas de
     journal, l'outil le dira « jamais lancé » — un faux positif honnête,
     préférable au faux négatif qu'on vient de payer cinq fois ;
   - un lancement ne prouve pas qu'il ait ESSAYÉ le geste. L'outil dit « il
     n'a pas pu le voir », jamais « il l'a vu ».

 Code de sortie : 0 si toute source livrée a été portée à l'écran au moins
 une fois ; 1 s'il reste des commits qu'aucune exécution n'a vus.
"""
from __future__ import print_function

# La console Windows est en cp1252 : sans ceci, un ✅ fait planter
# l'outil au moment ou il rend son verdict.

import datetime
import io
import os
import re
import subprocess
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

RACINE_ARBRES = u"D:/Projets/2026/Nkentseu"
# ⚠️ TROIS niveaux, pas deux. La premiere version en remontait deux :
#    `git log` s'executait dans `Applications/`, ne trouvait aucun commit,
#    et l'outil annoncait « tout est livre ». **Un faux vert dans l'outil
#    ecrit pour empecher les faux verts.** Trouve parce qu'il a plante
#    juste apres, sur l'encodage -- pas parce que je l'ai controle.
DEPOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCES = u"Applications/NKUIDesign/src"


def journaux_interface():
    u"""Tous les journaux ecrits PAR UN LANCEMENT de l'interface."""
    trouves = []
    for base, dossiers, fichiers in os.walk(RACINE_ARBRES):
        # On ne descend pas dans .git : rien n'y est un journal d'execution.
        dossiers[:] = [d for d in dossiers if d != u".git"]
        if not base.replace(u"\\", u"/").endswith(u"/NKUIDesign/logs"):
            continue
        for f in fichiers:
            if re.match(r"^app_\d{4}-\d{2}-\d{2}_\d{6}_\d+\.log$", f):
                p = os.path.join(base, f)
                try:
                    trouves.append((os.path.getmtime(p), p))
                except OSError:
                    pass
    return sorted(trouves, reverse=True)


def commits_des_sources(depuis_horodatage):
    u"""Les commits touchant les sources, plus recents que l'horodatage."""
    date = datetime.datetime.fromtimestamp(depuis_horodatage)
    cmd = [u"git", u"log", u"--format=%h|%ad|%s", u"--date=format:%Y-%m-%d %H:%M",
           u"--since", date.strftime(u"%Y-%m-%d %H:%M:%S"), u"--", SOURCES]
    try:
        sortie = subprocess.check_output(cmd, cwd=DEPOT)
    except (subprocess.CalledProcessError, OSError) as e:
        print(u"⚠️  git n'a pas répondu (%s) — l'outil ne peut pas juger." % e)
        sys.exit(2)
    if isinstance(sortie, bytes):
        sortie = sortie.decode(u"utf-8", u"replace")
    return [l for l in sortie.split(u"\n") if l.strip()]


def principal():
    print(u"verif_livraison.py — ce que Rodolf peut VOIR")
    print(u"=" * 70)
    logs = journaux_interface()
    if not logs:
        # ⚠️ ON NE REND PAS 0 ICI. « Aucun journal » ne veut pas dire « rien à
        #    signaler » : ça veut dire que l'instrument ne voit rien, et un
        #    instrument aveugle ne doit jamais rendre un vert.
        print(u"🔴 AUCUN journal de lancement trouvé nulle part.")
        print(u"   L'interface n'a peut-être jamais été ouverte, ou elle est")
        print(u"   lancée depuis un endroit qui n'écrit pas de journal.")
        print(u"   Dans les deux cas : RIEN ne prouve qu'un geste soit visible.")
        return 1

    horodatage, chemin = logs[0]
    quand = datetime.datetime.fromtimestamp(horodatage)
    print(u"Dernière ouverture de l'interface : %s"
          % quand.strftime(u"%Y-%m-%d %H:%M:%S"))
    print(u"   (%s)" % chemin.replace(u"\\", u"/"))
    print(u"")

    jamais_vus = commits_des_sources(horodatage)
    if not jamais_vus:
        print(u"✅ Tout ce qui est livré a été porté à l'écran au moins une fois.")
        print(u"   ⚠️  Cela ne dit PAS qu'il a essayé les gestes — seulement")
        print(u"      qu'il a PU les voir.")
        return 0

    print(u"🔴 %d commit(s) qu'AUCUNE exécution n'a jamais portés à l'écran :"
          % len(jamais_vus))
    print(u"")
    for ligne in jamais_vus:
        parts = ligne.split(u"|", 2)
        if len(parts) == 3:
            print(u"   %s  %s" % (parts[1], parts[2][:70]))
        else:
            print(u"   %s" % ligne[:88])
    print(u"")
    print(u"⚠️  Un défaut signalé sur CES gestes ne s'explique pas par le code :")
    print(u"    ils ne sont pas dans le binaire qu'il a lancé. Reconstruire ET")
    print(u"    relancer AVANT de chercher une cause dans les sources.")
    return 1


if __name__ == u"__main__":
    sys.exit(principal())
