# Multi-chat IA — proposition de design (À VALIDER avec Rihen)

> **Statut : PROPOSITION, rien n'est implémenté.** Demande de Rihen (22 juil
> 2026) : « plusieurs chats qui se voient, chacun avec son éditeur de chat et
> son IA qui répond, sans forcément venir directement dans le panneau de chat ;
> ça va permettre d'interagir avec plusieurs agents qui travaillent sur le même
> workspace mais à des niveaux différents du projet. »
>
> Ce document existe pour qu'on tranche **avant** d'écrire du code. Chaque
> section ouverte se termine par une question marquée **❓**.

---

## 1. Ce qui existe déjà (point de départ réel)

`AiPanel` (`Shell/NkAiPanel.h`) est un panneau par **agent** (`mKind` : 0
Assistant général, 1 Claude Code, 2 Codex, 3 NkAI). Il contient déjà :

- `NkVector<ChatSession> mChats` — **plusieurs conversations coexistent déjà en
  mémoire**, mais une seule est visible (`mActiveChat`, sélection par onglets).
- Chaque `ChatSession` possède **son propre état complet** : brouillon de
  saisie, modèle, mode, portée, autorisation d'édition, température, max tokens,
  instructions système, effort, thinking, `claudeSessionId`, messages, scroll.
- `mBusyChat` — le chat **cible** d'une réponse en cours : on peut déjà changer
  de conversation pendant qu'une réponse arrive, elle ne se perd pas.
- Persistance par workspace (`<ws>/.nkcode/ai_chats_<kind>.cfg`), y compris le
  `claudeSessionId` → un chat restauré **reprend sa session CLI**.

**Conséquence importante :** le modèle de données est déjà multi-chat. Ce qui
manque est (a) l'**affichage simultané**, (b) l'exécution **réellement
parallèle**, (c) la notion de **portée par agent**.

Limite technique actuelle à connaître : le backend Claude Code utilise **un
seul** `NkPipeProc mClaudeProc` par panneau → **un tour à la fois par panneau**.
Le parallélisme réel exige un processus (ou une requête) **par chat actif**.

---

## 2. Décision structurante : où vivent les chats simultanés ?

### Option A — Un panneau, plusieurs chats empilés verticalement
Le panneau IA affiche N conversations les unes sous les autres, chacune avec son
fil + son champ de saisie, hauteurs redimensionnables.

- ✅ Aucun changement d'architecture de dock ; tout reste dans `AiPanel`.
- ✅ Vue « tour de contrôle » : on voit tout d'un coup d'œil.
- ❌ Étroit : le panneau IA est une sidebar (~220-400 px). Deux fils de
  discussion lisibles en simultané, pas cinq.

### Option B — Un panneau de dock PAR chat (façon onglets/splits de l'éditeur)
Chaque chat devient un panneau nommé (« IA — Kernel », « IA — NKCode »),
dockable/splittable n'importe où par l'utilisateur, comme les autres panneaux.

- ✅ Utilise le **dock existant** : l'utilisateur choisit lui-même la
  disposition (côte à côte, en bas, sur un second écran plus tard).
- ✅ Chaque chat peut être large → réellement lisible.
- ✅ Cohérent avec le reste de l'IDE (rien de spécial à apprendre).
- ❌ Demande une création **dynamique** de panneaux (aujourd'hui les panneaux
  sont enregistrés statiquement au démarrage dans `main.cpp`).
- ❌ La barre d'activité doit gérer une liste variable.

### Option C — Hybride : le panneau IA reste le « hub », les chats peuvent être *détachés*
Par défaut tout est dans le panneau IA (comme aujourd'hui). Un bouton
« détacher » sur un chat en fait un panneau de dock autonome.

- ✅ Zéro régression : qui ne veut qu'un chat ne voit aucun changement.
- ✅ Le multi-chat devient un choix de l'utilisateur, pas une imposition.
- ❌ Deux chemins de rendu à maintenir (dans le hub / détaché).

**Ma recommandation : Option C**, en implémentant d'abord la brique de
l'Option B (panneau dynamique par chat) et en gardant le hub actuel comme vue
par défaut. On livre par étapes sans jamais casser l'existant.

**❓ Question 1 — quelle option ?** (A, B, C, ou autre)

---

## 3. « Son IA qui répond sans forcément venir dans le panneau de chat »

C'est la phrase que je veux être sûr de bien comprendre. Trois lectures
possibles :

1. **Notification** — l'agent travaille en fond ; quand il a fini, une pastille
   ou une entrée dans un journal signale « chat 3 a répondu », sans voler le
   focus. Le texte reste dans son chat.
2. **Sortie ailleurs que dans le fil** — la réponse produit un **artefact** :
   fichiers modifiés (diff à valider), entrée dans le panneau Sortie, tâche
   cochée dans une liste. Le fil ne sert qu'à piloter.
3. **Agent autonome sans fil visible** — on lui confie une mission, il n'y a
   pas de conversation à lire, seulement un état (« en cours / terminé /
   bloqué ») et son résultat.

Les trois sont utiles et ne s'excluent pas ; l'ordre de livraison change tout.

**❓ Question 2 — laquelle en premier ?** (mon avis : 1 d'abord, car sans elle
le multi-chat est inutilisable — on ne peut pas surveiller N fils à la main ;
puis 2, qui est la vraie valeur « plusieurs agents qui travaillent ».)

---

## 4. « À des niveaux différents du projet » → portée par agent

C'est, à mon avis, **le cœur de l'idée** : plusieurs agents sur le même
workspace, mais chacun cantonné à une partie. Il existe déjà un réglage
`scope` par chat (fichier courant / sélection / workspace). Proposition :
l'étendre en **portée de dossier ou de projet**.

- `scope = PROJET` → l'agent ne voit/modifie que `Applications/NKCode/**`
- `scope = DOSSIER` → un chemin choisi dans l'explorateur (clic droit →
  « Ouvrir un chat IA sur ce dossier »)
- Un badge visible en tête du chat rappelle sa portée (impossible de se tromper
  d'agent).

Intérêt concret pour le développement de Nkentseu : un agent sur `Kernel/AI`,
un sur `Applications/NKCode`, un sur `Kernel/Runtime/NKMedia` — chacun avec son
contexte, sans se marcher dessus.

Côté Claude Code, la portée se traduit naturellement : répertoire de travail du
processus + instructions système. **Garde-fou nécessaire** : deux agents qui
éditent le même fichier. Proposition minimale : verrou informatif par fichier
(« NKCode-Kernel est en train de modifier ce fichier ») plutôt qu'un blocage
dur, et jamais deux `editAuth = Appliquer` sur des portées qui se recouvrent
sans avertissement explicite.

**❓ Question 3 — la portée par dossier/projet fait-elle partie de la V1, ou
c'est une V2 ?**

**❓ Question 4 — que fait-on si deux agents modifient le même fichier ?**
(avertir / interdire / laisser faire et compter sur git)

---

## 5. Coût et limites à accepter d'avance

- **Processus** : un `claude` par chat actif. 3 agents = 3 processus CLI + 3
  interpréteurs de contexte. À surveiller (RAM, quota de compte partagé — le
  popover « Compte et utilisation » agrège déjà tous les chats).
- **Quota** : les fenêtres de limite (`rate_limit_event`) sont **communes** au
  compte. Trois agents épuisent le quota trois fois plus vite ; il faudra
  l'afficher clairement, sinon l'utilisateur ne comprendra pas son blocage.
- **Place à l'écran** : au-delà de 2-3 chats visibles, ça devient illisible sur
  un écran de portable. La vue « hub » (liste + statut) reste indispensable.

**❓ Question 5 — combien d'agents simultanés visons-nous réellement ?** (2-3
confortables, ou 5+ avec une vue compacte de type tableau de bord ?)

---

## 6. Découpage proposé (si l'Option C est retenue)

| Étape | Contenu | Risque |
|---|---|---|
| 1 | Notification de fin par chat (pastille + statut dans le hub), sans changer la disposition | faible |
| 2 | Exécution réellement parallèle : un processus backend **par chat** au lieu d'un par panneau | moyen (cycle de vie, `Shutdown`) |
| 3 | Détacher un chat en panneau de dock (panneaux dynamiques) | moyen (enregistrement dynamique, layout) |
| 4 | Portée par dossier/projet + badge + verrou informatif | moyen |
| 5 | Vue tableau de bord (N agents, statut, dernière action) | faible |

Chaque étape est livrable et testable seule. L'étape 1 apporte déjà un vrai
confort même sans multi-chat visible.

**❓ Question 6 — on part sur ce découpage, ou tu veux voir le visuel
(détachement/dashboard) plus tôt pour juger de l'ergonomie ?**

---

## Résumé des questions ouvertes

1. Option A, B ou C pour l'affichage simultané ?
2. « Répondre sans venir dans le panneau » = notification, artefact, ou agent autonome — dans quel ordre ?
3. Portée par dossier/projet en V1 ou V2 ?
4. Conflit de deux agents sur un même fichier : avertir, interdire, ou laisser git arbitrer ?
5. Combien d'agents simultanés visés ?
6. Découpage en 5 étapes validé, ou visuel d'abord ?
