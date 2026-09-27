# NKUIDesign — spécification de l'application

### Document 1 — ce qu'on attend de l'application

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Version 0.2 (nom de travail « NKDesign ») ; **réécrit le 27/09/2026** à la demande
> de Rodolf : *« la spécification doit être complète, car on va devoir réécrire
> l'application pour qu'elle passe par `.nkgui` »* — puis *« réécris tous les
> documents si tu le juges nécessaire, de 1 à 21 »*.
>
> Ce document absorbe la spécification produit de la refonte (ancien doc 19). Il est
> **autonome** : on comprend ce que doit être NKUIDesign sans en lire un autre. Les
> renvois servent au détail.
>
> **Autorité** (voir l'index, doc 0) : pour le **produit**, ce document fait foi ;
> pour le **langage**, le doc 2 ; pour l'**interface**, le doc 3 ; pour le **modèle et
> l'interface écrite en `.nkgui`**, le doc 5. Les documents de mesure (13, 14, 16, 17)
> et d'historique (4, 6, 8, 10, 11, 12) ne l'emportent sur aucun d'eux.

---

## 0. En une phrase

**NKUIDesign est l'atelier d'interface de la famille Rihen : il ouvre, dessine,
programme (en script et en Blueprint), simule, teste et publie les documents `.nkgui`
de toutes les applications — les siennes comprises — avec leurs thèmes, leurs
composants et leurs actions, en montrant à chaque instant ce que l'application
peindra vraiment.**

---

## 1. Positionnement

NKUIDesign réunit quatre métiers d'habitude séparés :

| métier | à la façon de | ce qu'il produit |
|---|---|---|
| **le design visuel libre** | Figma, Lunacy | formes, vectoriel, booléens, texte, effets, cadres par cible |
| **l'édition de composants d'interface réels** | un éditeur de formulaires | des widgets NKGui avec leur **rôle** et leurs propriétés fonctionnelles |
| **la programmation visuelle du comportement** | les **Blueprints** d'Unreal Engine | ce que l'interface **fait** : réactions, conditions, navigation, messages — en nœuds **ou** en script, les deux étant la même chose |
| **la vérification** | un banc de test | une simulation **navigable** de l'application (Bureau, Web, Mobile) et des **tests d'interaction** rejouables |

Le tout **assisté par l'IA**, qui n'est jamais une boîte noire : ce qu'elle produit
est un document ordinaire, éditable comme ce qu'on a fait à la main.

### 1.1 Ce qui a changé en septembre 2026

| date | fait | conséquence |
|---|---|---|
| 14/09 | l'**interaction** existe dans NKGui : états d'apparence résolus, comportements évalués, `Callback` appelés, `bind` tenus | un document `.nkgui` n'est plus seulement une image : il **agit** |
| 17/09 | `Host`, `sizeRel` / `minSize` / `maxSize`, `DockSpace` par zones nommées | un document peut décrire **une application réelle**, pas un écran de démonstration |
| 26/09 | `include` résolu ; **composants** au format ; **événements** branchés (`Click`, `Changed`, `Hover`) ; NKUIDesign **exporte** en `.nkgui` | les interfaces **se partagent** entre documents et entre applications |
| 26–27/09 | **sept jeux de spécifications** (NkAnima, NKScena, Nogee, NKCraft, PV3DE, NkAntenne, NKUIDesign) écrits pour un **agent** qui produit des `.nkgui` à la main | des **centaines** de documents vont exister — écrits par un agent, pas par NKUIDesign |
| 27/09 | Rodolf **décide** les extensions du format 0.4 (booléens et formes, instructions d'interface, conditions, carte d'application, tests), l'**exécution des Blueprints**, et un **système de thème par application** au style **Unreal Engine 5** | NKUIDesign doit les **porter** : ce document les spécifie |

📌 **Le point le plus important** : si NKUIDesign ne sait pas **ouvrir** les documents
que l'agent écrit, ils vivront **à côté** de l'outil, et personne ne pourra les
retoucher à la souris. L'outil de design de la maison serait contourné par sa propre
production. D'où la première exigence : **le document de NKUIDesign est le `.nkgui`.**

### 1.2 D'un outil général vers l'atelier de la famille — sans fermer la porte au général

| avant | après |
|---|---|
| le document est un `.nkuidoc`, exporté en SVG / PNG / `.nkgui` | **le document est le `.nkgui`** ; ce que l'éditeur seul retient va dans un fichier voisin `.nkgui.meta` |
| un fichier à la fois | **un projet = un dossier d'interfaces** : les applications, `Commun/`, le graphe des `include`, la carte de chaque application |
| des couleurs saisies | **des jetons**, résolus par des **thèmes** que chaque application définit |
| un identifiant libre par bouton | **une action**, choisie dans le **registre** de l'application |
| « l'aperçu doit être fidèle » (principe) | **la fidélité se voit** : ce que le moteur peint, compte, refuse |
| un aperçu d'une fenêtre | **une simulation navigable** de toute l'application, par plateforme |
| « tester » = cliquer à la main | **des tests** écrits, enregistrés, rejoués, qui peuvent échouer |
| parité Lunacy d'abord | **les besoins des applications de la famille d'abord** ; la parité ensuite |

**Rien de ce qui fait l'outil général n'est retiré** : la toile, le vectoriel, les
cadres, les effets, l'export d'images restent. Un client de Rihen qui conçoit une
interface hors de la famille le peut toujours.

---

## 2. Utilisateurs

| qui | ce qu'il fait dans NKUIDesign |
|---|---|
| **le développeur** de la famille (Rodolf, l'équipe) | ouvre les documents de son application, les ajuste, vérifie la fidélité et les actions, lance les tests avant de livrer |
| **le designer** | dessine les écrans, les composants, les formes, les thèmes ; pose une partie du comportement sans dépendre d'un développeur |
| **l'agent** (Claude) | écrit des `.nkgui` à partir des spécifications ; NKUIDesign les **ouvre**, les **valide** et les **montre** tels que le moteur les peindra |
| **l'auteur d'une application tierce** (client de Rihen) | conçoit son application sur NKGui avec **son propre thème**, ses composants, ses tests |
| **l'utilisateur final** d'une application | ne voit pas NKUIDesign ; il **choisit parmi les thèmes** que l'auteur a livrés, si l'auteur l'a permis |

Le cas type : le design et les interactions de base sont posés dans NKUIDesign ; le
développeur ne fait que **brancher** la logique métier (disque, réseau, moteur) par
des callbacks nommés — le **contrat**.

---

## 3. Principes

1. **Le `.nkgui` est le document.** Aucune donnée d'application n'existe seulement dans
   un format d'éditeur. *(Ancien principe « un seul modèle de vérité », enfin tenu.)*
2. **Un fichier écrit par quelqu'un d'autre garde son style et ses commentaires.**
   Ouvrir puis enregistrer sans rien changer donne **le même octet**. Modifier un
   bouton ne change **que** les lignes du bouton.
3. **Non destructif.** Promouvoir une forme en widget, faire un booléen, faire d'une
   sélection un composant : tout se défait, rien ne perd l'information visuelle.
   Aplatir est le seul geste destructif, et il le dit.
4. **Contrat, pas implémentation.** Un comportement dit **quoi** déclencher (nom et
   signature), jamais **comment** l'application le fait.
5. **Le Blueprint et le script sont une seule chose.** Deux écritures d'un même
   comportement, compilées vers la même représentation et exécutées par le même
   évaluateur, dans NKUIDesign comme dans l'application.
6. **La fidélité se voit avant le lancement.** Ce que le monteur peint, compte ou
   refuse est montré **sur la toile**. Écrire ce qui n'est pas encore peint reste
   **voulu** (« écris-le quand même, il voyage ») : on le montre, on ne l'interdit pas.
7. **Ce qui est simulé passe par le même chemin que ce qui tourne** : mêmes
   instructions, même carte d'application, mêmes points de connexion ; seules les
   **doublures** des services diffèrent, et elles se voient comme telles.
8. **Une interaction n'est prouvée que par un test qui peut échouer.**
9. **Un jeton plutôt qu'une couleur ; une action du registre plutôt qu'un nom libre.**
10. **Chaque application a son thème**, défini par son auteur ; elle peut en livrer
    plusieurs au choix de ses utilisateurs, et en recevoir par greffon.
11. **L'IA accélère, elle ne décide pas.** Tout ce qu'elle propose passe par un aperçu,
    s'applique en **une** opération annulable, et devient un document ordinaire.
12. **NKUIDesign est soumis à ses propres règles** : son interface est écrite en
    `.nkgui`, dans le thème de la famille, et passe ses propres contrôles.
13. **Compter, jamais estimer.** Parité, fidélité, couverture de tests, actions :
    des nombres mesurés par des outils, avec la liste nommée de ce qui manque.
14. **Chercher avant de spécifier** (doc 8) : avant d'écrire qu'une chose est à
    construire, la chercher dans le code et dire où l'on a cherché.

---

## 4. Les modules

Chaque module dit **ce qu'il fait**, **ce qui existe déjà** et **le détail** (doc 3
pour l'interface, doc 2 pour le langage).

### 4.1 Le fil : ouvrir, éditer, enregistrer un `.nkgui`

- **Ouvrir** un fichier `.nkgui` ou un **dossier** entier. Le lecteur existe
  (`NkGuiArchive`, `NkGuiRoundTrip` : équivalence **et** identité à l'octet prouvées
  sur le corpus) ; il manque le **passage du document lu au modèle de l'éditeur**.
- **Enregistrer** en respectant le **style** du fichier (indentation, fins de ligne —
  `NkGuiRoundTrip` le détecte), ses **commentaires** et ses **lignes vides**.
- **Ce que l'éditeur seul retient** — position des cadres sur la toile, zoom, notes de
  conception, planches d'essai, disposition des nœuds de Blueprint, contenu fictif des
  `Host` — va dans le fichier voisin **`.nkgui.meta`**, jamais dans le `.nkgui` :
  l'application ne lit pas de données d'éditeur. Un `.nkgui` sans `.meta` s'ouvre
  quand même (disposition automatique).
- **`.nkuidoc`** : **lu pour migration** (décision D1), puis abandonné. La migration
  dit ce qu'elle convertit, ce qu'elle ne peut pas convertir, et garde une copie
  `<document>.avant-nkgui`.
- **Critère** : les six documents existants (NkAnimaEditor ×4, Nogee ×2) s'ouvrent, se
  modifient à la souris, s'enregistrent — **le diff git ne contient que la
  modification**.

### 4.2 Le projet : la famille en un coup d'œil

- **Explorateur de projet** : les applications (NKCraft, NkAnima, NKScena, Nogee, PV3DE,
  NkAntenne, NKUIDesign…), leurs documents, `Commun/`, leurs thèmes, leurs tests ;
  pour chaque document, ses **compteurs** (rôles inconnus, actions inconnues,
  apparences non peintes, hôtes servis / réclamés, tests en échec).
- **Graphe des inclusions** : qui inclut qui ; un cycle ou un fichier introuvable se voit
  **avant** le lancement.
- **Recherche dans tout le projet** : un identifiant, une action, un jeton, un
  composant, un texte.
- **Nouveau projet** : vierge, depuis un gabarit (dont « Application de la famille
  Rihen », qui pose `Commun/`, la carte, le thème Rihen UE5), ou via l'IA.

### 4.3 La toile Design

- **Toile infinie** : toutes les pages d'un document sont des **cadres** posés sur une
  seule toile, chacun avec sa **cible** (Bureau 1440×900, Mobile 390×844, Web…).
- **Outils** : sélection, cadre / section / groupe, formes (rectangle, rectangle
  arrondi, ellipse, triangle, polygone, étoile, ligne, flèche), vectoriel (plume,
  crayon, courbe, ciseaux), **booléens**, texte, média (image, icône, masque), mesure.
- **Édition vectorielle** : sommets à quatre types (dur, miroir, asymétrique, libre),
  poignées de Bézier, rayon par sommet, ajout / suppression / coupe / fermeture.
- **Les booléens** *(Rodolf, 27/09 : « les booléens sont importants pour composer les
  formes graphiques qui vont permettre de poser le design des widgets »)* : union,
  soustraction, intersection, exclusion — **non destructifs** (un groupe qui porte une
  opération), imbricables, avec règle de remplissage ; aplatir, vectoriser un contour ou
  un texte, masque. Une forme composée devient la **surface d'un widget**
  (`appearance { shape }`) et le clic peut suivre la forme (`hitShape`) ; elle peut
  devenir une **icône maison**, sans licence à suivre.
- **Effets** : remplissages (uni, dégradés linéaire / radial / angulaire / losange,
  image), contours, ombres portées et internes empilées, flous, modes de fusion,
  opacité.
- **Disposition** : modes de taille (fixe, contenu, fraction, poids, étendre) avec
  bornes, ancrages, espacements, alignements, points de rupture.
- **Aide au placement** : magnétisme aux objets, repères, règles, grille, distances.
- Ce qui existe : **86 comportements sur 191** de la référence Lunacy sont livrés et
  comptés (doc 14), dont la sélection, les transformations, les calques, l'essentiel de
  l'édition de sommets et le triangulateur de formes concaves (`NkEarcut`).

### 4.4 La hiérarchie

L'arbre de tous les éléments d'une page (formes et widgets), les pages comme racines,
les **composants du projet** en dessous ; icônes de rôle, badges de problème qui
remontent aux parents, marque d'instance (losange), **marque de fidélité** ; glisser
pour réordonner ou changer de parent, avec retour visuel du refus.

### 4.5 L'inspecteur

Panneau contextuel à onglets **Design** (cible, disposition, ancrage, alignement,
espacement, apparence, typographie, effets, points de rupture), **Widget** (rôle,
propriétés du rôle, **action**, disponibilité, infobulle) et **Comportement**
(événements du rôle, leur état lié / non lié / invalide, les comportements qui les
écoutent). Toute couleur se choisit **dans les jetons** (P23 `TokenField`).

### 4.6 La palette, les rôles, les composants

- **Palette** générée depuis le vocabulaire du moteur (doc 7), jamais maintenue à la
  main ; les **rôles proposés** (P2–P23, P28) y sont, badgés « proposé », et se posent
  **avec leur doublure**.
- **Rôles** : attribuer un rôle à une forme, en changer (avec la liste des
  conséquences), en retirer ; un rôle donne un point de départ complet, jamais une cage.
- **Composants** : créer depuis une sélection (Ctrl+K ; Ctrl+Alt+K, le geste de Lunacy, reste un alias), instancier, surcharger
  propriété par propriété, détacher, propager — modèle du doc 15.

### 4.7 Le Blueprint et le script — le comportement

*(Rodolf, 27/09 : « n'oublie surtout pas les blueprints. »)*

- **La toile Behavior** est un éditeur de **Blueprints** au langage visuel de la
  famille (celui de NKCraft et de Nogee) : nœuds à bandeau coloré par famille
  (événement, flux, donnée, interface, action), broches d'exécution dans le bandeau,
  broches de données typées, fils colorés par type, commentaires-cadres, nœuds de
  renvoi, recherche de nœuds contextuelle, mini-carte.
- **Ce qu'un Blueprint sait faire** : répondre à un événement, lire n'importe quel widget,
  décider (branches, cas, boucles), **agir sur un autre widget**, ouvrir un écran ou une
  modale, afficher un message et **brancher sur la réponse**, notifier, appeler le code
  de l'application (`Callback`) ou un service du système (`call`), émettre un
  événement, changer de thème. Fonctions et macros réutilisables, partagées par
  `include`.
- **La vue Code** : le même comportement en script, avec coloration (thème
  `NkGuiSyntax`), complétion des widgets, actions, écrans, services et callbacks,
  diagnostics en direct.
- **Conversion Code ⇔ Nœuds** : toujours dans le sens Code → Nœuds ; dans l'autre sens
  tant que le graphe reste dans le sous-ensemble commun (sinon Code en lecture seule,
  bandeau).
- **Exécution** : le Blueprint est **compilé** vers la représentation du script et
  exécuté par le **même évaluateur** — en simulation et dans l'application.
- **Débogage** : en simulation, le chemin exécuté **s'illumine**, chaque nœud porte
  son **compteur**, une branche jamais prise reste terne ; **points d'arrêt**, pas à
  pas, lecture des valeurs sur les fils.
- **Portée** : un comportement appartient à un widget, à une page, à un composant ou à
  l'application ; le sélecteur de portée filtre la toile.

### 4.8 Les contrats et le registre d'actions

- **Gestionnaire des contrats** : tous les `controller` et `callback` du projet — nom,
  signature, éléments qui les emploient, état (lié en test, jamais lié, non déclaré).
- **Registre d'actions** de chaque application : identifiant, libellé, raccourci,
  **état** (✅ servie · ✚ à créer), **raison de grisé**, domaine. Il est **généré**
  (sonde de l'application et tables des spécifications, décision D6), jamais saisi à
  la main. Le champ **Action** d'un élément interactif est une liste filtrée du
  registre ; une action inconnue est soulignée **à l'écriture**.
- **Vue Actions** : qui déclenche quoi, où ; les actions ✚ les plus demandées — la
  liste de travail des développeurs.

### 4.9 L'animation

Machine à états par widget (repos, survol, appui, focus, focus visible, désactivé), feuille
d'exposition et éditeur de courbes, trois familles (transition, ambiance, effet
continu), réduction du mouvement ; prévisualisées dans NKUIDesign, **écrites** dans le
document, jouées par le runtime le jour où il saura le faire.

### 4.10 Le système de design : thèmes et jetons

*(Rodolf, 27/09 : « on peut définir pour chaque application créée un système de thème
[…] utiliser des thèmes existants, créer leur propre thème, […] que leur application
intègre plusieurs thèmes au choix de leurs utilisateurs, ou intégrer des plugins pour
le changement de thème. »)*

- **Chaque application a son système de thème**, défini par son auteur : ses
  **jetons** (`@nom`), leur **sens** (une phrase, obligatoire), leur **famille**, et
  leurs **valeurs par variante** (Sombre, Clair, et d'autres).
- **Partir de l'existant** : la bibliothèque de thèmes (**Rihen UE5**, **GitHub Pro**,
  **Contraste élevé**, ceux du projet, ceux des greffons) ; hériter d'un thème ou le
  dupliquer.
- **Créer le sien** dans l'**éditeur de thème** : liste des jetons, valeurs par
  variante côte à côte, **où chaque jeton sert** (recherche inverse), **contrôle de
  contraste** de chaque paire texte / fond, **aperçu** Sombre / Clair côte à côte sur
  la toile.
- **Plusieurs thèmes au choix de l'utilisateur final** : l'application déclare ceux
  qu'elle embarque, son défaut, si l'utilisateur peut choisir, si elle suit le mode
  clair / sombre du système.
- **Thèmes par greffon** : des données seulement, préfixées ; ils donnent des valeurs,
  ne retirent jamais un jeton.
- **Dans l'inspecteur**, une couleur se choisit **dans les jetons** ; une couleur libre
  reste possible et est **signalée** (« couleur en dur — restera sombre en thème
  clair »).
- **Nos applications, NKUIDesign compris, utilisent Rihen UE5** (décision de Rodolf,
  27/09 : le style Unreal Engine 5, « celui que je préfère » ; GitHub Pro reste livré).

### 4.11 La bibliothèque commune

- **Bibliothèque = `Commun/composants.nkgui` + les bibliothèques de chaque
  application** ; chaque composant montre **ses instances dans tout le projet**.
- **Modifier un composant** → **analyse d'impact** avant d'enregistrer (« 214
  instances dans 5 applications ; 3 surchargent `label` »).
- Provenances : Projet, Partagé, Importé, Système ; mises à jour avec différence ;
  règle de fourche pour ce qui n'est pas à nous (doc 15 §15.5).

### 4.12 La fidélité

- **Vue Fidélité** : l'élément tel que le monteur le peindra, **rendu par NKGui
  lui-même**, à côté du design.
- **Marques** sur la toile et dans la hiérarchie : ✓ peint · ◐ compté non peint · ✕
  refusé ; au survol, la raison dans les mots des compteurs.
- **Compteurs en direct** dans la barre d'état : les mêmes que la sonde des
  applications (doc 2 §22).

### 4.13 Les zones hôtes

Un `Host` est un objet de première classe de la toile : rectangle hachuré avec son nom
et son `hint`, **contenu fictif** pour la présentation (jamais exporté), et, pour les
hôtes d'extension des panneaux communs, **qui les remplit** dans le projet.

### 4.14 États, tailles, cibles

Sélecteur d'**état** sur la toile (repos, survol, appui, focus, focus visible,
désactivé) pour voir et éditer `appearance(État)` ; un cadre affiché à **plusieurs
tailles** côte à côte ; `sizeRel` / `minSize` / `maxSize` comme **poignées** ; cibles
trois classes Bureau / Mobile / Web (Tablette = gabarit d'appareil de la classe Mobile), zone sûre, orientation, décoration de fenêtre,
curseurs, rapport de transposition.

### 4.15 La simulation — l'application, navigable, avant l'application

*(Rodolf, 27/09 : « la simulation lance une fenêtre semblable à celle simulée, en
connaissant soit le point de départ, soit à partir de quelle fenêtre simuler ; et cette
fenêtre de simulation est navigable comme une véritable application, en distinguant le
cas PC, navigateur et mobile. »)*

- **La carte de l'application** (`application.nkgui`) décrit les écrans, leur type, le
  point de départ.
- **▶ Simuler** (F5) démarre au point de départ ; **Simuler à partir de…** (Ctrl+F5)
  démarre sur l'écran choisi avec un **état de départ** réglable.
- **Navigable** comme la vraie application, selon la plateforme : vraies fenêtres sur
  Bureau, cadre de navigateur avec routes et historique sur Web, cadre de téléphone avec
  pile d'écrans, geste retour, clavier virtuel et zone sûre sur Mobile.
- **Pendant la simulation** : le fil de navigation, la **carte** avec transitions
  parcourues et jamais parcourues, les **doublures** des services (réussite **ou**
  échec), les **variables** (lire, forcer), le débogueur de Blueprint, la bascule de
  plateforme sans quitter.
- Rendu par **NKGui réel** : ce qu'on voit est ce que l'application montrera.

### 4.16 Les tests d'interaction

*(Rodolf : « généralement les tests se font en appelant des messages de réponse ou en
appelant un autre widget ».)*

- **Écrire** un test : un point de départ, des gestes, des doublures, des attentes
  (écran ouvert, message affiché, autre widget changé, callback appelé).
- **Enregistrer** un test pendant une simulation : les gestes deviennent des
  instructions ; on ajoute les attentes **en cliquant sur ce qu'on attend**.
- **Lancer** un test, un fichier, tous — à l'écran (on voit le rejeu) ou sans fenêtre
  (la construction s'arrête si un test échoue).
- **Rapport** : réussis, échoués (avec l'image de l'écran au moment de l'échec,
  l'attendu contre l'obtenu), **couverture** (écrans, transitions, branches,
  callbacks, doublures jamais atteints).
- **Vérifier les tests** : chaque test est relancé **sans** l'élément qu'il attend ; un
  test qui reste vert ne regardait pas ce qu'il croit.
- **Les mêmes tests** tournent dans l'application réelle.

### 4.17 Les contrôles de conception

Des **règles déclarées par projet** (D5), vérifiées en continu, listées dans le panneau
**Contrôles** : aucune couleur hors jetons, tout élément interactif a une action du
registre, toute icône a un mot, un élément désactivé dit pourquoi, un composant a une
racine, une condition ne lit que des widgets existants… Chaque règle a un nom, une
**raison** (le document qui la pose), une sévérité, et **« Corriger »** quand la
correction est mécanique.

### 4.18 La source

**Mode Source** : le texte `.nkgui` synchronisé avec la toile, coloré, éditable ;
sélectionner d'un côté sélectionne de l'autre. **Split Design | Source** est la vue
par défaut de l'agent et du développeur.

### 4.19 Voir dans l'application ; de la spécification à l'arbre

- **Voir dans l'application** : l'application cible **relit** le document enregistré sans
  être relancée (surveillance de fichier, `NkFileWatcher`).
- **Coller un arbre** écrit dans la notation des spécifications (`Rôle "id"   prop:
  valeur`, indenté) → un document valide. Les sept jeux de spécifications deviennent
  **directement exploitables** dans l'outil.

### 4.20 L'IA

Voir §6.

### 4.21 Export et validation

- **Enregistrer** est l'export principal (le `.nkgui` est le document). **Valider**
  (F7) produit le rapport : diagnostics du doc 2 §12, compteurs, contrôles, tests.
- **Exports secondaires** : PNG (livré), SVG (partiel), PDF et code (absents) — pour les
  clients externes (D4).
- **Découpage en fichiers** : proposer de sortir des composants ou un thème dans un
  fichier inclus.

### 4.22 Greffons

Formats d'import, composants, effets, nœuds de Blueprint, **thèmes** : ajoutés par des
greffons (`.nkgreffe`) à préfixe, avec permissions en langage ordinaire, isolation,
rejet motivé, dépendances de document, migrations (doc 3 §20bis).

### 4.23 Console et préférences

Console à onglets **Validation · Simulation · Tests · Greffons · Système** ; un seul
backend pour l'éditeur et la simulation. Préférences : thème **de l'éditeur**,
raccourcis, enregistrement automatique, IA (local / distant, modèle), grille et
magnétisme par défaut, langue.

---

## 5. Architecture

```
NKUIDesign (application)
 ├─ Modèle d'édition           NkUIDocument (le document ouvert), historique unifié,
 │                             sélection — construit DEPUIS l'archive, écrit VERS elle
 ├─ Fil .nkgui                 NkGuiArchive (lecture / écriture à l'octet)   ─┐ partagé
 │                             NkGuiInclusions · NkGuiComposants              │ avec le
 │                             NkGuiValidate (validateur)                     │ runtime
 ├─ Rendu                      NKGui : toile, fidélité, simulation            │
 │                             NkGuiMonteur · NkGuiInteraction · NkGuiCoquille ┘
 ├─ Compilateur de comportement Script ⇄ Blueprint ⇄ représentation commune
 ├─ Thèmes                     sections `theme`, résolution des jetons, contraste
 ├─ Simulation et tests        carte d'application, doublures, rejeu, couverture
 ├─ Registre d'actions         sonde des applications + tables des spécifications
 ├─ Géométrie                  vectoriel, booléens, triangulation (NkEarcut → noyau)
 ├─ IA                         DesignAI : Propose / Commit / Discard / Retract
 └─ Interface de NKUIDesign    ÉCRITE EN .nkgui (doc 5), montée par NkGuiCoquille
```

- **Le lecteur, le validateur, le monteur et l'évaluateur sont ceux du runtime** : aucune
  divergence possible entre ce que l'éditeur écrit, ce qu'il montre et ce que
  l'application exécute.
- **Le modèle d'édition** reste nécessaire (sélection, historique, géométrie éditable),
  mais il est **une vue** de l'archive : il ne possède rien que le fichier ne porte pas,
  sauf ce qui va au `.nkgui.meta`.
- **Couches** du moteur respectées : Foundation → System → Runtime → Engine → Apps ;
  zéro STL ; NKMemory ; NKLogger ; construction Jenga.

---

## 6. L'IA

### 6.1 Points d'entrée

| contexte | ce que l'IA produit | résultat |
|---|---|---|
| nouveau projet via IA | une description du produit → la carte et des écrans complets | un projet, tout éditable |
| toile, cadre vide | « un formulaire de connexion » | un groupe inséré à l'endroit choisi |
| toile, sélection | « plus dense », « version mobile » | une variante en aperçu |
| toile Behavior, widget choisi | « ce bouton réinitialise les trois champs au-dessus » | un **Blueprint** proposé (nœuds et câblage) et des noms de callbacks, **jamais** d'implémentation |
| tests | « teste le cas où le réseau est coupé » | un test proposé, avec sa doublure en échec |
| thème | « un thème clair pour NkAnima » | des valeurs de jetons proposées, **contraste vérifié** |
| spécification | un arbre de spécification collé | un document, par la même porte que le collage manuel |

### 6.2 Garanties (non négociables)

1. **Aperçu avant application** : rien n'est écrit tant que l'utilisateur n'a pas validé
   (`Propose` / `CommitProposal` / `DiscardProposal`, livrés).
2. **Une proposition = une opération annulable**, avec la **liste des changements**,
   chacun décochable, et sa **justification**.
3. **Sortie = données normales** du document : aucun format parallèle.
4. **Badge « Généré par IA »**, informatif, retiré à la première modification manuelle.
5. **Callbacks jamais inventés côté implémentation.**
6. **Chercher dans la bibliothèque avant de générer** ; **demander** quand un composant
   manque ; **proposer** un rôle, jamais l'attribuer.
7. **Local ou distant toujours visible** ; jamais de repli silencieux de l'un à l'autre.
8. **Composer d'abord avec le commun** : pour une application de la famille, une
   proposition n'emploie que des composants communs, des jetons du thème et des
   actions du registre — ou dit ce qui manque.

### 6.3 La vision en quatre étages (doc 18)

1. **Lego** — composer avec les pièces existantes (disponible).
2. **Page par page** — depuis une description, ou depuis une spécification (§4.19).
3. **Reconnaissance** — « bouton » → le bon composant du catalogue.
4. **Depuis zéro** — la maquette comme document, puis les composants, puis les pages ;
   demande un corpus (doc 6).

---

## 7. Le constat au 27/09/2026

### 7.1 Ce qui est fort

- **~84 000 lignes**, une fenêtre qui tourne, suivie écart par écart contre les planches
  (doc 17).
- **86 comportements Lunacy livrés sur 191**, comptés (doc 14), avec la liste nommée des
  absents.
- **Le format** : grammaire, validateur, aller-retour prouvé **à l'octet**, exemples
  valides et fautifs ; export `.nkgui` branché.
- **Les composants** : extraction, instance, surcharges par propriété, détachement,
  propagation, variables et styles (doc 15).
- **L'IA** : pipeline de proposition, validation, retrait, recette 18 / 18 (doc 10).
- **Des chapitres que peu d'outils ont pensés** : doublures de simulation, greffons,
  indisponibilité, infobulles, décoration de fenêtre.

### 7.2 Ce qui manque, par ordre de gravité

| # | manque | pourquoi c'est grave | module |
|---|---|---|---|
| **M1** | NKUIDesign **n'ouvre pas** un `.nkgui` ; son document est un `.nkuidoc` | le travail de l'agent ne peut pas être retouché ; deux formats contredisent le principe 1 | 4.1 |
| **M2** | pas de **système de thème** : ni jetons, ni éditeur, ni thèmes par application | les applications reposent sur des jetons que personne ne peut voir ni régler | 4.10 |
| **M3** | pas de **registre d'actions** | ~800 actions déclarées par les spécifications ; une faute de frappe n'est vue qu'au lancement | 4.8 |
| **M4** | la **fidélité** au monteur est invisible à la conception | ombre, état, police : comptés sans être peints, découverts au lancement | 4.12 |
| **M9** | on ne peut tester qu'**un widget seul** ; les **Blueprints** ne s'exécutent pas | le langage ne sait ni ouvrir une fenêtre, ni afficher un message, ni agir sur un autre widget ; pas de carte, pas de plateformes | 4.7, 4.15, 4.16 |
| **M5** | les priorités suivent la parité Lunacy | les **booléens (0 / 8)** sont indispensables à la famille ; d'autres absents ne le sont pas | 4.3 |
| **M6** | l'inspecteur respire mal (écart E1) | rangées de 26 px, gouttières de 3 à 8 px, sections vides | doc 3 §12 |
| **M7** | l'interface de NKUIDesign est **codée à la main** (`Panels.h` 21 000 lignes, `main.cpp` 10 500) | l'outil qui produit des `.nkgui` n'en consomme aucun | doc 5 |
| **M8** | le thème est GitHub Dark Pro | la famille est au style UE5 (décision du 27/09) | 4.10 |

---

## 8. Exigences non fonctionnelles

| exigence | mesure |
|---|---|
| aller-retour | ouvrir puis enregistrer sans modification = **même octet** ; une modification ne touche que ses lignes |
| annulation unifiée | une action de Blueprint, de thème, de test ou de toile s'annule pareil ; une proposition de l'IA = une étape |
| enregistrement automatique | toutes les 60 s dans un fichier de secours, jamais par-dessus le document ; historique local des versions |
| performance | toile fluide (60 images/s) avec 2 000 éléments ; Blueprint fluide avec 500 nœuds (masquage hors vue, mini-carte) ; ouverture d'un document de 3 000 nœuds en moins d'une seconde |
| fidélité | la vue Fidélité et l'application montrent **les mêmes pixels** sur un document de référence |
| simulation | démarre en moins de 2 s ; un changement du document est visible en simulation sans la relancer |
| tests | un test sans fenêtre de 20 gestes en moins de 200 ms |
| accessibilité de l'éditeur | contraste AA dans ses thèmes, navigation au clavier complète, pas d'information portée par la couleur seule |
| multilingue | l'interface de NKUIDesign est traduisible (clé stable + libellé) ; ghomala' et diacritiques combinants |
| hors ligne | tout fonctionne sans réseau, sauf l'IA distante (et elle le dit) |
| sécurité | un greffon ne touche que ce que ses permissions disent ; une clé d'IA n'est jamais écrite dans un document |

---

## 9. Phasage

| palier | contenu | critère de fin |
|---|---|---|
| **R1 — le fil** | 4.1 ouvrir / enregistrer `.nkgui`, commentaires et style gardés, `.nkgui.meta`, migration `.nkuidoc` ; 4.2 explorateur et graphe des inclusions ; 4.18 mode Source | les 6 documents existants ouverts, modifiés, enregistrés : **diff = la modification seulement** ; Source et toile synchrones sur 100 modifications aléatoires |
| **R2 — la famille et ses formes** | 4.10 thèmes et jetons (section `theme`, éditeur, contraste, Rihen UE5 / GitHub Pro / Contraste élevé) ; 4.11 bibliothèque commune ; 4.8 registre d'actions ; **NKUIDesign passe au thème Rihen UE5** ; booléens, `geometry` / `shape`, remplissage non convexe **dans NKGui** | le thème Rihen UE5 conçu **dans** NKUIDesign, Sombre et Clair ; une action mal orthographiée soulignée avant l'enregistrement ; **un bouton à coin coupé composé par soustraction, monté dans NkAnimaEditor avec sa forme**, qui ne réagit pas au clic dans le coin |
| **R3 — la vérité** | 4.12 fidélité ; 4.13 hôtes ; 4.14 états et tailles ; 4.17 contrôles ; **instructions d'interface et conditions** ; **exécution des Blueprints** ; 4.15 carte et simulation navigable Bureau / Web / Mobile ; 4.16 tests | la vue Fidélité et NkAnimaEditor montrent les mêmes pixels ; **la simulation de NKScena part de l'accueil, ouvre la fenêtre principale, affiche une confirmation, revient — en Bureau puis en Mobile** ; **le même comportement écrit en Blueprint et en script donne la même trace d'exécution** ; un test enregistré rejoue à l'identique et **échoue si l'on retire le `message`** |
| **R4 — la vitesse** | 4.19 voir dans l'application, spécification → arbre ; refonte de l'inspecteur (M6) | la barre d'outils de Nogee collée depuis sa spécification devient un document valide en une minute, vue **dans Nogee** sans relance |
| **R5 — le miroir** | l'interface de NKUIDesign en `.nkgui` (doc 5) | `Panels.h` réduit de moitié ; NKUIDesign modifie sa propre barre d'outils et la voit changer |
| **R6 — l'IA** | étages 1 à 3 du §6.3 sur la bibliothèque commune, les jetons et le registre | « la barre d'outils de NKScena » → une proposition faite **uniquement** de composants communs, de jetons et d'actions du registre ; l'IA **demande** quand un composant manque |

📌 **Ce qui est en pause** (pas abandonné) : l'édition vectorielle avancée au-delà de ce
que les booléens exigent, les exports SVG / PDF / code, le dépouillement de Figma,
Sketch et Canva. **Règle de reprise** : un comportement en pause revient quand une
interface de la famille le demande, ou quand un client externe le paie.

---

## 10. Décisions

| # | question | décision | état |
|---|---|---|---|
| D1 | `.nkuidoc` | lu pour migration, puis abandonné au profit de `.nkgui` + `.nkgui.meta` | proposé, en vigueur dans ce document |
| D2 | le thème de NKUIDesign et de la famille | **Rihen UE5** (style Unreal Engine 5) par défaut ; GitHub Pro livré en alternative | ✅ **Rodolf, 27/09** |
| D3 | la couleur de sélection sur la toile | ambre `#F2980E` (règle de la famille : bleu = interface, ambre = sélection dans la zone de travail) | 🔴 à confirmer : les utilisateurs de Figma attendent du bleu |
| D4 | la parité Lunacy | en pause jusqu'à R5, **sauf** booléens et vectorisation de contour (R2) | proposé |
| D5 | où vivent les règles de contrôle | `Resources/Interface/controles.nkgui`, versionné avec les interfaces | proposé |
| D6 | le registre d'actions | **généré**, jamais saisi | proposé |
| D7 | Ctrl+K | composant ici ; assistant IA sur Ctrl+Maj+K | proposé |
| D8 | les extensions du format 0.4 | **décidées** : booléens / `geometry` / `shape`, instructions, conditions, carte, tests | ✅ **Rodolf, 27/09** |
| D9 | la navigation dans l'application réelle | servie par la coquille à partir de la **même** carte | proposé |
| D10 | le système de thème | un par application, thèmes existants / propres / multiples au choix de l'utilisateur / par greffon | ✅ **Rodolf, 27/09** |
| D11 | l'exécution des Blueprints | compilés vers la représentation du script, même évaluateur | ✅ **Rodolf, 27/09** (« n'oublie surtout pas les blueprints ») |
| D12 | l'action portée par un enfant de composant | (a) racine pour un composant à **une** action ; (b) `id` qui échappe au préfixe pour un composant à **plusieurs** actions (doc 2 §14.3) | 🔴 recommandation |
| D13 | Ctrl+A ou A pour tout sélectionner dans la famille | Ctrl+A ici (Lunacy, et livré) | 🔴 famille |

---

## 11. Glossaire court

| mot | sens |
|---|---|
| **document** | un fichier `.nkgui` |
| **carte** | le document `application` qui liste les écrans d'une application |
| **écran** | ce que la carte désigne : fenêtre, dialogue, feuille, panneau, superposition |
| **rôle** | ce qu'est un widget (`Button`, `Slider`…), du vocabulaire du doc 7 |
| **action** | l'identifiant d'un élément interactif, qui est aussi le nom de son callback |
| **Blueprint** | un comportement écrit en nœuds ; équivalent d'un script |
| **jeton** | un nom de valeur (`@accent`) résolu par le thème actif |
| **thème / variante** | un ensemble de valeurs de jetons / une de ses déclinaisons (Sombre, Clair) |
| **fidélité** | ce que le moteur peindra réellement de ce qui est écrit |
| **doublure** | la réponse simulée d'un service du système, réussite ou échec |
| **hôte** | une zone `Host` que l'application remplit |

---

*Documents liés : 0 (l'index et l'autorité), 2 (le langage), 3 (l'interface
humaine), 5 (le modèle et l'interface en `.nkgui`, pour l'agent), 7 (le vocabulaire),
9 (la grammaire : décisions), 15 (les composants), 18 (la vision IA).*
