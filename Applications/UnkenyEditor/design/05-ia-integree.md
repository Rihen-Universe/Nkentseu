# UnkenyEditor — l'IA intégrée

### Document 05 — R18 : un modèle branché comprend Unkeny dès sa première connexion, et agit sur tout

> Rihen, 30/09/2026 : *« une partie IA, pour qu'un modèle branché comprenne dès la
> première connexion comment marche Unkeny et puisse interagir avec : aider à créer,
> concevoir, discuter, écrire le GDD… bref la totale. »* — 07h45 : *« tout doit être
> ouvert. »*
>
> Rihen, 01/10/2026 : *« Ne pas oublier le panneau IA dans Unkeny, et trouver comment
> brancher des modèles comme Ollama, etc. ; sur mon autre PC je vais installer Qwen
> juste pour les tests en local. »* — puis : *« il doit aussi être rétractable, ou se
> poser comme onglet à côté de Détails et Monde. »*

Le seul garde-fou accepté comme « ne fermant rien » : **toute action de l'IA passe par
l'historique** (Ctrl+Z la défait) et **toute action irréversible demande confirmation**.

---

## 1. Où cliquer

| geste | où |
|---|---|
| ouvrir l'IA | **Fenêtre > IA (assistant)**, ou **Ctrl+I** : l'onglet **IA** du groupe **Détails \| Monde** (à droite) passe au premier plan |
| la détacher en panneau à part | bouton **Détacher** de la barre de l'onglet IA, ou **Fenêtre > IA : détacher en panneau à part** |
| la replier | le **chevron ›** de l'en-tête du panneau à part : il devient une **bande** de 28 px (« I A », un point quand un tour est en vol) ; un clic sur la bande le déplie |
| la remettre en onglet | bouton **Rattacher** du panneau à part |
| la fermer (panneau à part) | la **×** de son en-tête ; Ctrl+I la rouvre |
| choisir le fournisseur et le modèle | la **pastille du modèle** en bas du panneau (`qwen2.5:7b-instruct`) ; la liste vient du serveur |
| régler les fournisseurs | **Fenêtre > Réglages de l'IA : fournisseurs de modèles…**, le bouton **Réglages** du panneau, ou « / » > **Réglages des fournisseurs** |
| voir ce que l'IA voit | « / » > **Ce que l'IA voit** : le message système engendré, maintenant |
| tester | « / » > **Tester la connexion**, ou le bouton du même nom dans les réglages |
| confirmer / refuser | les boutons **Confirmer** / **Refuser** sous le bloc de l'outil qui attend |
| défaire | **Ctrl+Z** (clavier de l'éditeur), ou **Annuler cette action** sous le dernier effet |

**La disposition choisie est retenue** (onglet ou à part, repliée, ouverte, largeur) dans
`%APPDATA%\Nkentseu\IA\unkeny_panneau.txt`. Pour une capture : `--ia`, `--ia=onglet`,
`--ia=panneau`, `--ia=replie`, `--ia-reglages`.

---

## 2. Les fournisseurs — UNE seule couche, partagée

Trois applications parlaient déjà à des modèles, chacune à sa façon (NKCode par `curl`
avec la clé **en ligne de commande**, NKCraft par NKConverse mais « l'onglet Ollama
n'est pas câblé », NKUIDesign sans outils). Une quatrième écriture pour Unkeny aurait
été la divergence de trop : **tout ce qui parle à un fournisseur vit dans
`Kernel/System/NKConverse`**, et Unkeny, NKCraft, NKCode peuvent s'en servir.

| fichier | ce qu'il fait |
|---|---|
| `NkConverseFournisseurs.h` | les **genres** (Ollama, compatible OpenAI, API Anthropic, CLI Claude, processus local), les **réglages**, les **clés**, la **liste des modèles**, **Tester la connexion**, la **conversation en flux avec outils** |
| `NkConverseTransport.h` | une requête HTTP lue **en flux** ; https sans TLS compilé → `curl` |
| `NkConverseChatFlux.h` | la conversation et la sonde (lister, tester) sur un **fil de travail** : la fenêtre ne gèle pas |
| `NkConverseJson.h` | un arbre JSON (lire, parcourir, réécrire, fusionner) |
| `NkConverseFauxServeur.h` | un **faux serveur local** qui parle Ollama, OpenAI et Anthropic : les bancs |
| `NKNetwork/HTTP/NkHTTPClient.h` | `NkHTTPRequest::surCorps` : le corps lu **pendant** qu'il arrive (chunked retiré, délai d'inactivité) |
| `NKEditorKit/NkAiReglagesVue.h` | la fenêtre des réglages (partagée ; elle n'appelle aucun service) |

| genre | adresse par défaut | clé | liste des modèles | outils natifs | flux |
|---|---|---|---|---|---|
| **Ollama** | `http://127.0.0.1:11434` | aucune | `/api/tags` + capacité « tools » par `/api/show` | si le modèle les annonce | `/api/chat`, NDJSON |
| **compatible OpenAI** (LM Studio, llama.cpp server, vLLM, service en ligne) | `http://127.0.0.1:1234/v1` | facultative (`Bearer`) | `/v1/models` | oui | `/v1/chat/completions`, SSE |
| **Claude (API Anthropic)** | `https://api.anthropic.com` | **obligatoire** (`x-api-key`) | `/v1/models` | oui | `/v1/messages`, SSE |
| **Claude (CLI Claude Code)** | — | aucune (le CLI est connecté) | l'aide du CLI | non → format texte | réponse entière |
| **Modèle local (processus GGUF)** | le chemin de `NKDesignLLM.exe` | aucune | le modèle saisi (`.gguf` ou nom Ollama) | non → format texte | réponse entière |

**Les réglages** (adresse, modèle, température, contexte, réponse max, mode des outils)
sont écrits dans `%APPDATA%\Nkentseu\IA\fournisseurs.txt` — **hors de tout dépôt**,
partagés par toutes les applications. **Aucune clé n'y entre.**

**Les clés**, dans l'ordre où elles sont cherchées :
1. la variable propre au fournisseur `NK_IA_CLE_<ID>` (ex. `NK_IA_CLE_CLAUDE_API`) ;
2. les variables d'usage : `ANTHROPIC_API_KEY` (ou `NKCODE_ANTHROPIC_KEY`), `OPENAI_API_KEY` ;
3. le fichier `%APPDATA%\Nkentseu\IA\cles\<id>.cle`, écrit par le champ **Clé** des réglages.

La clé n'est **jamais affichée** (des points pendant la frappe, `sk-a…9f2c` ensuite),
**jamais écrite dans un projet ni dans un journal**, **jamais passée en ligne de
commande** : sans TLS compilé (le défaut du dépôt, `NK_ENABLE_TLS` est resté optionnel
depuis le conflit avec la lecture des PDF), l'https passe par `curl.exe` de Windows avec
les en-têtes dans un **fichier temporaire** (`-H @fichier`), effacé dès que curl a fini.

**Tester la connexion** ne génère rien et ne facture rien : la liste des modèles suffit à
prouver l'adresse, la clé et le modèle. Le verdict met **le geste qui répare en tête** :

| message | cause | geste |
|---|---|---|
| « Lancez Ollama (« ollama serve », ou l'application) ou corrigez l'adresse : personne ne répond à … » | serveur absent, mauvaise IP, pare-feu | lancer Ollama ; vérifier l'IP ; ouvrir le port 11434 (§ 3.1) |
| « Installez le modèle : « ollama pull qwen2.5:7b » — le serveur répond, mais il ne l'a pas (installés : …) » | modèle absent | `ollama pull …` sur le PC du serveur, puis **Actualiser** |
| « Saisissez la clé dans Réglages (ou la variable ANTHROPIC_API_KEY) » | clé absente | saisir la clé, **Enregistrer** |
| « Vérifiez la clé dans Réglages : le service la refuse (401 : …) » | clé refusée | corriger la clé |
| « Construisez avec NK_ENABLE_TLS=1, ou installez curl » | https sans TLS ni curl | rare sous Windows 10/11 (curl y est livré) |
| « Réessayez : pas de réponse dans le délai. C'est NOTRE plafond d'attente… » | premier chargement d'un gros modèle | réessayer (le modèle est alors en mémoire) |
| « Connecté : 2 modèle(s) disponible(s), « qwen2.5:7b » prêt — rien ne quitte ce PC. » | tout va bien | — (« l'invite QUITTE ce PC » pour un autre PC ou un service en ligne : la pastille de lieu passe à **distant**) |

---

## 3. Brancher Qwen par Ollama sur l'autre PC — pas à pas

> Rien n'a été installé sur ce PC pour écrire ce document (consigne) : les preuves passent
> par le faux serveur (§ 8). Les commandes ci-dessous sont à taper par Rihen.

### 3.1 Sur l'autre PC (celui qui fait tourner le modèle)

1. **Installer Ollama** : <https://ollama.com/download> (Windows, macOS ou Linux).
2. **Tirer Qwen** (un modèle qui sait les **outils** : c'est ce qui permet d'agir) :
   ```
   ollama pull qwen2.5:7b
   ```
   ≈ 4,7 Go, tient sur une carte de 8 Go. Variantes : `qwen2.5:14b` (≈ 9 Go, meilleur
   raisonnement), `qwen2.5-coder:7b` (scripts C++), `qwen3:8b` si la version d'Ollama le
   propose. Essai local : `ollama run qwen2.5:7b "Bonjour"`.
3. **Écouter sur le réseau** (par défaut Ollama n'écoute que `127.0.0.1`) :
   - **Windows** : `setx OLLAMA_HOST 0.0.0.0:11434`, puis **quitter** Ollama (icône de la
     zone de notification > Quit) et le relancer ;
   - **Linux** (service) : `sudo systemctl edit ollama`, ajouter
     ```
     [Service]
     Environment="OLLAMA_HOST=0.0.0.0:11434"
     ```
     puis `sudo systemctl daemon-reload && sudo systemctl restart ollama` ;
   - **macOS** : `launchctl setenv OLLAMA_HOST "0.0.0.0:11434"`, puis relancer l'application.
4. **Ouvrir le port** (Windows, PowerShell **administrateur**), sur le réseau **privé**
   seulement — Ollama n'a **aucune authentification** :
   ```
   New-NetFirewallRule -DisplayName "Ollama 11434" -Direction Inbound -Protocol TCP -LocalPort 11434 -Action Allow -Profile Private
   ```
   (plus strict : ajouter `-RemoteAddress <IP du PC d'Unkeny>`).
5. **Trouver son adresse** : `ipconfig` → « Adresse IPv4 », par exemple `192.168.1.42`.
6. **Vérifier depuis le PC d'Unkeny** :
   ```
   curl http://192.168.1.42:11434/api/tags
   ```
   doit lister `qwen2.5:7b`.

### 3.2 Dans UnkenyEditor (le PC de travail)

1. **Fenêtre > Réglages de l'IA**.
2. **+ Ajouter** : un fournisseur « Ollama (autre PC du réseau) » apparaît, adresse
   `http://192.168.1.20:11434` — **corriger l'IP** (`http://192.168.1.42:11434`).
3. **Actualiser** : la liste ▾ du champ **Modèle** se remplit depuis le serveur
   (« 7.6B Q4_K_M · 4.7 Go · … · outils »). Choisir `qwen2.5:7b`.
4. **Tester la connexion** : « Connecté : … « qwen2.5:7b » prêt — l'invite QUITTE ce PC. »
5. **Utiliser ce fournisseur** (il devient celui du panneau ; la pastille de lieu dit
   **distant**, en orange : l'invite part vers l'autre PC), puis **Enregistrer**.
6. Dans l'onglet **IA** : « ajoute une caisse au centre ». Le premier tour charge le modèle
   depuis le disque (des dizaines de secondes) ; les suivants sont rapides.

Qwen **sur le même PC** : le fournisseur par défaut « Ollama (ce PC) »
(`http://127.0.0.1:11434`), sans rien d'autre que les étapes 1-2.

### 3.3 Les autres serveurs

| serveur | lancer | adresse dans Unkeny |
|---|---|---|
| **LM Studio** | onglet *Developer* > *Start Server* (et *Serve on Local Network* pour un autre PC) | `http://127.0.0.1:1234/v1` |
| **llama.cpp** | `llama-server -m qwen2.5-7b-instruct-q4_k_m.gguf --port 8080 --jinja` (`--jinja` : les outils) | `http://127.0.0.1:8080/v1` |
| **vLLM** | `vllm serve Qwen/Qwen2.5-7B-Instruct --enable-auto-tool-choice --tool-call-parser hermes` | `http://IP:8000/v1` |
| **Claude, API** | une clé de la console Anthropic, dans **Réglages > Clé** | `https://api.anthropic.com` |
| **Claude, CLI** | Claude Code installé et connecté (`claude auth login`, ou NKCode > Comptes) | — |
| **NKDesignLLM** (GGUF du dépôt) | — | le chemin de `NKDesignLLM.exe` ; modèle : un `.gguf` ou un nom Ollama |

---

## 4. Comment l'IA « comprend Unkeny » dès la connexion

Le message système n'est **pas un texte figé** : `NkEditeurIADescription`
(`Ia/NkEditeurIAOutils.cpp`) l'**engendre à chaque tour** depuis l'éditeur :

- le rôle, les **garde-fous** (Ctrl+Z, confirmations, chemins relatifs, rien en jeu) et
  les **repères** (mètres, y vers le haut, « au centre » = le centre de la vue) ;
- **l'éditeur maintenant** : le projet, la scène, l'état (édition / jeu), la vue (centre,
  zoom), la sélection, **chaque entité** (uid, nom, type, position, composants) ;
- **le catalogue** : les acteurs (`NkActeurSimInfo`), les composants
  (`NkComposantEditeurNom`), les commandes, les types d'assets (`NkEditeurNatureFichier`,
  `NkEditeurFiltreImport`), les scripts C++ et Blueprint du projet, **les nœuds
  Blueprint** (`NkBpProtos` : type, libellé, prises), le GDD ;
- pour un modèle sans outils natifs : la consigne et la liste des outils (§ 6).

Un acteur ou un nœud ajouté au moteur apparaît à l'IA **sans une ligne ici**. « / » >
**Ce que l'IA voit** montre ce message tel qu'il part.

---

## 5. Agir sur tout : les outils

Chaque outil **est une commande de l'éditeur** (`Ia/NkEditeurIAOutils.cpp`, table `Table()`).

| outil | ce qu'il fait | défaire / confirmer |
|---|---|---|
| `lire_scene` | entités (uid, nom, type, position, composants), sélection, vue | lecture |
| `lire_entite` | **toutes** les valeurs d'une entité, dans la forme de sauvegarde de la scène | lecture |
| `creer_acteur` | un acteur du catalogue en (x, y) — sans x/y : au **centre de la vue** | Ctrl+Z |
| `creer_entite` | une entité vide, et ses composants | Ctrl+Z |
| `modifier_entite` | nom, position, rotation, échelle, activité, parent | Ctrl+Z |
| `modifier_valeurs` | **n'importe quelle valeur** (sprite, collisionneur, corps, lumière, émetteur, composants du jeu…) : un objet JSON fusionné dans la forme de sauvegarde ; couleur en `"#RRGGBB"` | Ctrl+Z |
| `ajouter_composant` / `retirer_composant` | un composant (matière pour un corps mou) | Ctrl+Z |
| `supprimer_entite`, `dupliquer_entite`, `selectionner` | — | Ctrl+Z |
| `poser_script` | un Blueprint du projet ou une classe C++ (`cpp:X`) sur une entité | Ctrl+Z |
| `commande` | jouer, pause, arrêter, pas, **enregistrer**, annuler, refaire, cadrer, grille, collisionneurs, créer un prefab, **scène neuve** | enregistrer par-dessus et scène neuve sur modifications : **confirmation** |
| `lister_contenu`, `lire_fichier` | le projet, ses fichiers texte | lecture |
| `ecrire_fichier` | un fichier texte du projet | Ctrl+Z ; **écraser : confirmation** |
| `supprimer_fichier` | un fichier du projet | **confirmation** ; Ctrl+Z le rend pendant la session |
| `creer_dossier` | un dossier du projet | — |
| `ecrire_script_cpp` | `Contenu/Scripts/<nom>.cpp` (sans code : le modèle commenté qui liste toute l'API) ; recompilé et rechargé à chaud | Ctrl+Z ; écraser : confirmation |
| `ecrire_blueprint` | `Contenu/Scripts/<nom>.nkbp` : nœuds (type du catalogue, x, y, valeurs) et liens ; **compilé**, une erreur désigne son nœud | Ctrl+Z ; écraser : confirmation |
| `ecrire_gdd` | `Documents/GDD.md` : ajouter une section, ou tout remplacer | Ctrl+Z ; remplacer : confirmation |

---

## 6. Les garde-fous (dans l'éditeur, pas dans la consigne)

Dire au modèle « ne supprime rien sans demander » ne protège rien : un 7 milliards de
paramètres l'oubliera. Ce qui protège :

1. **Une photo avant chaque outil qui modifie** (`NkEditeurRetenir`) ; si l'outil n'a rien
   changé, la photo est retirée (un Ctrl+Z qui ne défait rien serait un mensonge).
2. **Le journal des fichiers** (nouveau, dans `NkHistoriqueEditeur`) : un fichier écrit ou
   supprimé est retenu **avec** la photo (`NkEditeurRetenirFichier`) ; Ctrl+Z rend son
   contenu octet pour octet, ou l'efface s'il n'existait pas ; Ctrl+Y refait. Il sert à
   l'IA aujourd'hui, aux greffons demain (document 04).
3. **La confirmation est décidée par l'éditeur** (`NkEditeurIAIrreversible`), avant
   l'exécution : supprimer un fichier, en écraser un, remplacer tout le GDD, enregistrer
   par-dessus la scène, une scène neuve sur des modifications. La boucle **s'arrête** ; la
   question entière s'affiche ; **Refuser** renvoie au modèle « l'utilisateur a refusé ».
4. **Les chemins** : relatifs au projet, sans `..`, jamais absolus.
5. **En jeu, rien ne se modifie** (l'historique est celui de l'édition).
6. **Huit tours d'outils au plus** par demande.
7. **Les outils s'exécutent sur le fil de l'interface**, jamais sur celui du modèle.

---

## 7. Les modèles sans appel d'outils natif

Le modèle écrit des blocs que l'éditeur analyse :
```
<outil nom="creer_acteur">{"acteur": "caisse", "x": 0, "y": 0}</outil>
```
Le format Hermes `<tool_call>{"name": …, "arguments": {…}}</tool_call>` (ce qu'écrit Qwen
quand son gabarit d'outils n'est pas branché) est lu aussi. Le texte montré à l'écran
ne contient pas ces blocs. **Auto** (le défaut) choisit : outils natifs si le serveur
les annonce (Ollama : capacité « tools » de `/api/show`), sinon texte ; un serveur qui
refuse les outils (400 « does not support tools ») fait **repasser en texte tout seul**.

---

## 8. Preuves

- **Banc** : `UnkenyEditor.exe --banc-ia` (et `--selftest`, à la fin) : **41 témoins
  verts**, sans Ollama, sans modèle, sans réseau — un faux serveur local sur de vrais
  sockets. (ia1-3) liste, test (ok / modèle absent / serveur absent), clés
  (Bearer, 401, clé absente sans rien envoyer), Anthropic en flux ; (ia4) conversation
  **en flux** (le texte grandit en plusieurs images) avec le système engendré ; (ia5)
  « ajoute une caisse au centre » ; (ia6) **Ctrl+Z la retire, et seulement elle** ; (ia7)
  une suppression **attend** la confirmation, Refuser / Confirmer, Ctrl+Z rend le
  fichier ; (ia8-9) format texte et repli automatique ; (ia10) OpenAI en flux, le GDD ;
  (ia11) hors du projet refusé ; (ia12) description engendrée ; (ia13) onglet / détaché /
  replié / retenu / rattaché ; (ia14) un Blueprint écrit, relié et **compilé** (un type
  inconnu : refus nommé), un script C++ au modèle de l'éditeur, une valeur changée par
  `modifier_valeurs` (« #FF0000 ») et rendue par Ctrl+Z. **Contre-épreuves** (une mutation, le témoin passe au
  rouge) : (ce1) nom d'outil faux, (ce2) sans photo avant l'outil, (ce3) sans
  confirmation, (ce4) réponse en un seul morceau.
- **Captures** hors écran : `UnkenyEditor.exe --captures-ia=DOSSIER` →
  `References\Captures\ia-unkeny\` : 01 l'onglet IA à côté de Détails et Monde, 02 le
  panneau détaché, 03 replié, 04 la caisse posée, 05 la même annulée, 06 la confirmation,
  07 la liste des modèles du serveur, 08 « Tester la connexion » serveur absent, 09 la clé
  masquée.

---

## 9. Ce qui reste

- **Essayer avec le vrai Qwen** sur l'autre PC (§ 3) : aucun vrai modèle n'a tourné ici.
- **NKCode** : son panneau garde son `curl` avec la clé **en ligne de commande**
  (`NKCode/Shell/NkAiPanel.h`, l. ~6217) et Ollama figé sur `llama3.2` ; à porter sur
  `NkConverseFournisseurs.h` (non touché ici : la branche `nkcode/apparences` y travaille).
- **NKCraft** : « l'onglet Ollama n'est pas câblé au modeleur » — `NkIaDiscuter` le
  permet désormais (même remarque : la branche de renommage y travaille).
- Les **chats** du panneau ne survivent pas à la fermeture de l'éditeur.
- La clé est rangée **en clair** dans le profil de l'utilisateur ; prochaine étape :
  DPAPI (Windows) / Trousseau (macOS).
- `modifier_valeurs` relit toute la scène depuis sa forme JSON : coûteux sur une grande
  scène ; à remplacer par l'écriture d'un composant par ses champs décrits.
- Les commandes des **greffons** (document 04, « ouvert à l'IA ») : la table des outils
  est le point d'extension.
- Le CLI Claude et le processus local rendent la réponse **d'un bloc** (pas de flux).
