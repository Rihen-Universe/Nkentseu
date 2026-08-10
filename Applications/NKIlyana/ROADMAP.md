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

### ✅ DÉBLOQUÉ le 2026-08-10 — on enseigne un GESTE, pas des faits

La sortie a été trouvée en changeant ce qu'on enseigne. On ne lui apprend pas
des faits, mais **un geste** : « étant donné un texte et des mots, rends la
phrase du texte qui contient ces mots ». Cet exemple-là se fabrique **par pure
recopie** — le contexte est un passage réel, la question est faite de mots
**extraits de ce passage**, la réponse est une phrase **copiée mot pour mot**.
Aucune machine n'affirme quoi que ce soit, donc rien ne peut être faux qui ne le
soit déjà dans le corpus. Mode `--citations` (voir `NkIlyanaCitation.h`).

**Second enseignement, aussi important** : un quart des exemples porte sur des
mots **absents** du passage, et la réponse juste est « Je ne trouve pas cela dans
ce texte. » — vérifiée mécaniquement avant d'être écrite. Sans ces exemples-là, un
modèle à qui on n'a montré que des réponses trouvées apprend qu'il faut
**toujours** répondre, et invente quand le texte se tait.

Trois défauts trouvés en fabriquant les premiers exemples, tous corrigés :
- un passage contenant déjà « Question: » ou « Reponse: » **déplace le repère de
  masquage** de l'entraînement : le modèle apprendrait à prédire le contexte au
  lieu de la réponse, sans qu'aucune erreur ne soit signalée. Ces passages sont
  maintenant refusés ;
- un contexte d'**une seule phrase** rend l'exercice trivial : la réponse est le
  contexte entier, et on enseigne à recopier son entrée au lieu de choisir. Trois
  phrases minimum ;
- un mot présent **plusieurs fois** dans le contexte ne désigne pas une phrase
  unique : l'exercice aurait deux solutions. Seuls les mots à occurrence unique
  sont retenus.

### L'ancienne question, conservée pour mémoire

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

## ⛔ RÉGRESSION OUVERTE (2026-08-11) — activer TLS a cassé la lecture PDF

**Symptôme** : `--ajouter` sur un PDF qui fonctionnait rend désormais
`contenu 0 o, 0 operations`. Le document n'est plus ouvert du tout. Le même
fichier donnait auparavant 21 Mo de contenu et ~2 000 000 d'opérations.

**Ce qui a changé entre les deux mesures**, et rien d'autre :
1. `NKNetwork.jenga` : TLS passé d'opt-in à **actif par défaut** ;
2. `NKIlyana.jenga` : ajout de `NKMbedTLS` aux dépendances ;
3. `NKIlyana.jenga` : ajout de `bcrypt` aux bibliothèques Windows ;
4. `NkPdfFont.h` : deux accesseurs **inline** (pas de changement de disposition
   mémoire, donc peu suspect).

**Écarté** : les objets périmés. NKMedia ET NKIlyana ont été entièrement
reconstruits, la régression persiste.

**Prochaine étape — COUPER LE SUSPECT, pas le réparer.** Reconstruire avec
`NK_DISABLE_TLS=1` (et sans `NKMbedTLS` ni `bcrypt` dans NKIlyana.jenga) puis
relancer la même mesure. Si la lecture PDF revient, la cause est bien le lien ;
sinon elle est ailleurs et les trois changements ci-dessus sont innocents. Cette
mesure doit être faite AVANT toute tentative de correction.

**Hypothèse à vérifier ensuite** : un conflit de symbole entre mbed-TLS et le
décompresseur utilisé par le PDF — les deux embarquent du code de bas niveau, et
l'éditeur de liens choisit alors silencieusement l'un des deux.

⚠️ En attendant, l'aspiration de sites fonctionne (elle a besoin de TLS) mais la
lecture PDF ne fonctionne plus. Les deux ne peuvent pas coexister dans cette
construction — c'est précisément ce qu'il faut résoudre.

## PDF — diagnostic du 2026-08-10 sur le fonds réel (D:\softwareRenderer)

**Le fonds** : 8,4 Go, 267 PDF, 175 archives, 67 vidéos.

**Mesure sur 12 PDF, avec `pdftotext` comme ORACLE boîte noire** (légitime : on
compare des sorties, on ne lit aucun code) :

| | résultat |
|---|---|
| PDF dont l'oracle tire du texte | **6 / 12** |
| PDF à zéro caractère **même pour l'oracle** | **6 / 12** — documents-images, hors de portée sans OCR |
| PDF acceptés par notre lecteur | **2 / 12** |

**L'écart à combler est donc de 4 documents**, pas de 10. La moitié du fonds est
constituée de scans que rien ne lira sans reconnaissance de caractères.

### ⚠️ L'hypothèse « il manque /Encoding » était FAUSSE

J'allais implémenter `/Encoding` → noms de glyphes → Unicode. Une sonde des
dictionnaires de polices (script Python, diagnostic seulement) a montré que ces
documents **ONT leur table `/ToUnicode`** :

| document | polices | avec /ToUnicode |
|---|---|---|
| ebin.pub 2d-computer-graphics | 17 | **17 (100 %)** |
| computergraphics OpenGL | 491 | 417 (85 %) |
| Game Audio Programming | 11 | 9 |

Le chantier `/Encoding` aurait donc réparé un défaut qui n'est pas celui-là.
**Mesurer avant d'implémenter a évité un travail entier à côté de la plaque.**

### Le vrai défaut, localisé

Sur `ebin.pub` (17/17 polices avec table) notre lecteur rend :
`695450 caractères rencontrés, dont 666545 sans équivalent lisible (96 %)`.

La table existe, elle est trouvée, **et elle ne rend rien**. Les polices en cause
sont massivement **Type0 / CIDFontType2 en encodage Identity** — deux octets par
caractère. Pistes à examiner dans cet ordre :

1. `ParseToUnicode` : `DecodeStream` échoue-t-il sur le flux CMap (filtre non
   géré) ? Un échec y est **silencieux** — la police reste valide, simplement
   sans aucune entrée. C'est la piste la plus probable.
2. Le code cherché dans `mUniCodes` est-il bien celui que `NextCode` produit pour
   une police à deux octets ?
3. `ToUnicode()` fait une **recherche linéaire** sur des milliers d'entrées, pour
   chacun des ~700 000 caractères : correct mais coûteux — à indexer une fois le
   fond réparé.

### ✅ Instrumentation posée, et elle a tranché

Deux compteurs distinguent désormais « le document ne déclare rien » (limite du
document) de « il déclare une table que nous ne savons pas lire » (notre défaut) :
`AvaitToUnicode()` vs `HasToUnicode()`, remontés dans les `Stats` du rendu.

**Mesure sur `ebin.pub` : 273 006 tables déclarées, 273 006 effectivement lues.**

⛔ **La piste 1 est RÉFUTÉE** : les tables se décompressent et se lisent toutes.
Le `bfchar`/`bfrange` fonctionne.

**Il ne reste donc qu'une explication**, et elle est précise : le code présenté à
`ToUnicode(code)` **n'est pas celui que la table indexe**. Ces polices sont des
Type0/CIDFontType2 en Identity — deux octets par caractère —, et la table indexe
ces codes à deux octets. Le point de rupture est donc dans `NextCode()` (lecture
du code dans la chaîne) ou dans ce que l'appelant transmet ensuite.

**Point d'entrée pour la prochaine session** : instrumenter `NkPdfFont::ToUnicode`
pour afficher, sur les 20 premiers appels d'un document en échec, le code demandé
à côté des 5 premiers codes présents dans `mUniCodes`. Si les ordres de grandeur
diffèrent (par exemple un octet contre deux), la cause est immédiate. C'est UNE
mesure, et elle devrait clore le sujet.

⚠️ Ne pas repartir sur `/Encoding` + noms de glyphes : cette voie a été explorée
et ne concerne pas ces documents.

## Journal du 2026-08-10 — ce qui a marché, ce qui a échoué

**PHASE 2 (identité) — RÉUSSIE.** Batterie **4/19 → 8/19**. Elle nomme son père et
sa mère. Corpus : 25 % d'identité entrelacée avec de la prose (`--melange`).
Modèle promu : `ilyana_phase2.nkgp`.

**PHASE 3 (citation) — ÉCHOUÉE deux fois, la troisième invalidée.**

| essai | pas d'apprentissage | batterie | citation | verdict |
|---|---|---|---|---|
| 3a | 1e-05 (calendrier hérité) | 6/19 | non | non promu |
| 3b | 2e-04 (calendrier neuf) | 5/19 | non | non promu |
| 3c | 1e-04 | 4/19 | non | **NUL** — run mort au pas 30 |

⚠️ **Le verdict 3c ne vaut rien** : le run s'est arrêté après 161 s. J'ai mesuré
une batterie sur un modèle entraîné trente pas et failli en conclure quelque
chose. Toujours vérifier qu'un run VIT avant d'attendre son résultat.

### Trois défauts du MOTEUR trouvés en cherchant pourquoi

1. **Reprendre un run ≠ commencer une phase.** Le calendrier du pas d'apprentissage
   se prolongeait, si bien qu'une phase destinée à enseigner un comportement
   héritait d'un pas déjà au plancher (1e-05). Assez pour graver un nom répété des
   milliers de fois — d'où la réussite de la phase 2 — très insuffisant pour un
   geste nouveau. → `--nouvelle-phase` (commit `f5b62505`).

2. **Les fenêtres coupaient les exemples, et lui apprenaient à INVENTER.** Le lot
   était prélevé à un décalage tiré au hasard dans un flux plat de tokens. Un
   exemple structuré de ~180 tokens dans une fenêtre de 256 n'est entier qu'une
   fois sur trois — et quand la fenêtre commence au milieu du contexte, le modèle
   voit une question suivie de « Reponse: » avec un contexte **amputé**, et on lui
   apprend à produire une phrase qui n'y figure pas. C'est exactement le
   comportement qu'on cherchait à combattre. → une fenêtre sur deux démarre à un
   début de bloc (commit `ce296ee0`). **Hypothèse encore non testée** au moment où
   ces lignes sont écrites.

3. **Le filet de sécurité tuait des runs légitimes.** Il calculait
   `(initiale − actuelle) / initiale` et concluait « perte figée » dès que le
   résultat était négatif. Or une perte qui **monte** est du mouvement : le calcul
   a lieu, et une hausse au démarrage d'une phase, pendant la montée en puissance
   du pas, est normale. Il devait regarder la **valeur absolue**. Un garde-fou qui
   tue ce qu'il devait protéger coûte le run *et* la confiance dans la mesure.

**3d — fenêtres alignées, run complet (94 min) : batterie 6/19, citation NON.**
L'alignement a fait remonter la batterie de 5 à 6, sans donner le geste. Signe
instructif : elle produit désormais le *style* d'une réponse sourcée
(« "Le nom de la localité", est attesté sous les formes… ») — elle a appris à quoi
RESSEMBLE une citation, pas à lire celle qu'on lui donne.

**Bilan : trois essais valides, trois échecs, chacun testant un mécanisme
différent (pas d'apprentissage, alignement, dosage). Ce n'est donc plus un
problème de recette.**

### Ce qu'il reste à examiner maintenant que les recettes sont épuisées

L'alignement des fenêtres écarté, la piste suivante n'est plus une recette mais la
**taille** : recopier une phrase depuis un contexte demande au modèle d'apprendre
à faire correspondre des motifs à distance, ce qui est connu pour n'émerger qu'à
partir d'une certaine échelle. Ce serait alors un argument pour le passage à
~32 M paramètres (déjà envisagé pour le vocabulaire 32k), et non pour un réglage
de plus.

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
