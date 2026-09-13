#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verif_accents.py — les chaines d'INTERFACE portent-elles leurs accents ?

=============================================================================
 POURQUOI CET OUTIL EXISTE, ET C'EST UNE LECON AVANT D'ETRE UN SCRIPT
=============================================================================
 Le 2026-08-28, les chaines d'interface de NkUIDesign ont toutes ete
 accentuees d'un coup. Mesure faite le soir meme, sur la capture suivante :
 **les chaines ecrites APRES ce correctif etaient reparties sans accents.**

     « ce document est agence en colonnes et rangees »
     « le Launcher modal ... n'est pas encore branche »

 Le correctif avait traite l'ETAT, pas la SOURCE DU GESTE. Le reflexe d'ecrire
 sans accents — herite de la tolerance qui ne vaut que pour `echanges/` et les
 messages de commit — est reste intact. Sans controle, la prochaine chaine
 repartira sans accents, et personne ne le verra avant une capture.

=============================================================================
 CE QU'IL FAIT, ET CE QU'IL NE PEUT PAS FAIRE
=============================================================================
 ⚠️ IL NE CONNAIT PAS LE FRANCAIS, et il ne pretend pas le connaitre. Il ne
    porte aucun dictionnaire : **le depot est son propre dictionnaire.** Il
    collecte tous les mots ACCENTUES deja ecrits dans les sources, les
    « desaccentue », et signale toute chaine d'interface qui contient une de
    ces formes desaccentuees.

    Consequence a connaitre : un mot accentue que le depot n'ecrit NULLE PART
    ne sera pas trouve. L'outil ne dit donc pas « tout est correct » — il dit
    « voici ce qui contredit l'usage du depot ». C'est un signal, pas une
    preuve, et le mode `--liste` existe precisement pour la relecture humaine
    que ce signal ne remplace pas.

 ⚠️ LES HOMOGRAPHES SONT EXCLUS, ET ILS SONT LA MOITIE DU TRAVAIL. « cote » et
    « cote » (cote/coté), « tache » (tache/tache), « sur » (sur/sur), « du »,
    « ou », « mur », « pres », « a », « la » : la forme sans accent est un
    VRAI mot. Les signaler donnerait un outil qui crie a chaque ligne, donc un
    outil qu'on cesse de lancer. Un controle bruyant ne protege rien.

 ⚠️ UNE CHAINE QUI **CITE** UN DRAPEAU OU UNE CLE NE S'ACCENTUE JAMAIS, et
    c'est la lecon la plus chere du balayage du 28/08. La passe automatique a
    transforme le texte d'aide

        --pool-controles        les temoins du pool de chaines
        --theme=<nom>           theme au lancement

    en `--pool-contrôles` et `--thème=<nom>`. Les accents etaient justes ; les
    DRAPEAUX, eux, s'appellent toujours `--pool-controles` et `--theme=`.
    L'aide s'etait mise a decrire un programme qui n'existe pas, et un
    utilisateur qui la recopie recoit « drapeau inconnu ». **Une aide qui ment
    est pire qu'une aide sans accents.** Meme famille pour un titre de panneau
    cite par `FocusPanel("...")` : accentuer un cote sans l'autre casse le lien.
    → Corollaire heureux, mesure le meme jour : `FocusPanel("Hierarchie")`
      cherchait un panneau intitule « Hiérarchie ». Le lien etait DEJA rompu, et
      `Ctrl+J` journalisait `FocusPanel -> FAUX` sans que personne le lise. Une
      chaine qui sert de CLE doit se lire des deux cotes a la fois.

 ⚠️ IL NE REGARDE QUE LES CHAINES D'INTERFACE, jamais les commentaires ni les
    identifiants. Une chaine est retenue si elle contient un ESPACE et au moins
    six caracteres : un libelle, pas un nom de cle, pas un chemin, pas un
    format. Les chaines qui commencent par `--` (drapeaux) et celles qui
    ressemblent a un chemin sont ecartees.

=============================================================================
 USAGE
=============================================================================
   python Tools/verif_accents.py <fichier|dossier> [...]      -> controle
   python Tools/verif_accents.py --liste <fichier|dossier>    -> toutes les
                                                                 chaines, a relire

 Code de sortie : 0 si rien n'est signale, 1 sinon. Il peut donc servir de
 garde-fou dans un enchainement.
"""

import os
import re
import sys
import unicodedata

LITTERALE = re.compile(r'"((?:[^"\\]|\\.)*)"')
MOT = re.compile(r"[A-Za-zÀ-ſ]+")

EXTENSIONS = (".h", ".hpp", ".inl", ".cpp", ".cc")

# ⚠️ Formes SANS accent qui sont de VRAIS mots francais : jamais signalees.
#    Sans cette liste, l'outil crie sur « du », « ou », « la », « a », « sur »
#    a chaque ligne -- et devient un outil qu'on n'ouvre plus.
HOMOGRAPHES = {
    "a", "la", "ou", "du", "sur", "mur", "mure", "cote", "cotes", "tache",
    "taches", "pres", "des", "les", "ces", "ses", "cet", "cette", "notre",
    "votre", "pecheur", "role", "roles", "mode", "modes", "cause", "pose",
    "pate", "jeune", "foret", "interne", "externe", "site", "cine", "note",
    "notes", "sale", "date", "dates", "page", "pages", "cadre", "cadres",
    "arbre", "arbres", "titre", "titres", "ordre", "ordres", "libre",
    "libres", "fibre", "type", "types", "liste", "listes", "texte", "textes",
    "image", "images", "table", "tables", "case", "cases", "ligne", "lignes",
    "forme", "formes", "somme", "pomme", "gomme", "comme", "homme", "femme",
    "programme", "gramme", "trame", "lame", "dame", "same",
    # Faux positifs mesures le 28/08, avec leur raison :
    "version",  # « versión » traine quelque part dans le depot (espagnol)
    "cibles",   # « cibles » (noms) vs « ciblés » (participe) : les deux vivent
    "poses",    # « tu poses » vs « posés »
    "branches", # branches d'arbre vs « branchés »
    "decide", "cree", "demande", "invalide", "existe", "livre", "registre",
    "applique", "commande", "source", "place", "nomme", "modifie", "conserve",
    "vide", "rejoue", "forcee", "autorise", "refuse", "edite",
}


def sans_accent(mot):
    return "".join(c for c in unicodedata.normalize("NFD", mot)
                   if unicodedata.category(c) != "Mn")


def fichiers_source(racines):
    """Tous les fichiers source sous les racines donnees, fichiers compris."""
    vus = []
    for r in racines:
        if os.path.isfile(r):
            vus.append(r)
            continue
        for dossier, sous, noms in os.walk(r):
            # Les repertoires de sortie ne sont pas des sources.
            sous[:] = [d for d in sous if d not in ("Build", "Externals", ".git", "dist")]
            for n in noms:
                if n.endswith(EXTENSIONS):
                    vus.append(os.path.join(dossier, n))
    return vus


def lexique_accentue(fichiers):
    """LE DEPOT EST SON PROPRE DICTIONNAIRE.

    Rend {forme_sans_accent: {formes accentuees rencontrees}}. On ne retient
    que les mots d'au moins quatre lettres : en dessous, la proportion
    d'homographes explose (`ete`, `age`, `ane`...).
    """
    lex, maj = {}, {}
    for f in fichiers:
        try:
            texte = open(f, encoding="utf-8").read()
        except (UnicodeDecodeError, OSError):
            continue
        for mot in MOT.findall(texte):
            nu = sans_accent(mot)
            if nu == mot or len(mot) < 4:
                continue
            lex.setdefault(nu.lower(), set()).add(mot.lower())
            if mot[0].isupper():
                maj[nu.lower()] = True
    return lex, maj


def chaines_interface(texte):
    """Les litterales qui ressemblent a un LIBELLE, pas a une cle technique."""
    for m in LITTERALE.finditer(texte):
        t = m.group(1)
        if len(t) < 6 or " " not in t:
            continue
        if t.startswith("--") or t.startswith("#"):
            continue
        # ⚠️ UN CHEMIN, PAS UNE PHRASE : « a/b » SANS espaces autour du separateur.
        #    La premiere regle etait « contient un / » -- et elle ecartait
        #    « Vierge / Gabarit / IA », c'est-a-dire une des DEUX chaines que ce
        #    controle devait justement attraper. Un filtre trop large ne fait pas
        #    de bruit : il fait un SILENCE, et un silence ressemble a un succes.
        if re.search(r"\S/\S", t):
            continue
        yield m.start(), t


def main(argv):
    # ⚠️ LA CONSOLE WINDOWS EST EN CP1252, ET L'OUTIL ECRIT DU FRANCAIS. Sans
    #    cette ligne, le rapport s'interrompait sur un UnicodeEncodeError APRES
    #    avoir imprime ses resultats : le controle avait travaille, et sortait
    #    quand meme en erreur. Un outil qui plante a la derniere ligne se lit
    #    comme un outil casse.
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError):
        pass

    liste_seule = "--liste" in argv
    racines = [a for a in argv[1:] if not a.startswith("--")]
    if not racines:
        print(__doc__)
        return 2

    cibles = fichiers_source(racines)
    if not cibles:
        print("Aucun fichier source sous : %s" % ", ".join(racines))
        return 2

    # Le lexique se construit sur TOUT le depot, pas sur les seules cibles :
    # une application ecrit peu, le depot ecrit beaucoup.
    racine_depot = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    lex, majuscules = lexique_accentue(fichiers_source([
        os.path.join(racine_depot, "Applications"),
        os.path.join(racine_depot, "Engine"),
        os.path.join(racine_depot, "Kernel"),
    ]))

    signales = 0
    for f in cibles:
        try:
            texte = open(f, encoding="utf-8").read()
        except (UnicodeDecodeError, OSError):
            continue
        entetes = False
        for pos, t in chaines_interface(texte):
            ligne = texte[:pos].count("\n") + 1
            if liste_seule:
                if not entetes:
                    print("\n== %s" % os.path.relpath(f, racine_depot))
                    entetes = True
                print("  %5d  %s" % (ligne, t))
                continue
            fautifs = []
            # ⚠️ UN JETON QUI COMMENCE PAR `--` EST UN NOM DE DRAPEAU, PAS UN MOT.
            #    Il est retire AVANT l'analyse : l'aide qui decrit `--theme=<nom>`
            #    doit garder ce nom tel quel, sinon elle decrit un programme qui
            #    n'existe pas. Sans cette ligne, le controle reclamait
            #    eternellement deux corrections qu'il ne fallait PAS faire -- et
            #    un controle qui a tort deux fois de suite cesse d'etre lu.
            analysable = " ".join(j for j in t.split() if not j.startswith("--"))
            for mot in MOT.findall(analysable):
                bas = mot.lower()
                if bas in HOMOGRAPHES or len(mot) < 4:
                    continue
                if sans_accent(mot) != mot:
                    continue  # deja accentue
                variantes = lex.get(bas)
                if not variantes:
                    continue
                # ⚠️ L'ACCENT SUR LA DERNIERE LETTRE NE COMPTE PAS, et cette regle
                #    supprime a elle seule la grande majorite des faux positifs.
                #    « modifie / modifié », « place / placé », « commande /
                #    commandé », « existe / existé » : la forme nue est le verbe
                #    conjugue ou le substantif, un VRAI mot. Seul un accent place
                #    AVANT la derniere lettre trahit une forme qui n'existe pas
                #    sans lui — « detection », « demarrage », « reponse ».
                #    📌 Limite assumee, et il faut la connaitre : « agence » pour
                #       « agencé » passe donc au travers. La chaine qui le portait
                #       est quand meme signalee, par « rangees / rangées » qui s'y
                #       trouve. Un controle qui attrape la CHAINE a fait son
                #       travail ; c'est l'humain qui relit les mots.
                interessantes = [v for v in variantes
                                 if any(sans_accent(c) != c for c in v[:-1])]
                if not interessantes:
                    continue
                # Un nom propre ou un nom d'API (« Metal ») n'est pas « métal ».
                # On n'accuse une Majuscule que si le depot ecrit AUSSI la forme
                # accentuee avec une majuscule.
                if mot[0].isupper() and not majuscules.get(bas):
                    continue
                fautifs.append("%s -> %s" % (mot, "/".join(sorted(interessantes))))
            if fautifs:
                if not entetes:
                    print("\n== %s" % os.path.relpath(f, racine_depot))
                    entetes = True
                print("  %5d  %s" % (ligne, t[:100]))
                for x in fautifs:
                    print("         %s" % x)
                signales += 1

    if liste_seule:
        return 0
    if signales:
        print("\n%d chaine(s) d'interface a relire." % signales)
        print("⚠️ Ce sont des SIGNAUX, pas des verdicts : l'outil compare a l'usage")
        print("   du depot, il ne connait pas le francais. Un faux positif se")
        print("   corrige en ajoutant le mot a HOMOGRAPHES, avec sa raison.")
        return 1
    print("Aucune chaine d'interface signalee (%d fichier(s))." % len(cibles))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
