# -*- coding: utf-8 -*-
"""VERIFIE QUE LES MOTS DE RODOLF SONT INTACTS DANS LES DOCUMENTS.

⚠️ POURQUOI CE FICHIER EXISTE — un risque MESURE, pas imagine (2026-09-02).

   L'agent Noge a decouvert qu'un remplacement automatique avait mange des
   citations dans ses propres rapports. Le meme jour, un balayage de renommage
   (`NKAnimation` -> `NKAnima`, correct par ailleurs) a traverse
   `Applications/NKUIDesign/design/` et modifie deux lignes de documents que
   personne ici n'editait.

   Les citations de Rodolf ne sont pas de la prose : ce sont les PIECES qui
   fondent des decisions closes (Q51, Q53, le principe « composant = acte »).
   Une citation alteree par un `sed` de passage ne se voit pas -- elle se
   DECOUVRE des mois plus tard, quand quelqu'un rouvre une decision en croyant
   lire ce qui a ete dit.

   *Un document qui porte une decision doit pouvoir prouver qu'il n'a pas
   bouge.* C'est ce que fait ce script : il exige la presence EXACTE de chaque
   citation, et nomme celle qui manque.

⚠️ CE QU'IL NE FAIT PAS : il ne verifie pas que la citation est FIDELE a ce que
   Rodolf a dit (personne ici ne peut le faire a sa place). Il verifie qu'elle
   n'a pas change depuis qu'elle a ete consignee -- c'est une garde
   d'integrite, pas de veracite. Les deux sont utiles ; les confondre serait
   se croire protege de la mauvaise chose.

Usage :  python verif_citations.py
Sortie : 0 = toutes presentes ; 1 = au moins une alteree (elle est nommee).
"""
import io
import os
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
ICI = os.path.dirname(os.path.abspath(__file__))

# (fichier, etiquette, fragment EXACT qui doit s'y trouver)
# Fragments volontairement COURTS et distinctifs : un extrait trop long casse
# au premier retour a la ligne redistribue par un formateur, et le faux
# positif userait la garde jusqu'a ce qu'on l'ignore.
CITATIONS = [
    (u"15_Modele_Composants.md", u"Q51 revision - propagation",
     u"si je modifie l'arrondi du composant"),
    (u"15_Modele_Composants.md", u"Q51 revision - instance locale",
     u"modifier l'arrondi d'un bouton ne touche pas les boutons du"),
    (u"15_Modele_Composants.md", u"Q51 revision - systeme par copie",
     u"toujours modifié par copie"),
    (u"15_Modele_Composants.md", u"Q51 precision - tiers",
     u"ce qui se passe avec les composants locaux"),
    # DEUX exemplaires : le bloc de citation et le rappel en prose du §15.6.
    (u"15_Modele_Composants.md", u"Q51 generalisation - propriete",
     u"la couleur n'est qu'une propriété", 2),
    (u"15_Modele_Composants.md", u"Q51 premiere reponse (remplacee)",
     u"non, sauf si ces instances sont"),
    (u"15_Modele_Composants.md", u"Q53 - nombre exact",
     u"plus tard on en ajoutait, ça devrait montrer le nombre exact"),
    (u"15_Modele_Composants.md", u"principe - composant = acte",
     u"un composant ne sera composant que lorsque l'utilisateur aura"),
    (u"13_Lunacy_reference_interaction.md", u"les trois miroirs",
     u"Lunacy définit plusieurs types de miroir"),
    (u"13_Lunacy_reference_interaction.md", u"point miroir par defaut",
     u"par défaut je veux Miroir"),
    (u"16_Etat_apparence.md", u"la demande qui a ouvert le chantier apparence",
     u"je veux vraiment avoir un design très beau"),
    (u"18_Vision_IA_Design.md", u"vision IA - le Lego",
     u"là c'est juste du rangement, du Lego"),
    # ⚠️ FRAGMENT COURT, ET LE PREMIER ESSAI A PROUVE POURQUOI : celui-ci
    #    disait « il propose un wireframe et part de ce wireframe » et la garde
    #    a crie a l'alteration -- alors que le document etait INTACT : la
    #    citation y traverse un retour a la ligne avec son « > » de bloc-quote.
    #    *Un premier essai qui accuse un document sain use la garde jusqu'a ce
    #    qu'on l'ignore.* Un fragment ne doit jamais franchir une fin de ligne.
    (u"18_Vision_IA_Design.md", u"vision IA - wireframe",
     u"propose un wireframe et part de ce wireframe"),
]


def principal():
    perdues = []
    caches = {}
    for entree in CITATIONS:
        fichier, etiquette, fragment = entree[0], entree[1], entree[2]
        # 🔴 LE NOMBRE D'OCCURRENCES FAIT PARTIE DE LA GARDE, et il a fallu une
        #    MUTATION pour s'en apercevoir : le premier essai testait « le
        #    fragment est-il present ? ». On a donc altere la citation
        #    « la couleur n'est qu'une propriete »... et la garde est restee
        #    VERTE, parce que la phrase apparait DEUX fois (le bloc de citation
        #    et un rappel en prose) et que la seconde a sauve la premiere.
        #    ⚠️ *Une garde qui demande « existe-t-il au moins un exemplaire ? »
        #       ne protege aucun exemplaire en particulier.* On exige donc le
        #       COMPTE : altere l'un des deux, et il tombe a 1.
        attendu = entree[3] if len(entree) > 3 else 1
        chemin = os.path.join(ICI, fichier)
        if fichier not in caches:
            if not os.path.isfile(chemin):
                perdues.append((fichier, etiquette, u"FICHIER ABSENT", 0, attendu))
                caches[fichier] = u""
                continue
            caches[fichier] = io.open(chemin, encoding="utf-8").read()
        vu = caches[fichier].count(fragment)
        if vu != attendu:
            perdues.append((fichier, etiquette, fragment, vu, attendu))

    if perdues:
        print(u"🔴 CITATION(S) ALTEREE(S) — une decision close a perdu sa piece :")
        for fichier, etiquette, fragment, vu, attendu in perdues:
            print(u"   %s  [%s]" % (fichier, etiquette))
            print(u"      attendu %d exemplaire(s), trouve %d : %s"
                  % (attendu, vu, fragment))
        print(u"   ⚠️ NE PAS « reecrire de memoire » : retrouver le texte dans")
        print(u"      echanges/nkuidesign.questions.md ou l'historique git.")
        return 1
    print(u"%d citation(s) de Rodolf verifiees, toutes intactes." % len(CITATIONS))
    return 0


if __name__ == u"__main__":
    sys.exit(principal())
