# -*- coding: utf-8 -*-
"""Controle de coherence entre ELEMENTS_A_DESSINER.md et SPECIFICATION_VISUELLE.md.

POURQUOI IL EXISTE
  Le 22/08, deux lignes de l'inventaire (D6 et D7) etaient marquees « non
  tranche » alors que la specification les avait deja instruites, aux § 8.4 et
  § 11.4. Deux seances de decision ont ete preparees pour Rodolf sur des points
  deja travailles -- et RIEN ne l'aurait signale.

  Ce n'est pas un incident : c'est une propriete du dispositif. Deux documents
  decrivent le meme objet, l'un avance, l'autre pas, et la divergence est
  SILENCIEUSE PAR CONSTRUCTION. Elle se reproduira.

CE QU'IL VERIFIE
  Chaque ligne des tableaux de l'inventaire porte, dans sa colonne d'etat, un
  renvoi « §N.M » vers le paragraphe de la specification qui doit la trancher.
  Une ligne OUVERTE dont le paragraphe est DECIDE ou PROPOSE signale un
  inventaire en retard.

CE QU'IL NE VERIFIE PAS, ET POURQUOI -- mesure faite, pas supposee
  Pas de test inverse (« la ligne est faite alors que le paragraphe est ouvert ») :
  la colonne d'etat de l'inventaire melange DEUX AXES. La coche y signifie
  « deja dessine dans une planche », pas « decide ». Un element peut donc etre
  dessine alors que son paragraphe garde des points ouverts, et c'est normal.
  Essaye le 22/08 : 4 signalements, 0 trouvaille. Un controle qui signale du
  normal finit ignore, puis desactive.

DEUX PIEGES QU'IL EVITE, ET ILS ONT ETE MESURES
  1. IL S'ANCRE SUR LES LIGNES DE TITRE, JAMAIS SUR LE CORPS. Chercher « DECIDE »
     dans un paragraphe accrocherait « ce point n'est pas decide » ou « avant
     d'etre decide » : le controle rendrait un vert pour une phrase. Un titre est
     un endroit qui NE PEUT PAS etre une phrase.
  2. IL RETIRE LA NEGATION AVANT DE CHERCHER L'AFFIRMATION. « NON TRANCHE »
     contient « TRANCHE » -- meme piege que chercher `erreur` et attraper
     `aucune erreur`.

SA LIMITE, ECRITE PLUTOT QUE TUE
  Un renvoi pointe un PARAGRAPHE, or un paragraphe traite souvent plusieurs
  elements. Quand il porte a la fois du travail fait ET un point ouvert, le
  controle NE PEUT PAS dire lequel concerne la ligne. Il l'annonce alors comme
  NON CONCLUANT au lieu de rendre un vert : un cas qui ne peut pas discriminer
  doit le dire, pas se taire.

USAGE
  cd Kernel/Runtime/NKGraph && python3 verifie_coherence.py
  Code de retour 1 si une divergence est detectee.
"""
import io
import re
import sys

INV = "ELEMENTS_A_DESSINER.md"
SPEC = "SPECIFICATION_VISUELLE.md"

RE_NEGATION = re.compile(u"NON\\s+TRANCHÉS?")
RE_DECIDE = re.compile(u"DÉCIDÉ|VALIDÉ|TRANCHÉ")
RE_PROPOSE = re.compile(u"PROPOSÉ")
RE_NUM = re.compile(u"^(\\d+(?:\\.\\d+)?)[ .]")
RE_LIGNE = re.compile(u"^\\| ([A-E]\\d+[a-z]?) \\|.*\\| ([^|]*) \\|$")
RE_RENVOI = re.compile(u"§(\\d+(?:\\.\\d+)?)")

OUVERT_1 = u"⚠️"        # triangle d'avertissement
OUVERT_2 = u"\U0001F7E1"      # rond jaune : instruit, en attente


def etats_specification(chemin):
    """Rend {numero de paragraphe: set d'etats}, lus dans les TITRES seuls."""
    etats = {}
    courant = None
    for ligne in io.open(chemin, encoding="utf-8").read().split("\n"):
        if not ligne.startswith("##"):
            continue                      # le corps du texte n'est JAMAIS lu
        titre = ligne.lstrip("#").strip()
        num = RE_NUM.match(titre)
        if num:
            courant = num.group(1)
            etats.setdefault(courant, set())
        if courant is None:
            continue
        reste = RE_NEGATION.sub(u"", titre)   # la negation d'abord
        if reste != titre:
            etats[courant].add("NON_TRANCHE")
        if RE_DECIDE.search(reste):
            etats[courant].add("DECIDE")
        if RE_PROPOSE.search(reste):
            etats[courant].add("PROPOSE")
    return etats


def lignes_inventaire(chemin):
    out = []
    for i, l in enumerate(io.open(chemin, encoding="utf-8").read().split("\n"), 1):
        m = RE_LIGNE.match(l)
        if not m:
            continue
        cle, etat = m.group(1), m.group(2)
        # Deux degres d'ouverture, et ils n'attendent pas la meme chose :
        #   SANS_PROPOSITION : « non tranche, et je n'ai rien a proposer »
        #   EN_ATTENTE       : « instruit, proposition ecrite, il manque un oui »
        if OUVERT_1 in etat:
            ouvert = "SANS_PROPOSITION"
        elif OUVERT_2 in etat:
            ouvert = "EN_ATTENTE"
        else:
            ouvert = None
        ren = RE_RENVOI.search(etat)
        out.append((i, cle, ouvert, ren.group(1) if ren else None))
    return out


def main():
    spec = etats_specification(SPEC)
    inv = lignes_inventaire(INV)
    print("%d paragraphes de specification, %d lignes d'inventaire"
          % (len(spec), len(inv)))

    retard, douteux, morts, sans = [], [], [], []
    for i, cle, ouvert, ren in inv:
        if ren is None:
            sans.append(cle)
            continue
        if ren not in spec:
            morts.append((i, cle, ren))
            continue
        if ouvert is None:
            continue
        et = spec[ren]
        # « en attente » + paragraphe « propose » : les deux disent la meme
        # chose, il manque un oui de Rodolf. Ce n'est pas une divergence.
        interesse = ("DECIDE" in et) or (ouvert == "SANS_PROPOSITION" and "PROPOSE" in et)
        if not interesse:
            continue
        (douteux if "NON_TRANCHE" in et else retard).append((i, cle, ren, sorted(et)))

    dur = 0
    if retard:
        dur = 1
        print("\n[EN RETARD] l'inventaire dit OUVERT, la specification a deja travaille :")
        for i, c, r, et in retard:
            print("   ligne %-4d %-5s -> paragraphe %-5s est %s. A reporter dans l'inventaire."
                  % (i, c, r, "+".join(et)))
    if morts:
        dur = 1
        print("\n[RENVOI MORT] paragraphe absent de la specification :")
        for i, c, r in morts:
            print("   ligne %-4d %-5s -> paragraphe %s introuvable" % (i, c, r))
    if douteux:
        print("\n[NON CONCLUANT] paragraphe MIXTE -- le renvoi est trop grossier pour trancher :")
        for i, c, r, et in douteux:
            print("   ligne %-4d %-5s -> paragraphe %-5s est %s. A verifier a la main."
                  % (i, c, r, "+".join(et)))
    if sans:
        print("\n[sans renvoi] %d ligne(s) hors controle : %s" % (len(sans), ", ".join(sans)))
    if not dur:
        print("\nOK : aucune divergence detectable entre l'inventaire et la specification.")
    return dur


if __name__ == "__main__":
    sys.exit(main())
