# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/mesure_formulaire.py
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# LE FORMULAIRE D'UNE FAMILLE, MESURE SUR LE MODELE LOCAL, SANS L'APPLICATION.
#
# Ce que ca mesure, et rien d'autre : etant donne le TEXTE d'invite que
# `NkCreaEcrireFormulaire` envoie, que repond le 7B ? Deux criteres, ECRITS
# AVANT la premiere course (22/09, cf. modelisation-ia.reponses.md) :
#   C_A  la valeur ecrite dans un champ enumere est l'une des valeurs permises
#        (aucune correction par defaut n'est necessaire) ;
#   C_B  la valeur est PLAUSIBLE pour l'objet demande -- l'ensemble acceptable
#        est ecrit ici, objet par objet, AVANT de lancer quoi que ce soit.
#
# SEUILS, ECRITS AVANT LA PREMIERE COURSE (3 objets x 3 repetitions = 9) :
#   le correctif n'est retenu que si, apres, C_A >= 8/9 ET C_B >= 6/9,
#   et qu'aucun des deux ne BAISSE par rapport a l'avant.
#
# ⚠️ CE JUGE NE LIT PAS L'APPLICATION. Il rejoue l'invite telle quelle. Un
#    formulaire mesure a travers l'application melangerait le modele, la lecture
#    du document, la correction par defaut et la pose. Ici on isole l'invite.
#
# USAGE : python mesure_formulaire.py --invite AVANT|APRES --repetitions 3
# -----------------------------------------------------------------------------
import argparse
import json
import re
import sys
import time
import urllib.request

for _f in (sys.stdout, sys.stderr):
    try:
        _f.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

OLLAMA = "http://127.0.0.1:11434/api/generate"

PERMISES = ["droit", "evase", "ventru", "goulot", "coupe", "dome", "conique", "colonne"]

# ── L'ENSEMBLE ACCEPTABLE, ECRIT AVANT LA PREMIERE COURSE ────────────────────
# Il dit ce qu'une silhouette PLAUSIBLE serait pour chaque objet du jeu
# revolution (Tools/Genia/epreuve_revolution.txt). Il n'est pas « la bonne
# reponse » : plusieurs formes conviennent, et c'est le point.
JEU = [
    dict(cle="verre", demande="modelise moi un verre de table",
         ok={"droit", "evase", "coupe"}, H=(0.07, 0.25), L=(0.05, 0.12)),
    dict(cle="vase", demande="un vase a fleurs",
         ok={"ventru", "goulot", "evase"}, H=(0.15, 0.80), L=(0.08, 0.50)),
    dict(cle="bol", demande="un bol",
         ok={"coupe", "evase"}, H=(0.04, 0.15), L=(0.10, 0.30)),
]

# ── LES DEUX INVITES, RECOPIEES DU C++ AU CARACTERE PRES ─────────────────────
LIGNE_AVANT = ("famille revolution silhouette <droit|evase|ventru|goulot|coupe|dome|conique|colonne> "
               "largeur <metres> hauteur <metres> paroi <metres, 0 si plein>")
AIDE_AVANT = ("largeur et hauteur REELLES de l'objet (un verre : 0.08 x 0.12, paroi 0.003 ; "
              "un bol : 0.16 x 0.08, paroi 0.004 ; un vase : 0.2 x 0.35, paroi 0.006).")

# APRES : la meme ligne, plus l'EFFET de chaque silhouette -- la seule chose que
# le modele n'avait pas. Les effets sont recopies de NkCreaSilhouettes.
EFFETS = [
    ("droit", "flancs verticaux"),
    ("evase", "s'elargit vers le haut"),
    ("ventru", "renfle au milieu, col plus etroit"),
    ("goulot", "corps droit puis long col etroit"),
    ("coupe", "petit pied puis flancs arrondis, large ouverture"),
    ("dome", "arrondi, se referme en haut"),
    ("conique", "se retrecit en pointe"),
    ("colonne", "base et chapiteau plus larges qu'un fut droit"),
]
# u du DERNIER couple de NkCreaSilhouettes : sous 0,25 la forme se REFERME en
# haut, donc elle ne peut pas etre un recipient. Derive de la table, pas recopie.
U_HAUT = dict(droit=1.0, evase=1.0, ventru=0.58, goulot=0.30, coupe=1.0, dome=0.05,
              conique=0.05, colonne=1.0)


def _lignes(marque_fermees, mots=" -- SE REFERME EN HAUT : ne peut pas etre un recipient (ni verre, ni vase, ni bol)"):
    out = []
    for n, e in EFFETS:
        if marque_fermees and U_HAUT[n] < 0.25:
            e += mots
        out.append("- %s : %s" % (n, e))
    return "\n".join(out)


_TETE = "\nCHOISIS la silhouette par sa FORME, jamais par le nom de l'objet :\n"
AIDE_APRES = AIDE_AVANT + _TETE + _lignes(False)
AIDE_APRES2 = AIDE_AVANT + _TETE + _lignes(True)
AIDE_APRES3 = AIDE_AVANT + _TETE + _lignes(True, " -- fermee en haut, donc pleine et non creuse")


def invite(demande, variante):
    aide = dict(AVANT=AIDE_AVANT, APRES=AIDE_APRES, APRES2=AIDE_APRES2, APRES3=AIDE_APRES3)[variante]
    return ("Tu remplis UN formulaire. La demande est reconnue comme un objet de la famille "
            "\u00ab revolution \u00bb.\n"
            "Reponds par UNE SEULE ligne, exactement de cette forme, en remplacant chaque <...> par une "
            "valeur :\n%s\n%s\nRien d'autre : pas de phrase, pas de partie.\n\nDemande : %s\nLigne :"
            % (LIGNE_AVANT, aide, demande))


def demander(texte, modele):
    corps = json.dumps(dict(model=modele, prompt=texte, stream=False,
                            options=dict(temperature=0.2, num_predict=120))).encode("utf-8")
    r = urllib.request.Request(OLLAMA, data=corps, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(r, timeout=180) as f:
        return json.loads(f.read().decode("utf-8")).get("response", "")


def ligne_famille(rep):
    """VRAI si une ligne commence par `famille ` -- le seul cas ou l'application
    considere le formulaire REMPLI (NkCreaDocFamille). ⚠️ Ce critere manquait a
    la premiere version de ce juge, qui lisait la silhouette n'importe ou dans la
    reponse : il etait PLUS PERMISSIF QUE L'APPLICATION, et il a verdi (bol
    `evase`) un cas que l'application comptait NON REMPLI et remplacait par les
    valeurs par defaut d'un verre."""
    for l in rep.splitlines():
        l = l.strip().lstrip("`-* 	")
        if l[:8].lower() == "famille ":
            return True
    return False


def lire(rep):
    """Rend (silhouette, largeur, hauteur) tels que le C++ les lirait."""
    m = re.search(r"silhouette\s+([A-Za-z_\u00e0-\u00ff]+)", rep)
    sil = m.group(1).lower() if m else ""
    g = lambda c: float(re.search(c + r"\s+([0-9]*\.?[0-9]+)", rep).group(1)) if re.search(c + r"\s+([0-9]*\.?[0-9]+)", rep) else -1.0
    return sil, g("largeur"), g("hauteur")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--invite", default="AVANT", choices=["AVANT", "APRES", "APRES2", "APRES3"])
    p.add_argument("--repetitions", type=int, default=3)
    p.add_argument("--modele", default="qwen2.5-coder:7b")
    a = p.parse_args()
    nA = nB = nD = nF = n = 0
    for item in JEU:
        for k in range(a.repetitions):
            t0 = time.time()
            rep = demander(invite(item["demande"], a.invite), a.modele)
            sil, L, H = lire(rep)
            cA = sil in PERMISES
            cB = sil in item["ok"]
            cD = (item["L"][0] <= L <= item["L"][1]) and (item["H"][0] <= H <= item["H"][1])
            cF = ligne_famille(rep)
            nF += cF
            n += 1
            nA += cA
            nB += cB
            nD += cD
            print("%-6s #%d  silhouette=%-10s L=%.3f H=%.3f  C_A=%s C_B=%s C_dim=%s C_lu=%s  (%.1fs)"
                  % (item["cle"], k + 1, sil or "(vide)", L, H,
                     "vert" if cA else "ROUGE", "vert" if cB else "ROUGE",
                     "vert" if cD else "ROUGE", "vert" if cF else "ROUGE", time.time() - t0))
    print("\n[%s] C_A permise %d/%d   C_B plausible %d/%d   C_dim %d/%d" % (a.invite, nA, n, nB, n, nD, n))


if __name__ == "__main__":
    main()
