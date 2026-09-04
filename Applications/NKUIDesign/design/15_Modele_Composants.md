// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# 15. Le modèle des composants de document — écrit AVANT le code

> **Le but de NkUIDesign, dans les mots de Rodolf** : *« nkuidesign qui me
> permettra de designer nos premiers composants »*.
>
> C'est **la ligne d'arrivée**, et au 02/09 le chapitre 9 est à **0 sur 10**
> pendant que l'ensemble est à 66 livrés sur 174. Tout le reste avance et ce qui
> justifie l'atelier n'est pas commencé. **Ce document ouvre le chantier par le
> modèle, pas par le code** — parce que trois de ses décisions sont
> *structurelles* : elles seront impossibles à ajouter après.

**Ce document ne livre rien.** Il décide. Le code vient après, et une décision
non tranchée par Rodolf reste **ouverte** ici plutôt que d'être prise à sa place
(§15.6).

À lire avec `12_Dessiner_vers_Composant.md` (01/09), qui avait déjà tranché deux
points, et `13_…` §9 pour ce que la source décrit.

---

## 15.1 ⚠️ DEUX NOTIONS PORTENT LE MÊME MOT, ET LES CONFONDRE COÛTERAIT TOUT

Le modèle porte **déjà** un champ `component` sur chaque nœud. Il ne désigne
**pas** ce dont il est question ici.

| | **composant DE CODE** (existant) | **composant DE DOCUMENT** (ce chantier) |
|---|---|---|
| d'où il vient | déclaré en C++, `NkComponentRegistry` | **dessiné** dans l'éditeur |
| quand il existe | à la compilation | à l'exécution, dans un `.nkuidoc` |
| exemples | `content_browser`, `tree_view` | « mon bouton primaire » |
| champ | `NkUINode::component` | **nouveau**, §15.3 |

🔴 **Ils ne doivent pas partager le champ `component`.** La tentation est réelle
— un seul nom, une seule résolution — et elle serait fausse : un composant de
code a une **durée de vie statique** et n'existe pas dans le fichier ; un
composant de document **vit dans le fichier** et doit voyager avec lui. Les
mélanger, c'est rendre un document non ouvrable dès que le binaire change de
version. *Deux natures voisines, deux champs.*

📌 **ET LA MEILLEURE DÉFENSE DE CETTE DÉCISION EST ARRIVÉE TOUTE SEULE, LE JOUR
MÊME.** Le tout premier cas de recette écrit sur ce modèle cherchait le mot
« composant » dans un fichier pour vérifier qu'aucune déclaration n'y était
écrite — et il trouvait la **clé de nœud** `composant`, celle du composant *de
code*. Il déclarait donc le document non conforme alors qu'il l'était.

> **Si un contrôle de trois lignes les confond déjà, un champ partagé les aurait
> confondues pour toujours.**

*Cette phrase remplace un paragraphe d'explication, et elle est là pour le
prochain qui trouvera la séparation superflue.*

---

## 15.2 LES QUATRE OBJETS, ET RIEN DE PLUS

- **La déclaration** — l'arbre de référence, nommé, rangé en tête du document.
  C'est *l'identité* de la forme.
- **L'instance** — un nœud qui *pointe* une déclaration au lieu de porter son
  propre contenu.
- **L'écart** (*override*) — ce qu'une instance change par rapport à sa
  déclaration, **propriété par propriété**.
- **La provenance** — qui est l'auteur de la déclaration. Elle existe déjà
  (`NkProvenance`) mais dit le *genre* d'auteur, pas **qui** ; §15.5 en dépend.

---

## 15.3 CE QUI S'EXTRAIT, ET CE QUI RESTE À L'INSTANCE

*Repris de `12_…` §12.3(b), qui l'avait tranché par propriété et non en bloc —
la source Lunacy confirme cette découpe.*

| propriété | dans la déclaration | surchargeable par l'instance |
|---|---|---|
| géométrie du tracé (`sommets`, arrondis) | ✅ c'est l'identité | ❌ |
| remplissages, bordures, effets | ✅ par défaut | ✅ |
| **texte** | ❌ | ✅ **toujours** — deux boutons ne disent pas la même chose |
| position, rotation, miroirs | ❌ **jamais** | ✅ propres à l'instance |
| **taille** | ⚠️ **fixe dans la première tranche** — voir §15.7 | |
| `verrouille` / `masque` | ❌ | ✅ — ce sont des états de vue, pas d'identité |

**La règle de fond, et c'est celle qui rend le reste acceptable** :

> **Une surcharge gagne sur la mise à jour.** Le composant se propage à ses
> instances **sauf** là où un écart existe. Sans cette règle, mettre à jour un
> composant écraserait le travail fait sur chaque instance — et personne
> n'oserait plus toucher à une déclaration.

---

## 15.4 🔴 EXTRACTION ET DÉTACHEMENT ARRIVENT ENSEMBLE — NON NÉGOCIABLE

*« Créer un composant » sans « détacher » enferme l'utilisateur dans une décision
qu'il ne peut pas défaire.* Le premier utilisateur qui a besoin d'une variante et
ne peut pas la faire **cessera de créer des composants** — et l'atelier aura
produit exactement l'inverse de son but.

**Donc la première tranche livre les deux, ou aucun** :

| geste | ce qu'il fait |
|---|---|
| **Extraire** (`Ctrl+Alt+K`) | la sélection devient une déclaration + une instance à sa place |
| **Détacher** (`Ctrl+Alt+D`) | l'instance redevient un sous-arbre ordinaire, écarts **fusionnés dedans** |

⚠️ **Le détachement fusionne les écarts, il ne les jette pas.** Une instance dont
le texte a été surchargé doit garder *son* texte en se détachant. Jeter les
écarts serait une perte de travail silencieuse — la pire espèce.

⚠️ **Et l'aller-retour doit être NEUTRE** : extraire puis détacher immédiatement
rend un sous-arbre **équivalent à l'original**. C'est la garde que la recette
tiendra, et c'est elle qui prouve que l'extraction n'a rien perdu en chemin.

---

## 15.5 🔴 LA RÈGLE DE FORK — STRUCTURELLE, DONC MAINTENANT

**Décision de Rodolf** : un composant **tiers** se **duplique à la modification**
(*fork*) au lieu de se modifier en place.

Elle vient de ce que l'atelier alimente : *« des millions d'utilisateurs peuvent
créer des composants, les commercialiser ou les partager »*. Modifier en place la
déclaration de quelqu'un d'autre casserait l'identité sous laquelle elle a été
partagée — et rendrait toute mise à jour de l'auteur d'origine inapplicable.

**Ce que ça impose au MODÈLE, et c'est pour ça que ça ne peut pas attendre** :

1. une **identité d'auteur** sur la déclaration — `auteur/composant@version`,
   pas seulement le *genre* d'auteur que `NkProvenance` porte aujourd'hui ;
2. un **lien d'origine** sur toute déclaration forkée : de quoi elle dérive, et
   à quelle version. Sans lui, un fork est indiscernable d'une création, et on
   perd la seule information qui permettra plus tard de proposer « l'original a
   changé, veux-tu rejouer ta modification ? » ;
3. un **prédicat « m'appartient-elle ? »** consulté **avant toute écriture** sur
   une déclaration. C'est une porte, pas un test dispersé aux sites d'appel —
   la même discipline que `NkNoeudAttrapable`.

> *Ajoutée après coup, cette règle demanderait de réécrire tous les documents
> déjà produits pour leur inventer une identité d'auteur qu'ils n'ont pas.* C'est
> exactement ce qu'on ne peut pas faire — d'où sa place ici, avant la première
> ligne de code.

📌 **Ce qu'elle ne demande PAS tout de suite** : le partage, le téléchargement,
la place de marché. Seulement que **le modèle sache dire à qui appartient une
déclaration**, et **de quoi elle dérive**. Le reste se construit dessus.

---

## 15.6 ✅ Q51, RÉVISÉE ET VALIDÉE (02/09) — propagation AUTOMATIQUE chez soi, choix à trois branches sur l'instance, la PROPRIÉTÉ comme seul critère

> **Les mots de Rodolf (révision du 02/09, remplaçant sa première réponse)** :
> *« si je modifie l'arrondi du composant, ça modifie pour tous les boutons qui
> en héritent [...] modifier l'arrondi d'un bouton ne touche pas les boutons du
> même composant [...] modifier un bouton offre la modification par copie et
> création d'un nouveau composant, ou alors la modification du composant
> lui-même — tant que ce dernier n'est pas un composant système, qui est
> toujours modifié par copie. »*
>
> **Sa précision, même jour** : *« ce qui se passe avec les composants locaux
> se fait aussi avec les composants tiers, à préciser : donc toujours en copie,
> pas en modification — sauf si c'est notre composant ou une dérivée de
> composant qu'on a créée. »*
>
> **Et sa généralisation** : *« la couleur n'est qu'une propriété — on peut
> avoir des boutons du même composant avec leurs propres propriétés. »*

### Les quatre règles

**R1′ — Modifier la DÉCLARATION propage AUTOMATIQUEMENT.** Toutes les
instances suivent immédiatement — **les bits surchargés tiennent** (son
exemple validé : deux boutons, un bleu, un rouge — l'arrondi du composant
change les deux, chacun garde sa couleur). La règle du masque d'écarts (§15.3)
est inchangée ; **seul le déclencheur a changé** : automatique, plus explicite.

**R2′ — Modifier une INSTANCE est par défaut une surcharge LOCALE** (les
autres instances ne bougent pas), et l'outil offre le choix à trois branches :
*garder comme variation locale* / *appliquer au composant original* / *créer
un nouveau composant*.

**R3 — Le critère unique est la PROPRIÉTÉ, pas la catégorie.** Plus de
distinction système/tiers dans la règle de modification :
- **à moi** = créé par moi **ou copie/dérivée que j'ai faite** d'un composant
  d'autrui → le dialogue à trois branches s'affiche ;
- **pas à moi** = le kit **et** le tiers, indistinctement → **copie, seule
  voie, sans dialogue** — et 📌 **le fork m'appartient** : on copie une fois,
  puis on travaille librement sur sa branche. C'est ce qui rend le système
  vivable à l'échelle « des millions d'auteurs ».
La frontière est `NkPeutModifierDeclaration` — le prédicat existant, confirmé
comme LE critère, jamais recalculé au site d'appel.

**R4 — La propagation automatique (R1′) vaut pour les composants du document /
de l'auteur.** Pour un composant **d'un autre auteur** (partagé, futur
marché) : pas de propagation automatique de **version** — l'état « en
retard » + la mise à jour volontaire restent la règle. *Automatique chez soi,
volontaire quand ça vient d'ailleurs.* R3 et R4 se côtoient sans se toucher :
*je ne peux pas modifier le composant d'autrui (R3), et sa nouvelle version ne
me traverse pas sans mon accord (R4).* Les trois états d'instance (à jour /
en retard / détachée) survivent — « en retard » ne concerne plus que le tiers.

### Les quatre scénarios validés par Rodolf, un par un

1. ✅ « Je change l'arrondi du COMPOSANT → mes deux boutons s'arrondissent,
   le bleu reste bleu, le rouge reste rouge » — **oui**.
2. ✅ « Je change l'arrondi d'UN bouton → l'autre ne bouge pas, et l'outil
   propose : variation locale / appliquer à l'original / nouveau composant »
   — **oui**.
3. ✅ « Composant du kit → jamais “appliquer à l'original”, toujours ma
   copie » — **oui**.
4. ✅ « Composant d'un autre auteur, nouvelle version → mes pages ne changent
   pas seules, je vois “en retard” et je décide » — **oui**.

### 📌 La structure existante EST la décision — pas une approximation

La généralisation de Rodolf (*« la couleur n'est qu'une propriété »*) est la
définition exacte du masque d'écarts déjà codé : **un bit par propriété,
jamais une copie des valeurs**, chaque propriété surchargeable indépendamment,
par instance. Rien à refondre : le modèle du §15.3 porte la décision telle
quelle.

### Les cas de banc à écrire au chantier (par le geste, comme toujours)

- *arrondi de la DÉCLARATION → les deux instances suivent ET gardent chacune
  sa surcharge* (le jumeau vivant de « détacher fusionne ») ;
- *arrondi d'UNE instance → l'autre ne bouge pas* ;
- le dialogue à trois branches n'apparaît que si `NkPeutModifierDeclaration`
  dit oui — sinon copie, sans dialogue ;
- le panneau/menu qui liste les branches ne porte aucun libellé de plus que
  les trois.

### La trace de la RÉVISION — pour que personne ne « retrouve » l'ancienne décision

⚠️ **LES DEUX RÉPONSES SONT DU 02/09** — la révision est venue quelques heures
après la première, le même jour. *(Ce paragraphe disait « hier » /
« aujourd'hui » : une déformation de ma part, corrigée le soir même. Une date
fausse dans la trace d'une révision est exactement ce qui fait rouvrir une
décision close.)*

**Première réponse (02/09 — REMPLACÉE)** : *« non, sauf si ces instances sont
mises à jour. Mais de base la modification est proposée en copie ou non — mais
toujours en copie pour des composants système. »* → c'était R1/R2 inversés :
instances gelées, mise à jour explicite partout.

**Révision (02/09, même jour — EN VIGUEUR)** : la propagation redevient
automatique **chez soi** (R1′), le « volontaire » ne subsiste que pour la
version d'un composant d'autrui (R4).

Ce qui n'a PAS bougé d'une réponse à l'autre : les surcharges tiennent
toujours ; le fork est la seule voie sur ce qui n'est pas à moi ; la version
dans `auteur/nom@version` porte le retard. ⚠️ Un lecteur qui tomberait sur la
**première** citation hors de cette section lirait l'INVERSE de la règle en
vigueur — c'est précisément pourquoi la révision est datée et gardée ici.

---
## 15.7 LES DEUX PIÈGES QUI RESTENT, NOMMÉS PLUTÔT QUE DÉCOUVERTS

### La taille, et le 9-slice

Nos sommets sont **unitaires** (−1..1, relatifs à la boîte) : une instance
redimensionnée **déforme** son tracé. Juste pour une flèche, **faux pour un
bouton à coins arrondis**, dont les coins doivent garder leur rayon en pixels.

📌 **La première tranche pose les instances à TAILLE FIXE, et le DIT.** Ce n'est
pas un oubli : c'est un périmètre. *Sans cette phrase, la première instance
étirée passerait pour un bug.* Notre **ancrage** répond à la même question que
les *resizing constraints* de Lunacy et sera la piste — après.

### 🔴 Les états — LA RÉCONCILIATION EST FAITE, ET ELLE NE COÛTE RIEN (02/09)

**Je devais réconcilier avant d'écrire un troisième mécanisme. J'ai regardé, et
il n'y a pas de troisième mécanisme à écrire.**

Ce que le dépôt porte **déjà**, dans le format `.nkgui` :

> **`Normal` · `Hover` · `Pressed` · `Focus` · `FocusVisible` · `Disabled`** —
> une **liste FERMÉE**, tranchée par **Rodolf le 2026-08-27**, exprimée par des
> blocs `appearance(État) { … }` qui portent des **surcharges de propriétés**, et
> dont **l'ordre de la table EST la priorité** (`Disabled > Pressed > Hover >
> FocusVisible > Focus > Normal`). Deux contrôles la tiennent : le format sait
> **dire non** à un état hors liste, et l'ordre est vérifié — *« une liste qu'on
> documente sans la faire respecter est un commentaire »*.

**Trois conséquences, et elles vont toutes dans le même sens :**

1. **la forme est la même que la nôtre** — `appearance(État)` porte des
   surcharges *par propriété*, exactement comme `NkUINode::ecarts`. Les états
   d'un composant de document sont donc **des écarts indexés par état**, pas une
   seconde structure ;
2. **la priorité est déjà décidée**, et elle est tenue par un contrôle. On n'a
   rien à trancher là-dessus ;
3. **les trois états de Lunacy** (*Défaut / Survol / Pressé*) sont un
   **sous-ensemble** des six. Suivre la source ne demande donc **aucune
   divergence** — seulement de n'en exposer qu'une partie, ou pas.

➡️ **Il n'y a plus de chantier « états » au sens où §15.7 le craignait.** Il
reste **une** question, et elle est petite — mais elle appartient à Rodolf parce
que **c'est lui qui a fermé la liste** :

> **Un composant de document expose-t-il les SIX états de notre liste fermée, ou
> seulement les TROIS que Lunacy nomme ?**

| | **six** (notre liste) | **trois** (la source) |
|---|---|---|
| pour | cohérent avec le format, rien à traduire, `Focus` et `Disabled` sont réels dans une interface | plus simple à l'écran, exactement ce qu'un designer venant de Lunacy attend |
| contre | trois onglets d'état que personne ne remplira peut-être | il faudra les rouvrir le jour où l'on voudra `Disabled` — et un composant sans état désactivé est incomplet pour une vraie interface |

✅ **TRANCHÉE PAR RODOLF (Q53, 02/09) : LES SIX.** Et il ajoute — *« et si
plus tard on en ajoutait, ça devrait montrer le nombre exact. »*

**Traduction en contrainte de STRUCTURE, pas en note** (mandat coordinateur) :

1. **Le panneau des états ne porte AUCUNE liste en dur — il ITÈRE la table
   fermée du format** (celle des blocs `appearance(État)`, dont l'ordre EST la
   priorité, tenue par un contrôle). Une seule source de vérité. Un septième
   état ajouté à la table doit apparaître dans le panneau **sans qu'une ligne
   du panneau change**.
2. **Le banc va avec** : un cas qui compare le nombre de rangées du panneau au
   nombre d'entrées de la table — le jumeau de `NkNbRaccourcisCtx()` qui a
   remplacé le `6` en dur du dispatcher. Même famille de défaut (le nombre
   recopié qui dérive), même remède (le compte vient de la table).

À coder au chantier composants (avec « mettre à jour », §15.8-5/6) — pas
avant : la migration d'espacement reste le fil.

### Le mécanisme voisin — la note d'origine, gardée

Lunacy a des **états de composant** (Défaut / Survol / Pressé). **Nous avons déjà
la notion, ailleurs** : nos composants *de code* portent des **états
d'apparence**. ⚠️ **Deux mécanismes voisins à réconcilier avant d'en écrire un
troisième** — c'est le défaut que ce dépôt a payé avec le peintre écrit deux
fois.

📌 *Gardée telle quelle parce qu'elle a fait son travail : c'est elle qui a
imposé d'aller regarder avant d'écrire, et ce qu'on a trouvé en regardant (le
paragraphe ci-dessus) a supprimé le chantier au lieu de le cadrer.* **Un doute
écrit au bon moment coûte une lecture et économise un mécanisme.**

---

## 15.8 L'ORDRE DE LIVRAISON

1. **Le modèle** : déclaration locale au document, instance, écart par propriété,
   identité d'auteur + lien d'origine (§15.5). Additif — un document sans
   composant se réenregistre **octet pour octet**.
2. **Extraire + détacher ensemble** (§15.4), avec la garde d'aller-retour neutre.
3. **Le retour visuel** : une instance **dit** qu'elle en est une, une propriété
   surchargée se distingue d'une héritée, « aller à la déclaration » existe.
   *C'est la moitié qui se néglige* (`12_…` §12.3(d)).
4. **Les surcharges** propriété par propriété, puis « réinitialiser ».
5. **La propagation et le choix à trois branches** (Q51 révisée, §15.6
   R1′–R4) : modifier la déclaration propage AUTOMATIQUEMENT (surcharges
   tenues) ; modifier une instance = surcharge locale + dialogue à trois
   branches derrière `NkPeutModifierDeclaration` ; « en retard » + mise à
   jour volontaire pour le TIERS seulement (R4), visibles sur la pilule.
6. Le reste de §9 (états, échanger, supprimer→cadres) — après.

⚠️ **La déclaration vit DANS LE DOCUMENT qui l'a créée**, et rien d'autre pour
l'instant (`12_…` §12.3(a)). La bibliothèque partagée est un chantier de
*résolution de dépendances*, pas d'éditeur ; les mélanger mêlerait deux problèmes
dont un seul est urgent. Et ça ne la ferme pas : une déclaration locale se
**promeut** plus tard.

---

## 15.9 LA PALETTE EST LIVRÉE (02/09) — et la section COMPOSANTS a changé de sujet

**« Réutiliser » existe** : `InstancierComposant` (la porte), et le double-clic
de la section COMPOSANTS de la Hiérarchie (le geste) — cible = la sélection,
repli sur la première page, refus qui parle sans page. Preuves dans
`--recette-gestes` (27/27), mutations « lien sans chair » et « racine au lieu
de la page » payées.

⚠️ **La section COMPOSANTS liste désormais les déclarations DU DOCUMENT**, plus
le registre du kit — qui vivait là EN DOUBLE de la palette du rail, et dont la
planche 091913 ne parle pas (elle y montre `Btn_Primaire`, badge « Button » :
un composant de document). C'est la séparation §15.1, appliquée aux panneaux :
*le kit dans la palette du rail, le document dans la Hiérarchie.* L'état vide
se dit (« Ctrl+Alt+K sur un élément en crée un »).

## 15.10 🔴 AUCUNE HYPOTHÈSE « ÉCRAN PLAT » DANS LES PROPRIÉTÉS D'UNE PAGE (mandat VR, 02/09)

Direction VR/AR/XR actée (`ROADMAP_PRODUITS.md` §5 au parent) : **la CIBLE
portera la projection** (mètres, degrés — clé `unite`, réservée, additive,
absente = pixels), **jamais la page**. Et l'ENTRÉE (rayon, regard, mains) ne va
nulle part dans le document — une propriété « rayon » serait le `si (mobile)`
de la VR.

**Hypothèses « écran » croisées dans le format — SIGNALÉES, pas corrigées** :

1. `NkFormatPage` (le catalogue des formats, `Formats.h`) n'a **aucune colonne
   d'unité** : `w, h` en pixels, point. Le jour où une catégorie « Casque »
   arrive, le type ne sait pas la dire — la clé `unite` du document le sait,
   le catalogue pas encore.
2. Le texte libre de `cible` (« Mobile 390 x 844 ») **grave les pixels dans le
   libellé lui-même** : la valeur et son unité vivent dans une chaîne
   d'affichage. Tant que `unite` n'est pas exploitée, c'est cohérent ; le jour
   venu, le libellé devra être composé, pas stocké.

Rien d'autre : aucune propriété d'entrée (souris, survol, rayon) n'est écrite
dans le document aujourd'hui — les interactions vivent dans le kit, pas dans le
fichier.

## 15.11 🔴 PRINCIPE — un composant est un ACTE, jamais une déduction (Rodolf, 02/09)

> Ses mots : *« un composant ne sera composant que lorsque l'utilisateur aura
> défini son design comme tel. »*

Le geste existant (l'extraction explicite, `Ctrl+Alt+K`) n'est donc pas un
détail d'interface : c'est **le principe du modèle**. Être un composant est un
**engagement** — instances, propagation automatique (§15.6 R1′), identité,
filiation s'y accrochent. *Un engagement se prend, il ne se subit pas.*

Trois interdictions en découlent, à opposer à toute proposition future :

1. **Aucune promotion automatique.** Pas d'heuristique « répété N fois →
   composant » ; le dessin reste du dessin tant que la main n'a pas décidé.
2. **L'IA ne décrète pas le statut.** Elle propose, ou crée un composant si la
   consigne de l'utilisateur le demande explicitement — jamais de sa propre
   initiative. C'est le prolongement de la règle maison « l'IA écrit la donnée
   éditable » : elle n'écrit pas un STATUT.
3. **Collage, import et transposition ne créent pas de composants** en douce.

⚠️ Le comportement actuel est déjà conforme — cette section n'appelle aucun
code : elle existe pour que la prochaine « bonne idée » de promotion
automatique rencontre une décision datée, pas un vide.

## 15.12 L'ORIGINE D'UN COMPOSANT — un FAIT de modèle, pas une question (02/09)

Rodolf : *« lister tous les composants, ou seulement les composants système, ou
externes, ou nos propres composants. »* Les quatre vues sont livrées. Ce qui
suit n'attend aucun arbitrage — c'est ce que la mise en œuvre a **mesuré**.

**Le prédicat seul ne suffit pas, et ce n'est pas un défaut du prédicat.**
`NkPeutModifierDeclaration` répond « à moi / pas à moi » — c'est exactement ce
que la règle de fork lui demande. Mais **système et tiers sont tous deux « pas
à moi »** : il les confond, et il a raison de les confondre, puisque la règle
de modification est la même pour les deux (copie seule).

**La distinction vient donc de la SOURCE, pas du prédicat** :

| origine | où elle vit | modifiable ? |
|---|---|---|
| **Système** | `NkComponentRegistry` — des composants de **code** | non → copie |
| **Tiers** | `doc.declarations`, auteur ≠ le nôtre | non → copie |
| **À moi** | `doc.declarations`, auteur vide ou le nôtre (fork compris) | **oui**, en place |

➡️ **Conséquence directe : le filtre traverse DEUX listes.** Ce n'est pas un
détail de panneau, c'est une propriété du modèle — le §15.1 sépare les deux
**natures**, il n'interdit pas de les **lister** ensemble quand c'est ce qu'on
demande. `NkOrigineDe` s'appuie sur le prédicat au lieu de le doubler : un
second critère aurait divergé du premier au premier composant partagé.

⚠️ **Et une conséquence qui a failli coûter cher : sous filtre, la ligne ne
porte plus son rang.** La 3ᵉ ligne peut être le 3ᵉ composant du kit *ou* la 1ʳᵉ
déclaration du document, selon la vue. S'y fier aurait **posé le mauvais
composant en silence** — un indice resté plausible après que sa signification a
changé. D'où la table `LigneCompos {systeme, index}`, consultée par le geste.

## 15.13 🔴 RÈGLE DE MODÈLE — une FORME est une FEUILLE ; seuls les GROUPES ont des enfants (Rodolf, 03/09)

Ses mots : *« dans Lunacy un graphique ne peut pas être parent. Donc si on prend un
groupe ou un composant, dans ce dernier on peut retrouver des sous-groupes ou des
graphiques — et c'est le cas de Bouton_Connexion. »* Puis : *« on peut avoir des
groupes simples, des groupes d'union, intersection etc., des groupes qui
représentent un composant — et il y a une hiérarchie de groupes à mettre en
place. »*

### Ce que ça fixe

| nature du nœud | peut porter des enfants | ce qui le distingue |
|---|---|---|
| **forme** (rect, ellipse, text, ligne, polygone…) | **non — une feuille** | elle a une géométrie propre et rien dedans |
| **groupe simple** | oui | n'a **pas** de géométrie propre : son englobant est celui de ses enfants |
| **groupe booléen** (union, intersection, soustraction, exclusion) | oui | ses enfants sont des tracés ; **il EST le chapitre 3** (opérations booléennes, 0/8) — même objet, pas un second |
| **groupe composant** | oui | un groupe qui **représente** une déclaration (§15.1) ; ses enfants sont son contenu |
| **planche** (artboard) | oui | le cadre de premier niveau — déjà `frame` chez nous |

Et une **hiérarchie de groupes** : un groupe peut contenir des groupes, à toute
profondeur. La transformée se compose le long de cette hiérarchie (`NkMatEffective`).

### Ce que notre modèle autorise AUJOURD'HUI — mesuré, pas supposé

`NkUIDocument::AddChild(parent, …)` n'a **aucune garde** sur la nature du parent :
n'importe quel nœud peut recevoir un enfant. Dans le document de Rodolf
(`Dashboard_Admin`), **six `rect` portent des enfants** : `Bouton_Connexion`
(un texte), `Panel_Nav` (cinq textes), `Carte_Actifs`, `Carte_Revenu`,
`Carte_Attrition` (deux textes chacune), `Graphique` (douze barres).

**C'est la cause profonde du désordre de `Bouton_Connexion`** : c'est une FORME
qui joue le rôle d'un groupe. Elle a sa propre géométrie (un rect bleu à
rayon 4) **et** un enfant — deux natures dans un nœud. Tourner la forme tourne
son rect ; son enfant, lui, suit une composition à part. Tout ce qui a été
observé le 03/09 (fond tourné, texte droit, cadre désaxé) tient à ce mélange.

### Ce que ça demande, dans l'ordre

1. **Un champ de nature** sur le nœud : `feuille | groupe_simple | groupe_booleen(op) | groupe_composant | planche`. Le format reste additif (clé absente = déduite : `frame` → planche, `component` posé → groupe composant, enfants présents → groupe simple, sinon feuille).
2. **La garde dans `AddChild`** : refuser un enfant sous une feuille, en le DISANT. Et la migration inverse pour les six cas de son document : `Bouton_Connexion` devient un **groupe composant** contenant un rect ET un texte — c'est exactement ce que Lunacy ferait.
3. **Le groupe simple n'a pas de géométrie** : son rectangle est l'englobant de ses enfants, recalculé, jamais écrit. C'est ce qui rend le redimensionnement d'un groupe **naturel** : redimensionner un groupe, c'est appliquer une échelle à ses enfants (§ lot « transformée complète », réponse de Rodolf : *le texte subit la mise à l'échelle*).
4. **Les groupes booléens** ne se conçoivent pas ici : ils sont le chapitre 3 tel quel, et ce chapitre reçoit la nature `groupe_booleen(op)` comme SON objet.

⚠️ Rien de tout cela n'est codé au moment d'écrire ces lignes : cette section est la
règle **avant** le code, comme Rodolf l'a demandé. Le lot « transformée complète »
s'appuie dessus ; le coder sans elle aurait reconstruit le mélange qu'on vient
de mesurer.

### L'échelle PAR NATURE — une seule règle (Rodolf, 03/09)

> *« Le redimensionnement descend dans l'arbre, et une feuille est là où l'arbre s'arrête. »*

C'est **une** règle, pas deux : la poignée d'un **groupe** redimensionne tout ce
qu'il contient, récursivement — tailles **et** positions relatives, **texte
compris** ; la poignée d'une **feuille** ne redimensionne qu'elle. Le banc qui la
fixe : redimensionner `Graphique` change la taille et la position relative de ses
onze barres ; redimensionner une barre ne bouge aucune autre (sonde 46).

**Comment c'est porté (codé le 03/09, commit `5f74ecaf`)** : le facteur est
**porté par le nœud** (`echelle_x` / `echelle_y`, additifs, absents à 1), composé
en descendant dans `NkMatEffective`, et lu par toutes les tailles **à travers le
peintre** (`NkComponentPaint::PushTransform`) — le texte subit la mise à
l'échelle parce qu'il passe par la même matrice que sa boîte, pas parce qu'on
recopie une taille de police. La poignée d'un groupe écrit son échelle et
recale sa position (le bord tiré suit la souris, l'autre tient) ; la poignée
d'une feuille écrit sa taille, comme avant. Rien n'est recopié sur les enfants :
une seule clé dans le fichier (sonde 46c).

**Le refus par axe** (même lot) : un enfant peut refuser la position, la
rotation ou l'échelle (`refus_position` / `refus_rotation` / `refus_echelle`).
Il ne subit pas l'axe refusé — ni de ses ancêtres, ni de lui-même. C'est dans
`NkMatEffective`, donc le dessin et le pointage le lisent au même endroit
(sonde 47) ; la hiérarchie le marque `[refus P R E]`, l'inspecteur le règle.

**La poignée d'un objet tourné** : Lunacy tranche — ses poignées tournent avec
la boîte. Les nôtres aussi : le cadre, les huit poignées et les quatre arcs se
dessinent sous la matrice du nœud, et le survol, la réclamation et l'armement
ramènent la souris par l'inverse de cette matrice. *Pointage et dessin lisent la
même matrice.*

**État au 03/09, 22 h** : la transformée entière est codée et sondée (45–47,
162/162) ; « Se connecter » tourne avec `Bouton_Connexion` sous la souris. La
**garde de feuille** (étape 2 ci-dessus) n'est **pas** codée : elle demande le
champ de nature (étape 1), que Rodolf n'a pas encore tranché — poser une clé de
nature sans lui aurait été inventer le format.

### Le modèle, dans ses mots (Rodolf, 03/09 soir) — ce qui remplace le tableau à cinq lignes

> *« On a soit des nœuds — graphiques ou texte ; quand on parle de graphique ce
> sont des graphiques vectoriels ou non, des images, etc. — et on a des groupes :
> un groupe est un élément qui collecte ou contient des groupes et des feuilles.
> Maintenant on peut avoir des groupes spéciaux qu'on nomme groupes booléens : un
> groupe spécialement conçu pour les intersections, etc. — ça veut dire que
> lorsqu'on applique des opérations booléennes, ça crée un groupe. On pourrait
> définir d'autres formes de groupe, par exemple pour l'animation, mais rien à
> voir je pense. Donc oui, chaque rectangle est un graphique et ne peut en aucun
> cas contenir d'autres graphiques ou groupes. »*

1. **Deux familles, pas quatre** : les **feuilles** (graphiques vectoriels,
   images, texte) et les **groupes** (contiennent groupes et feuilles). La
   nature est **binaire** ; c'est le *genre* de groupe qui a des variantes.
2. **Genre de groupe** : `simple` · `booleen` · `composant` (déjà dans le modèle
   via la déclaration) · **et d'autres possibles plus tard** (l'animation, citée
   sans engagement). Le genre est donc **une clé additive à valeur libre : un
   genre inconnu se relit et se réémet intact** — la règle du type de dégradé,
   pas un `enum` fermé.
3. 🔑 **Une opération booléenne CRÉE un groupe.** C'est le pont avec le
   chapitre 3 : l'opération n'est pas un attribut d'une forme, c'est un
   **constructeur de groupe**. Les booléennes se conçoivent une fois, ici.
4. **Une feuille ne contient rien, en aucun cas.** La garde s'écrit — sur les
   gestes, pas dans `AddChild` : le chargement ne passe pas par eux.

### Codé le 03/09 (commit du lot « feuilles et groupes »)

- **La clé** : `groupe = <genre>`. Additive ; **absente = inférée** (un nœud
  avec enfants est un groupe, une planche `frame` est toujours un groupe, sinon
  une feuille) — les documents actuels se relisent octet pour octet ; genre
  libre, inconnu préservé (sonde 48a : `groupe = animation` fait l'aller-retour) ;
  **écrite dès qu'un groupe est créé** (Ctrl+G pose `simple`), pour qu'un groupe
  **vide reste un groupe**. Un seul prédicat, `NkEstGroupe`, lu par la pose, le
  dépôt, la création, le reparentage et la hiérarchie. **Un groupe déclaré ne
  peint rien** : son apparence est celle de ses feuilles.
- **La garde** : `NkPickFreeContainer` ne rend jamais une feuille (pose, dépôt
  de palette, création à l'outil) ; le dépôt sur une feuille, le glisser dans la
  hiérarchie et « Imbriquer dans » refusent **en le disant** (`NkRefusFeuille`).
- **La migration des six rectangles de Rodolf** — un **ENVELOPPEMENT**, pas un
  ré-étiquetage : un `rect` à enfants est un graphique avec un fond et une
  bordure ; le déclarer groupe lui ferait perdre son dessin. Donc un groupe neuf
  prend sa place (rang, boîte, étiquette, rôle, rotation, miroirs, échelle), le
  rect devient sa **première feuille** à l'origine (« … (fond) »), ses anciens
  enfants suivent aux mêmes positions relatives. Trois garde-fous : **la version
  d'avant conservée** sous `<document>.avant-groupes` (une fois) · **faite à
  l'ouverture, en le disant** dans la barre (jamais en silence à la sauvegarde ;
  rien n'est enregistré tant qu'on n'enregistre pas) · **le témoin** : la
  géométrie peinte (chaque commande du peintre sous sa matrice) est **identique
  avant et après**, sur un cas construit **et sur son document en lecture seule**
  (sonde 48b/48e : `Bouton_Connexion`, `Panel_Nav`, `Carte_Actifs`,
  `Carte_Revenu`, `Carte_Attrition`, `Graphique` ; 62 commandes, identiques).
  La **capture d'écran avant/après** reste due dès que la machine est libre.

## 15.14 🔴 VARIABLES ET STYLES — un nom pour une valeur, un nom pour un ensemble (04/09)

Rodolf, en voyant le sélecteur : *« pourquoi pour le choix des couleurs on n'a
pas de color picker et des **styles comme Lunacy** ? »* Sa capture le montre :
la ligne de remplissage ne porte pas un code hexadécimal mais un **nom**,
`Dark Primary`, et le rail de gauche a `Styles` et `Variables` en entrées de
premier rang.

### Le périmètre, tranché

| | ce que c'est | exemple |
|---|---|---|
| **Variable** | un nom pour **UNE valeur** — l'atome | `Dark Primary` = `#1976D2` ; `Rayon M` = `8` |
| **Style** | un nom pour **UN ENSEMBLE de propriétés** | style de calque (remplissages + bordures + effets) ; style de texte (police, taille, graisse, couleur) |

Un style peut **référencer** des variables. La propagation est celle de §15.1 /
Q51, **mot pour mot** : la modification chez soi se propage, les surcharges
locales tiennent — **un seul mécanisme dans le code**, pas deux. C'est déjà
mesuré pour les instances de composants (sonde 58), et la sonde y a d'ailleurs
trouvé que la propagation ne comparait que le **nombre** de remplissages : elle
compare désormais le contenu.

### Ce qui décide l'ordre

Le mandat de longue date — *« Dark Pro / Light Pro dans TOUTES les
applications »* — **est** l'usage des **variables à modes** : une variable porte
plusieurs valeurs selon un mode, et le document entier bascule. Sans elles, le
thème se recopie à la main partout.

1. **variable de couleur** (débloque les thèmes) ;
2. **styles de calque et de texte** ;
3. **modes** en dernier — mais **la place réservée dans le format dès
   maintenant** (clé additive, valeur libre, inconnu préservé : la règle du
   type de dégradé et du genre de groupe).

**Validé par délégation** (Rodolf, 04/09 : *« ce que tu juges mieux en vue de ce qui
existe dans Lunacy et de nos conversations »*) — ce paragraphe est la décision.

**État codé au 04/09** (commit `a428bb2b`, sonde 63) : la **variable de couleur** —
`NkVariable { clé, nom, valeur, valeurs par mode }`, déclarée dans le document
(`variable = primaire #1976d2 nom="Dark Primary" @sombre=#0d47a1`), le mode courant
(`mode = sombre`), la **référence** « @clé » partout où une couleur se pose ; une
référence vers une variable absente est **dite** (magenta, nom en rouge), jamais
silencieusement noire ; la propagation Q51 vient avec la référence, et une
surcharge locale (un littéral posé sur une instance) tient — même mécanisme que
les composants. **Pas encore codé** : le rail `Styles` / `Variables` (créer, lier,
renommer depuis l'interface), les styles de calque et de texte, la bascule de mode
dans l'interface.

**Le geste d'entrée, vu chez Lunacy (04/09, capture
`2026-09-04_lunacy_modele_oklab_rangee_creer_variable.png`)** : la variable de couleur
se crée **dans le sélecteur**, par un bouton « Create Color Variable » sous la rangée
du modèle — pas depuis un rail. Le rail `Variables` sert ensuite à la retrouver,
la renommer, la lier. Nommé aussi, pas fait : **le plan du
sélecteur suit le modèle** (LCH, LAB, OKLAB montrent une autre nappe que le carré
saturation / valeur).

**État codé au 05/09** (commits `69af37b2` et `34a1b2bf`, sondes 68a-f, 239/239) :

- **le bouton « Créer une variable de couleur »** sous la rangée modèle : un clic
  crée « Couleur N » (clé `couleur_N`) depuis la couleur courante — remplissage
  **ou arrêt de dégradé** — et cette couleur la **référence** ; rien ne change à
  l'écran, le pied et la Console le disent ;
- quand la couleur courante **référence** une variable, le sélecteur montre
  pastille · nom (ou « @x : variable absente » en rouge) · **« Détacher »** (le
  littéral que l'œil voyait ; absente : magenta, dit) ; le tampon hexa montre la
  valeur **résolue** ; les trois écritures (carré SV, hexa, RGB / HSB) passent par
  **une porte** : si la couleur référence une variable, c'est **la variable** qui
  s'écrit (`PoserValeur`, dans le mode courant s'il est déclaré, dans le défaut
  sinon) — tout ce qui la référence suit, la référence tient, **aucun écart
  d'instance** n'est posé (le remplissage de l'instance n'a pas changé). C'est la
  propagation Q51 par la référence, le même chemin qu'`a428bb2b` ;
- le modèle : **un seul visiteur** de toutes les couleurs (clé simple, texte, bord,
  remplissages et arrêts, bordures, effets, états, arbres des déclarations), lu
  trois fois — `CompterUsagesVariable`, `DetacherVariable`, `SupprimerVariable`
  (**refuse** tant qu'elle est utilisée, le nombre dit). *Un champ de couleur
  ajouté ailleurs serait un usage invisible : c'est pour ça qu'il n'y a qu'une
  table* ;
- **deux modes** (clair / sombre) : le document se réenregistre avec ses valeurs
  par mode et son mode courant, se relit, et rend différemment selon le mode
  (sonde 68e) — la place de Dark Pro / Light Pro est **éprouvée**, la bascule
  d'interface reste nommée ;
- **le rail `Variables`** (gauche, onglet à côté de la Hiérarchie, la place de
  Lunacy) : pastille du mode courant, **nom renommable sur place** (clic, frappe,
  clic ailleurs), clé « @… », une ligne par mode, badge d'usages ×N, **poubelle
  gardée** (« utilisée par N remplissages — détachez-les d'abord ») ; le mode
  courant est **dit** en tête ; vide, le rail dit où l'on crée une variable. Le
  témoin sans fenêtre clique dans les rectangles que le panneau a dessinés
  (`RectNom`, `RectPoubelle`), pas dans une géométrie devinée.

**Le geste attend l'œil de Rodolf** (l'onglet, le renommage à la souris, le rendu
du bouton dans le popover) — listé, pas déclaré livré. **Fait ensuite (05/09,
sonde 68g)** : **lier une variable existante** depuis le sélecteur — « Lier ˅ » à
droite de « Créer une variable », la liste dépliée dans le popover (six au plus,
le pied renvoie au rail au-delà), un clic fait de la couleur une référence.
**Nommé, pas fait** : la **bascule de mode** dans l'interface ; le nom d'une
variable dans le popover de **bordure** ; le glisser d'une variable depuis le
rail vers une pastille. Les **styles** : §15.15.

## 15.15 ✅ STYLES DE CALQUE ET DE TEXTE — le plan, écrit avant le code (05/09), puis CODÉ la même nuit

> **État codé (05/09, nuit — commits « modèle + format + propagation », « rail Styles »,
> « rangée de section » ; sondes 69a-g, 246/246 ; mutation : `PropagerStyle` qui ignore
> le style → six cas rouges).** Le plan ci-dessous a été exécuté tel quel, avec trois
> écarts assumés, dits ici plutôt que découverts :
>
> 1. **`NkStyle.apparence` est un nœud sans géométrie** (pas des listes à plat) : c'est
>    ce qui donne « les mêmes lecteurs » sans second parseur — `style = …` ouvre un
>    troisième porteur (`dansStyle`, comme `dansDecl`) et `fond_i` / `bord_i` / `effet_i`
>    / `police_px` / `graisse` / `couleur_texte` vont à l'apparence. Les écrivains
>    `EcrireFonds` / `EcrireBords` / `EcrireEffets` sont **extraits** d'`EcrireNoeud`
>    (déplacés, pas recopiés) et servent aussi à l'**empreinte** d'un ensemble — ce que
>    le fichier écrirait — qui est l'unique comparateur.
> 2. **La propagation est une copie à la modification** (`PropagerStyle`, le patron de
>    `PropagerVersInstances`), pas une résolution au dessin : c'est *le* mécanisme des
>    instances, sous *leurs* bits. Le peintre, le pointage et l'export n'ont rien eu à
>    apprendre. L'écart se **détecte** à l'édition humaine (`MarkHumanEdit` →
>    `DetecterEcartsStyle` : le nœud lie un style et l'ensemble diffère → la main vient
>    d'écrire) — une porte, pas seize sites.
> 3. **La police (famille) n'est pas dans le style de texte** : le nœud ne la porte pas
>    (`police_px`, `graisse`, `couleur_texte` seulement). Un paramètre déclaré non
>    honoré serait pire qu'absent.
>
> Fait aussi, non prévu : `NewDocument` et `Load` ne vidaient pas `variables` (un
> chargement gardait celles du document d'avant) — vidées maintenant. Nommé : la
> capture Lunacy du rail Styles manque (la place de « Créer » vient de l'icône de FILLS) ;
> les bordures et les effets se lient par la rangée de REMPLISSAGES (un style de calque
> est l'ensemble des trois). Lier une variable existante depuis le sélecteur : fait
> ensuite (68g).

**Le plan tel qu'il a été écrit avant le code** (gardé : il dit pourquoi les choix sont ce
qu'ils sont).

### Ce que c'est (rappel du périmètre tranché)

Un **style de calque** = un nom pour un ensemble **remplissages + bordures +
effets** ; un **style de texte** = un nom pour **police + taille + graisse +
couleur**. Un style peut **référencer des variables** (ses couleurs sont des
`@clé` comme ailleurs — rien de neuf à écrire pour ça, `NkGCouleur` résout).

### Le modèle

- `NkStyle { clé, nom, genre ("calque" | "texte", texte libre, inconnu préservé),
  fills, borders, effets, police, taille, graisse, couleurTexte, inconnus }` —
  déclaré dans le document (`NkUIDocument::styles`), comme `variables`.
- Le nœud porte **une référence par genre** : `styleCalque = @clé`,
  `styleTexte = @clé` (clés additives, absentes tant qu'aucun style n'est lié ;
  un document d'avant se réenregistre octet pour octet).
- 🔴 **La propagation est celle de Q51 / §15.6, mot pour mot, et par le MÊME
  mécanisme que les instances** : le nœud lié **hérite** des listes du style tant
  qu'il n'a pas posé d'écart ; poser une couleur sur un nœud lié **matérialise**
  la liste (`MaterialiserFills`, déjà là) et lève le bit `EcartRemplissages` —
  exactement ce que fait une instance vis-à-vis de sa déclaration. Modifier le
  style propage donc à tout ce qui le lie **sauf** les propriétés surchargées.
  Pas un second chemin : la table `NkTousLesEcarts` nomme déjà les bits.
- **Résolution** : `FondEffectif()`, les bordures et les effets lus par le
  peintre passent par une porte `NkListesEffectives(n, doc)` qui rend celles du
  nœud si l'écart est levé, celles du style sinon — **un seul endroit**, lu par
  le peintre, le pointage et l'inspecteur.

### Le format

```
style = primaire genre=calque nom="Bouton primaire"
style_fond_1 = primaire @accent 100
style_bord_1 = primaire #30363d 2
style_effet_1 = primaire ombre #000000 0 2 8
style = titre genre=texte nom="Titre" police=Inter taille=24 graisse=700 couleur=@encre
```
Les lignes `style_*` réutilisent **les mêmes lecteurs** que `fond_i` / `bord_i` /
`effet_i` d'un nœud (une clé de style devant) : pas un second parseur.

### L'interface

- Le rail **`Styles`** (à côté de `Variables`) : liste, nom renommable, aperçu
  (une pastille des remplissages), usages ×N, poubelle gardée — même gabarit que
  `VariablesPanel`, dont on **extrait** le squelette de rangée plutôt que de le
  recopier.
- Dans l'inspecteur : en tête de FILLS / BORDERS / EFFECTS, une rangée « Style :
  (aucun) ˅ » pour lier / délier ; « Créer un style depuis ce calque » dans le
  menu de la section (le geste de Lunacy : *Create style* sur la section).
- Un style **lié** montre ses lignes **grisées** (héritées) jusqu'à la première
  édition, qui les matérialise et le dit (« surcharge locale — Réinitialiser »).

### Les témoins, écrits d'avance

1. un style de calque lié à deux rectangles : modifier le style change les deux ;
   poser une couleur sur l'un lève l'écart, il tient, l'autre suit ;
2. un style de texte lié à deux textes : idem sur la taille ;
3. l'aller-retour fichier : les lignes `style_*`, l'inconnu préservé, un
   document sans style ne gagne aucune ligne ;
4. un style dont une couleur est `@variable` : changer la variable change les
   deux rectangles (les deux propagations se composent) ;
5. la suppression gardée (« utilisé par N calques ») ;
6. le rail : renommer, garde, supprimer (le gabarit de 68f).

**Coût estimé** : moyen — un lot d'une nuit, à condition de ne pas écrire un
second chemin de propagation. C'est le point à surveiller à la relecture.
