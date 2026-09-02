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

## 15.6 ❓ LA QUESTION QUI REVIENT À RODOLF — NON TRANCHÉE ICI

> **Quand l'auteur modifie une déclaration, ses instances suivent-elles ?**

Les deux réponses sont défendables et **ne se rattrapent pas l'une l'autre** :

| | **A — les instances suivent** (mise à jour vivante) | **B — les instances gèlent** (mise à jour explicite) |
|---|---|---|
| pour | c'est *le* bénéfice d'un composant : corriger une fois, corriger partout | rien ne bouge sous les pieds ; on met à jour quand on veut |
| contre | modifier une déclaration peut changer douze écrans **sans qu'on les regarde** | le bénéfice s'évapore : douze instances à mettre à jour à la main |
| ce que ça impose | rien de plus (la propagation est le défaut) | une **version** par instance, et un signal « une mise à jour existe » |

**Ce que fait la source** : Lunacy propage (A), avec la règle « sauf là où une
surcharge existe » qui est déjà en §15.3.

📌 **Ma recommandation, et ce n'est qu'une recommandation** : **A**, parce que
c'est la raison d'être d'un composant, et parce que §15.3 en amortit déjà le
danger — le travail fait sur une instance est protégé par ses écarts. **Mais B
devient obligatoire le jour où les composants se partagent** : on ne veut pas
qu'une déclaration d'un tiers change nos écrans sans qu'on l'ait demandé.

➡️ **Posé à Rodolf dans `echanges/nkuidesign.questions.md`. Rien ne s'écrit sur
ce point avant sa réponse** — et le reste du chantier n'en dépend pas, donc il
n'est pas bloquant.

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

📌 **Ma recommandation : les six.** La liste est fermée *et déjà validée* ; en
exposer trois créerait un second vocabulaire d'états à côté du premier — ce qu'on
cherche précisément à éviter. Mais **je ne tranche pas** : c'est la liste de
Rodolf. Posée en Q53.

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
5. Le reste de §9 (états, échanger, supprimer→cadres) — après.

⚠️ **La déclaration vit DANS LE DOCUMENT qui l'a créée**, et rien d'autre pour
l'instant (`12_…` §12.3(a)). La bibliothèque partagée est un chantier de
*résolution de dépendances*, pas d'éditeur ; les mélanger mêlerait deux problèmes
dont un seul est urgent. Et ça ne la ferme pas : une déclaration locale se
**promeut** plus tard.
