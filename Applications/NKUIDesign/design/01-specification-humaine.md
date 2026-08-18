# NkUIDesign — Spécification de l'interface
### Document 1/3 — version lisible par un humain (produit, ergonomie, justifications)

> **À qui s'adresse ce document.** À quelqu'un qui doit comprendre *ce qu'est*
> NkUIDesign, *pourquoi* il est fait ainsi, et *ce qui existe aujourd'hui contre
> ce qui reste à écrire*. Il ne contient aucune structure de données ni aucun
> prompt : ceux-là sont dans `02-specification-claude.md` (implémentable) et
> `03-specification-banani.md` (écrans et visuel).
>
> **Règle d'or tenue partout dans les trois documents** : ce qui existe se dit
> comme existant, ce qui n'existe pas se dit comme à faire. Jamais un espoir
> présenté comme un acquis. Chaque section porte donc son état, et l'état est
> daté du **2026-08-19 (fin de journée)**, branche `feat/noge-inventaire`,
> commit `002566f7`.
>
> ⚠️ **Un état n'est pas un acquis : il a une date.** Ce document a été rédigé une
> première fois quelques heures plus tôt et affirmait que *la fenêtre n'avait
> jamais été ouverte*. C'était vrai à l'écriture et faux à la relecture — un autre
> agent a ouvert la fenêtre entre-temps. Toutes les affirmations d'état ont été
> revérifiées contre le dépôt, pas recopiées. **Refaites-le avant de vous appuyer
> sur elles.**
>
> **Ce document est incomplet par construction**, et c'est voulu (règle du
> corpus, Rodolf, 2026-08-16) : il est un point de départ, pas un contrat. Une
> lacune de couverture est une information de planification, jamais un motif
> d'arrêt. Là où il comble un silence, il le dit — pour que la proposition se
> voie et puisse être reprise.

---

## 0. Sommaire

1. [L'exigence qui commande tout : la prise en main](#1-lexigence-qui-commande-tout--la-prise-en-main)
2. [Ce qu'est NkUIDesign, et ce qu'il n'est pas](#2-ce-quest-nkuidesign-et-ce-quil-nest-pas)
3. [État réel au 2026-08-19 — la table de vérité](#3-état-réel-au-2026-08-19--la-table-de-vérité)
4. [Les pages et les écrans](#4-les-pages-et-les-écrans)
5. [L'interface : disposition, ancrage, barres, commandes, raccourcis](#5-linterface--disposition-ancrage-barres-commandes-raccourcis)
6. [Les thèmes](#6-les-thèmes)
7. [Les langues](#7-les-langues)
8. [Les icônes](#8-les-icônes)
9. [Les widgets et les composants — la déclaration](#9-les-widgets-et-les-composants--la-déclaration)
10. [Les paramètres, dont le backend graphique](#10-les-paramètres-dont-le-backend-graphique)
11. [L'export](#11-lexport)
12. [L'IA intégrée](#12-lia-intégrée)
13. [Ce qui n'est pas tranché et remonte à Rodolf](#13-ce-qui-nest-pas-tranché-et-remonte-à-rodolf)

---

## 1. L'exigence qui commande tout : la prise en main

Rodolf l'a posée explicitement, et elle passe **avant** les fonctionnalités :
l'outil doit être **simple à prendre en main, y compris pour l'IA**. Ce n'est
pas un vœu d'ergonomie en fin de liste ; c'est un critère qui tranche des
décisions de structure. Il est donc en tête, et il se retrouve dans chaque
section suivante.

### 1.1 Ce que « simple à prendre en main » veut dire ici, concrètement

Cinq engagements, et chacun est vérifiable :

1. **On produit quelque chose de visible dans les soixante premières secondes.**
   Ouvrir l'application donne déjà un document (`BuildStarterDocument`), pas une
   page blanche. Une page blanche oblige à connaître le vocabulaire avant
   d'avoir rien vu.
2. **Rien d'important n'est caché derrière un savoir préalable.** Tout ce qui se
   fait au clavier se fait aussi par un menu ou par la palette de commandes, et
   la palette affiche le raccourci à côté du nom — c'est ainsi qu'on apprend les
   raccourcis sans les apprendre.
3. **Un manque se voit toujours, il ne se devine jamais.** Un composant déclaré
   sans fonction de dessin peint un **cartouche portant son nom et la mention
   qu'il n'est pas branché** — pas un rectangle vide qu'on prendrait pour un bug
   de mise en page. Une clé de traduction absente s'affichera comme clé absente.
   Un backend graphique indisponible refuse et le dit. C'est la même discipline
   partout : *ce qui manque se signale, il ne disparaît pas.*
4. **Aucun panneau ne demande de comprendre le format de fichier.** L'utilisateur
   pose des composants, tire des bords, remplit des champs. Le fichier est un
   résultat.
5. **Une erreur ne détruit rien.** Recharger un document compte et affiche le
   nombre de lignes inconnues ; un chargement « réussi » qui n'appliquerait rien
   est le pire des deux mondes, donc il ne se présente jamais comme réussi.

### 1.2 « Y compris côté IA » — et c'est la partie qu'on rate

La prise en main d'une IA, ce n'est pas un bouton « générer ». C'est que la
machine puisse **apprendre le format sans qu'on le lui explique**, et que sa
sortie **entre par la même porte que la main**.

| ce qui rend l'outil simple pour un humain | l'équivalent exact côté IA |
|---|---|
| un document de départ déjà rempli | un **exemple canonique** à imiter, produit par l'outil lui-même |
| des libellés en français | un **vocabulaire fermé et court** dans le fichier : `fixed`/`content`/`fraction`/`weight`/`expand`, `row`/`column`/`grid`/`anchor` — pas d'expression libre |
| « annuler » qui rattrape une erreur | le **rejeu** : une proposition qui ne survit pas à un aller-retour par le format est écartée, mécaniquement |
| un manque qui se voit | un mot inconnu **compté** et affiché, jamais ignoré |
| ne pas avoir à connaître le format | l'IA produit **la même donnée que la souris**, donc l'humain reprend son travail sans conversion |

⚠️ **Le point dur, et il est structurel** : le vocabulaire du prompt de l'IA doit
être **engendré par les mêmes fonctions que celles qui écrivent le fichier**. Si
le prompt récite une liste de mots recopiée à la main, elle divergera du format
au premier ajout, et l'IA produira du texte que l'outil ne relit plus. C'est
déjà tenu dans le code, et la sonde le vérifie (essai 28b).

### 1.3 Ce qui, dans l'outil, existe *pour* la prise en main

- **Aucun panneau ne connaît le nom d'un composant.** Palette, arbre,
  réglages et catalogue d'IA bouclent tous sur le registre ou sur les tables de
  la déclaration. Conséquence directe pour l'utilisateur : **un composant ajouté
  à la bibliothèque apparaît partout sans qu'on touche à l'éditeur** — donc ce
  qu'il a appris sur un composant vaut pour le suivant.
- **Les bornes des curseurs viennent de la déclaration**, pas de l'éditeur. Deux
  vérités pour une même borne, c'est un utilisateur qui apprend une règle fausse.
- **La position n'est jamais saisie.** On tire un bord : l'outil écrit une
  *taille* ou un *poids*. On déplace un nœud : l'outil écrit un *parent* et un
  *rang*. L'utilisateur n'a donc rien à réapprendre quand la fenêtre change de
  taille, de DPI ou de langue — son travail suit.

---

## 2. Ce qu'est NkUIDesign, et ce qu'il n'est pas

**La description de Rodolf, mot pour mot** : *« NkUIDesign : designer des
interfaces utilisateur, des composants, tester leur fonctionnement, utiliser si
possible les blueprint pour simuler l'interactivité, exposer les callbacks liés
à chacun, etc. — la totale, quoi. »*

Donc **cinq capacités, pas une** :

| # | capacité | état |
|---|---|---|
| 1 | **composer** une interface à partir de composants existants | ✅ **existe** |
| 2 | **créer** de nouveaux composants (pas seulement assembler) | 📝 à faire |
| 3 | **tester leur fonctionnement** — instancier depuis la description, à l'exécution | 🟡 partiel : le dessin lit la déclaration ; aucune fenêtre n'a encore été ouverte |
| 4 | **simuler l'interactivité par blueprint** | 📝 à faire — les événements sont déclarés, rien ne s'y branche |
| 5 | **exposer les callbacks** de chaque élément | ✅ déclarés avec leur charge ; ❌ aucun consommateur |

### 2.1 Ce que NkUIDesign n'est pas

- **Ce n'est pas un constructeur d'interfaces au pixel.** Un constructeur
  enregistre où vous avez lâché la souris, et son résultat est faux dès que la
  fenêtre change de taille. NkUIDesign enregistre **pourquoi c'était là**, et le
  résultat se recalcule. C'est littéralement la ligne de partage.
- **Ce n'est pas un générateur de code.** L'IA comme la main produisent **la
  déclaration**, jamais du C++. Du code généré serait une voie sans retour : ni
  reprise à la souris, ni continuation par l'IA.
- **Ce n'est pas un quatrième système de description.** Le dépôt en porte déjà
  trois sans utilisateur. NkUIDesign est **l'usage qui leur manquait** — il est
  le premier consommateur de la réflexion d'interface, de l'interpréteur
  blueprint et de l'inspecteur. Construit sans eux, il deviendrait le quatrième
  dormeur.

### 2.2 Où il se pose dans la pile

```
   application  (NkUIDesign, Nogee, NkAnimaEditor, NK3DModeler, PV3DE)
        │              spécialisation seule
   NKEditorKit   coquille (ancrage, barres, palette) + COMPOSANTS composés
        │              + la FORME DE DÉCLARATION d'un composant
   NKGui         primitives : bouton, nœud d'arbre, saisie, glisser-déposer
        │
   Nkentseu      événements, fenêtres, rendu, sérialisation, journalisation
```

NkUIDesign **n'invente pas une pile — il compose la nôtre**. Et il ne réécrit
pas ce que la bibliothèque porte : la bibliothèque n'est pas une prison, elle
est le **choix par défaut**. Ce qui est proscrit, c'est de réécrire *sans raison
écrite* ce qui existe.

---

## 3. État réel au 2026-08-19 — la table de vérité

C'est la section à lire avant toutes les autres. Elle est la garantie que le
reste du document ne se lit pas comme une promesse.

### 3.1 Ce qui existe et tourne

| chose | preuve |
|---|---|
| **l'application TOURNE** | fenêtre ouverte en **OpenGL**, trois lancements, trois fois le même résultat, fermeture propre |
| l'application se construit | `jenga build --target NKUIDesign` : **20/20 SUCCESS** |
| la sonde sans écran | `NKUIDesign --probe` : **72/72** |
| la forme de déclaration se vérifie sans rien lier | banc de la forme : **43/43**, dont une série de **témoins qui doivent rougir** |
| le dessin des composants ne dépend pas de l'interface | banc de neutralité : vert, avec **des témoins qui échouent bien** |
| deux composants complets | `content_browser` et `tree_view` — déclaration + modèle + dessin |
| cinq panneaux ancrés | Palette, Composition, Aperçu, Propriétés, IA |
| quatre commandes avec raccourcis | `Ctrl+S`, `Ctrl+R`, `Ctrl+N`, `Ctrl+Q` |
| le choix du backend graphique | `--gfx=`, `NK_GFX_API`, résolution **pure et vérifiée par la sonde**, journal *demandé / source / retenu / pourquoi*, refus explicite |
| deux thèmes | Sombre et Clair, rôles nommés, chargement texte avec héritage |
| le document ne contient aucune coordonnée | vérifiable : cherchez `x`, `y`, `position` parmi les champs d'un nœud |
| la provenance est écrite **et relue** | l'aller-retour la compte comme appliquée |
| **le gabarit répété** | un élément déclare `une fois` / `par entrée` / `par entrée récursif` |
| **les libellés sont des clés** | décidé et vérifié dans la forme — voir § 7.3, point 4 |

#### ⚠️ Ce que l'ouverture de la fenêtre a immédiatement appris

Le premier témoin visuel a montré **deux défauts que 68 essais verts n'avaient
pas vus**. C'est la leçon la plus utile de ce document, et elle vaut pour tout ce
qui suit :

1. **Le journal du backend n'écrivait rien** — il imprimait littéralement ses
   propres accolades au lieu des valeurs. La trace **existait et ne disait
   rien**. Cause structurelle, et elle est instructive : le choix de backend
   vivait dans le point d'entrée, donc **hors de portée d'une sonde sans écran**
   — *le seul code que le GPU touchait était le seul code sans témoin*. Corrigé
   en rendant la résolution **pure**, appelée par le programme **et vérifiée par
   la sonde**.
2. **Le navigateur de contenu se peignait en MAGENTA FRANC** — parce que les noms
   de rôles de thème sont canoniquement en `snake_case` et que des rôles étaient
   déclarés en `PascalCase` : aucun ne tombait juste, et le thème repliait sur sa
   couleur « ça doit sauter aux yeux ». **Le repli a fait son travail.** Ce que
   les 68 essais ne pouvaient pas voir : le résolveur de la sonde **acceptait
   n'importe quel nom** — *un résolveur qui dit oui à tout ne peut pas voir un
   nom faux*.

🟡 **Reste ouvert** : **10 jetons d'un composant restent non résolus**. Ils sont
**comptés et nommés** dans la sortie de sonde, et ils appartiennent au fichier
d'un autre agent — **comptés, pas corrigés à sa place**.

⚠️ **Ce que « l'application tourne » ne veut PAS dire** : aucune conformité aux
planches de référence n'est revendiquée. La fenêtre s'ouvre et se ferme
proprement ; **elle n'a pas été comparée à une cible visuelle**.

### 3.2 Ce qui n'existe pas — et qu'aucune phrase de ce document ne présentera autrement

| manque | portée |
|---|---|
| ⚠️ **aucune conformité visuelle** aux planches de référence | la fenêtre s'ouvre ; elle n'a été comparée à aucune cible |
| **le catalogue multilingue** : aucun catalogue de traduction dans NKGui | § 7 : la forme déclare des clés, **rien ne les résout encore** |
| **les icônes** : aucune notion d'icône dans NKGui ; l'adaptateur peint un carré plein ; 193 glyphes définis deux fois | § 8 est intégralement à faire |
| **les blueprints** : `NKGraph` est hors des 175 modules du build, aucun éditeur visuel | § 9.7 |
| **la création de composants ex nihilo** | on compose ce qui est déclaré |
| **l'export** | rien n'est écrit ; § 11 est une cible |
| **le launcher** | l'application ouvre directement l'éditeur |
| **l'édition d'un thème dans l'outil** | le format texte existe, l'éditeur non |
| **l'ancrage complet** (marges par bord) | différé, nommé dans le code |
| **le modèle d'IA spécialisé** | il s'entraînera sur des déclarations, et il n'y en a presque pas |
| **la zone sûre** : aucune ancre, aucune simulation d'appareil | § 9.5bis est intégralement à faire |

⚠️ **Un piège de nom à connaître** : `NKEditorKit/NkEditorExport.h` **n'a rien à
voir avec l'export d'interfaces** — ce sont les macros `dllexport`/`dllimport`
du module. Quiconque cherche l'export du § 11 par le nom du fichier trouvera le
mauvais fichier et croira que la chose existe.

---

## 4. Les pages et les écrans

### 4.1 Carte de navigation cible

```
Écran d'accueil (launcher)                                    📝 à faire
  ├─ Nouveau document ─────────────────┐
  ├─ Ouvrir / Importer ────────────────┤
  └─ Nouveau par l'IA ─────────────────┤
                                       ▼
        Fenêtre principale (éditeur)                          ✅ existe (5 panneaux)
          ├─ Barre de titre                                   ✅ (coquille)
          ├─ Barre de menus                                   🟡 (la coquille la porte, l'application ne la remplit pas)
          ├─ Barre d'outils                                   📝 à faire
          ├─ Panneau Palette          (gauche)                ✅
          ├─ Panneau Composition      (gauche)                ✅
          ├─ Panneau Aperçu           (centre)                ✅
          ├─ Panneau Propriétés       (droite)                ✅
          ├─ Panneau IA               (bas)                   ✅ (la place, pas le modèle)
          ├─ Panneau Comportement     (bas)                   📝 à faire
          ├─ Panneau Icônes           (centre, second onglet) 📝 à faire
          ├─ Barre d'état             (bas)                   🟡 (la coquille la porte)
          ├─ Palette de commandes     (superposition)         ✅ (coquille) + 4 commandes
          ├─ Fenêtre Aperçu interactif (secondaire)           📝 à faire
          ├─ Fenêtre Gestionnaire de callbacks (secondaire)   📝 à faire
          ├─ Dialogue Export (modal)                          📝 à faire
          └─ Fenêtre Paramètres (secondaire)                  📝 à faire
```

### 4.2 Les écrans, un par un

#### E1 — Écran d'accueil *(à faire)*

Point d'entrée avant tout document. Il porte : les documents récents en
vignettes (nom, date, aperçu du dernier état), les quatre actions de départ
(nouveau, ouvrir, importer, nouveau par l'IA), l'accès aux paramètres, et le
numéro de version.

*Pourquoi il n'existe pas encore, et pourquoi ce n'est pas grave* : l'application
ouvre directement l'éditeur avec un document de départ. Pour la prise en main
(§ 1.1), **c'est mieux que rien du tout** — un écran d'accueil qui précède une
page blanche n'apprend rien. Il devient utile le jour où il y a des documents à
retrouver.

**État vide** : première utilisation, aucune vignette — l'écran met en avant
« Nouveau document » et « Nouveau par l'IA », avec une ligne expliquant en une
phrase ce que fait l'outil.

#### E2 — Fenêtre principale, vue Composition *(existe)*

C'est l'écran de travail, et le seul qui existe aujourd'hui. Cinq panneaux :

| panneau | position | ce qu'il fait | état |
|---|---|---|---|
| **Palette** | gauche | liste les composants **du registre** — jamais une liste en dur — et les pose dans le document | ✅ |
| **Composition** | gauche | l'arbre du document : un composant dans un autre, même mécanisme aux deux échelles ; renommer, reparenter, réordonner | ✅ |
| **Aperçu** | centre | dessine le document **avec la fonction même** qu'une application appellera ; c'est aussi là que se simuleront les **zones sûres** d'un appareil (§ 9.5bis) | ✅ / 📝 pour la simulation |
| **Propriétés** | droite | **boucle sur les tables de la déclaration** : paramètres, variantes, jetons, métriques, taille, agencement, provenance | ✅ |
| **IA** | bas | un prompt en français, un backend remplaçable, une sortie qui passe par la même porte que la main | ✅ (la place) |

⚠️ **Ce que « existe » veut dire ici, exactement** : ces panneaux compilent, la
sonde les exerce sans écran, et **la fenêtre s'ouvre** depuis le 19/08. Mais leur
conformité aux planches de `Applications/Nogee/design/` **n'est pas
revendiquée** : personne ne les a comparés à une cible visuelle. Et le premier
regard a déjà rapporté deux défauts (§ 3.1) — c'est ce qu'on doit attendre du
second.

#### E3 — Panneau Comportement *(à faire)*

En bas, ancrable, extensible en plein écran, deux sous-onglets :

- **Événements** — la liste des événements déclarés par le composant sélectionné,
  avec leur charge (`onSelect(index: Int, path: String)`). Cette liste **est déjà
  produite par le code** : elle se lit dans la déclaration et s'imprime.
- **Graphe** — le blueprint branché sur un événement. C'est ici que
  l'interpréteur du dépôt trouverait son premier consommateur.

*Ce qui manque pour le faire* : rien du côté de la déclaration (les événements y
sont, avec leur charge). Tout du côté du graphe : `NKGraph` est hors du build et
aucun éditeur visuel n'existe.

#### E4 — Panneau Icônes *(à faire)*

Second onglet de la zone centrale. Rodolf : **les icônes se dessinent dans
NkUIDesign**. Voir § 8.

#### E5 — Aperçu interactif *(à faire)*

Une fenêtre séparée où l'interface **tourne pour de vrai** : les événements
partent, les callbacks s'affichent dans une console, la souris et le clavier
agissent. C'est le « tester leur fonctionnement » de Rodolf.

*Pourquoi une fenêtre séparée et pas le panneau Aperçu* : dans le panneau, un
clic doit **sélectionner un nœud pour l'éditer** ; dans l'aperçu interactif, le
même clic doit **activer le bouton**. Deux sens pour un même geste : deux
fenêtres, et l'utilisateur sait toujours laquelle il regarde.

#### E6 — Gestionnaire de callbacks *(à faire)*

Vue d'ensemble : tous les événements de tous les nœuds du document, ce à quoi
ils sont branchés, et **ce qui ne l'est pas**. Un événement non branché est une
information, pas une erreur — mais il doit se voir.

#### E7 — Dialogue Export *(à faire)* → § 11

#### E8 — Fenêtre Paramètres *(à faire)* → § 10

---

## 5. L'interface : disposition, ancrage, barres, commandes, raccourcis

### 5.1 Le principe de disposition

Trois colonnes et deux bandeaux, à la manière des planches de référence :
outils à gauche, travail au centre, propriétés à droite ; barres en haut, état
en bas. **Densité élevée, texte petit, pas d'espace perdu** — c'est un outil
professionnel, pas une application grand public. Angles droits sur les panneaux
ancrés ; arrondis discrets sur les boutons et les champs.

### 5.2 L'ancrage *(porté par la coquille — existe)*

L'ancrage n'est **pas** écrit par NkUIDesign : il vient de `NKEditorKit`, et
c'est la règle du dépôt (aucune application ne réimplémente ce que la
bibliothèque porte).

Ce que la coquille sait faire aujourd'hui, et que NkUIDesign hérite sans une
ligne : ajouter un panneau sur un côté, le focaliser, le fermer, le détacher, le
maximiser, le replier, sauver et recharger la disposition, la réinitialiser.

**Ce que NkUIDesign en fait** : il pose ses cinq panneaux sur quatre côtés au
démarrage. **Ce qu'il n'en fait pas encore** : sauver la disposition de
l'utilisateur entre deux sessions — la coquille sait le faire, l'application ne
l'appelle pas.

### 5.3 La barre de titre *(coquille — existe)*

Titre du document et état de modification, logo de l'application, boutons de
fenêtre. La coquille accepte un logo par une **poignée de texture opaque** —
même principe que les icônes (§ 8) : l'identifiant appartient à
l'application, pas à la bibliothèque.

⚠️ **Le titre doit dire ce qui est ouvert et s'il est modifié.** C'est aujourd'hui
un titre fixe. Petit, mais c'est le premier endroit où l'utilisateur cherche
« est-ce que j'ai enregistré ? ».

### 5.4 La barre d'état *(coquille — existe ; contenu à faire)*

Le pied de fenêtre porte, de gauche à droite : le **backend graphique retenu**
(§ 10 — c'est là qu'on le lit sans ouvrir un journal), le nœud sélectionné et son
composant, le compte de nœuds du document, la **langue courante** (§ 7), et
l'indicateur d'enregistrement.

*Pourquoi le backend dans la barre d'état* : parce que la règle du dépôt exige
qu'on sache sur quoi on mesure. Des heures ont été perdues à croire qu'on
testait Vulkan alors qu'on testait OpenGL.

### 5.5 Les menus *(coquille — existe ; contenu à faire)*

La coquille porte le mécanisme (barre de menus, menu Fichier, menu applicatif,
menu des panneaux, menu contextuel avec sous-menus). NkUIDesign ne les remplit
pas encore. La cible :

| menu | entrées |
|---|---|
| **Fichier** | Nouveau · Ouvrir · Enregistrer · Enregistrer sous · Importer · **Exporter** · Récents · Quitter |
| **Édition** | Annuler · Rétablir · Couper · Copier · Coller · Dupliquer · Supprimer |
| **Composant** | Poser… · Promouvoir en composant · Éditer la déclaration · Variantes |
| **Affichage** | Panneaux · Thème (Clair/Sombre/…) · **Langue** · Réinitialiser la disposition · Zoom |
| **Comportement** | Événements · Graphe · Gestionnaire de callbacks · Aperçu interactif |
| **Aide** | Documentation · Raccourcis · À propos |

⚠️ **Annuler / Rétablir n'existent pas**, et c'est le manque le plus coûteux pour
la prise en main : un outil de design sans annulation se manipule avec peur. À
mettre haut dans l'ordre des travaux.

### 5.6 La palette de commandes *(coquille — existe ; 4 commandes)*

Une superposition, ouverte au clavier, qui filtre par nom toutes les commandes
enregistrées **et affiche leur raccourci**. C'est le mécanisme d'apprentissage
décrit en § 1.1 : on cherche par le nom, on repart avec le raccourci.

Quatre commandes sont enregistrées aujourd'hui — `Document: Enregistrer`,
`Document: Recharger`, `Document: Nouveau`, `Application: Quitter`.

**Règle à tenir** : *toute action de menu est aussi une commande*. Une action
qui n'existe que dans un menu est introuvable par quelqu'un qui ne connaît pas
l'arborescence des menus. Le nommage suit `Domaine: Verbe complément`, parce que
la palette filtre sur le texte et que le domaine en tête regroupe visuellement.

### 5.7 Les raccourcis *(mécanisme existant, table dédiée dans le kit)*

| raccourci | action | état |
|---|---|---|
| `Ctrl+N` | Nouveau document | ✅ |
| `Ctrl+S` | Enregistrer | ✅ |
| `Ctrl+R` | Recharger | ✅ |
| `Ctrl+Q` | Quitter | ✅ |
| `Ctrl+Z` / `Ctrl+Y` | Annuler / Rétablir | 📝 |
| `Ctrl+D` | Dupliquer le nœud | 📝 |
| `Suppr` | Supprimer le nœud | 📝 |
| `Ctrl+P` | Palette de commandes | 📝 (la coquille l'ouvre, aucun raccourci n'y mène) |
| `F5` | Aperçu interactif | 📝 |
| `Ctrl+,` | Paramètres | 📝 |

Les raccourcis passent par la table du kit (`NkShortcutTable`) : ils sont donc
**données**, pas conditions écrites en dur — condition nécessaire pour qu'on
puisse un jour les remapper depuis les paramètres.

---

## 6. Les thèmes

### 6.1 La règle, et elle est absolue

**Aucune couleur, aucune taille, aucun rayon en dur. Nulle part.** Tout passe
par un **jeton** : un nom stable que le thème résout. C'est l'exigence de
Rodolf sur la bibliothèque de composants (*« en changeant si possible le
thème »*), et c'est ce qui permet à un même composant de servir quatre éditeurs.

Le test est simple et se pose avant chaque ligne : *si je change de thème, cette
valeur suit-elle ?* Si non, c'est une valeur en dur, même si elle est jolie.

### 6.2 Ce qui existe *(NkTheme, dans le kit)*

- **Un rôle = un nom + une position d'énumération.** L'énumération est l'accès du
  code (indexation directe, vérifiée à la compilation) ; le **nom** est l'accès du
  fichier (un thème s'écrit à la main dans un fichier texte). L'énumération est
  **append-only** : un rôle inséré au milieu décalerait tous les suivants et un
  thème déjà enregistré relirait les mauvaises couleurs.
- **Deux thèmes complets** : *Sombre* (palette GitHub Dark) et *Clair*. Le clair
  n'**inverse** pas les gris, il les **remplace** — inverser donnerait des gris
  moyens sales — et les couleurs porteuses de sens y sont **assombries** pour
  rester lisibles.
- **Les couleurs porteuses de sens sont dans le thème**, pas dans le code : axes,
  états de sélection, types de contenu, en-têtes de nœuds de graphe. Laissées en
  dur, le thème clair serait illisible et personne ne s'en apercevrait avant la
  capture d'écran.
- **L'héritage au chargement** : un thème utilisateur ne redéfinit presque jamais
  quarante couleurs, il en change trois. Le chargement part **toujours** d'une
  base complète et n'écrase que ce que le fichier mentionne. Un chargeur naïf
  laisserait les autres à zéro — c'est-à-dire noir — et le thème paraîtrait
  « chargé » tout en étant inutilisable.
- **Les rôles d'application s'enregistrent.** Un éditeur 3D a besoin d'un
  « anneau de brosse » qui ne concerne pas les autres. Ces rôles vivent dans une
  table d'extension, sous un nom préfixé (`nk3d.anneau_brosse`). Conséquence
  recherchée : un thème écrit pour une application se charge **sans erreur** dans
  une autre — ses rôles inconnus sont **comptés, pas rejetés**.
- **Le contraste se mesure.** Le thème sait dire sa pire paire. Un correctif a
  déjà été imposé par cette mesure : un ambre à 2,35 de contraste passait sous le
  seuil de 3,0 et le contour de sélection se perdait dans le fond.

### 6.3 Ce qui manque

| manque | conséquence |
|---|---|
| **les jetons de métrique** (espacements, hauteurs de ligne, rayons au-delà des quatre existants) | les métriques vivent aujourd'hui dans la déclaration de chaque composant ; deux composants voisins peuvent donc respirer différemment sans que rien ne le signale |
| **l'éditeur de thème dans l'outil** | on écrit un thème dans un fichier texte, à l'aveugle |
| **la bascule Clair/Sombre à l'exécution** | les deux thèmes existent ; rien ne les échange en cours de session |

### 6.4 L'édition d'un thème — la cible

Un panneau ou une fenêtre où l'on voit, **côte à côte, le rôle et son rendu** :

1. la liste des rôles, groupée (structure, texte, accents, états, types, graphe,
   vue 3D), avec un aperçu de la couleur et sa valeur ;
2. un sélecteur de couleur, et la modification **s'applique immédiatement à
   l'interface entière** — c'est le seul moyen honnête de juger une couleur ;
3. le **contraste affiché en continu**, avec la pire paire nommée, et un
   avertissement sous le seuil ;
4. **hériter d'un thème de base** au lieu de repartir de zéro ; enregistrer sous
   un nom ; recharger, avec le **compte de lignes inconnues** affiché ;
5. les rôles d'application affichés dans une section à part, préfixés, pour qu'on
   voie tout de suite ce qui appartient à l'outil et ce qui appartient à l'hôte.

⚠️ **Le piège à éviter** : proposer une palette « libre » où l'utilisateur pose
une couleur sur un élément particulier. Ce serait rouvrir la porte de la valeur
en dur, du côté utilisateur cette fois. **On édite un rôle, jamais un élément.**

---

## 7. Les langues

> **Presque rien de cette section n'existe.** Un seul point est acquis : **la
> forme de déclaration décide que les libellés sont des clés** (§ 7.3, point 4).
> Tout le reste — catalogue de traduction, résolution, changement à chaud,
> invalidation des mesures, sélecteur — est **à faire**, et n'existe ni dans
> NkUIDesign ni dans NKGui.

### 7.1 Où ça vit — et ce n'est pas ici

**Le multilingue vit dans NKGui, pas dans NkUIDesign** (Rodolf, 2026-08-18).
C'est la règle du socle partagé appliquée au texte : le foyer est la
bibliothèque, donc **toutes** les applications du dépôt en héritent — éditeurs,
jeux, démos, PV3DE — au lieu que chacune bricole la sienne.

**Cette section décrit donc comment NkUIDesign *consomme* celui de la
bibliothèque**, et ce qu'il exige d'elle. Elle ne décrit pas un système de
traduction propre à l'outil : en écrire un ici serait exactement la faute que le
dépôt vient de mesurer sur les peintres — la même chose écrite deux fois, par
deux mains, sans contact.

### 7.2 La distinction qu'il ne faut jamais confondre

| ce qui change | effet |
|---|---|
| **le backend graphique** (§ 10) | **redémarrage** de l'application |
| **la langue** | **à chaud** — l'interface se retraduit sans rien fermer |

Ce sont deux réglages voisins dans la fenêtre Paramètres et **opposés** dans leur
coût. Les traiter pareil serait une régression de confort dans un cas, et un
mensonge dans l'autre.

### 7.3 Les quatre points, et le troisième est celui qu'on rate

1. **Le texte passe par une clé.** Jamais un littéral figé dans le dessin. Un
   libellé écrit en clair dans le code est un libellé qui ne sera jamais traduit,
   et personne ne saura qu'il existe.

2. **Changer de langue recalcule la mise en page** — pas seulement la chaîne.
   C'est le point qu'une implémentation rate, et il mérite d'être dit lentement :

   > Un mot allemand est plus long qu'un mot anglais. Un libellé qui tenait
   > déborde. **Tout cache de largeur de texte doit être invalidé au changement
   > de langue** — sinon on affiche la nouvelle langue avec les mesures de
   > l'ancienne, et le défaut est *visuel*, pas fonctionnel : rien ne plante,
   > tout est décalé.

   **Le témoin qui le prouve**, et il doit exister avant qu'on déclare la chose
   faite : une **pseudo-langue d'essai** dont chaque traduction est
   systématiquement plus longue que le français. On bascule dessus ; si une
   largeur ne change pas, le cache n'a pas été invalidé. Ce témoin ne demande ni
   traducteur, ni GPU, ni seconde langue réelle — il tourne dans la sonde. *Un
   contrôle qui ne peut pas échouer ne mesure rien* : celui-ci échoue tant que
   l'invalidation n'est pas écrite.

3. **Une clé manquante se voit.** Elle s'affiche **comme clé manquante**
   (le nom de la clé, marqué), jamais comme un vide silencieux. Un libellé vide
   se lit comme un bug de dessin ; une clé affichée se lit comme une traduction à
   faire — et se retrouve par recherche de texte. C'est la discipline de tout ce
   corpus : ce qui manque se signale.

4. **Les interfaces produites par NkUIDesign sont traduisibles aussi.** Les
   libellés d'un composant déclaré sont des **clés**, pas du texte. Sinon l'outil
   produit des interfaces monolingues — et il serait absurde qu'un outil
   multilingue fabrique des interfaces qui ne le sont pas.

   ✅ **C'est décidé et c'est dans la forme, depuis le 19/08.** Et la décision
   mérite d'être connue, parce qu'elle est contre-intuitive : **aucun champ n'a
   été ajouté**. La tentation était de poser une clé *à côté* du libellé ; ç'aurait
   été **deux sources de vérité pour une même chose** — le défaut que cette forme
   existe précisément pour supprimer, et déjà payé deux fois dans le mois.

   > **Le libellé EST la clé.** Il n'y a rien à synchroniser parce qu'il n'y a
   > qu'un champ.

   Une clé a une **forme** contrôlable (pas d'espace, pas d'accent, pas de
   majuscule), et un libellé écrit en clair se signale **en note, jamais en
   erreur** — rougir aurait cassé le travail d'un autre agent qui n'avait rien
   cassé.

   ⚠️ **La migration n'est PAS faite, et c'est délibéré** : l'application affiche
   encore les titres tels quels, et **aucun catalogue n'existe dans NKGui**.
   Migrer maintenant afficherait `content_browser.title` à l'écran à la place de
   « Navigateur de contenu ». **L'ordre est donc : le catalogue d'abord, la
   migration ensuite.**

5. **La police doit couvrir la langue.** Le français passe partout ; une écriture
   non latine exige un atlas adapté. **À dire plutôt qu'à découvrir** : si la
   police ne couvre pas la langue choisie, l'outil le signale au moment du choix.

### 7.4 Ce que NkUIDesign expose à l'utilisateur

- Un **sélecteur de langue** dans les Paramètres *et* dans le menu Affichage, qui
  agit **immédiatement**.
- La **langue courante dans la barre d'état**.
- Un **compteur de clés manquantes** pour la langue courante, visible : c'est ce
  qui transforme « la traduction est incomplète » en une quantité qu'on peut
  suivre.
- **Français et anglais au minimum**, changeables sans redémarrer. Contexte
  immédiat : **150 étudiants à la rentrée**.

---

## 8. Les icônes

> **Rien de cette section n'existe non plus.** NKGui n'a aucune notion d'icône ;
> l'adaptateur de peinture actuel dessine, à la place d'une icône, un **carré
> plein de la bonne couleur et de la bonne taille** — donc *la mise en page est
> déjà juste* le jour où l'atlas arrive, mais aucune icône n'est visible.

### 8.1 Pourquoi c'est dans NkUIDesign

Rodolf, 2026-08-18 : **les icônes se dessinent dans NkUIDesign**. La conséquence
heureuse est mesurée : les **193 glyphes définis deux fois** dans le dépôt (102
d'un côté, 91 de l'autre) deviennent **un jeu unique, dessiné et éditable**.

### 8.2 Les quatre décisions déjà prises

1. **Vectorielles.** Une icône est un jeu de **chemins**, pas une image. Deux
   raisons, et les deux sont des exigences du corpus : elle **se recolore par
   jetons de thème** (donc elle suit le thème clair sans qu'on redessine), et
   elle **suit le facteur DPI** sans se flouter.
2. **La rastérisation en atlas est une étape de sortie, pas la source.** On
   édite des chemins ; on produit un atlas.
3. **Le dessin se fait par poignée opaque, jamais par une énumération figée.**
   NKGui charge un atlas et dessine par identifiant. Le **vocabulaire d'icônes
   appartient au projet, pas à la bibliothèque** — c'est ce qui évite qu'une
   énumération commune grossisse à chaque application, exactement comme pour les
   rôles de thème (§ 6.2). C'est déjà tenu dans le type de l'interface de
   peinture : l'icône y est un entier opaque.
4. **Recolorables par rôle.** Une icône ne porte pas sa couleur : elle porte un
   **rôle** que le thème résout au dessin.

### 8.3 Ce que l'éditeur d'icônes doit offrir *(cible)*

- une **grille de travail** avec repères, aimantation, et l'icône rendue à ses
  tailles réelles à côté (16, 20, 24, 32) — parce qu'une icône jolie en grand
  est souvent illisible en petit, et c'est le seul moyen de le voir ;
- les **outils de chemin** : trait, courbe, rectangle, cercle, opérations
  booléennes, épaisseur de trait ;
- l'**aperçu simultané en thème clair et sombre** ;
- le **jeu d'icônes** comme une bibliothèque nommée, avec recherche, et
  l'affectation d'un rôle de couleur par icône ;
- l'**import** des glyphes existants pour absorber les 193 en double sans les
  redessiner ;
- l'**export de l'atlas** avec sa table de poignées (§ 11).

### 8.4 Le point de vigilance

⚠️ **Une icône n'est pas un composant.** Elle n'a ni rôle de capacité, ni
événement, ni variante. La tentation de la faire entrer dans la déclaration de
composant existera — il faut y résister : ce serait faire payer à la forme un
domaine qui n'est pas le sien, exactement le raisonnement qui a séparé la
déclaration de composant de la réflexion d'objets.

---

## 9. Les widgets et les composants — la déclaration

C'est le cœur du produit. **Un éditeur ne peut composer que ce qui est décrit par
des données** : si un composant n'existe qu'en C++ compilé, aucun éditeur ne peut
l'assembler, le paramétrer ni le sauver.

### 9.1 Ce qu'un composant déclare — les neuf choses

| # | ce qui est déclaré | à quoi ça sert | état |
|---|---|---|---|
| 1 | **les paramètres** | ce qu'une application règle (nombres, textes, booléens, énumérations) avec leurs **bornes** | ✅ |
| 2 | **les jetons de thème** | les rôles de couleur que le composant consomme — aucune couleur en dur | ✅ |
| 3 | **les métriques** | les longueurs nommées (écart entre cartes, hauteur de ligne…) | ✅ |
| 4 | **les variantes** | *un modèle, N rendus* — grille, liste dense, colonnes | ✅ |
| 5 | **les points de greffe** | où l'application ajoute **son propre dessin** (un badge, une colonne) sans modifier le composant | ✅ |
| 6 | **les événements** | ce que le composant **émet**, avec sa charge | ✅ |
| 7 | **le rôle** | la **capacité** attribuée à une apparence (bouton, bascule, liste…) | ✅ |
| 8 | **l'arbre de sous-éléments** | une apparence est un arbre, pas une image plate | ✅ |
| 9 | **la provenance** | qui a écrit, qui a vérifié, qui a corrigé | ✅ |

Plus, transversalement : **les propriétés de taille et d'agencement** (§ 9.5) et
**l'ancre** — zone sûre ou bord de l'écran (§ 9.5bis).

### 9.2 Le rôle — séparer l'apparence de la capacité

On dessine une apparence, puis on lui **attribue une capacité**. C'est ce qui
permet à **une poignée de rôles de servir des milliers d'apparences** ; sans
cela, chaque dessin embarquerait son comportement, et une bibliothèque de
composants deviendrait une bibliothèque de cas particuliers.

Neuf capacités au catalogue : `container`, `label`, `button`, `toggle`,
`text_field`, `slider`, `list`, `tree`, `drop_target`.

⚠️ **Le contrat est vérifié** : un rôle annoncé et non honoré rougit. Déclarer
« ceci est un bouton » sans émettre ce qu'un bouton émet est une erreur, pas une
approximation.

### 9.3 L'arbre — le même mécanisme aux deux échelles

*« Une apparence est un arbre, pas une image plate ; une interface complète est
un composant qui en contient d'autres. »* Il n'y a donc **pas de type
« document » distinct d'un type « nœud »** : le document est un nœud qui en
contient d'autres. Le jour où l'on voudra enregistrer un document comme
composant réutilisable, **c'est la même structure qui partira** — pas une
conversion.

Un nœud **sans composant est un cadre** : il ne dessine rien, il **arrange**.
C'est ce qui permet à l'arbre d'exister avant qu'un composant conteneur soit
déclaré, et ce n'est pas un composant fantôme — il se lit dans le fichier, se
voit dans l'arbre, et le rendu ne peint rien pour lui.

### 9.4 Les événements et les callbacks

Un composant déclare **ce qu'il émet et avec quelle charge**. Sans cela, ni
l'éditeur ni un blueprint ne peuvent s'y brancher.

⚠️ **La règle qui gouverne la charge** : *une charge interprétable seulement en
possédant l'objet émetteur n'est pas une charge, c'est un pointeur déguisé.* D'où
`onSelect(index: Int, path: String)` et non `onSelect(index: Int)` seul :
l'`index` sert au code, le `path` sert au graphe, qui n'a pas le modèle sous la
main.

⚠️ **Un événement nomme un FAIT, pas un GESTE.** `onActivate`, pas
`onDoubleClick` : un double-clic active, la touche Entrée aussi.

### 9.5 La taille et l'agencement — la position est un résultat

Précision de Rodolf : *« grâce à la définition de s'il est extensible ou garde sa
taille, on peut définir qu'il soit responsive ou non ; ce sont ces propriétés et
celles de ses parents qui définissent tout ça. »*

Cela **dissout la difficulté du dessin absolu** :

- **l'enfant déclare** : `fixed` (figé) · `content` (à son contenu) ·
  `fraction` (une part du parent) · `weight` (à poids) · `expand` (extensible),
  avec `min` et `max` ;
- **le parent déclare son agencement** : `row` · `column` · `grid` · `anchor` ;
- **la position devient un résultat, plus une donnée.**

**Aucun `x`, aucun `y`, nulle part.** L'outil laisse poser à la souris, mais il
écrit ces propriétés-là. La souris n'est pas interdite — elle est **traduite** :
tirer un bord écrit une taille ou un poids ; déplacer un nœud écrit un parent et
un rang. **Jamais un point.**

⚠️ **Un piège dont le dépôt a déjà payé le prix** : deux solveurs qui
interprètent ces mots chacun de leur côté, c'est **deux sémantiques pour une
même déclaration**. Le cas s'est produit — l'un répartissait le reste en boucle,
l'autre en une passe — et il a fallu que le second adopte le premier. Le sens de
`extensible`, `à poids`, `min`, `max` est fixé **à un seul endroit**, et tout le
reste appelle.

### 9.5bis La zone sûre — une **ancre**, pas une marge

**Ajout de Rodolf** : *« Pour le design d'interface mobile, penser à la zone
sûre. »* Ce n'est pas un détail d'affichage mobile : c'est une **seconde ancre**
dans le système d'agencement, et elle s'inscrit exactement dans la logique du
§ 9.5 — *la position est un résultat, pas une donnée*.

> **État : 📝 rien n'existe.** Ni notion de zone sûre, ni ancre, ni simulation.

#### Le point qu'une spécification rate d'habitude

**La zone sûre n'est pas une constante.** Elle change à la rotation, à
l'ouverture du clavier, en écran partagé, et d'un appareil à l'autre. Elle **se
demande à la plateforme à l'exécution** — jamais codée en dur. Une marge fixe est
juste sur le téléphone qu'on a sous la main et **fausse sur le modèle suivant**.

C'est le même raisonnement que celui qui interdit d'enregistrer une coordonnée :
une valeur qui dépend du contexte d'affichage n'a rien à faire dans le document.

#### Ce que ça impose à la déclaration : deux ancres, choisies **par élément**

Un élément déclare s'il s'ancre **à la zone sûre** ou **au bord de l'écran**.

| jusqu'au **bord** | dans la **zone sûre** |
|---|---|
| fonds, images de couverture, dégradés, listes qui défilent sous la barre | texte, boutons, champs — **tout ce qui se lit ou se touche** |

**Et c'est la subtilité qui sépare une interface mobile correcte d'une
bricolée** : un fond qui s'arrête à la zone sûre laisse des bandes disgracieuses
en haut et en bas ; un bouton qui déborde sous l'indicateur de geste est
**inatteignable**. Les deux fautes sont commises par le même réflexe — appliquer
le même traitement à tout.

⚠️ **Jamais un réglage global.** « L'application respecte la zone sûre » est une
phrase qui produit les bandes disgracieuses ; « l'application l'ignore » produit
les boutons inatteignables. **Le choix se fait élément par élément**, et c'est
pour cela qu'il appartient à la déclaration.

#### Ce que NkUIDesign doit offrir

**Simuler des zones sûres dans l'éditeur** : encoche haute, indicateur de geste
bas, coins arrondis, rotation portrait/paysage, clavier ouvert. Sans cette
simulation, le défaut se découvre **sur le téléphone** — c'est-à-dire au moment
où il coûte le plus cher, et où personne n'a l'outil sous la main pour le
corriger.

Concrètement, dans le panneau Aperçu : un sélecteur d'appareil simulé à côté du
sélecteur de largeur, et un affichage des zones masquées **par-dessus** le rendu,
que l'on peut montrer ou cacher.

### 9.6 La provenance — trois champs, et ils ne sont pas décoratifs

`auteur` · `vérifié` · `corrigé`. Portés par la déclaration **et** par
l'instance, **persistés** dans le fichier.

Pourquoi maintenant : chaque déclaration produite ici est de la **donnée
d'entraînement** — mais **les sources n'ont pas la même valeur**. Sans
provenance, tout se vaut et le modèle apprend **la moyenne**. Trois champs
aujourd'hui contre une refonte plus tard.

Et la boucle qui se referme : quand l'IA propose et que la main corrige, la
correction se marque toute seule. **Le corpus se fabrique sans effort
supplémentaire.**

### 9.7 Les blueprints — ce qui manque, dit franchement

Les événements sont déclarés avec leur charge, et le bloc de contrat s'écrit
déjà au format cible. **Mais rien ne s'y branche** : l'interpréteur du dépôt est
réel et sans consommateur, son module est hors du build, et aucun éditeur visuel
n'existe. C'est le plus gros morceau restant de la description de Rodolf, et il
ne se fera pas par accident.

### 9.8 Créer un composant *(à faire)*

Aujourd'hui on **compose** ce qui est déclaré. La cible ajoute : dessiner une
apparence, lui attribuer un rôle, nommer ses paramètres et ses jetons, déclarer
ses événements — et **promouvoir un sous-arbre du document en composant**
réutilisable. Cette dernière voie est la plus importante pour la prise en main :
on ne crée pas un composant depuis rien, on **promeut** quelque chose qui
marche déjà.

---

## 10. Les paramètres, dont le backend graphique

### 10.1 La fenêtre Paramètres *(à faire)* — quatre sections

| section | contenu |
|---|---|
| **Apparence** | thème (Clair/Sombre/personnalisé), éditer un thème (§ 6.4), taille de police, densité |
| **Langue** | langue de l'interface — **à chaud** (§ 7), police et couverture, compte de clés manquantes |
| **Rendu** | **backend graphique** — **au redémarrage** (§ 10.2), échelle DPI, plafond d'images par seconde |
| **Édition** | dossier de travail, raccourcis, enregistrement automatique, backend d'IA (§ 12) |

### 10.2 Le backend graphique — la règle, et elle est stricte

**Toute application du dépôt doit laisser choisir son backend graphique** parmi
ceux disponibles : OpenGL, Vulkan, DX11, DX12, Metal.

**Ce qui existe déjà dans NkUIDesign** :

- `--gfx=auto|opengl|vulkan|dx11|dx12|metal|software` en ligne de commande ;
- `NK_GFX_API=…` en variable d'environnement ;
- **l'ordre attendu** : la ligne de commande gagne sur la variable
  d'environnement, qui gagne sur la détection automatique ;
- **le choix est journalisé au démarrage**, avant toute création de contexte :
  *quel backend a été demandé, par quelle source, lequel a été retenu* ;
- **un backend indisponible se dit — il ne se remplace pas en silence** : un
  backend demandé et refusé fait **échouer le lancement avec la raison**. Un
  repli muet est de la famille « ça répond toujours » : on croit mesurer ce qu'on
  a demandé, et on mesure autre chose ;
- **la détection automatique reste le défaut.** L'option sert à forcer, pas à
  obliger l'utilisateur à choisir ;
- ⚠️ **`metal` est accepté à l'analyse et refusé à la résolution** : l'énumération
  de la coquille n'a pas d'entrée Metal. Le taire reviendrait à lancer autre
  chose en silence sur macOS. **Le manque est porté au canal** — il est dans le
  kit, pas dans l'application.

**Bénéfice immédiat, mesuré** : quand le GPU est occupé — une campagne
d'entraînement, par exemple — pouvoir forcer un backend plus léger est parfois la
différence entre tester et attendre des heures.

### 10.3 Changer de backend **redémarre l'application** — et comment le rendre indolore

C'est la précision de Rodolf, et elle ne se contourne pas : un contexte
graphique ne se remplace pas sous une fenêtre vivante.

**Ce que l'interface doit faire, dans cet ordre** :

1. **prévenir avant de valider** — un texte explicite : *« Le changement de
   backend graphique nécessite un redémarrage de NkUIDesign. »* Pas une note en
   petit après coup ;
2. **sauver avant** — le document, la disposition des panneaux, la sélection, la
   langue, le thème. Un redémarrage qui perd le travail transforme un réglage en
   punition ;
3. **restaurer après** — on rouvre sur le même document, la même disposition, le
   même nœud sélectionné. L'utilisateur doit avoir l'impression d'un
   clignotement, pas d'un recommencement ;
4. **dire si ça a échoué** — si le backend demandé est refusé au redémarrage, on
   le dit, on nomme la raison, et **on ne rebascule pas en silence** sur un
   autre. Proposition (à valider) : rouvrir avec l'ancien backend **en l'annonçant
   explicitement**, plutôt que de laisser l'application morte.

⚠️ **Ne jamais confondre avec la langue** : la langue change **à chaud**, sans
rien fermer. Deux réglages voisins, deux coûts opposés — l'interface doit le
rendre évident, pas le laisser découvrir.

---

## 11. L'export

> **Rien n'existe.** Et attention au piège de nom : `NkEditorExport.h` du kit
> concerne les macros de liaison, pas cette section.

### 11.1 Ce qu'on exporte, et l'ordre d'importance

| # | sortie | pour qui | pourquoi |
|---|---|---|---|
| 1 | **la déclaration** (le format natif) | l'outil lui-même, l'IA, une autre machine | c'est **la** sortie : elle se relit sans perte |
| 2 | **le bloc de contrat** (événements et callbacks) | le développeur qui branche l'interface | il s'écrit **déjà** aujourd'hui, la sonde l'imprime |
| 3 | **l'atlas d'icônes** + sa table de poignées | l'application qui affiche les icônes | § 8 |
| 4 | **le thème** en fichier texte | une autre application du dépôt | § 6 |
| 5 | **les clés de traduction** du document | le traducteur | § 7.3, point 4 |
| 6 | une **image** de l'interface | une revue, une publication | pratique, pas structurant |

### 11.2 La contrainte non négociable : l'aller-retour ne dégrade pas

Le format éditable est **l'interface entre l'IA et l'outil**. Donc chaque maillon
— import, éditeur, sérialiseur — doit **relire ce qu'il écrit sans rien perdre**.
Un aller-retour qui dégrade casse tout le principe de la §12.

**Ce qui est déjà mesuré** : écrit → texte → relu → **même dessin**, zéro
différence. Et le **compte d'inconnus** est affiché : une ligne que le relecteur
ne comprend pas est comptée, pas ignorée.

### 11.3 Ce que l'export **n'est pas**

**Ce n'est pas une génération de code.** Exporter du C++ compilable serait
séduisant et serait une faute : la sortie ne se relit pas, l'IA ne peut plus
continuer un travail, et la main ne peut plus revenir à la souris. Le dialogue
d'export peut **montrer** le bloc de contrat à recopier — il ne produit pas
l'implémentation.

### 11.4 Le dialogue Export — cible

Une modale : cases des sorties à produire, dossier de destination, et un
**rapport de validation** avant écriture — ce qui est déclaré et non branché, les
rôles annoncés non honorés, les clés de traduction manquantes, les événements
sans callback. **Le rapport s'affiche même quand tout va bien** : un rapport qui
n'apparaît qu'en cas d'erreur n'apprend jamais à le lire.

---

## 12. L'IA intégrée

### 12.1 Le principe, et il vaut pour tout Rihen

*« L'IA fait, mais on peut ajuster manuellement, elle peut continuer ou éditer
l'existant, et on garde le dernier mot. »*

**Tout système génératif de Rihen produit la donnée native éditable du domaine,
jamais un artefact terminal.** Pour l'interface : l'IA produit **la déclaration**
(rôle, arbre, taille, jetons, événements), **pas du code**.

**Les quatre propriétés qui en découlent, et qui sont le produit :**

1. **Indiscernabilité** — une fois posé, ce que l'IA a fait et ce que la main a
   fait sont **le même objet**. Aucune trace de second rang, aucun « contenu
   généré » qu'on ne pourrait qu'accepter ou jeter.
2. **Reprise dans les deux sens** — l'IA continue un travail humain, la main
   corrige un travail de l'IA, **sans perte**.
3. **Le dernier mot à l'humain, toujours** — jamais de « régénérer depuis zéro »
   comme seule issue.
4. **Deux voies d'égale dignité** — on dialogue par prompt **ou** on modifie
   directement. Ni l'une ni l'autre n'est le mode dégradé.

### 12.2 Ce qui existe : **la place**, pas le modèle

⚠️ **Le modèle spécialisé n'existe pas.** Il s'entraînera sur des déclarations, et
il n'y en a presque pas encore. C'est la boucle posée par Rodolf : **l'outil
produit le corpus qui entraîne le modèle**. Le code d'aujourd'hui ne contient
aucune intelligence et n'en revendique aucune.

Ce qui est livré, et qui coûte quelques dizaines de lignes aujourd'hui contre une
refonte plus tard :

1. **un point d'entrée de prompt**, en français ;
2. **un backend remplaçable** (Claude, Ollama, Ilyana demain) derrière une
   interface qui ne suppose rien de l'un ni de l'autre ;
3. ⚠️ **la sortie passe par la MÊME PORTE que la main** : l'IA produit un
   **document**, qui atterrit dans l'arbre par la fonction **qu'utilise aussi le
   copier-coller**. Jamais du code, jamais un artefact à part ;
4. **la provenance se remplit toute seule** — `auteur = ia`, puis la correction
   humaine se marque ensuite ;
5. **le rejeu comme vérificateur** : une proposition qui ne se rejoue pas à
   l'identique est **écartée**. Le moteur est le juge, pas l'œil.

### 12.3 Le rejeu — la pièce qui compte

*« Une paire fausse est apprise fidèlement. »* Un corpus synthétique non vérifié
est pire qu'un petit corpus vrai. Ici le vérificateur est **mécanique** :

```
texte proposé  →  document  →  ré-sérialisé  →  relu  →  MÊME mise en page ?
```

Ce que ce contrôle attrape : une référence inventée, une structure incohérente,
une valeur que l'écrivain ne sait pas réécrire — exactement les fautes qu'un
modèle produit quand il écrit « à peu près » le bon format.

⚠️ **Ce qu'il n'attrape pas, et il faut le dire avec lui** : une interface
parfaitement bien formée et **laide** passe le rejeu sans broncher. Le rejeu
vérifie la **forme**, jamais le **goût**. Le goût reste à l'humain, et c'est
précisément pour cela que le dernier mot lui revient.

### 12.4 Ce qui n'est pas livré, et qui est nommé

- **Aucun backend réseau.** Les sockets écrites à la main ailleurs dans le dépôt
  ne seront **pas recopiées ici** — ce serait une troisième copie, dans une
  application, alors que la directive du dépôt est exactement l'inverse. **Le
  client HTTP doit monter dans un module partagé** ; le manque est porté au canal.
- **En attendant, le backend FICHIER n'est pas un pis-aller** : l'outil écrit le
  prompt, on le passe au modèle de son choix — y compris à la main — et on rend
  la réponse. L'outil est utilisable dès aujourd'hui avec n'importe quel modèle.
- Aucune conversation multi-tours, aucun flux, aucun asynchrone.

### 12.5 L'IA côté prise en main — les trois choses à tenir

1. **Le prompt système est engendré**, jamais recopié : son vocabulaire vient des
   mêmes fonctions que celles qui écrivent le fichier (§ 1.2).
2. **La proposition s'aperçoit avant d'être posée**, et elle se pose **par la
   même porte** que le collage — donc annuler une proposition d'IA et annuler un
   collage sont le même geste.
3. **On peut demander à l'IA de continuer**, pas seulement de recommencer : elle
   reçoit le document courant. Sans cela, la « reprise dans les deux sens » du
   § 12.1 serait une phrase.

---

## 13. Ce qui n'est pas tranché et remonte à Rodolf

Ces points sont **ouverts**. Aucun n'est comblé en silence dans ce document.

1. **La charge « collection ».** Une sélection multiple veut porter *l'ensemble*
   des entrées choisies ; aucun des huit types de charge n'est une liste, et
   c'est délibéré — la table est exactement celle du format cible. Deux issues :
   (1) ajouter `List[T]` à la spécification **et** à la table — petit et direct,
   mais **c'est modifier la spécification** ; (2) ajouter la notion de
   **propriété exposée** — un état qu'un blueprint peut *lire* sans que ce soit
   une charge d'événement, ce qui servirait aussi le chemin courant, le filtre,
   le nœud courant. **Le mur a été rencontré par deux mains indépendamment**,
   donc il est structurel.
2. **Le format de fichier définitif** (texte, JSON, binaire) et la convergence
   avec `.nkgui` v0.2.
3. **Le rôle exact de l'IA spécialisée** — génère-t-elle la description ou autre
   chose ?
4. **Le calendrier.** La **rentrée de septembre passe avant** : ~150 étudiants,
   six cours, semaine du 8 septembre.
5. **La charte visuelle de NkUIDesign** : les documents antérieurs du dossier
   parent donnent une palette gris-bleu propre à l'outil, alors que les jetons du
   kit portent une palette GitHub Dark/Light et que les planches de référence du
   dépôt sont celles de l'éditeur. **Deux specs qui divergent : on ne tranche
   pas.** Le document 3 produit les maquettes sur les jetons du kit (§ 6.2), avec
   la palette antérieure notée en variante — pour que le choix reste à Rodolf.
6. **Le repli d'un backend refusé au redémarrage** (§ 10.3, point 4) : rouvrir
   avec l'ancien backend **en l'annonçant** est une proposition, pas une décision.

---

*Documents liés : `02-specification-claude.md` (structures, contrats, témoins,
ordre d'implémentation) · `03-specification-banani.md` (écrans, états, prompts
visuels) · et, dans le dossier parent, les quatre documents antérieurs, dont la
spécification du langage de description, qui reste la cible de convergence.*
