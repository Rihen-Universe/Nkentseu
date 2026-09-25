#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

LE CRITERE « UNE SONDE NE LAISSE RIEN DERRIERE ELLE »
=====================================================

*Une sonde qui pollue l'etat de l'utilisateur est une sonde qui modifie ce
qu'elle mesure.* Mesure du 25/09 : la liste des projets recents de Rodolf portait
**105 entrees**, dont la plupart etaient des coquilles creees par nos propres
courses.

⚠️ CE CRITERE NE FAIT PAS CONFIANCE AU RECENSEMENT DES SITES D'ECRITURE, ET
   C'EST TOUT SON INTERET. On peut chercher dans le code tous les endroits qui
   ecrivent chez l'utilisateur et en oublier un -- c'est meme le cas le plus
   probable. Celui-ci mesure le RESULTAT : il photographie l'etat AVANT, lance la
   course, et compare APRES. Un site d'ecriture que personne n'a recense rougit
   quand meme.

Usage :
    sonde_etat_utilisateur.py --avant <cliche.json>
    ... lancer la course ...
    sonde_etat_utilisateur.py --apres <cliche.json>

Sortie : VERT si rien n'a bouge, ROUGE avec le nom de CHAQUE fichier touche, et
un code de sortie non nul -- un critere sans pouvoir d'arret est une decoration.
"""

import hashlib
import json
import os
import sys

# ── CE QUI APPARTIENT A RODOLF, ET QU'UNE COURSE NE DOIT PAS TOUCHER ─────────
# Chaque entree porte son application et sa PORTE de redirection, pour que le
# rouge dise quoi faire au lieu de seulement dire que c'est casse.
ETATS = [
    ("~/.nk3dmodeler_recent.cfg",        "NK3DModeler", "NK_RECENTS"),
    ("~/.nkcode_recent.cfg",             "NKCode",      "(aucune porte -- a ecrire)"),
    ("~/.nkcode_recent_names.cfg",       "NKCode",      "(aucune porte -- a ecrire)"),
    ("~/.nkcode/window.cfg",             "NKCode",      "(aucune porte -- a ecrire)"),
    ("~/AppData/Roaming/NK3DModeler",    "NK3DModeler", "(themes utilisateur)"),
]

# Les etats RELATIFS a l'arbre de travail : ils ne sont pas « chez Rodolf » au
# sens du profil, mais ils sont a lui quand il lance l'application lui-meme.
ETATS_ARBRE = [
    ("logs/nkuidesign_ui.cfg",           "NKUIDesign",  "NK_UI_ETAT"),
    ("logs/nk3dmodeler_ia_chats.txt",    "NK3DModeler", "NK_AI_CHATS (+ garde NK_SONDE)"),
    ("logs/nkuidesign_ia_chats.txt",     "NKUIDesign",  "NK_AI_CHATS (+ garde NK_SONDE)"),
]


def empreinte(chemin: str):
    """Taille + date + empreinte du contenu pour un fichier ; pour un dossier, le
    nombre d'entrees et la date la plus recente. `None` si absent -- une absence
    est un etat comme un autre, et sa DISPARITION doit rougir."""
    if not os.path.exists(chemin):
        return None
    if os.path.isdir(chemin):
        n, recent = 0, 0.0
        for r, _d, f in os.walk(chemin):
            for x in f:
                n += 1
                try:
                    recent = max(recent, os.path.getmtime(os.path.join(r, x)))
                except OSError:
                    pass
        return {"type": "dossier", "entrees": n, "recent": int(recent)}
    st = os.stat(chemin)
    h = hashlib.sha256()
    try:
        with open(chemin, "rb") as f:
            for bloc in iter(lambda: f.read(65536), b""):
                h.update(bloc)
    except OSError:
        return {"type": "illisible"}
    # ⚠️ L'EMPREINTE DU CONTENU, PAS SEULEMENT LA DATE. Une application qui
    #    reecrit le MEME contenu a change la date sans rien changer : le dire
    #    « rouge » serait du bruit, et le bruit fait qu'on cesse de lire.
    return {"type": "fichier", "taille": st.st_size, "sha": h.hexdigest()[:16]}


def cliche(racine: str):
    maison = os.path.expanduser("~")
    out = {}
    for rel, app, porte in ETATS:
        chemin = rel.replace("~", maison)
        out[chemin] = {"app": app, "porte": porte, "etat": empreinte(chemin)}
    for rel, app, porte in ETATS_ARBRE:
        chemin = os.path.join(racine, rel)
        out[chemin] = {"app": app, "porte": porte, "etat": empreinte(chemin)}
    return out


def main() -> int:
    if len(sys.argv) < 3:
        print("[etat] REFUS : usage --avant|--apres <cliche.json> [racine]")
        return 2
    mode, fichier = sys.argv[1], sys.argv[2]
    racine = sys.argv[3] if len(sys.argv) > 3 else os.getcwd()
    actuel = cliche(racine)

    if mode == "--avant":
        with open(fichier, "w", encoding="utf-8") as f:
            json.dump(actuel, f)
        print("[etat] cliche AVANT : %d etat(s) photographie(s)" % len(actuel))
        return 0

    if mode != "--apres":
        print("[etat] REFUS : mode inconnu " + mode)
        return 2

    try:
        with open(fichier, encoding="utf-8") as f:
            avant = json.load(f)
    except OSError:
        print("[etat] REFUS : cliche AVANT introuvable (" + fichier + ")")
        return 2

    touches = []
    for chemin, cur in actuel.items():
        vieux = avant.get(chemin, {}).get("etat")
        if vieux != cur["etat"]:
            touches.append((chemin, cur["app"], cur["porte"], vieux, cur["etat"]))

    if not touches:
        print("[etat] VERT : la course n'a touche AUCUN etat de l'utilisateur "
              "(%d surveille(s))." % len(actuel))
        return 0

    print("[etat] ROUGE : %d etat(s) de l'utilisateur MODIFIE(S) par la course." % len(touches))
    for chemin, app, porte, vieux, neuf in touches:
        print("[etat]   %s" % chemin)
        print("[etat]     application : %s ; porte de redirection : %s" % (app, porte))
        print("[etat]     avant = %s" % (vieux,))
        print("[etat]     apres = %s" % (neuf,))
    print("[etat] Une sonde qui pollue l'etat de l'utilisateur est une sonde qui")
    print("[etat] modifie ce qu'elle mesure.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
