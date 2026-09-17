// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

# ATTENDU — LE PONT `.nkuidoc -> .nkgui` ET LE CHAMP `composant`

> Ecrit **avant** la premiere ligne de correctif, le 2026-09-18, apres la course
> du dorsal TEMOIN et avant toute modification de `NkGuiEcrire.h`.

## LE FAIT MESURE QUI OUVRE CE DOSSIER (et il est deja acquis)

Course du 18/09, dorsal `temoin` (une reponse fixe et JUSTE, qui est le zero du
banc) :

    N1 12/12   N2 12/12   N3 12/12
    NKGuiMonteTest --monter=Build/ia-temoin/d01.nkgui
      validation : 0 erreur(s)   montage : 3 monte(s), 0 role inconnu
      roles utiles: 0 widget(s) qui ne sont pas un simple conteneur
      VERDICT    : MONTE

Le `.nkgui` produit, en entier :

    Group "Ecran_1" { pos = (0,0) size = (0,0)
      Group "Titre_2" { }
      Group "Valider_3" { } }

La reponse disait `composant = etiquette` et `composant = bouton`. **Les deux
sont sortis en `Group`.** Les trois niveaux sont VERTS et le document ne montre
RIEN : c'est exactement le piege que le compteur `roles utiles` a ete ecrit pour
attraper, et il l'a attrape a sa premiere course.

## LA CAUSE, LUE ET NON DEDUITE

`NkUINode` porte **deux champs distincts** (`Document.h`, l. 873-935) :

    NkString component;   // la cle du REGISTRE  -- ce que `composant = ...` ecrit (l. 3786)
    NkString role;        // la taxonomie d'ecran -- ce que le MENU ecrit

`NkRoleNkguiDuNoeud` (`NkGuiEcrire.h`, l. 268) decide en **trois** regles :
role du vocabulaire `.nkgui` -> role en libelle francais -> nature dessinee
(`shape`). **Aucune ne regarde `component`.** Or l'invite envoyee au modele
(`DesignAI.h`, l. 525) ne propose QUE `composant = <nom du catalogue>` : le mot
`role` n'y figure pas. Le pont est donc, par construction, aveugle a tout ce
qu'un modele peut produire.

## CE QUE JE PREDIS, ET AVEC QUELLE CONDITION

### Ce qui DOIT bouger

| mesure | avant | apres (predit) | condition |
|---|---|---|---|
| `roles utiles` sur `d01.nkgui` du temoin | 0 | **2** | le temoin pose `etiquette` et `bouton`, et rien d'autre de non conteneur |
| role du noeud `Titre` | `Group` | `Text` | `etiquette` -> `Text` |
| role du noeud `Valider` | `Group` | `Button` | `bouton` -> `Button` |
| le mot porte par `Titre` | absent | `text = Titre` | le modele n'ecrit **que** `libelle`, jamais `texte` : sans un repli sur le libelle, le role juste afficherait une chaine vide — un role juste et muet vaut le `Group` |

### ⚠️ Ce qui NE DOIT PAS bouger — et c'est l'attendu qui attrape le degat collateral

Un attendu sur le resultat cherche ne peut jamais voir ce qu'on casse a cote.

| mesure | valeur d'aujourd'hui | apres |
|---|---|---|
| `NKGuiMonteTest` sur tout le corpus | **235 / 235** | 235 / 235 |
| `NKUIDesign --recette-ia` | **18 / 18** | 18 / 18 |
| `NKUIDesign --probe` | **362 / 363** (la 74 est ROUGE avant moi, voir plus bas) | 362 / 363, et la meme rouge |
| le document de travail `nkuidesign_document.nkuidoc` reecrit | identique | identique |

**Pourquoi je crois que le corpus ne bougera pas, et ou je peux me tromper** :
la nouvelle regle ne se declenche que si le noeud n'a **ni** role exploitable ;
un noeud du corpus qui porterait `composant` ET `shape` verrait aujourd'hui son
role derive de la forme et demain de son composant. **Je ne le suppose pas : je
compte ces noeuds avant de changer quoi que ce soit**, et si le compte n'est pas
nul, le chiffre entre dans le rapport avec la liste.

### Le NEGATIF, et il doit rougir

Une mutation qui rend la table des composants vide (`NK_PONT_SANS_COMPOSANT=1`)
doit ramener `roles utiles` a **0** sur `d01.nkgui`. Si le critere reste vert
sous cette mutation, il ne mesure rien.

## LES DEUX COMPOSANTS SANS EQUIVALENT, ET POURQUOI JE NE LES DEVINE PAS

Le catalogue compte **10** composants. Huit ont un equivalent evident dans le
vocabulaire `.nkgui`. Deux — `content_browser` et `tree_view` — sont des
**composites du kit** : un navigateur de contenu n'est pas un `ListBox`, il porte
un arbre, un fil d'Ariane et une recherche. Leur donner un role approchant
produirait un fichier qui **se monte** en montrant autre chose que ce qui a ete
demande, c'est-a-dire la pire des trois issues.

Ils seront donc **SIGNALES** par un compteur propre (`composantsSansRole`) et le
pont retombera sur la regle suivante. Un refus nomme est un succes ; un repli
muet est le defaut que ce depot passe son temps a retirer.

## CE QUI RESTE ROUGE AVANT MOI, ET QUE JE NE M'ATTRIBUE PAS

`--probe` rend **362 / 363** sur `2a9f5fa80`, sonde **74** (« LE MODE EDITION
SOUS LA MATRICE ») en echec : « poignees pres des positions non tournees : 3
(attendu 0) ». Les fusions R29 et R34 rendaient 363/363. La regression est donc
entree entre `347549dc1` et `2a9f5fa80`, c'est-a-dire dans le lot de
l'identifiant stable / DockSpace — **pas dans le lot du jour**. Elle est portee
au canal. Un banc deja rouge ne protege plus : je garde 362/363 comme reference
et je refuse d'y perdre une sonde de plus.
