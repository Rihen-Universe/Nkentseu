# -*- coding: utf-8 -*-
"""REFUSE DE JUGER un binaire plus vieux que ses sources.

⚠️ POURQUOI CE FICHIER EXISTE — deux verdicts faux LE MEME JOUR (2026-09-02),
   dans les deux sens, avec le meme mecanisme :

   1. le FAUX VERT : une mutation qui ne compilait pas a laisse l'ancien exe en
      place, et il a affiche « 26/26 PROUVEE » pendant que la source etait
      cassee — la mutation semblait morte, elle n'avait jamais couru ;
   2. le FAUX ROUGE : la mutation revertee mais PAS recompilee, et le balayage
      complet a affiche « 26/27 EN ECHEC » sur du code deja repare.

   Le sujet mesure se verifie par un TEMOIN — ici l'horodatage — jamais par la
   confiance. Ce script est le pas 0 de tout balayage de gardes : si l'exe est
   plus vieux que la plus recente source compilable, il ne dit pas « rebatis »,
   il dit « JE REFUSE DE JUGER » avec le fichier fautif, et sort en code 2.

Usage :  python verif_binaire.py            (depuis Applications/NKUIDesign)
         python verif_binaire.py --config Release
Sortie : 0 = le binaire est plus recent que toute source, on peut juger ;
         2 = binaire perime (le fichier plus recent est nomme) ;
         3 = binaire introuvable.
"""
import os
import sys
import io

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

RACINE = os.path.abspath(os.path.join(os.path.dirname(__file__), u"..", u".."))
CONFIG = u"Debug"
for i, a in enumerate(sys.argv):
    if a == u"--config" and i + 1 < len(sys.argv):
        CONFIG = sys.argv[i + 1]

EXE = os.path.join(RACINE, u"Build", u"Bin", CONFIG + u"-Windows", u"NKUIDesign",
                   u"NKUIDesign.exe")

# Les racines de SOURCES COMPILABLES seulement : un document de design modifie
# ne perime pas le binaire. Les trois etages, parce que le binaire les lie tous.
SOURCES = [
    os.path.join(RACINE, u"Applications", u"NKUIDesign", u"src"),
    os.path.join(RACINE, u"Engine", u"NKEditorKit", u"src"),
    os.path.join(RACINE, u"Kernel"),
]
EXTENSIONS = (u".h", u".hpp", u".c", u".cpp", u".inl")


def plus_recente():
    pire = (0.0, None)
    for base in SOURCES:
        for dossier, _, fichiers in os.walk(base):
            for f in fichiers:
                if not f.lower().endswith(EXTENSIONS):
                    continue
                chemin = os.path.join(dossier, f)
                try:
                    t = os.path.getmtime(chemin)
                except OSError:
                    continue
                if t > pire[0]:
                    pire = (t, chemin)
    return pire


# 🔴 ELLE NE SURVEILLAIT QU'UN BINAIRE, ET C'EST LA MOITIE DES CAS (02/09).
#    Rodolf a signale un plantage deja corrige ; la premiere piste fut le
#    Release, date de la VEILLE, pendant que le Debug etait a jour. La garde,
#    elle, ne regardait que le Debug -- elle disait donc « a jour » sur un
#    dossier ou dormait un executable d'hier.
#    ⚠️ *Une garde qui surveille un seul des deux livrables laisse passer
#       exactement la moitie des cas* -- et c'est le livrable qu'on N'A PAS
#       construit qui part chez l'utilisateur, puisque c'est celui qu'on oublie.
#    Elle les liste desormais TOUS LES DEUX : celui qu'on juge doit etre a
#    jour, et l'AUTRE est signale s'il est perime, sans faire echouer (on ne
#    reconstruit pas le Release a chaque essai du Debug -- mais on sait).
AUTRES = [u"Debug", u"Release"]


def exe_de(config):
    return os.path.join(RACINE, u"Build", u"Bin", config + u"-Windows", u"NKUIDesign",
                        u"NKUIDesign.exe")


def principal():
    if not os.path.isfile(EXE):
        print(u"binaire INTROUVABLE : %s" % EXE)
        return 3
    t_exe = os.path.getmtime(EXE)
    t_src, chemin = plus_recente()
    if chemin is not None and t_src > t_exe:
        print(u"REFUS DE JUGER : le binaire %s est PERIME de %.0f s." % (CONFIG, t_src - t_exe))
        print(u"   source plus recente : %s" % os.path.relpath(chemin, RACINE))
        print(u"   rebatir d'abord — un verdict lu maintenant serait celui d'un autre code.")
        return 2
    print(u"binaire %s a jour (plus recent que toute source de %.0f s)." % (CONFIG,
                                                                           t_exe - t_src))
    # L'AUTRE livrable : signale, jamais fatal.
    for c in AUTRES:
        if c == CONFIG:
            continue
        e = exe_de(c)
        if not os.path.isfile(e):
            print(u"   ⚠️  %s : ABSENT — si Rodolf lance celui-la, il ne lance rien de toi." % c)
        elif os.path.getmtime(e) < t_src:
            ecart = (t_src - os.path.getmtime(e)) / 3600.0
            print(u"   ⚠️  %s : PERIME de %.1f h — *un correctif compile mais non deploye est"
                  % (c, ecart))
            print(u"       indistinguable d'un correctif absent pour celui qui teste.*")
        else:
            print(u"   %s : a jour aussi." % c)
    return 0


if __name__ == u"__main__":
    sys.exit(principal())
