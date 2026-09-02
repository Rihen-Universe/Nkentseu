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
