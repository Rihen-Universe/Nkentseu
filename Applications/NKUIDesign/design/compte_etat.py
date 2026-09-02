# -*- coding: utf-8 -*-
"""Compte la COLONNE D'ETAT de `13_Lunacy_reference_interaction.md`.

⚠️ POURQUOI CE FICHIER EXISTE PLUTOT QU'UNE LIGNE DE `grep`.

Le compte de la colonne d'etat est la reponse a la question que Rodolf pose le
plus souvent (« as-tu tout porte ? »). Il a ete faux DEUX FOIS le 02/09, et les
deux fois dans le sens qui l'arrangeait -- c'est-a-dire en annoncant plus de vert
qu'il n'y en avait :

  1. le tableau du compteur portait les emblemes dans SES PROPRES en-tetes, donc
     il se comptait lui-meme ;
  2. la ligne de `grep` comptait les emblemes, pas les LIGNES : deux cellules
     « ce qui manque » ou la prose dit « ce ✅ etait faux » ajoutaient chacune un
     faux vert. Et la legende de §0.4 -- quatre lignes qui DEFINISSENT les quatre
     etats -- etait comptee comme du contenu.

*Un instrument de mesure qui figure dans sa propre mesure ment toujours en sa
faveur.* La parade n'est pas de faire attention : c'est de mettre le comptage
dans un outil qui sait ce qu'il ne doit pas compter.

REGLES, et chacune repare une erreur reelle :
  - une LIGNE de tableau compte pour UNE ligne, quel que soit le nombre
    d'emblemes qu'elle contient ; l'etat est le PREMIER embleme rencontre, celui
    de la colonne d'etat ;
  - la table de LEGENDE de §0.4 est exclue : elle definit le vocabulaire, elle ne
    decrit aucun comportement ;
  - les lignes de separation (`|---|`) et les en-tetes ne comptent pas.

Usage :  python compte_etat.py  [--chapitres]
"""
import io
import os
import re
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
CIBLE = os.path.join(ICI, "13_Lunacy_reference_interaction.md")
INVENTAIRE = os.path.join(ICI, "14_Etat_conformite.md")

LIVRE, PARTIEL, ABSENT, ECARTE = u"\u2705", u"\U0001f7e1", u"\u274c", u"\U0001f6ab"
EMBLEMES = (LIVRE, PARTIEL, ABSENT, ECARTE)

MARQUE_DEBUT = (u"<!-- AUTO:DEBUT \u2014 regenere par `compte_etat.py --document`. "
                u"NE RIEN ECRIRE ENTRE LES DEUX MARQUEURS. -->")
MARQUE_FIN = u"<!-- AUTO:FIN -->"

# La legende de §0.4 : ses quatre lignes sont exactement « | <embleme> **mot** | … ».
# On les reconnait a ce qu'elles n'ont que DEUX cellules et que la premiere
# commence par l'embleme -- une ligne de comportement, elle, en a quatre.
def est_legende(cellules):
    return len(cellules) == 2 and cellules[0][:1] in EMBLEMES


# 🔴 LE MARQUEUR D'OPT-OUT, ET POURQUOI AUCUNE HEURISTIQUE NE SUFFIT (02/09).
#    Une table de DESAMBIGUISATION (les trois sens de « miroir ») a exactement
#    la meme forme qu'une table de comportements : quatre cellules, un embleme
#    d'etat dans la derniere. Elle s'est donc comptee comme TROIS comportements
#    livres -- le total est passe de 174 a 177 sans qu'une ligne de code change.
#    ⚠️ *Un document qui explique le VOCABULAIRE n'est pas un document qui
#       decrit un COMPORTEMENT.* Aucune regle de forme ne peut les separer :
#       il faut que l'auteur le dise. D'ou ce marqueur, pose sur la ligne qui
#       precede la table :   <!-- PAS-UN-COMPORTEMENT -->
#    Les lignes de table qui suivent sont ignorees jusqu'a la fin du bloc.
# 🔴 REGLE D'ECRITURE, PAYEE DEUX FOIS : LE MARQUEUR SE COLLE A LA TABLE,
#    sur la ligne IMMEDIATEMENT au-dessus de son en-tete.
#    Pose deux lignes plus haut -- avant un titre, un paragraphe, une ligne
#    vide suivie de texte -- il est REFERME par la premiere ligne non-table,
#    et la table se compte quand meme. C'est arrive le 02/09 (table des trois
#    miroirs, 174 -> 177) puis le 03/09 (table des sommets, 174 -> 180) :
#    **les deux fois le total a monte sans qu'une ligne de code change.**
#    ⚠️ LA FERMETURE AUTOMATIQUE N'EST PAS LE DEFAUT -- c'est elle qui
#       empeche une trappe ouverte d'avaler la fin du fichier. Le defaut est
#       de l'oublier EN ECRIVANT le document, d'ou cette regle ici, a
#       l'endroit exact ou on vient chercher le nom du marqueur.
MARQUE_HORS_COMPTE = u"<!-- PAS-UN-COMPORTEMENT -->"


def compter(chemin):
    with io.open(chemin, "r", encoding="utf-8") as f:
        lignes = f.read().split("\n")
    chapitre = u"(hors chapitre)"
    ordre, par_chap, detail = [], {}, {}
    hors_compte = False
    for L in lignes:
        if MARQUE_HORS_COMPTE in L:
            hors_compte = True
            continue
        m = re.match(r"^## (.+)$", L)
        if m:
            chapitre = m.group(1).strip()
            hors_compte = False
            continue
        if not L.startswith(u"|"):
            # Une ligne NON-table referme le bloc marque : le marqueur ne vaut
            # que pour la table qui le suit immediatement, jamais pour la fin
            # du fichier -- une trappe qui reste ouverte finit par tout avaler.
            if L.strip():
                hors_compte = False
            continue
        if hors_compte:
            continue
        if re.match(r"^\|[\s:\-|]+\|?\s*$", L):
            continue  # ligne de separation
        cellules = [c.strip() for c in L.strip().strip(u"|").split(u"|")]
        embl = [c for c in L if c in EMBLEMES]
        if not embl or est_legende(cellules):
            continue
        if chapitre not in par_chap:
            par_chap[chapitre] = {e: 0 for e in EMBLEMES}
            detail[chapitre] = {e: [] for e in EMBLEMES}
            ordre.append(chapitre)
        e = embl[0]
        par_chap[chapitre][e] += 1
        detail[chapitre][e].append(cellules[0].replace(u"**", u"") if cellules else u"?")
    return ordre, par_chap, detail


def bloc_auto(ordre, par_chap, detail):
    """La partie CHIFFREE de l'inventaire : que des nombres et des libelles lus
    dans le document 13. Aucun jugement ici -- donc rien qui puisse vieillir en
    silence quand la colonne d'etat bouge."""
    t = {e: 0 for e in EMBLEMES}
    for d in par_chap.values():
        for e in EMBLEMES:
            t[e] += d[e]
    n = sum(t.values())
    L = [u"### Le compte global", u"",
         u"| lignes de comportement | livré | partiel | absent | écarté |",
         u"|---|---|---|---|---|",
         u"| **%d** | **%d** | **%d** | **%d** | **%d** |"
         % (n, t[LIVRE], t[PARTIEL], t[ABSENT], t[ECARTE]), u"",
         u"### Par chapitre — où l'on est fort, où l'on est faible", u"",
         u"| chapitre | livré | partiel | absent | écarté |", u"|---|---|---|---|---|"]
    for c in ordre:
        d = par_chap[c]
        L.append(u"| %s | %d | %d | %d | %d |"
                 % (c.split(u"—")[0].strip(), d[LIVRE], d[PARTIEL], d[ABSENT], d[ECARTE]))
    L.append(u"")
    for nom, e in ((u"PARTIELS", PARTIEL), (u"ABSENTS", ABSENT), (u"ÉCARTÉS", ECARTE)):
        L += [u"### La liste nommée — %s" % nom, u""]
        vide = True
        for c in ordre:
            if not detail[c][e]:
                continue
            vide = False
            L += [u"**%s**" % c.split(u"—")[0].strip(), u""]
            L += [u"- %s" % a for a in detail[c][e]]
            L.append(u"")
        if vide:
            L += [u"*(aucun)*", u""]
    return u"\n".join(L)


def regenerer(ordre, par_chap, detail):
    if not os.path.exists(INVENTAIRE):
        sys.stderr.write("14_Etat_conformite.md est absent : ecris-le d'abord, "
                         "avec ses deux marqueurs AUTO.\n")
        return 2
    with io.open(INVENTAIRE, "r", encoding="utf-8", newline="") as f:
        doc = f.read()
    i, j = doc.find(MARQUE_DEBUT), doc.find(MARQUE_FIN)
    if i < 0 or j < 0 or j < i:
        sys.stderr.write("marqueurs AUTO introuvables ou inverses : rien n'est ecrit.\n")
        return 2
    avant, apres = doc[:i + len(MARQUE_DEBUT)], doc[j:]
    neuf = avant + u"\n\n" + bloc_auto(ordre, par_chap, detail) + u"\n" + apres
    # ⚠️ ON VERIFIE AVANT D'ECRIRE. La prose qui entoure le bloc est du JUGEMENT
    #    ecrit a la main : si elle ne se retrouvait pas intacte, on detruirait le
    #    seul contenu que ce script ne sait pas refabriquer.
    if not neuf.startswith(avant) or not neuf.endswith(apres):
        sys.stderr.write("la prose autour du bloc n'est pas intacte : rien n'est ecrit.\n")
        return 2
    with io.open(INVENTAIRE, "w", encoding="utf-8", newline="") as f:
        f.write(neuf)
    with io.open(INVENTAIRE, "r", encoding="utf-8", newline="") as f:
        relu = f.read()
    ok = relu == neuf and MARQUE_DEBUT in relu and MARQUE_FIN in relu
    # on relit le disque : « ok » ne se declare pas.
    sys.stdout.write(u"14_Etat_conformite.md : %d octets ; marqueurs presents=%s ; "
                     u"relu identique=%s\n"
                     % (len(relu), MARQUE_DEBUT in relu and MARQUE_FIN in relu, relu == neuf))
    return 0 if ok else 2


def main():
    ordre, par_chap, detail = compter(CIBLE)
    if "--document" in sys.argv:
        return regenerer(ordre, par_chap, detail)
    tot = {e: 0 for e in EMBLEMES}
    detaille = "--chapitres" in sys.argv
    for c in ordre:
        d = par_chap[c]
        for e in EMBLEMES:
            tot[e] += d[e]
        print(u"%-56s %3d livre %3d partiel %3d absent %2d ecarte"
              % (c[:56], d[LIVRE], d[PARTIEL], d[ABSENT], d[ECARTE]))
    n = sum(tot.values())
    print(u"%-56s %3d        %3d         %3d        %2d   (total %d)"
          % (u"TOTAL", tot[LIVRE], tot[PARTIEL], tot[ABSENT], tot[ECARTE], n))
    if detaille:
        for c in ordre:
            for e, nom in ((ABSENT, u"ABSENT"), (PARTIEL, u"PARTIEL")):
                if detail[c][e]:
                    print(u"\n-- %s / %s" % (c[:60], nom))
                    for a in detail[c][e]:
                        print(u"     %s" % a[:100])
    return 0


if __name__ == "__main__":
    if sys.stdout.encoding and sys.stdout.encoding.lower() not in ("utf-8", "utf8"):
        sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main())
