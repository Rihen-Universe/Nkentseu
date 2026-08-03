# Idées d'architecture à ne pas perdre

> Décisions de principe posées par Rihen, à instruire AVANT les chantiers
> qu'elles conditionnent. Référencé depuis `CLAUDE.md`. Datées, avec le
> contexte qui les a fait naître — une idée sans son pourquoi se réinvente mal.

## 1. Système de plugins (2026-08-03)

**L'intention** : les utilisateurs doivent pouvoir programmer des utilitaires
pour le système **sans recompiler l'application entière**.

**Ce que ça implique, à trancher le moment venu :**
- Frontière de l'ABI : des plugins natifs (DLL/.so chargés à chaud) exigent une
  interface C stable — les types C++ de NKRenderer/NKGui ne peuvent pas
  traverser tels quels. Alternative : piloter par un langage embarqué
  (**NkSpark existe déjà** dans l'écosystème — voir la mémoire projet) ou par
  le substrat **NKGraph** (les nœuds comme unité d'extension).
- Surface exposée : quoi au juste ? (outils d'édition, importeurs/exporteurs,
  modificateurs, nœuds de matériaux, panneaux.)
- Découverte : dossier `<utilisateur>/NK3DModeler/plugins/` — même convention
  premier-trouvé-gagne que les thèmes et `data/icons.cfg`.
- Sécurité et versionnage : un plugin compilé contre une vieille interface ne
  doit pas planter l'application — numéro de version d'interface vérifié au
  chargement, refus poli sinon.

## 2. Tout paramètre réglable doit être animable (2026-08-03)

**L'intention** : quand le chantier animation s'ouvrira, chaque valeur exposée
dans l'interface (ciel, lumières, matériaux, transformations…) devra pouvoir
recevoir des **clés à une frame donnée** et être jouée.

**L'acquis qui rend ça crédible — à respecter dans TOUT nouveau code :**
- Les modificateurs ont déjà posé la règle (mémoire projet
  `project_modificateurs_pile_animables`) : une cible d'animation est un couple
  de **clés stables** (identifiant, nom du paramètre) — ne jamais renommer un
  nom publié, ne jamais renuméroter un enum.
- Le ciel vient de prouver le second prérequis : un paramètre n'est animable
  que s'il est **évalué à chaque image** (le passage du ciel cuit au ciel GPU a
  précisément été fait pour ça). Un réglage qui exige une régénération n'est
  pas animable — ou alors sa partie coûteuse doit être séparée (Prague : ciel
  visible en continu, éclairage sur demande).
- Le rôle de thème `nk3d.parametre_animable` existe déjà pour marquer ces
  paramètres dans l'interface.

**Règle pratique dès maintenant** : tout nouveau réglage doit répondre à la
question « que se passe-t-il si cette valeur change à chaque frame ? » — si la
réponse est « ça recuit / ça réalloue / ça recharge », il faut le découper
comme le ciel l'a été.

## 3. Plan sol infini, en option (2026-08-03)

**Le constat** : la grille infinie existe (`NkRender3D::SetInfiniteGridEnabled`)
et son shader porte déjà `cellColor.w` = opacité de l'intérieur (0 = voir à
travers, 1 = opaque).

**Deux niveaux de réponse :**
1. **Immédiat, gratuit** : grille opaque (`cellColor.w = 1`) + lignes coupées
   (`showMinor/showMajor = false`) = un sol visuel infini. Limite : non éclairé,
   ne reçoit ni ombres ni brouillard PBR — c'est un aplat.
2. **Le vrai sol infini** : un plan PBR qui **suit la caméra en XZ** (le quad de
   la grille sait déjà le faire), rendu dans la passe opaque avec matériau,
   ombres reçues et brouillard. Points d'attention : la profondeur à l'horizon
   (précision Z), le raccord avec la couleur « Sol » du ciel (les deux doivent
   pouvoir s'accorder), et l'exclusion du picking/sélection (ce n'est pas un
   objet de scène).

À faire comme option du panneau Rendu, à côté de la grille.
