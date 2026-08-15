# ROADMAP — ConquerorLab

> L'atelier de jeu de plateau confie aux stagiaires : ils y ecrivent un moteur de
> regles ou une IA en C++, l'atelier compile leur `.cpp` **pendant qu'il tourne**
> et le charge a chaud.
>
> Ce fichier est **suivi par git** : il est donc present dans tous les worktrees,
> contrairement au `CARNET.private.md` qui l'accompagne et qui ne survit ni au
> clone ni a la suppression du worktree. Ce qui est mesure et acquis monte ici ;
> le brouillon vivant reste au carnet.
>
> Derniere mise a jour : **2026-08-15**, branche `feat/conquerorlab`.

---

## 🔭 LES TROIS HORIZONS

### Court — la semaine

- **Faire trancher le schema d'etiquettes** par Rodolf (voir « Decisions en
  attente »), puis ecrire la conversion derriere **une seule fonction**
  (`NkcCellLabel.h`) pour que le schema reste interchangeable en une ligne.
- **Reprendre les noms des plateaux livres** : `carre_8x8_*.json` et
  `rectangle_8x6.json` sont des rectangles **en axial**, donc des
  **parallelogrammes pencheS a l'ecran**. Mesure a l'appui ci-dessous.
- Rejouer le parcours du kit sur une machine **sans MSYS2** — le seul obstacle du
  vrai stagiaire qui n'a jamais ete essaye.

### Moyen — le jalon

- **Source unique des 13 racines d'inclusion.** Aujourd'hui la liste vit dans
  `NkcLayout::Includes`, elle est **recopiee a la main** dans `Distribuer.ps1`, et
  le cours la cite. Rien ne les tient ensemble, et cette divergence a **deja**
  coute un kit non fabricable (cf. « Dette » ci-dessous). Le mecanisme doit
  comporter **un controle qui echoue** quand un consommateur diverge — sans quoi
  on remplace trois listes par une liste et trois copies.
- Le kit sur **Linux et macOS** : le code est ecrit pour, rien n'y a jamais
  tourne.

### Long — a quoi le module sert

**ConquerorLab n'est pas un jeu : c'est un banc d'essai pedagogique.** Sa reussite
ne se mesure pas au nombre de fonctionnalites mais a **ce qu'un stagiaire arrive a
faire sans lire les sources de l'atelier**. Tout arbitrage se tranche par la : une
fonctionnalite qui allonge le parcours d'entree coute plus qu'elle ne rapporte.

Corollaire, et il change les priorites : **un message d'erreur vaut mieux qu'une
fonctionnalite.** Les deux retours de stagiaires recus a ce jour portaient tous
deux sur un **silence**, jamais sur un manque.

---

## ✅ CE QUI EST MESURE ET ACQUIS

Provenance de tous les chiffres : **2026-08-15**, worktree
`D:\Projets\2026\Nkentseu\Nkentseu-conqueror`, branche `feat/conquerorlab`
(base `origin/main` `10452ae0`), **Release-Windows ET Debug-Windows**, jenga
v2.4.0, binaire reellement execute.

| | |
|---|---|
| amorcage des plateaux | **0 installe sur 15 livres** avant correctif -> **15/15** apres |
| kit stagiaire | ne pouvait **pas etre fabrique** -> **257 fichiers, 16,5 Mo** (Release) / **33,1 Mo** (Debug) |
| kit hors depot, autre disque | detecte `racine (kit)`, installe **14/14** grilles |
| module de stagiaire | **compile et charge en 0,7 s** — `module pret : MesReglesAMoi (1.0.0)` |
| grille deposee par le stagiaire | **vue et confirmee** : `15 retenue(s) sur 15 fichier(s) .json vu(s)` |
| Debug contre Release | **chaine identique** sur ce parcours — aucune divergence |

**La preuve d'acceptation est faite depuis le kit produit et rien d'autre** : kit
copie hors du depot, sur un autre disque, lance, une regle et une grille
deposees, les deux vues.

---

## 🧾 DETTES NOMMEES

### 1. `NkDirectory::GetFiles` — un nom qui laisse croire autre chose que ce qu'il fait

`GetFiles` rend des **chemins complets** ; son nom evoque des **noms de
fichiers**. La documentation, elle, est **juste et explicite** (« contenant les
chemins complets des fichiers trouves », `NkDirectory.h:170` et `:179`) : la dette
est dans le **nom**, pas dans le contrat.

**Quatre appelants de ConquerorLab** ont suppose la semantique inverse et recolle
le resultat derriere leur propre dossier — quatre fois la meme erreur au meme
endroit n'est pas quatre etourderies. Corrige ici ; **le cinquieme appelant fera
la meme supposition.**

**Balayage du 2026-08-15** — perimetre : tout le worktree, `--include=*.h,*.cpp,
*.hpp,*.inl`, sous-modules exclus. **28 sites d'appel, dont 24 hors
ConquerorLab : les 24 sont justes.** Deux portent meme un commentaire qui nomme
le piege (`NkOllamaLocate.h:78`, `NkCaseLoader.cpp:155`). *Contre-epreuve : la
methode d'inspection rejouee sur l'etat d'avant correctif (`origin/main`) voit
bien le defaut — la variable y etait meme nommee `names` alors qu'elle contenait
des chemins.*

### 2. Une liste, trois consommateurs, rien qui les tienne

`NkcLayout::Includes` (13 racines) est recopiee dans `Distribuer.ps1` et citee par
le cours. **La divergence a deja frappe** : `NKSerialization` etait reclame par
quatre listes et absent de la seule qui construit (`nkentseudependson` du
`.jenga`). Consequence mesuree : `Distribuer.ps1` s'arretait sur « Bibliotheque
introuvable » — **aucun kit ne pouvait exister** — et tout module de stagiaire
echouait au lien, **y compris l'exemple livre**. Corrige en ajoutant la
dependance, c'est-a-dire en **rendant la promesse vraie plutot qu'en la
reduisant**.

✅ **Un controle existe desormais : `verifier_la_pile.py`.** Il compare entre
elles les **cinq** listes qui decrivent la pile (`NkcLayout.h kRepo[]`,
`Distribuer.ps1 $modules`, `NkcModuleCompiler.h StackLibs`, `Distribuer.ps1
$libs`, `ConquerorLab.jenga nkentseudependson`) et **echoue avec un code de
sortie non nul**. Il refuse aussi de conclure si l'un des motifs ne matche plus :
cinq listes vides seraient « toutes egales », et ce serait reussir pour la
mauvaise raison.

**Valide en le rejouant sur l'incident qui l'a motive** : le `.jenga`
d'`origin/main` remis en place (NKSerialization absent), le controle sort

```
ECHEC   1 divergence(s) :
  - NKSerialization est liee aux modules du stagiaire mais ABSENTE de
    nkentseudependson : elle ne sera pas construite, donc le kit ne pourra pas
    etre assemble (incident du 2026-08-15)
```

avec le code 1 ; `.jenga` restaure, il repasse a 0. **Un garde-fou qui n'echoue
pas ne garde rien.**

Reste a faire : brancher ce controle **en tete de `Distribuer.ps1`** pour qu'il
soit lance sans qu'on y pense — un controle de sept secondes qui n'est jamais
lance vaut zero — et decider si `NkcLayout::Includes` doit etre **genere** depuis
un fichier de donnees plutot que verifie (question posee a Claude).

### 3. Deux fichiers source etaient invisibles a toute recherche

`NkcBoardLibrary.h` et `NkcSession.h` portaient chacun **un octet NUL brut** (un
`\0` mal echappe). `grep` les sautait **en silence** et `file` les disait `data`.
**Le defaut d'amorcage vivait dans l'un des deux.** Corrige, et promu au
`CLAUDE.md` parent.

**Balayage de controle, trois perimetres, tous avec contre-epreuve sur temoin :**

| perimetre | fichiers | porteurs |
|---|---|---|
| fichiers source suivis par git (worktree) | 3 098 | **0** |
| arbre `origin/main`, toutes extensions | 7 910 blobs | 2 (les deux ci-dessus, corriges sur la branche) |
| systeme de fichiers, **sous-modules et non-suivis inclus** | 4 757 | **0** |

---

## 🎯 DECISIONS EN ATTENTE (Rodolf)

### Le schema d'etiquettes des cases

**Ce n'est pas une representation a inventer : c'est une representation qu'on
jette.** Les plateaux naissent lisibles — `boards/_generer.py` les ecrit en
`(colonne, ligne)` puis convertit en axial, et son en-tete appelle cette
conversion « LE piege des plateaux ». On demande ensuite a l'utilisateur de
refaire le calcul de tete.

Perimetre propose et **valide** : etiquette en **affichage seul**, axial inchange
en memoire, en ABI et dans les `.json`, colonne `case` **ajoutee** au journal des
coups plutot que substituee — le cout de compatibilite tombe alors a **zero**.

**Le choix qui reste est le SCHEMA**, et il n'est pas neutre : mesure sur les
**14 plateaux livres, 652 cases**, aucun schema n'est parfait partout, et les deux
candidats sont **complementaires**.

| | cases etiquetees | collisions | trous | parfait sur |
|---|---|---|---|---|
| **A** lettre = colonne (offset odd-r) | 652 / 652 | 0 | 188 | les **6** plateaux `hexagone_*` |
| **B** lettre = axe axial `q` (diagonale) | 652 / 652 | 0 | 194 | carre, rectangle, parallelogramme |

*(Un « trou » est une etiquette possible qui ne designe aucune case — c'est ce qui
fait « sauter » la numerotation. Sur 3 160 paires de cases voisines, **les deux
schemas gardent un ecart maximal de 1** en colonne comme en rangee : la crainte
des voisins aux noms eloignes est infondee.)*

**Recommandation : A.** La topologie est `HEX_POINTY` — les **rangees** sont donc
les seules vraies droites a l'ecran (`CoordToUnit`, `NkcBoardRender.h:80` :
`x = √3(q + 0,5·r)`, `y = 1,5·r`). Le chiffre suit une ligne que l'oeil voit ; la
lettre zigzague d'une **demi-case**, ce qui reste lisible. Et A est exact sur les
plateaux `hexagone_*`, qui sont ceux du jeu.

### Les noms des plateaux livres ne decrivent pas leur forme

Consequence directe de la formule ci-dessus : un plateau rectangulaire **en
axial** est **penche a l'ecran**. Or `rectangle_8x6.json` et
`parallelogramme_6x7.json` ont la **meme signature structurelle** (0 trou sous B,
en escalier sous A) : ce sont la **meme forme**, sous deux noms differents. Et
`carre_8x8_hexagones.json` ne peut pas etre carre a l'ecran.

C'est du contenu livre, donc ce que l'utilisateur voit : **je ne renomme rien
sans arbitrage.**
