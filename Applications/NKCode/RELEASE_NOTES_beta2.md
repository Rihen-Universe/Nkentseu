# NKCode v0.1.0-beta.2 (Windows x64)

Merci aux testeurs de la bêta.1 : **11 rapports de bugs** en huit jours, avec
captures, étapes de reproduction et configurations. Cette version est presque
entièrement faite de vos retours.

## Ce que vos retours ont corrigé

| Issue | Problème | État |
|---|---|---|
| [#3](https://github.com/Rihen-Universe/NKCode-Beta/issues/3) · [#11](https://github.com/Rihen-Universe/NKCode-Beta/issues/11) | Impossible d'ouvrir un dossier qui n'est pas un workspace Jenga | ✅ **N'importe quel dossier s'ouvre** en mode édition simple (explorateur, éditeur, **terminal**, git). Le terminal démarre dans le dossier ouvert, workspace Jenga ou pas — il n'a besoin que d'un répertoire de départ. Seule la barre d'outils de construction reste réservée aux workspaces Jenga : sans `.jenga`, il n'y a rien à construire. Et si un `.jenga` **apparaît plus tard** dans le dossier (créé à la main, par `jenga init` dans le terminal intégré, ou récupéré par git), NKCode le détecte et **réactive la barre d'outils tout seul**. |
| [#7](https://github.com/Rihen-Universe/NKCode-Beta/issues/7) | Gel (« Not Responding ») au démarrage d'un projet | ✅ La détection des outils (PATH, variables d'environnement, WSL2) tournait sur le thread d'interface — le démarrage à froid d'une machine virtuelle WSL2 bloquait l'application plusieurs secondes. Déplacée en arrière-plan. |
| [#9](https://github.com/Rihen-Universe/NKCode-Beta/issues/9) · [#10](https://github.com/Rihen-Universe/NKCode-Beta/issues/10) | Le Jenga inclus ne s'activait pas / boutons de build inopérants | ✅ **Trois causes distinctes**, toutes corrigées : (1) le dossier de l'exécutable était déduit de `argv[0]`, peu fiable selon la façon de lancer le programme — NKCode ne trouvait donc pas son Python embarqué ; (2) seuls *Construire* et *Recompiler* passaient réellement par l'interpréteur embarqué — la liste des projets, *Nettoyer*, *Tests*, *Exécuter* et l'IntelliSense repartaient sur un `jenga` absent de votre machine (d'où un workspace qui s'ouvrait **sans aucun projet**, rendant les boutons inutilisables) ; (3) l'archive elle-même était incomplète : un dossier nécessaire au chargement de Jenga avait été retiré, ce qui cassait l'importation du module. **Toutes les commandes Jenga fonctionnent désormais sans Python installé**, y compris celles tapées dans le terminal intégré. |
| [#2](https://github.com/Rihen-Universe/NKCode-Beta/issues/2) | L'explorateur ne se rafraîchit pas après compilation | ✅ Re-scan automatique à la fin de chaque build/rebuild/clean/test. |
| [#4](https://github.com/Rihen-Universe/NKCode-Beta/issues/4) | Dédoublement à la création d'un nouveau fichier | ✅ Le champ de saisie était inséré deux fois à la racine du workspace. |
| [#5](https://github.com/Rihen-Universe/NKCode-Beta/issues/5) | `Ctrl+Shift+S` (Enregistrer sous) sans effet | ✅ Le raccourci était affiché dans le menu mais n'avait jamais été enregistré. |
| [#6](https://github.com/Rihen-Universe/NKCode-Beta/issues/6) | Modèle Ollama silencieux | ✅ Message d'erreur actionnable (vérifier `ollama serve`, `ollama pull llama3.2`) au lieu d'un « réseau/curl ? » générique. |
| [#1](https://github.com/Rihen-Universe/NKCode-Beta/issues/1) | Boutons du dialogue de suppression inertes à la souris | ✅ Corrigé après la bêta.1 (dialogue modal réécrit + routeur d'occlusion des surfaces flottantes). **À re-tester.** |

## Nouveautés

- **Vrai installeur Windows** — plus d'archive à extraire à la main : raccourcis
  Menu Démarrer/Bureau, désinstalleur, entrée « Programmes et fonctionnalités »,
  français et anglais. **Aucun droit administrateur nécessaire** (installation
  par utilisateur).
- **Mise à jour depuis l'IDE** — *Aide → Rechercher les mises à jour* indique
  s'il existe une version plus récente ; si vous acceptez, NKCode télécharge,
  se met à jour **sans réinstallation manuelle** et redémarre.
- **Diagnostic au démarrage** — le panneau *Sortie* indique désormais le dossier
  de l'exécutable et si le Jenga embarqué est actif (ou pourquoi il ne l'est
  pas). Merci de **coller ces deux lignes** dans vos futurs rapports de bugs :
  ça permet de diagnostiquer à distance.
- **`jenga` utilisable dans le terminal intégré** — un raccourci vers le Python
  embarqué est livré avec l'application : tapez `jenga info`, `jenga package`,
  `jenga gdb`… même sans Python installé.
- **Arguments d'exécution** — un champ *Arguments* dans la barre d'outils passe
  des paramètres à votre programme au lancement (équivalent de
  `jenga run --args`). Ils sont mémorisés par workspace.
- **Chat IA lisible** — le Markdown des réponses est mis en forme au lieu
  d'afficher sa syntaxe (`**gras**`, `### titres`, listes, `` `code` ``).
- **Exemples cliquables** depuis l'écran d'accueil : un clic sur un exemple
  demande où le cloner, puis l'ouvre.
- **Barre de menus complète** (11 menus) : chaque entrée est fonctionnelle ou
  visiblement grisée — aucun raccourci affiché qui ne fonctionne pas.
- **Panneaux latéraux plus étroits** : la largeur minimale de l'Explorateur et
  du panneau IA passe de 380 à 220 pixels.
- **Conversations IA conservées par workspace** (messages, brouillons, réglages
  et session de l'agent) — vous retrouvez vos échanges en réouvrant un projet.
- Corrections diverses du launcher : plus de doublons dans les projets récents,
  version de Jenga réellement détectée (au lieu d'un numéro figé), nombre de
  projets exact.

## Honnêteté

- **Windows x64 uniquement.** Linux et macOS demandent un équivalent du paquet
  Python embarqué : c'est prévu, ce n'est pas fait.
- **C'est une bêta** : il reste des bugs. Le multi-chat IA et une partie de la
  refonte de la gestion des clics sont encore en chantier.
- **NKCode et Jenga ne sont pas open source** (licence propriétaire) : seuls des
  binaires sont publiés ici.
- La mise à jour in-app ne pourra s'automatiser **qu'à partir de cette version**
  (la bêta.1 ne contenait pas d'installeur).

## Installation

1. Télécharger `NKCode-0.1.0-beta.2-win64-setup.exe`
2. L'exécuter (Windows peut afficher un avertissement : l'exécutable n'est pas
   encore signé — la signature de code est au programme)
3. Lancer NKCode depuis le Menu Démarrer

Rien d'autre à installer : Python et un compilateur Clang sont embarqués.

## Retours

Les [Issues](https://github.com/Rihen-Universe/NKCode-Beta/issues) sont
ouvertes et lues. Un « ça plante au démarrage » est déjà une contribution
utile — et si possible, joignez les deux lignes de diagnostic du panneau
*Sortie*.
