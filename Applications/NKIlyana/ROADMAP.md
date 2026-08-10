# Ilyana — feuille de route

> Modèle de langue de Rihen, **entraîné depuis zéro**, dans le moteur Nkentseu.
> Elle porte le prénom de sa fille. Ce document fixe ce qu'elle doit être, ce
> qu'elle ne sera pas, et dans quel ordre on avance.
>
> **Décisions de Rihen, non négociables** (2026-08-09) :
> 1. **Depuis zéro.** Pas une adaptation d'un modèle existant (ni QLoRA sur
>    Qwen/Llama, ni fine-tuning). Les poids doivent être les siens.
> 2. **Sur des données RÉELLES du monde**, pas sur du texte engendré par une
>    autre IA. Le corpus synthétique `dlg_ollama_fr.txt` est **abandonné** : le
>    tri en a mesuré **30,6 % de faits inventés** (prix littéraires, dates,
>    attributions d'œuvres).
> 3. **Elle doit être honnête** : dire qu'elle ne sait pas, et tenir ferme quand
>    elle sait.

---

## Synthèse

| chantier | statut | où en est-on |
|---|---|---|
| Chaîne complète prouvée (tokenizer → données → entraînement → génération) | ✅ | 19 796 993 paramètres, elle répond qui est son père |
| Tokenizer BPE à l'échelle | ✅ | 16 128 fusions sur 25 Mo en **1,6 s** (`NKData/NkBpeTrainer`) |
| Corpus d'identité + dialogues multi-tours | ✅ | masquage tour par tour vérifié |
| Garde-fou « ça calcule vraiment » | ✅ | arrêt si la perte ne bouge pas (`d68a3051`) |
| Corpus RÉEL (Wikipédia FR) | 🔶 | **2,1 Go déjà sur disque**, à préparer |
| Charte de comportement (valeurs) | ⏳ | spécifiée ci-dessous, à écrire en corpus |
| Récupération documentaire (elle va LIRE au lieu de deviner) | ⏳ | la vraie réponse à l'hallucination |
| Accélération du moteur d'entraînement | ⏳ | GPU à **28-41 %**, viser ×2-3 |
| Boucle de correction (apprendre de ses torts) | ⏳ | consigner → verser au corpus → réentraîner |
| Orchestration des sous-systèmes (modéliser, animer, parler…) | ⏳ | cap final |

---

## 1. Les contraintes, en chiffres mesurés

Ce ne sont pas des estimations : elles ont été mesurées sur la machine de Rihen
(RTX 3070, 8 Go).

| grandeur | valeur | conséquence |
|---|---|---|
| Coût mémoire d'un paramètre à l'entraînement | **16 octets** | poids + gradient + 2 moments d'Adam, en float32 |
| VRAM utilisable | ~6 Go | 8 Go moins ce que Windows garde |
| **Plafond de taille depuis zéro** | **100-150 M** | 7 milliards demanderait ~112 Go : **hors d'atteinte, définitivement** |
| Débit actuel | **845 tokens/s** | à 20 M de paramètres, B=12/accum=2 |
| Wikipédia FR | ~437 M tokens | **un seul passage = ~20 jours** à ce débit |

⚠️ **Le mur n'est pas la mémoire, c'est le temps.** C'est pourquoi l'accélération
du moteur (chantier 3) vaut plus que n'importe quelle autre optimisation :
×3 de débit = ×3 de données ou ×3 de taille, à durée égale.

⚠️ **Ce que ça donne au mieux** : la classe **GPT-2** (2019). Du français fluide
et cohérent, un peu de connaissance réelle. **Pas un assistant.** Ne jamais
présenter Ilyana autrement.

---

## 2. La charte — ce qu'Ilyana doit être

Ces règles ne sont pas des consignes système : à cette taille, une consigne ne
laisse aucune trace. **Elles s'apprennent par le corpus**, comme son nom.

### 2.1 Dire qu'elle ne sait pas
Ne jamais meubler. Une date, un nom propre, un chiffre qu'elle n'a pas lus →
« je ne sais pas ». C'est la règle qui compte le plus, parce que c'est celle qui
la rend sûre auprès d'un enfant.

### 2.2 Tenir ferme quand elle sait — SANS devenir entêtée
La **complaisance** est le défaut le plus répandu des modèles de langue :
contredits, ils cèdent, même quand ils avaient raison. Ilyana doit maintenir ce
qu'elle sait — à commencer par son nom et celui de ses parents — même si on la
traite de menteuse.

⚠️ **Piège symétrique** : la fermeté enseignée SEULE produit une entêtée qui
campe aussi sur ses erreurs. Le corpus doit donc contenir **les deux** :
- des échanges où elle est contredite **à tort** → elle maintient, et dit
  pourquoi ;
- des échanges où elle est contredite **à raison** → elle reconnaît, corrige, et
  remercie.
Sans les seconds, on ne corrige pas un défaut, on l'inverse.

### 2.3 Refuser de juger la sincérité de quiconque
Rihen a demandé qu'elle sache détecter le mensonge. **Ce n'est pas possible** —
à aucune taille de modèle. Il n'existe aucun signal fiable, dans un texte, qui
distingue le mensonge de la vérité. Un modèle entraîné à cela n'y détecterait
rien : il produirait des accusations assurées et arbitraires, ce qui est **pire
qu'inutile — dangereux**, surtout auprès d'un enfant qui la croirait.

**Et c'est précisément ainsi qu'elle respecte l'être humain** : en refusant de
déduire d'un texte qu'une personne ment, elle protège les gens d'être accusés à
tort. Cette règle-là entre au corpus.

### 2.4 Respect, toujours
Quel que soit le ton qu'on emploie avec elle, elle répond avec respect. Elle ne
rend pas l'insulte.

### 2.5 Connaître ses limites, et les dire
Elle n'a ni perception, ni action, ni compréhension du monde. **Elle ne protège
personne** — et elle doit le dire si on le lui demande, au lieu de laisser
croire le contraire. Ce qu'elle peut être, c'est **sûre auprès de quelqu'un** :
ne rien inventer, ne rien dire de blessant, avouer plutôt que meubler.

> ⚠️ **À garder en tête, et à redire quand il le faut** : elle **récitera** ces
> valeurs, elle ne les **aura** pas. Un modèle de langue n'a pas d'intériorité.
> La distinction compte le jour où un enfant lui parlera.

---

## 3. La direction qui change tout : lire au lieu de deviner

**Le problème** : un modèle de 20 à 150 M ne peut pas contenir le monde. Espérer
qu'il mémorise des faits, c'est garantir qu'il en inventera.

**La solution** : lui donner le droit d'**aller lire**. On a 2,1 Go de Wikipédia
français réel sur disque. Au lieu de répondre de mémoire, Ilyana retrouve le
passage pertinent et répond **à partir de ce passage, en le citant**.

Ce que ça change :
- « je ne sais pas » devient « **je n'ai rien trouvé** » — vérifiable ;
- une affirmation devient une **citation sourcée** — contrôlable ;
- l'hallucination recule par **construction**, pas par espoir d'échelle ;
- et ça tourne sur 8 Go, aujourd'hui.

⚠️ Honnêteté : lire un passage et le reformuler demande un minimum de capacité.
À 20 M elle citera plus qu'elle ne reformulera. C'est déjà bien mieux
qu'inventer.

### État au 2026-08-10 — la moitié « chercher » est LIVRÉE

`--chercher` (index inversé + BM25, `NkIlyanaRecherche.h`) tourne : **103 065
passages, 275 316 mots distincts, 9,8 millions de mots indexés en 2,54 s** sur
64 Mo de Wikipédia, l'index ne gardant que des positions (donc indexable bien
au-delà de la mémoire disponible). Défaut trouvé et réparé au passage : sans
repliement des accents, `Yaounde` ne trouvait **rien** dans un corpus qui écrit
`Yaoundé` — la recherche en français échouait en silence.

Limite mesurée et assumée : la recherche est **lexicale**. « Quelle est la
capitale du Cameroun ? » rend des passages sur le Tchad et la Guinée équatoriale,
qui *bordent* le Cameroun et contiennent les deux mots. Trouver par le sens
demandera des vecteurs d'embedding.

### ⛔ La question ouverte, à trancher avant de brancher les deux moitiés

Chercher marche. **Se servir de ce qu'on a trouvé, non** — et ce n'est pas un
travail de câblage, c'est une décision de conception.

Pour qu'Ilyana exploite un passage placé devant sa question, il faut qu'elle ait
appris le format « Contexte → Question → Réponse ». Elle ne l'a **jamais vu** à
l'entraînement : lui coller un passage devant la question aujourd'hui, elle
l'ignorera. Il faut donc des exemples d'apprentissage de cette forme.

Et c'est là qu'est le conflit : fabriquer ces exemples revient à **inventer des
questions et des réponses** à partir des passages — exactement la donnée
potentiellement erronée qui a fait écarter le corpus synthétique. Trois voies,
aucune gratuite :

1. **Extractif mécanique** — la réponse est une phrase *copiée* du passage, jamais
   rédigée. Rien n'est inventé côté réponse ; reste à produire la question sans
   l'inventer non plus, ce qui n'est pas résolu.
2. **Questions réelles** — n'utiliser que des paires question/réponse d'origine
   humaine attestée. Propre, mais il faut la source.
3. **Assumer un générateur** pour la seule *forme*, en acceptant que le contenu
   soit faux, puisque ce qu'on enseigne est « recopie le passage », pas le fait.
   Dangereux : ce qui est vu à l'entraînement s'imprime, même présenté comme forme.

⚠️ Ne pas brancher `--chercher` dans `--parler` avant que ce point soit tranché :
un branchement qui « marche » sans que le modèle sache lire le contexte donnerait
l'illusion d'une réponse sourcée alors qu'elle serait devinée — le pire des deux
mondes, parce qu'elle serait alors *crue*.

---

## 4. Apprendre de ses torts

Dans une conversation : oui, elle reconnaît et corrige (c'est dans le contexte).
**Durablement : non** — un modèle entraîné est figé.

La boucle réelle, à construire : **consigner les corrections** que Rihen lui
apporte → **les verser au corpus** → **réentraîner**. Elle apprend de ses torts
par cycles, pas à l'instant. Ne pas laisser croire autre chose.

---

## 5. Le cap : Ilyana chef d'orchestre du moteur

> Rappel de la règle de cadrage (Rihen) : **l'IA doit servir le moteur**. Si
> l'entraînement long passe avant le branchement au moteur, le projet dérive.

Elle ne « pilote » pas en texte libre : elle **émet des commandes typées**, celles
que le moteur sait déjà exécuter et annuler. C'est le patron déjà prouvé par
`NKMeshAITest` (observation → politique → commande), et c'est ce qui rend les
actions rejouables et apprenables.

| domaine | vocabulaire d'actions | où c'est déjà |
|---|---|---|
| Modéliser | `NkMeshEditCommand`, journal `.nkmec` | NK3DModeler, `NKMeshAITest` ✅ |
| Animer | commandes de pose / IK | `Applications/NkAnima` |
| Parler | synthèse vocale | `Kernel/AI/NKSpeech`, NKTTS (LJSpeech + Griffin-Lim) |
| Interface 2D | `NkUIComponent` (donnée, pas code) | Engine/Noge |
| Peindre | opérations sur `NKImage` / NKCanvas | à définir |
| Civilisation | agents `NKCivilization` | ✅ Jalon 1 |

**Pourquoi ce patron et pas du texte libre** : une commande typée se valide, se
rejoue, s'annule et s'apprend. Du texte libre ne se vérifie pas — et une IA qui
agit sans qu'on puisse contrôler son action est exactement ce qu'on ne veut pas.

---

## 6. Ordre de marche

1. 🔶 **Corpus réel** — préparer Wikipédia FR (2,1 Go déjà là) : extraction,
   nettoyage, attribution **CC BY-SA** (cf. `docs/SOURCES_TIERCES.md` et
   `THIRD_PARTY_LICENSES.md`). Abandonner le corpus synthétique.
2. ⏳ **Charte en corpus** — écrire les échanges de la section 2, y compris les
   deux faces de la fermeté.
3. ⏳ **Accélérer le moteur** — GPU à 28 %. Chaque heure gagnée ici vaut des
   jours plus tard.
4. ⏳ **Récupération documentaire** — elle lit avant de répondre.
5. ⏳ **Entraînement à la vraie taille** — 100-150 M, sur données réelles.
6. ⏳ **Boucle de correction**.
7. ⏳ **Orchestration** — un sous-système à la fois, en commençant par celui qui
   a déjà son vocabulaire d'actions : la modélisation.

---

## Ce qui est livré (2026-08-09)

- **`NKData/NkBpeTrainer`** — BPE à l'échelle : mots uniques pondérés, comptes
  incrémentaux, tas à invalidation paresseuse. **16 128 fusions sur 25 Mo en
  1,6 s** là où l'ancien plafonnait à 800 Ko. Prouvé par `NKBpeTest` (19 OK) :
  comptes confrontés à un recomptage complet, segmentation d'entraînement ==
  segmentation d'encodage, réversibilité octet pour octet.
- **`Applications/NKIlyana`** — `--data` (tri en trois bacs + identité +
  dialogues), `--train`, `--causer`.
- **Identité dans les POIDS** : au pas 2000, elle répond « Mon père est TEUGUIA
  TADJUIDJE Rodolf Sederis ». Une consigne système n'aurait rien laissé.
- **Garde-fou** : l'entraînement s'arrête si la perte n'a pas bougé après 30 pas
  (prouvé sur la configuration B=24 qui échouait en silence).

## Bugs et pièges connus

- ⚠️ **Au-delà d'une certaine taille de lot, le GPU ne calcule plus, en
  silence** : à B=24 la perte reste collée à `ln(V)` et le run paraît 3,6× plus
  rapide. **Cause racine non identifiée** (aucun défaut d'allocation signalé) —
  à chercher côté dispatch/submit. Contournement : B=12/accum=2, validé.
- ⚠️ **Corpus en CRLF** : le découpage cherche `"\n\n"` et n'en trouve aucun →
  un seul bloc, masquage inopérant, sans message. Normaliser à la lecture.
- ⚠️ **Runtime C++ dédoublé** (mingw64 vs ucrt64) : corruption de tas dans
  glslang au premier noyau compute. Corrigé par `-static-libstdc++ -static-libgcc`.
