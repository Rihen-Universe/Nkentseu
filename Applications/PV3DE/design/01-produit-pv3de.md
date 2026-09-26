# PV3DE — ce qu'on attend de l'application

### Document 1 — Spécification produit

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Rédigé le 26/09/2026 à partir des réponses de Rodolf du même jour :
> *« pour la formation », « pour étudiant », « l'apprenant interroge, examine —
> pas que la face, tout le corps —, il peut opérer même : un vrai apprenant en
> médecine devant un patient virtuel », « français et anglais dans un début »,
> « l'IA doit fonctionner hors ligne ».*
>
> **Ce document tranche la contradiction** que portait le dépôt : l'architecture
> (`Engine/Noge/ARCHITECTURE.md` §5.1) annonçait un outil pour *« assister un
> médecin dans le diagnostic différentiel en temps réel »* ; le code (le prompt de
> `NkConversationEngine`) disait *« tu incarnes un patient dans un contexte de
> simulation médicale »*. **PV3DE est un simulateur de formation.** Il n'est pas,
> et ne doit jamais être présenté comme, une aide au diagnostic sur de vrais
> patients.
>
> Compagnons : **02** (interface pour un humain), **03** (interface pour l'agent).

---

## 0. En une phrase

**PV3DE (Patient Virtuel 3D Émotif) met un étudiant en médecine face à un
patient virtuel qui souffre, parle, se laisse examiner de la tête aux pieds, réagit
aux soins — et jusqu'au bloc opératoire — puis lui montre, au débriefing, ce qu'il
a bien fait, ce qu'il a manqué, et pourquoi.**

---

## 1. Ce que PV3DE est, et ce qu'il n'est pas

| PV3DE **est** | PV3DE **n'est pas** |
|---|---|
| un **simulateur de consultation et de prise en charge**, pour **apprendre** | un outil d'aide au diagnostic sur de vrais patients |
| un patient **fictif**, construit à partir d'un **cas** écrit et **validé par des médecins** | un dossier médical ; aucune donnée de vrai patient n'y entre |
| un **entraînement** à l'interrogatoire, à l'examen, au raisonnement, aux gestes, à la communication | un substitut au stage hospitalier ou au compagnonnage |
| un outil d'**évaluation** formative (et, plus tard, d'examen type ECOS) | un dispositif médical |

**Conséquences, et elles sont dans le produit** :

1. **L'écran d'accueil et chaque rapport portent la mention** « Simulation à but
   pédagogique — ne constitue pas un avis médical ».
2. **Le « diagnostic différentiel » calculé par `NkDiagnosticEngine` devient le
   corrigé du cas**, vu par le formateur et montré à l'apprenant **au débriefing
   seulement** — jamais pendant la séance (ce serait donner la réponse).
3. **L'export FHIR** ne sert plus à un dossier de vrai patient : il devient une
   **trace pédagogique** (l'observation que l'apprenant a rédigée, comparée au
   corrigé), avec une identité **fictive**. Les champs « nom / prénom » du
   `ReportPanel` actuel désignent **le patient fictif du cas**.
4. **Le contenu médical est validé** : un cas porte un **statut** (brouillon · en
   relecture · **validé par** un médecin nommé, avec date) et ses **sources**. Un
   cas non validé ne sort pas en mode Examen.

---

## 2. Pour qui

| utilisateur | ce qu'il fait |
|---|---|
| **l'apprenant** (étudiant en médecine d'abord ; plus tard infirmier, sage-femme) | passe des séances face à un patient, lit son débriefing, suit ses progrès |
| **le formateur** (enseignant, médecin) | choisit et **écrit** des cas, fixe les grilles d'évaluation, suit une promotion, anime une séance collective |
| **le relecteur médical** | valide un cas (contenu, cohérence clinique, sources) |
| **l'établissement** | installe PV3DE dans une salle (parfois **sans Internet**), récupère les résultats |

**Contexte de départ** : facultés de médecine et écoles de santé, **en français et
en anglais** (le Cameroun enseigne dans les deux), avec des **salles sans
connexion fiable** — d'où l'IA **hors ligne**.

---

## 3. La séance — ce que fait l'apprenant

Une séance suit **le déroulé d'une vraie prise en charge**. Chaque étape est
**libre** (on revient à l'interrogatoire après l'examen, comme dans la réalité) ;
la barre des étapes montre où l'on en est, elle n'impose pas l'ordre.

| étape | ce que fait l'apprenant | ce que fait le patient |
|---|---|---|
| **1. Accueil** | lit la situation de départ (« Homme de 54 ans, amené aux urgences pour douleur thoracique »), se présente | salue, exprime son état (douleur, peur) |
| **2. Interrogatoire** | pose ses questions **à la voix ou au clavier**, en français ou en anglais : motif, histoire de la maladie, antécédents, traitements, allergies, mode de vie, contexte | **répond dans son rôle** : ce qu'il sait, avec ses mots, sa personnalité ; il peut se tromper, minimiser, refuser, s'inquiéter |
| **3. Examen clinique** | examine **le corps entier** (§4) : constantes, inspection, palpation, percussion, auscultation, examens spécialisés — en demandant le consentement, en installant le patient | réagit : grimace et se protège quand on palpe la zone douloureuse, respire fort à la demande, refuse si on ne lui a rien expliqué |
| **4. Examens complémentaires** | prescrit biologie, imagerie, ECG… | les résultats arrivent **après un délai réaliste** (accéléré) |
| **5. Raisonnement** | note ses **hypothèses**, les classe, écrit son diagnostic | — |
| **6. Prise en charge** | prescrit (médicament, dose, voie), pose des gestes (voie veineuse, oxygène, sonde…), oriente (hospitaliser, bloc, sortie) | **évolue** : s'améliore si le traitement est juste, s'aggrave s'il ne l'est pas ou si l'on tarde (§5) |
| **7. Bloc opératoire** | si le cas l'exige : prépare, vérifie, opère (§6) | anesthésié ; ses constantes réagissent aux gestes |
| **8. Synthèse** | rédige son **observation médicale** (structure imposée par le cas) | — |
| **9. Débriefing** | lit son évaluation, revoit la séance, compare au parcours d'expert | — |

### 3.1 Les trois modes de séance

| mode | aides | temps | évaluation |
|---|---|---|---|
| **Entraînement** | indices sur demande, pause, retour en arrière, constantes commentées | libre (accéléré possible) | formative, détaillée |
| **Examen** (type ECOS) | **aucune** | **chronométré**, stations | grille notée, erreurs critiques éliminatoires |
| **Démonstration** | le formateur joue, la classe regarde (projeté) | libre | — |

Plus tard : **séance collective** (un apprenant examine, la classe observe et
annote) et **équipe** (plusieurs apprenants sur le même patient : urgence,
réanimation).

---

## 4. L'examen du corps entier

C'est ce qui sépare PV3DE d'un « visage qui parle ». **Tout le corps est
examinable**, et chaque manœuvre donne un **résultat écrit dans le cas**.

### 4.1 Les gestes

| famille | gestes | ce que l'apprenant perçoit |
|---|---|---|
| **Constantes** | tension, pouls, fréquence respiratoire, SpO₂, température, glycémie capillaire, poids, taille, douleur (EVA) | le **moniteur** ou l'instrument affiche la valeur |
| **Inspection** | regarder une région, la peau, les muqueuses, la respiration, la démarche | **ce qui se voit est rendu** : pâleur, ictère, cyanose, éruption, œdème, plaie, tirage respiratoire, sueurs (shaders peau / yeux de PV3DE) |
| **Palpation** | palper une région (abdomen par quadrants, aires ganglionnaires, pouls périphériques, thyroïde…) | **la réaction du patient** : grimace (FACS), retrait, défense ; et le **résultat** du cas (« défense de la fosse iliaque droite ») |
| **Percussion** | percuter thorax, abdomen | le **son** (mat, tympanique, normal) |
| **Auscultation** | poser le stéthoscope sur un **foyer** (cardiaques, pulmonaires, abdomen, vaisseaux) | le **son** du cas en ce point (souffle, crépitants, sibilants, silence) |
| **Neurologique** | réflexes, force, sensibilité, pupilles, nerfs crâniens, Glasgow | les réponses du patient (le membre bouge ou non, la pupille se contracte) |
| **Spécialisés** | otoscopie, examen de la gorge, fond d'œil, toucher rectal / vaginal (**avec consentement explicite et chaperon**), examen des seins, examen des organes génitaux | le résultat du cas, **en texte et en image** |

### 4.2 Les règles de l'examen

- **Consentement** : avant un examen qui déshabille ou touche une zone intime,
  l'apprenant **explique et demande**. Le patient accepte, hésite ou refuse selon
  sa personnalité ; **l'oubli est noté** au débriefing (communication).
- **Pudeur** : le patient est découvert **par région**, jamais entièrement ; la
  région examinée seulement.
- **Installation** : allongé, assis, debout, sur le côté — le patient **bouge**
  (NkAnima : postures, IK), et certaines positions sont **impossibles** dans
  certains états (un patient en détresse ne tient pas debout).
- **Le patient ne révèle rien qu'on ne cherche pas** : un souffle cardiaque
  n'apparaît que si l'on ausculte le bon foyer.

---

## 5. Un patient qui évolue

Le patient n'est pas une fiche figée. Trois couches, de la plus simple à la plus
riche :

| couche | ce qu'elle fait | exemple |
|---|---|---|
| **Scénario** (écrit dans le cas) | des **états** et des **transitions** déclenchés par le temps ou par les actes | « si pas d'antibiotique avant 60 min → choc septique » |
| **Physiologie** (modèle continu) | les constantes varient ensemble, selon des modèles simples (hémodynamique, respiration, température, glycémie) | une hémorragie fait monter le pouls, baisser la tension, pâlir la peau |
| **Pharmacologie** | chaque médicament a un effet, un délai, une durée, des contre-indications, des interactions | l'adrénaline en cas d'anaphylaxie : la tension remonte en quelques minutes ; un antibiotique auquel le patient est allergique : réaction |

**Le temps de la séance** peut être **accéléré** (entraînement) : une nuit
d'observation dure quelques minutes, mais le **temps simulé** est affiché.

**L'émotion suit la clinique** : `NkClinicalState` (douleur, nausée, fatigue,
anxiété, dyspnée, conscience) pilote `NKEmotion` → visage (FACS, désormais dans
NKAnima), corps (respiration, posture, tremblements) et voix. **C'est l'existant, et
c'est le cœur émotif du produit.**

---

## 6. Le bloc opératoire

Rodolf : *« il peut opérer même ».* C'est la partie la plus ambitieuse ; elle se
construit **par niveaux**, et le produit dit honnêtement lequel il sait faire.

| niveau | ce que l'apprenant fait | ce que l'application sait faire |
|---|---|---|
| **1. Procédure guidée** | prépare le patient, **vérifie la check-list de sécurité** (OMS), choisit l'anesthésie, **enchaîne les étapes** d'une intervention (voie d'abord, exploration, geste, fermeture) en choisissant les **instruments** et l'**ordre** | une intervention = un **graphe d'étapes** écrit dans le cas, avec les erreurs possibles et leurs conséquences (saignement, contamination) ; la vue montre l'**anatomie par couches** |
| **2. Gestes simples** | **sutures**, incision, nœuds, hémostase, à la souris ou à la tablette | geste tracé, jugé (espacement, profondeur, tension) |
| **3. Chirurgie simulée** | couper, disséquer, écarter dans des **tissus déformables** | simulation de tissus mous (NkAnima : déformables, découpe) — **recherche**, pas avant les paliers tardifs |
| **4. Immersion** | la même chose **en réalité virtuelle**, les mains suivies | NKXR |

L'**anesthésie** est vécue par ses **constantes** (moniteur) : le patient endormi
réagit aux gestes par la tension et le pouls, et l'apprenant doit les surveiller.

⚠️ **Les modèles anatomiques par couches** (peau, graisse, muscles, organes,
vaisseaux, os) sont un **gros chantier d'assets** : ils se font dans **NKCraft**,
se rigent dans **NkAnima**, et se valident avec des anatomistes.

---

## 7. Le débriefing

**Le vrai produit, c'est le débriefing.** Une séance sans retour n'apprend rien.

- **La grille** du cas (type ECOS), cochée automatiquement : questions posées ou
  oubliées, gestes faits ou oubliés, examens pertinents / inutiles / dangereux,
  diagnostic, prise en charge, **communication** (présentation, consentement,
  explication, empathie, reformulation).
- **Les erreurs critiques**, à part et en tête (médicament contre-indiqué, oubli
  d'un geste vital, délai dangereux).
- **Le raisonnement** : les hypothèses de l'apprenant, dans l'ordre et le moment
  où il les a écrites, **comparées au diagnostic différentiel du cas** (le
  corrigé, enfin visible).
- **Le parcours d'expert** : ce qu'un médecin aurait fait, étape par étape, à
  côté de ce que l'apprenant a fait.
- **La relecture** : la séance **rejouée** (patient, paroles, gestes, constantes),
  avec une frise où chaque acte est un point cliquable.
- **Ce que le patient a ressenti** : « le patient s'est senti écouté », « il a eu
  peur quand vous avez parlé d'opération sans expliquer ».
- **L'observation rédigée**, comparée au modèle.

---

## 8. Le cas — l'unité de contenu

Un **cas** (`.nkcase`, format déjà annoncé par le README) contient tout ce qui fait
un patient et sa séance :

| partie | contenu |
|---|---|
| **identité fictive** | nom, âge, sexe, **apparence** (personnage NKCraft / NkAnima), langue, métier, contexte social |
| **personnalité** | `NkPersonality` existe déjà : les cinq grands traits, tolérance à la douleur, expressivité, capacités (pleurer, refuser de répondre, minimiser, mentir sur la douleur…) |
| **histoire** | motif, histoire de la maladie, antécédents, traitements, allergies, mode de vie — **ce que le patient sait et dira**, et **comment** il le dit |
| **examen** | pour **chaque manœuvre** et chaque région : le **résultat** (texte, image, son, réaction du patient) |
| **examens complémentaires** | pour chaque examen : le résultat, le **délai**, l'image (radiographie, ECG…) |
| **évolution** | états, transitions, paramètres physiologiques, effets des traitements |
| **bloc** (facultatif) | le graphe d'étapes de l'intervention |
| **corrigé** | diagnostic, diagnostic différentiel, parcours d'expert, points d'enseignement |
| **grille** | items, poids, **erreurs critiques** |
| **métadonnées** | niveau (2ᵉ cycle, internat…), spécialité, durée, **statut de validation**, relecteur, **sources**, langues disponibles |

**La base actuelle** : 50 pathologies (dont le paludisme), 30 symptômes, un score
par poids. C'est un **amorçage**. Le contenu doit être **écrit par des
cas**, pas déduit d'une table de poids : un vrai patient n'est pas une liste de
symptômes pondérés.

📌 **Pathologies du contexte** à couvrir tôt, parce que ce sont celles que les
apprenants rencontreront : paludisme grave, drépanocytose, fièvre typhoïde,
tuberculose, VIH et infections opportunistes, méningite, diarrhées et
déshydratation de l'enfant, pré-éclampsie et hémorragie du post-partum,
morsure de serpent, traumatologie de la route — à côté des urgences universelles
(infarctus, AVC, sepsis, anaphylaxie).

---

## 9. L'IA — hors ligne

**Tout fonctionne sans Internet.** Les briques existent déjà dans le dépôt, à
assembler :

| fonction | brique | état |
|---|---|---|
| **entendre** l'apprenant (parole → texte), FR et EN | `NKSpeech/NkASR` | présente, bancs `NKASRTest` |
| **faire parler** le patient (texte → parole), avec l'émotion | `NKSpeech/NkTTS`, `NkVoiceSynth` ; modulation par l'état (souffle, tremblement — `NkVoiceModulator` prévu) | présente, `NKTTSTrain` |
| **répondre dans le rôle** | un **modèle de langue local** (NKAI : famille Qwen2 — inférence, SFT, bancs `NKQwen2Chat`, `NKQwen2SftTest`) ; le transport `NKConverse` | présent ; **Ollama** reste un backend de R&D ; **le backend Claude est à retirer du produit** (réseau) ou à garder pour le seul développement |
| **comprendre les gestes** | pas d'IA : les gestes sont des **actions** de l'interface | — |
| **aider le formateur** à écrire un cas | le même modèle local | à faire |

### 9.1 Les règles de l'IA du patient

C'est là que le produit peut se tromper gravement ; les règles sont strictes.

1. **Le patient ne sait que ce que le cas lui fait savoir.** Le modèle reçoit
   **les faits du cas** (histoire, ressenti, ce que le personnage sait) et la
   **personnalité** ; il n'invente **aucun fait clinique**. Une question hors du
   cas reçoit une réponse **de personnage** (« je ne sais pas, docteur »), jamais un
   symptôme inventé.
2. **Le patient ne donne jamais le diagnostic**, et ne parle pas en termes
   médicaux (sauf si le personnage est soignant).
3. **Chaque réponse est vérifiée** contre le cas avant d'être dite : une réponse
   qui affirme un fait absent du cas est **rejetée** et régénérée, ou remplacée
   par une réponse de repli. (Même règle que la famille : *sortie validée, jamais
   interprétée au mieux*.)
4. **La détection des questions** (« a-t-il demandé les allergies ? ») ne repose pas
   sur l'humeur du modèle : chaque item de la grille a des **formulations
   reconnues** (FR et EN), et le modèle ne fait que **classer** la question vers un
   item. Le classement est **journalisé**, et le formateur peut le corriger.
5. **Les modèles sont les nôtres, et leur licence voyage avec eux** (règle de la
   famille).
6. **Budget matériel** : les salles de cours n'ont pas de gros GPU. **Trois
   niveaux** : réponse **par règles** (`NkRulesBackend`, fonctionne partout),
   **petit modèle local** (le défaut), **modèle plus grand** si la machine le
   permet. Le produit choisit à l'installation et **le dit**.

---

## 10. Relations avec la famille

```
NKCraft ──(corps du patient, anatomie par couches, instruments)──┐
NkAnima ──(squelette, FACS, respiration, postures, gestes)───────┼──▶ PV3DE (sur Noge)
NKAI / NKSpeech / NKConverse ──(écoute, parole, dialogue)────────┘
```

- PV3DE est une **application sur Noge**, comme un jeu : scène (salle de
  consultation, chambre, bloc), patient, instruments.
- Les **personnages** viennent du **générateur de personnages de NKCraft** (toutes
  morphologies, tous âges) et sont rigués dans **NkAnima**.
- Le **calcul facial** est descendu dans **NKAnima** le 25/09 (« PV3DE le
  consomme au lieu de le posséder ») : c'est la bonne direction pour tout le
  reste (respiration, postures, tremblements → NKAnima).
- L'interface suit **le thème, les jetons et les composants de la famille**.

---

## 11. Phasage

| palier | contenu | critère de fin |
|---|---|---|
| **P0 — le socle** | PV3DE se construit et tourne ; format `.nkcase` v1 ; un patient 3D **corps entier** avec visage FACS et respiration ; **dialogue au clavier** par modèle local, règles §9.1 | un cas (douleur thoracique) joué au clavier : 30 questions, **zéro fait inventé** au contrôle contre le cas |
| **P1 — la consultation** | **voix** hors ligne (ASR + TTS), **FR et EN** ; grille d'interrogatoire ; débriefing de base (questions posées / oubliées, relecture) ; **10 cas validés** | un apprenant mène un interrogatoire complet **à la voix, sans Internet**, et reçoit sa grille |
| **P2 — l'examen du corps** | constantes et moniteur ; inspection (rendu des signes) ; palpation avec **réactions** ; percussion et auscultation (**sons**) ; neurologique ; consentement et pudeur ; positions | un abdomen aigu : la défense en fosse iliaque droite se trouve **seulement** en palpant là, et le patient grimace |
| **P3 — prendre en charge** | examens complémentaires avec délais ; hypothèses ; prescription ; **évolution** (scénario + physiologie + pharmacologie) ; mode **Examen** | un sepsis non traité en 60 min simulées se dégrade ; traité à temps, il s'améliore — et la grille le note |
| **P4 — le formateur** | **éditeur de cas** ; relecture et **validation médicale** ; suivi d'une promotion ; export des résultats ; séance de démonstration | un formateur écrit un cas de paludisme grave de bout en bout, un relecteur le valide, dix apprenants le passent |
| **P5 — le bloc, niveaux 1 et 2** | procédure guidée, check-list, instruments, anatomie par couches ; sutures | une appendicectomie guidée, de la check-list à la fermeture, avec ses erreurs possibles |
| **P6 — plus loin** | chirurgie sur tissus déformables ; **réalité virtuelle** ; séances en équipe ; **langues locales** (patients qui parlent pidgin, ewondo, fulfulde… — l'apprenant apprend aussi à consulter avec un interprète) | |

---

## 12. Règles transverses

1. **Simulation pédagogique, jamais aide au diagnostic réel** — et c'est écrit à
   l'écran.
2. **Aucune donnée de vrai patient** n'entre dans PV3DE.
3. **Le corrigé ne se voit qu'au débriefing** (ou par le formateur).
4. **Le patient n'invente rien** : ses réponses sont vérifiées contre le cas.
5. **Tout fonctionne hors ligne.**
6. **Un cas non validé ne sort pas en mode Examen.**
7. **Les données des apprenants restent dans l'établissement** (stockage local,
   export par le formateur).
8. **Toute action est une commande** (rejouable : c'est ce qui permet la relecture
   au débriefing).
9. **Zéro-STL, NKMemory**, conventions ; **interface en `.nkgui`**, thème de la
   famille.

---

## 13. Décisions

| # | question | proposition |
|---|---|---|
| D1 | le backend **Claude** (réseau) dans le produit | **retiré** du produit ; gardé pour le développement seulement |
| D2 | l'export **FHIR** | gardé comme **trace pédagogique** (identité fictive) ; formats d'échange avec les plateformes de cours (**xAPI**, **SCORM**) à étudier en P4 |
| D3 | `DiagnosticPanel` et `SymptomInputPanel` actuels | deviennent des **outils du formateur** (éditeur de cas, mode démonstration) ; **invisibles** à l'apprenant pendant la séance |
| D4 | partenaires médicaux | **indispensables avant P1** : une faculté ou un hôpital qui relit et valide les cas. Sans relecteur, pas de cas « validé », donc pas de mode Examen |
| D5 | infirmiers et sages-femmes | après P4 : mêmes cas, **grilles différentes** |
| D6 | nom affiché | « PV3DE » en interne ; un nom d'usage pour les établissements est à choisir |
