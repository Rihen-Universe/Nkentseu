# HumanModelingTools — l'interface, pour l'agent qui écrit les `.nkgui`

### Document 6 — Spécification d'interface côté agent (supplément à NKCraft)

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 27/09/2026.
> Traduction du **05** en cinq points : **arbre nommé**, **composants**,
> **actions**, **couleurs porteuses**, et **ce que tu ne sais pas encore
> reproduire**.
> **Si 05 et 06 se contredisent, 05 gagne.**
>
> **Le langage** est le `.nkgui` **0.4**. **Le vocabulaire** : NKUIDesign doc 7.
> **La notation** : `Rôle "identifiant"   propriété: valeur`, indentée ;
> `Rôle✚` = rôle proposé (écris sa doublure) ; `propriété✚` = propriété proposée.
>
> ⚠️ **EN RÉSERVE** jusqu'à la fin de NKUIDesign (04 §8, D5). Ne commence pas.

---

## 0. Ce qui est propre à ce supplément

1. **Fichiers en PascalCase, identifiants en camelCase.** Règle de Rodolf du
   27/09. Si un exemple de ce document s'en écarte, c'est une faute de ce
   document : écris en PascalCase et signale-la.
2. **Deux includes par document**, dans cet ordre : `Theme.nkgui` puis
   `ComposantsCraft.nkgui`. ⚠️ **Il n'existe pas de `Resources/Interface/Commun/`**
   — mesuré le 27/09, et trois applications s'y sont déjà trompées. Un `include`
   vers un dossier absent **ne fait pas crier le format** : il rend un document
   silencieusement amputé.
3. **Câbler, griser ou retirer.** Jamais un bouton muet. Un bouton non câblé
   porte sa raison : `disable "x" because "..."`.
4. **Rôle inconnu = l'ÉCRAN ENTIER refusé.** N'invente pas de rôle ; pour un
   rôle proposé, écris sa doublure avec les rôles existants.

---

## 1. Les documents à écrire

| fichier | §05 | rôle |
|---|---|---|
| `EspacePersonnage.nkgui` | 2 | la carte de l'espace : les trois zones et la barre de modes |
| `PanneauParties.nkgui` | 2 | l'arbre du graphe |
| `PanneauBibliotheque.nkgui` | 2 | les parties disponibles |
| `PanneauMesures.nkgui` | 3.2 | les valeurs nommées, avec leur unité |
| `BarreModes.nkgui` | 3 | Composer · Mesurer · Sculpter · Cadrer · Poser |
| `DlgRetirerPartie.nkgui` | 3.1 | la confirmation qui **nomme** ce qui part |
| `PanneauCadrage.nkgui` | 3.4 | l'image de référence et le mode silhouette |

---

## 2. Ce qui est un `Host`, et ce qui ne l'est pas

Règle de la famille (NKUIDesign doc 5 §1) : **une surface qui a ses propres
gestes est un `Host`.**

| `Host` — peint par l'application | document `.nkgui` |
|---|---|
| le **viewport** et ses rotules, les poignées, l'image de référence | les trois panneaux, la barre de modes, le dialogue |
| l'**arbre des parties** (glisser-déposer sur un socket) | son cadre et son en-tête |

⚠️ **L'ARBRE EST UN `Host`, ET SON CADRE UN DOCUMENT.** Le glisser-déposer d'une
partie sur un socket est un geste propre : le format ne le décrit pas. Mais le
cadre, le titre, la barre de recherche et les boutons sont de la chrome, donc du
document. La frontière passe **à l'intérieur** du panneau, pas autour.

---

## 3. Les arbres

### 3.1 `BarreModes.nkgui`

```
HBox "barreModes"                     gap: 2   align: Center
  ToggleButton "perso.modeComposer"   label: "Composer"   group: "modePerso"   icon: puzzle
  ToggleButton "perso.modeMesurer"    label: "Mesurer"    group: "modePerso"   icon: ruler
  ToggleButton "perso.modeSculpter"   label: "Sculpter"   group: "modePerso"   icon: hand
  ToggleButton "perso.modeCadrer"     label: "Cadrer"     group: "modePerso"   icon: image
  ToggleButton "perso.modePoser"      label: "Poser"      group: "modePerso"   icon: person-standing
```

### 3.2 `PanneauMesures.nkgui` (extrait)

```
VBox "panneauMesures"                 gap: 4
  EnTetePanneau "entete"              label: "Mesures"
  HBox "unite"                        gap: 4   align: Center
    Text "uniteLibelle"               text: "Unité"
    ToggleButton "perso.uniteAbsolue" label: "cm"     group: "unitePerso"
    ToggleButton "perso.uniteFraction" label: "× H"   group: "unitePerso"
  Separator "sep1"                    orientation: Horizontal
  Host "perso.mesures"                hint: "les mesures de la partie sélectionnée, une par ligne, avec son unité et sa chaîne de miroir"
  Separator "sep2"                    orientation: Horizontal
  VBox "couche"                       gap: 2
    Text "coucheTitre"                text: "Couche"
    HBox "coucheLigne"                gap: 4   align: Center
      Badge "perso.coucheEtat"        text: "vide"   tone: neutre
      Button "perso.coucheReinit"     label: "Réinitialiser"   icon: rotate-ccw
```

> 🔴 `perso.coucheReinit` EST LA PREUVE VISIBLE DE LA DÉCISION D2 (04 §2). Il ne
> peut exister que si le sculpt est une couche. S'il se grise « pas
> implémentable », ce n'est pas un bouton qui manque : c'est la conception qui a
> cuit le sculpt, et il faut le dire fort.

### 3.3 `DlgRetirerPartie.nkgui`

```
Dialog "dlgRetirerPartie"             title: "Retirer une partie"
  VBox "corps"                        gap: 8
    Text "question"                   bind: perso.retraitQuestion
    Text "detail"                     bind: perso.retraitDetail
    HBox "actions"                    gap: 6   align: End
      Button "perso.retraitAnnuler"   label: "Annuler"
      Button "perso.retraitValider"   label: "Retirer"   tone: danger
```

⚠️ `perso.retraitQuestion` se lie, elle ne s'écrit pas : le texte doit **nommer**
ce qui part (« retirer `cuisse G` et ses 2 descendants ? »). Un « Êtes-vous
sûr ? » générique fait cliquer sans lire — et une descendance entière disparaît.

---

## 4. Les actions

Domaine **`perso.`**, et lui seul.

| groupe | actions |
|---|---|
| modes | `modeComposer` `modeMesurer` `modeSculpter` `modeCadrer` `modePoser` |
| composer | `attacher` `retirer` `retraitValider` `retraitAnnuler` `dupliquerMiroir` `romprLien` |
| mesurer | `uniteAbsolue` `uniteFraction` `mesurePoser` |
| sculpter | `coucheReinit` |
| cadrer | `refCharger` `refOpacite` `silhouette` |
| poser | `poseRepos` `poseControleTraversee` |

---

## 5. Les couleurs porteuses

Par jeton, jamais en dur (05 §4) :

`@accent` socket libre · `@info.neutre` socket occupé · `@info.lien` mesure liée
par le miroir · `@info.modifie` partie sculptée · `@info.alerte` volumes qui se
traversent.

⚠️ `@info.lien` n'existe pas encore dans le thème de la famille. **Ne l'invente
pas dans ton document** : déclare-le dans `Theme.nkgui` de NKCraft, et
signale-le. Un jeton employé sans être déclaré ne fait pas échouer la lecture —
il rend une couleur par défaut, et personne ne voit que le lien ne se voit pas.

---

## 6. Ce que tu ne sauras pas reproduire, et qu'il faut noter au lieu d'inventer

Relevé au 27/09, et chacun a un coût :

1. **Un `bind` de liste ne porte que le RANG, pas la valeur.** L'arbre des
   parties et la bibliothèque en pâtiront.
2. **`set` ne pose que cinq propriétés** : `text`, `value`, `checked`, `visible`,
   `enabled` — **et `icon`/`image` depuis le 27/09**. Rien d'autre.
3. **Les composants n'ont pas de paramètres.** Une ligne de mesure devra se
   répéter, ou vivre dans un `Host`.
4. **`image` est posable mais n'est dessinée que par `Image`, `ImageButton` et
   `Tile`** — le rendu d'image a été branché le 27/09 ; vérifie l'état avant de
   t'appuyer dessus.
5. **Les animations ne sont pas jouées.** La section s'écrit, elle ne s'anime pas.
6. **Aucune liaison en lecture seule vers une valeur de l'application**, et le
   piège sérieux : **un chemin de modèle ABSENT ne provoque aucune erreur** — la
   condition vaut « indéfini ». Pose les chemins même à zéro, et fais-les
   vérifier par la sonde.

---

## 7. Ce que tu rends

Dans `echanges/humanmodelingtools.reponses.md`, **sans lettres accentuées**, les
six rubriques de la famille : ce qui est construit (avec les chiffres de la
sonde) · les négatifs, un par critère, en disant lesquels sont **concluants** ·
ce que le format ne sait pas dire, avec le coût · ce que 04/05/06 doivent
corriger · ce que tu n'as pas fait et pourquoi · les commits, et la confirmation
que **rien n'a été poussé**.

---

*Documents compagnons : `04-produit-humanmodelingtools.md`,
`05-interface-humaine-humanmodelingtools.md`.*
