# Tester Nkentseu sur un Mac

Ce guide explique comment construire Nkentseu sur votre Mac, lancer les
programmes et renvoyer les résultats. Suivez les étapes dans l'ordre et copiez
les commandes telles quelles. Vous n'avez pas besoin de connaître le projet.

Durée : environ 30 minutes de construction (la première fois), puis 15 minutes
où vous regardez les programmes.

Pourquoi on a besoin de vous : personne dans l'équipe n'a de Mac. Notre
intégration continue (CI) tourne sur un Mac virtuel. Elle vérifie que tout se
construit et que Metal dessine. En revanche, elle n'a pas de vrai écran, donc
pas d'écran Retina, et elle ne peut pas juger de ce que voit un humain :
netteté du texte, taille des fenêtres, passage d'un écran à l'autre.

---

## 1. Préparer le Mac (une seule fois)

Ouvrez l'application **Terminal** (Applications → Utilitaires → Terminal).

1. Installez les outils de développement d'Apple (compilateur `clang`, `git`,
   `python3`). Tapez cette commande, puis acceptez la fenêtre qui s'ouvre :

   ```bash
   xcode-select --install
   ```

   Si le Terminal répond « already installed », les outils sont déjà là : passez
   à la suite.

2. Vérifiez que tout répond :

   ```bash
   clang --version && git --version && python3 --version
   ```

   Chaque commande doit afficher une version. Il faut Python 3.9 ou plus récent.

Xcode complet n'est **pas** nécessaire : les outils de ligne de commande
suffisent.

---

## 2. Récupérer Jenga et Nkentseu

**Jenga** est l'outil qui construit Nkentseu. Il faut **exactement** la
version indiquée ci-dessous, sinon la construction s'arrête sur des erreurs qui
ne parlent pas de Jenga.

Copiez ce bloc en entier :

```bash
mkdir -p ~/nk && cd ~/nk
git clone --depth 1 --branch fix/macos-2.8.8 https://github.com/Rihen-Universe/Jenga.git jenga
git clone --depth 1 --branch feat/macos-metal --recurse-submodules --shallow-submodules \
    https://github.com/Rihen-Universe/Nkentseu.git nkentseu
```

> **Quand Jenga 2.8.8 sera publié**, remplacez `--branch fix/macos-2.8.8` par
> `--branch v2.8.8`. La bonne version est toujours écrite dans le fichier
> `nkentseu/Applications/NKCode/JENGA_VERSION`.

Ensuite, **dans chaque nouveau Terminal**, indiquez à Python où se trouve Jenga :

```bash
export PYTHONPATH="$HOME/nk/jenga"
cd ~/nk/nkentseu
python3 -c "import Jenga; print(Jenga.__version__)"
```

La dernière commande doit afficher un numéro de version, par exemple `2.8.7`
ou `2.8.8`. Si elle affiche une erreur, c'est que le `export PYTHONPATH=…` n'a
pas été fait dans ce Terminal.

### Mettre à jour plus tard

Si on vous demande de retester après une correction :

```bash
cd ~/nk/jenga && git pull
cd ~/nk/nkentseu && git pull && git submodule update --init --recursive --depth 1
```

---

## 3. Tout tester d'un coup (recommandé)

Un seul script fait tout le travail : il construit les programmes, prend des
captures automatiques, puis vous montre chaque programme et note vos réponses.

```bash
export PYTHONPATH="$HOME/nk/jenga"
cd ~/nk/nkentseu
bash Tools/Mac/tester-mac.sh
```

Il y a cinq étapes :

1. **Vérification des outils**, et relevé de votre Mac et de vos écrans.
2. **Construction** de 6 programmes. Chacun affiche `construit` ou `ECHEC`. La
   première fois, comptez 10 à 30 minutes. Les fois suivantes, c'est rapide.
3. **Captures automatiques** : des fenêtres s'ouvrent et se referment toutes
   seules. N'y touchez pas.
4. **À vous de regarder** : chaque programme s'ouvre à son tour. Lisez la
   consigne dans le Terminal, regardez la fenêtre, faites ce qui est demandé,
   puis **fermez la fenêtre** (croix rouge ou touche `Échap`). Répondez ensuite
   aux questions dans le Terminal : `o` pour oui, `n` pour non, puis un
   commentaire libre, ou `Entrée` si vous n'avez rien à ajouter.
5. **Bilan** affiché à l'écran.

Si la construction échoue, allez directement à l'étape 6 : l'erreur est dans
les journaux et c'est exactement ce dont on a besoin.

---

## 4. Ce que vous devez voir

| Programme | Ce qui doit apparaître |
|---|---|
| **Tuto01Fenetre** | Une fenêtre vide, au milieu de l'écran, d'une taille normale. La croix rouge la ferme immédiatement. |
| **Tuto02Renderer** | En haut à gauche, un panneau sombre avec trois lignes : `== Tuto 02 : NKRenderer est initialise ==`, `Backend : Metal`, `FPS ~ …`. |
| **Tuto03Scene** | Un **cube doré** (sombre et métallique) qui tourne au centre, une **sphère blanche** à sa droite, un **sol gris** et leurs **ombres** au sol. Le panneau de texte indique `Backend : Metal`. Si des **bandes sombres horizontales** traversent le bas du sol, notez-le : la CI en voit, DirectX 11 non. |
| **Tuto04Camera** | La même scène. La caméra tourne avec le bouton du milieu de la souris (ou à deux doigts sur le trackpad) et zoome à la molette. |
| **Tuto05Meshes** | Des objets. Un clic sélectionne celui qui est **sous** le curseur. `E`, `C` et `R` modifient la sélection. |
| **NkDames** | D'abord l'écran d'accueil RIHEN : logo, texte, « toucher pour passer » (référence : `docs/captures-reference/nkdames-metal-ci.png`). Un clic passe au plateau de dames. Tout est rendu en Metal (option `--backend=metal`). |

Des images de référence sont dans le dépôt, dossier `docs/captures-reference/` :

- `tuto03-metal-ci.png` : Tuto03 rendu en Metal par la CI (Mac virtuel) ;
- `tuto03-dx11-windows.png` : la même scène rendue par DirectX 11 sous Windows ;
- `nkdames-metal-ci.png` : l'écran d'accueil de NkDames rendu en Metal par la CI.

Votre Tuto03 doit leur ressembler : ciel gris-bleu, sol gris, cube doré sombre
et métallique, sphère blanche, ombres nettes au sol. Les seules différences
attendues sont la taille de l'image et les chiffres FPS. Les captures de chaque
passage de la CI sont aussi publiées sur GitHub Actions (artefacts
**macos-captures-metal** et **macos-captures-nkcanvas-metal**, téléchargeables
avec un compte GitHub) :
<https://github.com/Rihen-Universe/Nkentseu/actions/workflows/macos-noyau.yml?query=branch%3Afeat%2Fmacos-metal>

Les captures faites **sur votre Mac** sont dans `test-mac/captures/`. Ouvrez-les
avec Aperçu et comparez-les à ce que vous avez vu.

---

## 5. Vérifier le Retina

Un écran Retina a deux pixels physiques par point dans chaque direction. Deux
erreurs typiques sont possibles : une image deux fois trop petite (dans un coin
de la fenêtre), ou un texte flou parce qu'il a été agrandi.

1. **Est-ce que mon écran est Retina ?** Le script l'a relevé dans
   `test-mac/ecrans.txt`. Cherchez la ligne `Retina` ou `UI Looks like`. Tous
   les MacBook récents sont Retina.
2. **Netteté du texte** : dans Tuto02 et NkDames, approchez-vous de l'écran. Les
   lettres doivent avoir des bords nets, comme dans le Terminal. Un texte
   baveux ou « doux » est un défaut : notez-le.
3. **Taille** : l'image doit remplir **toute** la fenêtre. Si elle n'en occupe
   qu'un quart, en bas à gauche ou en haut à gauche, c'est un défaut de
   conversion entre points et pixels : notez-le.
4. **Redimensionnement** : tirez le coin de la fenêtre. L'image doit suivre, sans
   se déformer ni devenir floue.
5. **Deux écrans** (si vous avez un écran externe, idéalement non Retina) :
   glissez la fenêtre de Tuto03 ou de NkDames d'un écran à l'autre, puis
   revenez. Après chaque passage, l'image doit rester nette et remplir la
   fenêtre. Faites de même avec la fenêtre agrandie en plein écran (bouton
   vert).

Pour relancer un programme à la main, par exemple NkDames en Metal :

```bash
cd ~/nk/nkentseu
./Build/Bin/Debug-macOS/NkDames/NkDames --backend=metal
```

Les tutoriels choisissent Metal tout seuls. Pour forcer un autre dorsal :
`NK_GFX_BACKEND=software ./Build/Bin/Debug-macOS/Tuto03Scene/Tuto03Scene`.

---

## 6. Renvoyer les résultats

```bash
cd ~/nk/nkentseu
bash Tools/Mac/rassembler-journaux.sh
```

Le script fabrique un fichier `nkentseu-test-mac-<date>.zip` **sur votre
Bureau**. Envoyez-le tel quel. Il contient :

- vos réponses ;
- les captures d'écran ;
- les journaux de construction et d'exécution ;
- les rapports de plantage éventuels ;
- la description de votre Mac et de vos écrans.

Il ne contient rien d'autre de votre machine.

---

## En cas de souci

| Problème | Que faire |
|---|---|
| `No module named 'Jenga'` | Tapez `export PYTHONPATH="$HOME/nk/jenga"` dans ce Terminal. |
| `xcrun: error: invalid active developer path` | Tapez `xcode-select --install`. |
| La construction échoue | Pas besoin de chercher : lancez l'étape 6 et envoyez le zip. |
| Une fenêtre ne se ferme pas | Tapez `Cmd+Q`. Si elle résiste, `Cmd+Option+Échap`, puis « Forcer à quitter ». Notez-le dans votre réponse. |
| macOS dit que l'application « ne peut pas être ouverte » | Le programme n'est pas signé : lancez-le depuis le Terminal comme indiqué ci-dessus, pas en double-cliquant. |

Merci !
