# Langues locales camerounaises dans la chaîne IA de Nkentseu — analyse

> Analyse demandée par Rihen, priorité **ghomálá'**. Rendue le **2026-07-30**.
> Elle porte sur ce qu'il est *possible* de faire aujourd'hui avec la chaîne
> existante (BulkGen, NKSpeech, NKGpt/NKTrain), sur ce qui serait **faux** de
> faire, et sur l'ordre dans lequel avancer.

---

## 1. La conclusion d'abord

**Ne pas générer de corpus en langue locale avec un LLM.** C'est la seule
recommandation forte de ce document, et elle est fondée sur une mesure faite ici,
pas sur une intuition.

Sur les thèmes **historiques et culturels camerounais**, en **français** — donc
dans la langue où les modèles sont les plus solides — `qwen2.5:7b-instruct` et
`deepseek-r1:8b` produisent des noms, des dates et des filiations **plausibles et
faux**, avec la même assurance que les réponses correctes. La consigne de
perspective africaine corrige le *cadrage* du texte ; elle ne comble pas les
*lacunes factuelles*. C'est déjà consigné comme dette du corpus : les paires de
ces thèmes doivent être relues et sourcées avant tout entraînement.

Si le modèle invente sur un sujet camerounais **en français**, il inventera
davantage en ghomálá', où sa quantité de données d'entraînement est de plusieurs
ordres de grandeur inférieure. Un corpus ghomálá' généré serait :

- **indétectablement faux** pour qui ne parle pas la langue — donc pour la
  quasi-totalité des relecteurs disponibles ;
- **auto-renforçant** : entraîner dessus fige les erreurs et les rend fluides,
  ce qui les rend *plus* crédibles, pas moins ;
- **nuisible au-delà du projet** : un corpus faux publié sur une langue peu dotée
  devient une source pour les suivants. Le coût d'une erreur n'est pas symétrique
  entre une langue à milliards de tokens et une langue à quelques milliers.

La contrainte n'est donc pas technique mais **documentaire** : on ne peut pas
produire ce qu'on n'a pas les moyens de vérifier.

---

## 2. Ce qu'est le ghomálá', et pourquoi ça change la conception

Le ghomálá' est une langue **Grassfields** (groupe bamiléké) de l'**Ouest
Cameroun**, parlée notamment dans la région de Bafoussam, Baham et Bandjoun.
Trois propriétés ont des conséquences directes sur la chaîne technique :

| propriété | conséquence pour le code |
|---|---|
| **langue à tons** | le ton est **lexical et grammatical**, pas de l'intonation. Deux mots identiques à la lettre près peuvent différer par le seul ton. Une chaîne texte→parole ou parole→texte qui ignore le ton produit des mots *différents*, pas un accent. |
| **orthographe à diacritiques** (alphabet général des langues camerounaises) | caractères hors ASCII et hors Latin-1, **combinaisons** de lettre + marque tonale. Toute normalisation Unicode naïve (NFD/NFC mal choisie, `tolower` par octet, repli ASCII) **détruit du sens**. |
| **peu dotée en écrit** | il n'existe pas de masse textuelle exploitable pour un entraînement statistique. Ce qui existe est surtout **oral**, et souvent chez des locuteurs, pas dans des fichiers. |

**La conséquence de conception la plus importante** : pour une langue à tons, la
donnée *primaire* est l'**audio**, et le texte n'en est qu'une transcription
conventionnelle. La chaîne doit donc être bâtie autour de l'audio aligné, pas
autour du texte comme pour le français.

> Réserve honnête : les affirmations ci-dessus sont générales et sûres
> (appartenance, région, caractère tonal, alphabet à diacritiques). **Tout détail
> plus fin — inventaire tonal exact, variantes dialectales, conventions
> orthographiques concurrentes — doit être établi avec un locuteur ou une source
> écrite identifiée, pas déduit d'un modèle.** C'est précisément la règle que ce
> document défend.

---

## 3. Ce qui est faisable dès maintenant, sans locuteur

Trois chantiers ont une valeur réelle et **ne demandent aucune connaissance de la
langue** — donc aucun risque d'inventer :

### 3.1 Rendre la chaîne texte *capable* de la langue
Avant de traiter du ghomálá', il faut que rien dans le moteur ne le mutile :

- **normalisation Unicode explicite et unique** (choisir NFC, la documenter, la
  faire respecter à l'entrée de NKSpeech, NKGpt et du corpus) ;
- **casse et comparaison** qui ne passent pas par un `tolower` octet à octet ;
- **rendu de texte** : vérifier que la pile de police (NKFont) compose
  correctement lettre + diacritique combinant, et ne rend pas un carré ou deux
  glyphes disjoints ;
- **tokenisation** : un tokeniseur entraîné sur du français découpe une langue à
  diacritiques en fragments d'un caractère. Ce n'est pas fatal, mais ça doit être
  **mesuré** (tokens par mot) avant toute décision d'entraînement.

Chacun de ces points se teste avec **une poignée de mots de référence**, obtenus
d'une source écrite identifiée — pas besoin de comprendre la langue pour vérifier
qu'un mot ressort **identique** à ce qui est entré. C'est un test d'identité, la
même méthode que l'aller-retour du mode édition.

### 3.2 Traiter le ghomálá' comme un problème d'**audio**, pas de texte
NKSpeech est le bon point d'entrée, et l'ordre est imposé par la dépendance :

1. **collecte** : enregistrements de locuteurs, phrases courtes, conditions
   connues (une seule voix par session, micro fixe, silence). Sans consentement
   écrit et sans traçabilité de la source, la donnée est inutilisable ;
2. **alignement** texte ↔ audio, y compris la **marque tonale** ;
3. *seulement ensuite* : reconnaissance ou synthèse.

Sauter à l'étape 3 sans 1 et 2 revient à faire imiter au modèle une langue qu'il
n'a jamais entendue.

### 3.3 Décider maintenant ce qui vaut pour toutes les langues
Le ghomálá' est une **priorité**, pas un cas particulier : le Cameroun compte des
centaines de langues. Tout ce qui est écrit en dur pour une langue sera à refaire
pour la suivante. Les points 3.1 doivent donc être traités comme des propriétés
du moteur (**paramétrées par la langue**), pas comme un mode ghomálá'.

---

## 4. Ce qu'il ne faut pas faire, et pourquoi

| tentation | ce qui se passe réellement |
|---|---|
| Générer des paires Q/R en ghomálá' avec le modèle du corpus | Texte fluide, grammaticalement faux et lexicalement inventé, **impossible à repérer** sans locuteur. Le pire des cas : ça *a l'air* de marcher. |
| Traduire le corpus français vers le ghomálá' par LLM | Même problème, aggravé : les erreurs deviennent systématiques (donc apprises comme règles) au lieu d'être dispersées. |
| Entraîner un petit modèle « pour voir » sur quelques milliers de phrases | Produit un générateur de charabia convaincant. Le risque n'est pas l'échec, c'est le **faux succès** que personne dans l'équipe ne peut contredire. |
| Traiter le ton comme de la prosodie | Change les mots. Une synthèse « qui sonne bien » peut dire autre chose que le texte. |
| Se replier sur l'ASCII pour « simplifier » | Fusionne des mots distincts. La simplification supprime l'information qui porte le sens. |

---

## 5. Plan proposé, par ordre de dépendance

| # | étape | dépend d'un locuteur ? | livrable vérifiable |
|---|---|---|---|
| 1 | Normalisation Unicode unique + tests d'identité sur mots de référence | non | test qui échoue si un mot ressort modifié |
| 2 | Rendu des diacritiques dans NKFont | non | capture comparée caractère par caractère |
| 3 | Mesure de la tokenisation (tokens/mot) sur un échantillon | non | chiffre, pas une impression |
| 4 | Protocole de collecte audio (consentement, traçabilité, conditions) | **oui** | document + première session |
| 5 | Alignement texte/audio avec marque tonale | **oui** | jeu aligné, écoutable et relu |
| 6 | Décision entraînement (ASR ou TTS d'abord), au vu de 3 et 5 | **oui** | décision datée et argumentée |

Les étapes 1 à 3 sont du travail d'ingénierie pur, faisables tout de suite, et
elles **conditionnent** tout le reste : inutile de collecter de l'audio si la
chaîne texte abîme la transcription.

---

## 6. Lien avec le corpus en cours

Le corpus BulkGen (français, ~39 000 paires au 30/07/2026) **n'est pas concerné**
par ce document et ne doit pas l'être : y mêler de la langue locale générée
contaminerait un ensemble par ailleurs exploitable. La séparation déjà prévue en
deux bacs — « générique vérifiable » (maths, code, grammaire) et
« culturel/historique » en quarantaine — s'applique ici à l'identique : **la
langue locale serait un troisième bac, et il resterait vide tant qu'il n'est
alimenté que par un modèle.**
