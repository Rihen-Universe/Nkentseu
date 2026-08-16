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
> Derniere mise a jour : **2026-08-16**, branche `feat/conquerorlab`.
> **Chantier CLOS a cette date.** Ce qui suit est ecrit pour l'agent suivant, qui
> n'aura pas le contexte de celui-ci.

---

## 🔭 LES TROIS HORIZONS

### Court — pour qui reprend

- **Le retrait de la STL.** Autorise par Rodolf, jamais commence. Mesure du
  16/08 : **80 occurrences de `std::` dans 15 fichiers** de `src/`, plus
  **22 dans les en-tetes publics** — dont **19 dans `ConquerorRegleFacile.h`**,
  celui que le stagiaire lit en premier. Tous les remplacements existent
  (`NkString`, `NkVector`, `NkFunction`, `NkMutex`, `NkAtomic`). ⚠️ `std::snprintf`
  demande un **jugement, pas un remplacement** : troncature silencieuse d'un cote,
  allocation de l'autre. Un commit par fichier, le compte doit descendre de facon
  monotone.
- **L'ATH des joueurs** (demande de Rodolf, R2) : cartes sur les **bords** du
  plateau avec cadre de totem, au lieu d'une ligne. ⚠️ Le dossier `totems/` est
  **vide** : concevoir le cadre pour qu'il fonctionne **sans image**. L'origine des
  images n'a jamais ete tranchee — **question ouverte pour Rodolf**.
- Rejouer le parcours du kit sur une machine **sans MSYS2** — le seul obstacle du
  vrai stagiaire qui n'ait jamais ete essaye.

### Moyen — le jalon

- **Source unique des 13 racines d'inclusion.** Le **controle** existe
  (`verifier_la_pile.py`, voir dette 2) et suffit a empecher l'incident. Ce qui
  reste est la **generation** de `NkcLayout::Includes` depuis un fichier de
  donnees : c'est du C++ compile, il ne peut rien lire a l'execution. ⚠️ Si tu le
  fais : **un fichier genere doit dire qu'il est genere** (banniere) **et etre en
  lecture seule** — sinon quelqu'un l'edite et la generation suivante efface sa
  modification sans un mot.
- Le kit sur **Linux et macOS** : le code est ecrit pour, rien n'y a jamais
  tourne.
- **Reintegrer le chapitre 07 du cours avec son code.** Le chapitre existe
  (`md/07-regarder-et-habiller.md`, `tex/chapitres/07-regarder-et-habiller.tex`)
  mais **n'est pas dans le PDF** : il decrit le zoom, le multijoueur, les totems en
  images et la langue, dont le code n'est pas sur cette branche.

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

Provenance de tous les chiffres : **2026-08-16**, worktree
`D:\Projets\2026\Nkentseu\Nkentseu-conqueror`, branche `feat/conquerorlab`
(base `origin/main` `10452ae0`), **Release-Windows ET Debug-Windows**, jenga
v2.4.0, binaire reellement execute, kit copie **hors du depot, sur un autre
disque**.

| | |
|---|---|
| amorcage des plateaux | **0 installe sur 15 livres** avant correctif -> **15/15** apres |
| kit stagiaire | ne pouvait **pas etre fabrique** -> **264 fichiers, 16,7 Mo** (Release) / **33,5 Mo** (Debug) |
| module ecrit avec la voie facile | **compile et charge en 0,58 s** — `module pret : MesReglesAMoi (1.0.0)` |
| grille deposee par le stagiaire | **vue et confirmee** : `16 retenue(s) sur 16 fichier(s) .json vu(s)` |
| PDF du cours dans le kit | **empreinte SHA-256 identique** a celle du PDF regenere |
| Debug contre Release | **chaine de journal identique, ligne pour ligne** |
| voie facile au banc ABI du projet | **11 verifications OK, 0 echec** ; partie complete de **22 coups**, terminee d'elle-meme, empreinte deterministe |
| etiquettes de cases | **661 / 661** sur 15 plateaux, **0 collision**, 0 divergence contre une reference calculee independamment |

**La preuve d'acceptation porte sur le PARCOURS DU STAGIAIRE, pas sur les
correctifs** : kit copie hors du depot, une regle et une grille deposees en
suivant le LISEZMOI mot pour mot, les deux vues.

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

### 4. Le travail echoue sur une autre branche — NE PAS LE REECRIRE

Le 13/08 a 15:12, le commit **`b0ab15b9`** (« Materiaux : choisir le TYPE avant
la creation »), un commit **NK3DModeler**, a emporte par inadvertance
**23 fichiers de ConquerorLab et de son cours** sur la branche
`refonte-interface-nk3dmodeler`. Ils y ont dormi trois jours, invisibles de ce
cote.

**Recupere et prouve** (voir ci-dessus) : `ConquerorRegleFacile.h`,
`ConquerorFacile.h`, les 4 exemples, `mini_3x3.json`, les chapitres 07 et 08 du
cours, les fiches stagiaires A1 et A2, `LIVRAISON.md`, `SUJETS.md`.

⚠️ **PAS encore recupere, et c'est deliberé** — ces fichiers appartiennent au
chantier ATH/langue, hors du perimetre de cloture :

```
Applications/ConquerorLab/src/ConquerorLab/NkcBoardView.h       zoom et deplacement
Applications/ConquerorLab/src/ConquerorLab/NkcTotemLibrary.h    totems en images
Applications/ConquerorLab/src/ConquerorLab/NkcLang.h            langue de l'interface
Applications/ConquerorLab/totems/LISEZMOI.txt
Applications/ConquerorLab/boards/  hexagone_7x7_3j, hexagone_8x8_4j,
                                   carre_9x9_4j, hexagone_30x30_grand
```

Ils sont recuperables par `git checkout refonte-interface-nk3dmodeler -- <chemin>`.
⚠️ **Mais pas les fichiers marques `M`** de cette branche (`NkcSession.h`,
`NkcBoardPanel.h`, `NkcModuleHost.h`…) : ils portent **les correctifs de cette
branche-ci**, et une copie en bloc les ecraserait. La reprise de l'ATH demande une
fusion, pas une copie.

**La lecon, et elle est deja au corpus** : commiter par chemins explicites ne
protege pas seulement le travail des autres — ca empeche **de faire disparaitre le
sien** dans une branche ou personne ne le cherchera.

---

## 🎯 DECISIONS PRISES (le 2026-08-16)

### Les etiquettes de cases : **schema A**, et c'est fait

Lettre = colonne offset, chiffre = rangee. `C4`. Implante derriere
`NkcCellLabel.h`, **une fonction unique** : changer de schema coute une ligne.

**Ce n'etait pas une representation a inventer, c'est une representation qu'on
jetait** — `boards/_generer.py` ecrit les plateaux en `(colonne, rangee)` puis
convertit en axial, et son en-tete appelle cette conversion « LE piege des
plateaux ».

Perimetre applique : **axial inchange** en memoire, en ABI et dans les `.json` ;
a l'ecran l'etiquette **remplace** l'axial (« (3,-1) -> (4,-1) » ne se lit pas) ;
dans la trace copiee elle **s'ajoute** en deux colonnes `de vers` a cote des
colonnes `q/r`. **Cout de compatibilite : zero** — les traces deja enregistrees
restent rejouables.

Verification : **661 etiquettes sur 15 plateaux**, comparees a une reference
calculee independamment — **0 divergence, 0 collision, 0 etiquette illisible**.

### ⚠️ Les noms de plateaux : MA TROUVAILLE ETAIT FAUSSE, rien n'a ete renomme

J'avais rapporte le 15/08 que `rectangle_8x6.json` et `carre_8x8_*.json` etaient
**penches a l'ecran** et donc mal nommes. **C'est faux.** Mesure du 16/08,
plateau par plateau, en lisant la topologie de chacun :

| plateau | topologie | forme reelle |
|---|---|---|
| `rectangle_8x6`, `carre_8x8_*`, `plus`, `diamant`, `mini_3x3` | **SQUARE_4 / SQUARE_8** | bloc droit — `CoordToUnit` rend `{2q, 2r}`, **aucun cisaillement possible** |
| `hexagone_*` | HEX_POINTY / HEX_FLAT | bloc droit |
| `parallelogramme_6x7`, `parallelogramme_8x5` | HEX_POINTY | **penches** — et ils s'appellent « parallelogramme » |

**Les 15 noms livres sont exacts.** Mon erreur : j'avais deduit la forme d'une
signature d'etiquetage en appliquant le decalage hexagonal `q + (r>>1)` a **tous**
les plateaux, y compris les carres. L'escalier que j'observais etait un artefact
de mon instrument, pas une propriete du plateau. *Bon objet, bonne mesure,
mauvaise hypothese implicite.*

Le champ d'affichage `nom` a quand meme ete ajoute (`NkcBoardFile::libelle`,
retro-compatible : absent = nom de fichier), mais **pour une autre raison** : un
stagiaire qui depose `mon_plateau_v3_final.json` peut lui donner un nom lisible
sans renommer son fichier. **Aucun plateau livre n'a ete touche.**

---

## 🚫 CE QUI N'A JAMAIS ETE VERIFIE

A dire en clair, parce qu'un perimetre non enonce se transmet comme une
certitude.

| regime | etat |
|---|---|
| **Windows Release et Debug** | **verifie**, kit produit, parcours complet, journal identique |
| Linux, macOS | **jamais construit** — le code est ecrit pour, rien n'y a tourne |
| Android, Web | **jamais construit**. La compilation a chaud y est desactivee a la construction (`NKC_CAN_COMPILE 0`) : seuls les modules internes existeraient |
| **une machine sans compilateur** | **jamais execute**. La branche « aucun compilateur trouve » est desormais atteignable **par construction et par lecture** (elle etait du code mort), mais la seule facon de l'executer ici serait de renommer le `clang++` de la machine — ce qui casserait la compilation d'un autre agent en plein vol |
| le kit sur une machine **sans MSYS2** | **jamais essaye** — et c'est le premier obstacle du vrai stagiaire |
| l'atelier joue par un humain | non observe : les preuves passent par le journal et par le banc ABI du projet, pas par des clics |
