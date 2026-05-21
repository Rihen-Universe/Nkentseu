# Vision, Stratégie & Business Plan — Rihen Universe

**Version 1.0 — 20 mai 2026**
**Auteur : TEUGUIA TADJUIDJE Rodolf Séderis (Rihen)**
**Société : Rihen SARL, Cameroun**

---

## Table des matières

1. [Analyse stratégique — où nous en sommes](#1-analyse-strategique)
2. [Pertinence à l'ère de l'IA](#2-pertinence-ia)
3. [Infrastructure IA & ambitions spatiales](#3-infrastructure-ia-espace)
4. [Pilier de la technologie africaine de demain](#4-pilier-technologique-africain)
5. [Stratégies de réussite sans les barrières des grandes puissances](#5-strategies-reussite)
6. [Plan d'affaires complet](#6-business-plan)
7. [Ce que nous pouvons créer demain avec ce qui existe aujourd'hui](#7-roadmap-produits-future)
8. [Risques & mitigation](#8-risques)
9. [Besoins en financement](#9-financement)

---

<a id="1-analyse-strategique"></a>

## 1. Analyse stratégique — où nous en sommes

### Actifs technologiques actuels (mai 2026)

**Nkentseu** — Écosystème C++ modulaire cross-platform, structure 4 couches :


| Couche             | Modules                                                           | Statut        |
| ------------------ | ----------------------------------------------------------------- | ------------- |
| Foundation         | NKCore, NKMath, NKMemory, NKContainers, NKPlatform                | ✅ Production |
| System             | NKFileSystem, NKLogger, NKThreading, NKNetwork, NKTime            | ✅ Production |
| Runtime            | NKWindow, NKEvent, NKRHI, NKECS, NKImage, NKFont, NKUI, NKContext | ✅ Production |
| Runtime (en cours) | NKRenderer (PBR/Vulkan/GL OK), NKAudio, NKCollision               | 🔧 Actif      |
| Engine             | Moteur 2D/3D complet                                              | 🔧 Démarrage |

**Jenga** — Build system Python cross-platform avec :

- 7 plateformes : Windows, Linux, macOS, Android, iOS, tvOS, watchOS, Web (Emscripten)
- Packaging multi-format : MSI (WiX 5), EXE (Inno Setup), ZIP, DEB, PKG, DMG, APK, AAB
- Toolchain auto-detect : MSVC, Clang, GCC, MinGW
- Icon generator multi-plateforme (ICO, ICNS, mipmap, favicon)

**Pong Ultra Arena** — Premier titre commercial :

- Multijoueur LAN production-ready (identifiants Pays/Ville-Code aléatoires, scan UDP par nom, validation host explicite)
- Solo + IA + IA vs IA + Réseau
- 8 types d'obstacles, bonus/malus, animations, scenes responsive PC+mobile
- Build distribuable : APK Android + MSI/EXE/ZIP Windows

**Sujet de recherche PV3DE** — Patient Virtuel 3D Émotif pour le Diagnostic Éducatif (Master Recherche ENSPY 2025-)

### Trajectoire personnelle (atout unique)

Triple casquette à l'ENSPY (École Nationale Supérieure Polytechnique de Yaoundé) :

- **Diplômé ingénieur** (promotion 2023, Game Programming & Computing Graphics)
- **Enseignant** (depuis 2024)
- **Étudiant Master Recherche** (depuis 2025, PV3DE)

Cette combinaison fournit :

- Crédibilité académique pour publications & partenariats
- Revenus stables (enseignement) sans dépendre du capital risque
- Vivier de talents (étudiants) pour la communauté Nkentseu
- Réseau alumni ENSPY pan-africain

---

<a id="2-pertinence-ia"></a>

## 2. Pertinence à l'ère de l'IA — analyse honnête

### Nkentseu n'est pas un produit IA. C'est une infrastructure runtime.

À l'ère de l'IA, ce n'est ni "obsolète" ni "central" — c'est **complémentaire**. Voici pourquoi c'est pertinent malgré tout :

**1. L'IA générative produit du contenu qu'il faut héberger**
LLM génèrent du texte, des images, du 3D, du code. Quelqu'un doit construire le runtime qui affiche tout ça en temps réel. Unity et Unreal dominent mais imposent 5%+ de royalties. Nkentseu est une alternative open-source pour des cas où la propriété du stack matter.

**2. La simulation explose**

- Médicale (formation chirurgicale, patient virtuel)
- Scientifique (CFD, météo, agriculture)
- Industrielle (jumeaux numériques d'usine)
- Spatiale (missions, formation ingénieurs)
- Militaire/sécurité (formation, gestion de crise)

Tous ces domaines nécessitent un moteur 3D temps réel. Nkentseu est exactement ce moteur.

**3. Les jeux IA-natifs émergent**
NPCs avec LLM intégrés, génération procédurale, world-building dynamique. Les studios indépendants ne veulent pas payer Unity/Unreal — un moteur libre devient stratégique.

**4. PV3DE est l'exemple démonstrateur**
Patient Virtuel 3D Émotif = LLM + animation faciale + simulation comportementale + UI médicale. Nkentseu est le runtime. C'est exactement le type d'application IA + 3D temps réel qui n'existe pas ailleurs en Afrique.

### Ce que Nkentseu ne peut pas faire

- Entraînement de modèles LLM (besoin de Python, PyTorch, CUDA, milliers de GPU)
- Recherche fondamentale en IA (besoin de chercheurs PhD + budgets de FAANG)
- Concurrence à OpenAI/Anthropic/Google sur les modèles foundation

**Conclusion** : Nkentseu se positionne comme **runtime d'inférence et de présentation**, pas comme framework d'entraînement. C'est un créneau viable, défendable, avec moins de concurrents.

---

<a id="3-infrastructure-ia-espace"></a>

## 3. Infrastructure IA & ambitions spatiales

### Infrastructure IA propre — ce qui est faisable

**Possible avec nos briques actuelles** :

- Software d'orchestration (NKNetwork TCP/WS + NKThreading)
- Frameworks d'inférence locale (Nkentseu charge ONNX/GGUF, fait du sampling LLM)
- Outils de monitoring & déploiement (Jenga packaging)
- SaaS de fine-tuning pour PME africaines

**Impossible sans partenariat** :

- Construire des datacenters (50M-500M $ par site)
- Fabriquer des GPU/TPU (TSMC = Taïwan, Nvidia = USA)
- Concurrencer AWS/GCP/Azure sur l'IaaS pur

**Stratégie réaliste** :

1. **Phase 1 (2026-2028)** : Construire la couche logicielle d'infra IA en open-source. Louer le hardware (Hetzner, Scaleway, RunPod).
2. **Phase 2 (2028-2031)** : Partenariats avec les datacenters africains émergents (Google Casablanca 2024, AWS Cape Town 2020, Africa Data Centres Lagos). Offrir notre stack comme middleware.
3. **Phase 3 (2031+)** : Si la traction le justifie, datacenter propre (Yaoundé/Douala) en JV avec un opérateur télécom (Orange, MTN, Camtel).

### Ambitions spatiales

L'Afrique a 22 agences spatiales nationales (UA Space Agency basée au Caire). Le Cameroun a l'**ASE-CAM** depuis 2020. Le programme CamSat-1 prévoit le premier satellite camerounais.

**Ce que Nkentseu peut faire pour le spatial africain** :

- **Simulateurs de mission** (cf KSP, mais éducatif et pour ingénieurs)
- **Formation ingénieurs spatiaux** (visualisation orbite, propulsion, rentrée atmosphérique)
- **Vulgarisation grand public** (apps grand public sur l'espace africain)
- **Visualisation de données satellite** (imagerie agricole, météo, urbanisme)

**Ce que Nkentseu ne peut pas faire** :

- Fabriquer des satellites (hardware aérospatial, lanceurs, propulsion)
- Remplacer NASA/ESA/Roscosmos sur la R&D pure

**Stratégie réaliste** :

- Partenariat avec ASE-CAM, Agence Spatiale Algérienne (ASAL), Space Generation Advisory Council Africa
- Produire 1-2 simulateurs pédagogiques solides (CamSat-1 mission, lancement Ariane 6 vu d'Afrique, etc.)
- Devenir le **fournisseur software de référence** pour la formation spatiale africaine

---

<a id="4-pilier-technologique-africain"></a>

## 4. Pilier de la technologie africaine de demain

### Les chances sont réelles, sous conditions

**Atouts uniques** :

1. **Aucun concurrent direct** en Afrique sur le segment moteur 3D + build system open-source documenté en français
2. **Position académique ENSPY** = vivier de talents + crédibilité institutionnelle
3. **Build in public** depuis 2024 = narrative déjà construite (LinkedIn, GitHub)
4. **Identité africaine assumée** dès le naming (Nkentseu, Pong avec villes africaines, etc.) = différenciation émotionnelle forte
5. **Stack complet** : pas seulement un module, mais un écosystème entier

**Conditions critiques** :


| Condition                                                        | Impact si non rempli                      |
| ---------------------------------------------------------------- | ----------------------------------------- |
| Survivre économiquement 5+ ans                                  | Projet meurt par épuisement              |
| Documentation fr+en accessible                                   | Pas d'adoption par devs débutants        |
| 5-10 contributeurs externes en 18 mois                           | Reste "projet personnel" pas écosystème |
| Cas d'usage prouvés (PV3DE + 2-3 apps)                          | Pas de crédibilité, pas d'adoption      |
| Présence aux événements africains (AfricaTech, DevFest, etc.) | Pas de visibilité, pas de réseau        |

### Projections réalistes

- **Pilier régional Afrique francophone** : probable en 3-5 ans si exécution constante
- **Pilier pan-africain** : 7-10 ans, nécessite associés/co-fondateurs en Afrique anglophone
- **Pilier mondial** : 15-20 ans, nécessite une levée de fonds ou un sponsor stratégique

---

<a id="5-strategies-reussite"></a>

## 5. Stratégies de réussite sans les barrières des grandes puissances

### Il n'y a pas de secret magique

Les "secrets" des grandes puissances sont surtout :

- **Accumulation patrimoniale** sur 30-100 ans (Samsung 50 ans, Tencent 25 ans)
- **Capital de risque** disponible et investi tôt
- **Réseaux d'influence** académiques + politiques + médiatiques
- **Cluster effect** (Silicon Valley, Shenzhen, Tel Aviv)

Aucun de ces facteurs n'est rapidement reproductible en Afrique. Mais il existe des **stratégies vraiment efficaces** pour contourner cela.

### Les 10 vrais leviers

#### 1. Refuser de jouer leur jeu

Ne pas copier la Silicon Valley (financement VC = retour 10x exigé sur 5 ans). Nous sommes financés par notre patience, pas leur impatience.

#### 2. Open-source = arme stratégique

Sans capital marketing, c'est le SEUL moyen de gagner en visibilité gratuitement. Les contributeurs deviennent ambassadeurs. Les universités adoptent gratuitement → forment leurs étudiants → ces étudiants deviennent vos utilisateurs en entreprise.

#### 3. Niche africaine d'abord

- Langues africaines (Lingala, Wolof, Bambara, Fulfulde, Swahili) : TTS, STT, NLP. Personne d'autre ne le fait sérieusement.
- Contextes locaux : Mobile Money, marchés informels, agriculture pluviale, infrastructure intermittente.
- **Avantage défensif** : un GAFAM ne peut pas concurrencer sur ces niches sans embaucher des Africains. Et c'est cher pour lui.

#### 4. Pan-africain plutôt que national

- Marché camerounais seul = trop petit (28M habitants, PIB/hab modeste).
- Fédérer : Côte d'Ivoire, Sénégal, Kenya, Nigeria, Afrique du Sud, Rwanda, Maroc, Égypte.
- 1.4 milliard d'habitants en 2025, classe moyenne tech-éduquée en explosion.

#### 5. Diaspora reverse-flow

- 20-30 millions d'Africains à l'étranger, dont des seniors tech (Google, Microsoft, Meta).
- Beaucoup veulent contribuer "à distance" mais ne savent pas où.
- Avoir un projet visible + crédible = vous devenez le canal naturel.
- LinkedIn est l'arme principale.

#### 6. Patience générationnelle

- Ne pas viser "exit en 5 ans". Viser "infrastructure stable en 30 ans".
- Cette patience est en réalité un **avantage** : les VCs forcent la vitesse, vous pouvez choisir la qualité et la solidité.

#### 7. Lean radical

- Pas de bureau coûteux, pas d'équipe avant le revenu réel.
- Travailler là où vous êtes (Yaoundé, Cameroun). Le télétravail africain fonctionne très bien.
- Chaque dollar économisé = un mois de plus pour vous = un mois de plus que le concurrent qui a brûlé son cash.

#### 8. Cumul des casquettes

- Étudiant + enseignant + CEO simultanément = stratégie, pas fardeau.
- Crédibilité académique + revenus stables + indépendance entrepreneuriale.
- Les grandes puissances ne peuvent pas reproduire cela (chez eux, ces rôles sont séparés par les institutions).

#### 9. Persister 10 ans = devenir incontournable

90% des projets africains meurent par épuisement / dispersion / découragement. Tenir 10 ans en continuant à livrer = vous serez quasiment seul sur votre créneau. **C'est ça le vrai moat défensif.**

#### 10. Personne ne donne la permission

- Vous n'avez pas besoin que la Silicon Valley valide.
- Vous n'avez pas besoin qu'un VC investisse.
- Vous n'avez pas besoin qu'un GAFAM recrute.
- Vous construisez. C'est tout. Le monde voit à un moment donné, ou pas. La construction reste.

### Ce que les grandes puissances n'auront jamais


| Avantage                   | Pourquoi nous l'avons et eux non                                    |
| -------------------------- | ------------------------------------------------------------------- |
| Authenticité africaine    | Pas reproductible par embauche                                      |
| Réseau ENSPY              | Construit sur 7 ans de présence (étudiant→enseignant→chercheur) |
| Contexte local pratique    | Vivre les problèmes que les apps doivent résoudre                 |
| Patience générationnelle | Pas de pression VC, pas d'exit forcé                               |
| Cohérence du fondateur    | Une seule personne avec une vision, pas un comité                  |

---

<a id="6-business-plan"></a>

## 6. Plan d'affaires complet

### 6.1 Vision

> Faire de Rihen Universe le premier pilier d'infrastructure logicielle africaine, fournissant aux Africains et à l'humanité des outils ouverts, performants et culturellement enracinés pour construire le futur numérique du continent.

### 6.2 Mission

Construire, documenter et distribuer un écosystème complet de technologies (moteur 3D, build system, librairies systèmes, jeux, simulateurs, applications IA) :

- **100% conçu en Afrique**
- **Open-source ou licence permissive** pour favoriser l'adoption
- **Documenté en français et anglais**
- **Adapté aux contraintes africaines** (hardware modeste, connexion intermittente, contextes locaux)

### 6.3 Produits & services par secteur

#### A. Jeux vidéo & divertissement


| Produit                       | Description                                                               | Timeline         | Revenus estimés (Y3)         |
| ----------------------------- | ------------------------------------------------------------------------- | ---------------- | ----------------------------- |
| **Pong Ultra Arena**          | Pong moderne, multijoueur LAN/Internet                                    | 2026 (V1 livré) | 5-20 k $/an (premium + skins) |
| **Série Mini-jeux Rihen**    | 5-10 titres casual africains (Awalé, Songo, jeux d'origine du continent) | 2026-2028        | 30-100 k $/an                 |
| **Jeu narratif AAA africain** | RPG/aventure inspiré de la mythologie bantoue/yoruba                     | 2028-2031        | Variable (objectif 1M-10M $)  |

#### B. Éducation & formation


| Produit                       | Description                                                                         | Timeline  | Revenus estimés (Y3)                   |
| ----------------------------- | ----------------------------------------------------------------------------------- | --------- | --------------------------------------- |
| **Nkentseu Academy**          | Plateforme de cours gratuits + payants (C++, 3D, IA)                                | 2026-2027 | 20-80 k $/an                            |
| **Simulateurs pédagogiques** | Physique, chimie, biologie, math en 3D interactif (lycées/universités africaines) | 2026-2028 | Licences B2B universités 50-200 k $/an |
| **Bootcamp Nkentseu**         | Formation intensive 3-6 mois, Yaoundé + en ligne                                   | 2027      | 100-500 k $/an si volume                |

#### C. Santé


| Produit                       | Description                                                    | Timeline                               | Revenus estimés                                |
| ----------------------------- | -------------------------------------------------------------- | -------------------------------------- | ----------------------------------------------- |
| **PV3DE**                     | Patient virtuel 3D pour la formation médicale                 | Recherche 2025-2027, produit 2027-2029 | Licences hôpitaux universitaires 50-200 k $/an |
| **Apps santé contextuelles** | Suivi grossesse, vaccination, maladies tropicales en visuel 3D | 2028+                                  | Partenariats Ministère/ONG (UNICEF, OMS)       |

#### D. Industrie & jumeaux numériques


| Produit                 | Description                                               | Timeline | Revenus estimés                        |
| ----------------------- | --------------------------------------------------------- | -------- | --------------------------------------- |
| **Nkentseu Industrial** | Moteur de visualisation pour usines / mines / cimenteries | 2028+    | B2B licences 100k-1M $ par déploiement |
| **Apps de monitoring**  | Dashboards 3D pour processus industriels                  | 2027+    | Contrats annuels 20-100 k $ par client  |

#### E. Espace & aéronautique


| Produit                                | Description                                                             | Timeline  | Revenus estimés                           |
| -------------------------------------- | ----------------------------------------------------------------------- | --------- | ------------------------------------------ |
| **CamSat Sim**                         | Simulateur de mission CamSat-1 (formation + vulgarisation)              | 2027      | Subvention ASE-CAM + freemium grand public |
| **Sim formation ingénieurs spatiaux** | Suite pour les agences africaines (orbites, propulsion, communications) | 2028-2030 | Licences agences 200k-1M $                 |

#### F. Cinéma, animation, médias


| Produit                    | Description                                                                                          | Timeline | Revenus estimés                           |
| -------------------------- | ---------------------------------------------------------------------------------------------------- | -------- | ------------------------------------------ |
| **Nkentseu Realtime**      | Moteur de rendu temps réel pour animateurs africains (alternative Unreal pour productions modestes) | 2029+    | Licences studios 20-100 k $ par production |
| **Capture mocap low-cost** | Mocap webcam pour studios africains                                                                  | 2030+    | Logiciel à l'unité 500-5000 $            |

#### G. Architecture, BTP, immobilier


| Produit                        | Description                                                             | Timeline | Revenus estimés               |
| ------------------------------ | ----------------------------------------------------------------------- | -------- | ------------------------------ |
| **Nkentseu Visualizer**        | Visite virtuelle de plans architecturaux pour cabinets africains        | 2028+    | SaaS 50-500 $/mois par cabinet |
| **Plateforme immobilière 3D** | Visites virtuelles standardisées pour agences immobilières africaines | 2029+    | Commission % sur transactions  |

#### H. Tourisme & patrimoine culturel


| Produit                   | Description                                                                             | Timeline | Revenus estimés                           |
| ------------------------- | --------------------------------------------------------------------------------------- | -------- | ------------------------------------------ |
| **Patrimoine 3D Afrique** | Reconstitutions 3D de sites historiques (Bénin, Mali Tombouctou, Zimbabwe ruins, etc.) | 2028+    | Partenariats UNESCO/musées + tourism apps |
| **Guides interactifs**    | Apps touristiques 3D + AR pour les capitales africaines                                 | 2029+    | Apps freemium 0-5 $ + commissions          |

#### I. Agriculture, pêche, ressources naturelles


| Produit                         | Description                                                                       | Timeline | Revenus estimés                               |
| ------------------------------- | --------------------------------------------------------------------------------- | -------- | ---------------------------------------------- |
| **Sim agronomique**             | Simulation cultures (rendement, climat, fertilisation) pour formation et research | 2029+    | Subventions ministères agriculture 50-500 k $ |
| **Drones agricoles + Nkentseu** | Logiciel embarqué + analytics 3D pour drones d'épandage/surveillance            | 2030+    | Partenariats matériels 100k-1M $/an           |

#### J. Finance & Mobile Money


| Produit                        | Description                                                                      | Timeline | Revenus estimés               |
| ------------------------------ | -------------------------------------------------------------------------------- | -------- | ------------------------------ |
| **Nkentseu UI Banking**        | Composants UI haute performance pour apps bancaires africaines (low-end devices) | 2027+    | Licences banques 50-200 k $/an |
| **Visualisation transactions** | Dashboards Mobile Money 3D pour Orange Money / MTN MoMo / Wave                   | 2028+    | Partenariats opérateurs       |

#### K. Infrastructure IA


| Produit                 | Description                                                               | Timeline | Revenus estimés                 |
| ----------------------- | ------------------------------------------------------------------------- | -------- | -------------------------------- |
| **Nkentseu AI Runtime** | Inférence locale ONNX/GGUF dans le moteur (NPCs intelligents, apps IA)   | 2027+    | Open-source + support enterprise |
| **NkServe**             | Serveur d'inférence léger pour PME africaines (fine-tuning + hosting)   | 2028+    | SaaS 10-1000 $/mois              |
| **Stack IA souveraine** | Outils pour déployer LLM en local (langues africaines, contextes locaux) | 2029+    | Contrats États / ministères    |

#### L. Télécommunications & réseaux


| Produit                     | Description                                                                                | Timeline | Revenus estimés                         |
| --------------------------- | ------------------------------------------------------------------------------------------ | -------- | ---------------------------------------- |
| **NKNetwork Pro**           | Middleware réseau AAA (TCP/UDP/WS/P2P STUN/TURN/ICE) sous licence pour télécoms et jeux | 2027+    | Licences 50-500 k $/an                   |
| **Réseaux d'événements** | Solutions de streaming low-latency pour conférences africaines                            | 2028+    | Contrats événementiel 10-100 k $/event |

#### M. Recherche scientifique


| Produit               | Description                                                              | Timeline | Revenus estimés                |
| --------------------- | ------------------------------------------------------------------------ | -------- | ------------------------------- |
| **Outils chercheurs** | Visualisation de datasets, simulations, papers reproductibles            | 2027+    | Bourses + licences universités |
| **HPC light**         | Calcul distribué pour universités africaines (NKThreading + NKNetwork) | 2029+    | Subventions recherche           |

#### N. Outillage développeur (Jenga as a Service)


| Produit              | Description                                            | Timeline            | Revenus estimés               |
| -------------------- | ------------------------------------------------------ | ------------------- | ------------------------------ |
| **Jenga Open**       | Build system gratuit, open-source                      | 2026 (déjà actif) | $0 — captation d'utilisateurs |
| **Jenga Cloud**      | CI/CD cross-platform en cloud pour devs africains      | 2028+               | SaaS 5-50 $/mois par dev       |
| **Jenga Enterprise** | Support, formation, intégration pour grosses équipes | 2029+               | Contrats 20-200 k $/an         |

### 6.4 Marché cible

#### Marchés prioritaires

1. **Afrique francophone** (Cameroun, Côte d'Ivoire, Sénégal, RDC, Bénin, Togo, Burkina, Mali, Niger, Tchad, Gabon, Congo, Madagascar) — 250M habitants, 50M tech-actifs
2. **Afrique anglophone** (Nigeria, Kenya, Ghana, Afrique du Sud, Ouganda, Rwanda, Éthiopie, Tanzanie) — 800M habitants, 100M tech-actifs
3. **Afrique arabophone** (Maroc, Algérie, Tunisie, Égypte) — 200M habitants, marché fortement édité
4. **Diaspora africaine** (États-Unis, France, Royaume-Uni, Canada, Belgique) — 30M expats high-income

#### Marchés secondaires (Y5+)

- Caraïbes (Haïti, Martinique, Guadeloupe — francophonie)
- Amérique du Sud (Brésil — afrobrésilité)
- Asie (Inde — partenariats sud-sud)

### 6.5 Modèle économique multi-source


| Source                                             | Type         | Récurrent ? | Y3 Estimation  |
| -------------------------------------------------- | ------------ | ------------ | -------------- |
| Dons & sponsoring (SupporterScene)                 | Communauté  | Oui          | 5-30 k $/an    |
| Licences B2B (universités, hôpitaux, télécoms) | Enterprise   | Oui          | 100-500 k $/an |
| SaaS (Jenga Cloud, NkServe)                        | Subscription | Oui          | 50-300 k $/an  |
| Vente directe jeux/apps                            | Consumer     | Oui          | 30-150 k $/an  |
| Formation/bootcamp                                 | Service      | Saisonnier   | 100-500 k $/an |
| Subventions recherche (PV3DE et autres)            | Public       | Ponctuel     | 50-300 k $     |
| Contrats sur mesure (intégration custom)          | Service      | Variable     | 50-500 k $/an  |

**Diversification volontaire** : Aucun revenu ne dépasse 30% du total. Si l'un s'effondre, les autres tiennent.

### 6.6 Positionnement concurrentiel


| Concurrent             | Force                           | Notre angle vs eux                                                              |
| ---------------------- | ------------------------------- | ------------------------------------------------------------------------------- |
| Unity / Unreal         | Maturité, écosystème énorme | Open-source, 0 royalties, contextes africains, langues                          |
| Godot                  | Open-source mature              | Stack système complet (pas que moteur), made in Africa, écosystème intégré |
| Unity for Mobile India | Communauté locale Inde         | Communauté locale Afrique, francophonie                                        |
| Microsoft / Google     | Plateformes cloud globales      | Pas de dépendance, souveraineté logicielle africaine                          |

**Notre différenciation principale** : Aucun autre acteur ne combine (a) moteur 3D + (b) build system + (c) librairies système + (d) origine africaine + (e) langue française. C'est une niche défendable longtemps.

### 6.7 Stratégie go-to-market

#### Phase 1 (2026) — Fondation & crédibilité

- Pong V1 distribué (livré ✅)
- README + docs ARCHITECTURE traduites bilingue
- 10 LinkedIn posts/mois pour visibilité (build in public)
- Présentation NDC Yaoundé, conférences universités africaines
- 100 stars GitHub Nkentseu, 30 stars Jenga

#### Phase 2 (2027) — Premiers clients pilotes

- 2-3 universités africaines pilotes (ENSPY, INPHB, UCAD)
- 1 ONG ou ministère pilote (sim pédagogique)
- 5-10 contributeurs externes Nkentseu/Jenga
- Premier titre commercial (Awalé Online ou similaire)
- Revenus cumulés : 30-100 k $

#### Phase 3 (2028-2029) — Échelle régionale

- 10-20 clients B2B Afrique francophone
- Bootcamp Nkentseu première promo (20-30 étudiants)
- PV3DE produit fini, déploiement hospitalier
- Partenariat ASE-CAM ou autre agence
- Revenus annuels : 200-500 k $

#### Phase 4 (2030+) — Pan-africain

- Bureaux ou correspondants : Abidjan, Lagos, Nairobi, Cape Town
- 50+ clients B2B
- Premier jeu narratif AAA en production
- Levée de fonds (si pertinent) pour accélérer
- Revenus annuels : 1-5 M $

### 6.8 Équipe & gouvernance

#### Actuel (mai 2026)

- **Rodolf Séderis TEUGUIA TADJUIDJE (Rihen)** — CEO, CTO, lead dev, enseignant. Solo founder.

#### Recrutements prioritaires (Y1-Y2)

1. **Dev senior C++/3D** (1) — partenariat ou co-fondateur idéalement, sinon junior à former
2. **Designer UI/UX** (1, part-time) — pour les jeux et apps grand public
3. **Community manager** (1, part-time) — animer LinkedIn + GitHub + Discord
4. **Sales/business dev** (1, Y2-Y3) — démarcher les clients B2B universités/ONG

#### Gouvernance future

- SARL Cameroun (déjà enregistrée — Rihen Universe)
- Conseil consultatif (Y2) : 3-5 mentors (1 académique, 1 tech senior diaspora, 1 business africain, 1 international)
- Possibilité Foundation pour le code open-source (Y3+)

### 6.9 KPIs à suivre


| KPI                    | Y1 (2026) | Y3 (2028)   | Y5 (2030) |
| ---------------------- | --------- | ----------- | --------- |
| GitHub stars Nkentseu  | 200       | 2 000       | 10 000    |
| Contributeurs externes | 5         | 30          | 150       |
| Téléchargements Pong | 1 000     | 50 000      | 500 000   |
| Clients B2B actifs     | 0         | 10          | 50        |
| Revenus annuels        | < 10 k $  | 200-500 k $ | 1-5 M $   |
| Équipe (ETP)          | 1         | 5-8         | 20-40     |

---

<a id="7-roadmap-produits-future"></a>

## 7. Ce que nous pouvons créer demain avec ce qui est fait aujourd'hui

### Chaque module actuel ouvre un produit/marché futur

#### NKCore + NKMath + NKMemory + NKContainers + NKPlatform

**Aujourd'hui** : foundation C++ moderne, cross-platform, performante.
**Demain** :

- Vendre la stack à des entreprises africaines qui veulent du C++ moderne sans réinventer la roue
- Base pour outils HPC légers (calcul scientifique africain)
- Base pour drivers embarqués (drones, robots, IoT africain)

#### NKNetwork

**Aujourd'hui** : UDP/TCP/handshake, lobby Pong fonctionnel.
**Demain** :

- Middleware AAA (TCP/WS/P2P STUN/TURN/ICE) → licence aux studios de jeux africains
- Serveurs de matchmaking pour eSports africains
- VPN P2P léger (souveraineté réseau africaine)
- Streaming low-latency pour événements (concerts, conférences)
- Backend pour Mobile Money inter-opérateurs

#### NKImage

**Aujourd'hui** : codec PNG/JPG/BMP/TGA/QOI/WebP/SVG/HDR/GIF/PPM/ICO production-ready.
**Demain** :

- Suite de traitement d'images pour photographes africains (alternative légère à Photoshop)
- Pipeline d'images pour réseaux sociaux africains (compression intelligente bande passante limitée)
- Composant intégré aux apps Mobile Money pour la lecture de cartes d'identité, factures

#### NKFont + NKUI

**Aujourd'hui** : rendu de texte multi-tailles + UI immediate-mode.
**Demain** :

- Support typographies africaines (N'Ko, Tifinagh, Amharique, Geez)
- IDE éducatif fr/en pour apprendre la programmation (alternative à VSCode pour low-end devices)
- Framework UI pour apps administratives africaines (e-gouvernement)

#### NKRenderer (Vulkan + GL + GLES)

**Aujourd'hui** : PBR, IBL, shadows, materials système.
**Demain** :

- Moteur de rendu pour studios d'animation africains (alternative Unreal pour productions modestes)
- Moteur de visualisation industriel (jumeaux numériques d'usines/mines)
- Moteur AR/VR pour patrimoine culturel + tourisme
- Renderer pour CAO architecte (visites de plans en 3D temps réel)

#### NKContext (OpenGL/Vulkan/D3D/Metal)

**Aujourd'hui** : abstraction graphique multi-API.
**Demain** :

- Compatible avec toutes les configurations africaines (devices low-end Mali/Adreno, vieux GPU)
- Tableaux de bord industriels haute performance

#### NKEvent + NKWindow

**Aujourd'hui** : fenêtre + input cross-platform Windows/Linux/macOS/Android/iOS/Web.
**Demain** :

- Framework pour kiosques tactiles africains (banques, gares, hôpitaux)
- Support gamepads africains low-cost
- Plateforme pour bornes interactives publiques

#### Jenga (build system)

**Aujourd'hui** : 7 plateformes, packaging MSI/EXE/ZIP/APK/AAB/IPA/DEB/PKG/DMG.
**Demain** :

- **Jenga Cloud** : CI/CD pour devs africains à 5-50 $/mois (concurrent low-cost de GitHub Actions/Circle CI)
- **Jenga Enterprise** : support + intégrations entreprise (à terme remplacer CMake/Bazel pour les équipes africaines)
- Outil de scolarité (étudiants compilent leurs projets sans installer 50 outils)
- Plateforme de distribution d'apps africaines (alternative légère à Steam pour les jeux africains)

#### NKAudio (à venir)

**Demain** :

- Librairie audio pour les apps musicales africaines (Spotify-like pour la musique locale)
- Mocap audio pour podcasts africains
- Synthèse vocale fait pour les langues africaines (TTS Wolof, Lingala, Fulfulde, Swahili)

#### Pong + futurs jeux

**Aujourd'hui** : Pong Ultra Arena V1.
**Demain** :

- Mini-jeux casual africains : Awalé Online, Songo Online, Yele Online (avec leaderboards pan-africains)
- Roi du Foot Africain — simulateur de carrière footballeur africain (CAN, championnats nationaux)
- Tata Bantou — RPG narratif inspiré de la mythologie bantoue, marketing africain et international

#### PV3DE (sujet recherche)

**Demain** :

- Produit fini pour les facultés de médecine africaines (alternatives aux solutions occidentales chères)
- Extension aux pathologies tropicales (formation paludisme, drépanocytose, fièvre Lassa, Ebola)
- Adaptable à d'autres formations professionnelles (police, pompiers, douaniers — formation par simulation)

### Synergies entre modules = effets de levier

```
NKCore + NKMath + NKMemory  ──┐
                              ├─→ HPC scientifique africain
NKThreading + NKNetwork  ─────┘

NKRenderer + NKContext + NKWindow  ──┐
                                     ├─→ Suite création 3D + visualisation
NKImage + NKFont + NKUI  ────────────┘

Tout ce qui précède + Jenga  ────────→ Écosystème complet déployable partout

+ Pong / PV3DE / autres apps  ───────→ Vitrine + revenus + crédibilité
```

---

<a id="8-risques"></a>

## 8. Risques & mitigation


| Risque                                            | Probabilité | Impact   | Mitigation                                                                                 |
| ------------------------------------------------- | ------------ | -------- | ------------------------------------------------------------------------------------------ |
| Épuisement du fondateur (burnout)                | Élevée     | Critique | Triple casquette = revenus stables, pas pression VC, pace générationnel                  |
| Concurrence Unity/Unreal qui baisse les royalties | Moyenne      | Moyen    | Notre angle reste open-source + africanité, leur baisse de prix ne change pas notre niche |
| Pas de contributeurs externes en 18 mois          | Moyenne      | Élevé  | Documentation FR, formation ENSPY, présence événements, build in public                 |
| Pas de premiers clients B2B en 2027               | Moyenne      | Élevé  | Démarchage actif universités/ONG, partenariats facultés                                 |
| Régulations africaines imprévisibles            | Faible       | Faible   | SARL Cameroun stable, diversification revenus pan-africaine                                |
| Brain drain (associés partis)                    | Moyenne      | Moyen    | Co-fondateurs avec ancrage africain réel, pas juste revenu                                |
| Concurrence locale (autres dev camerounais)       | Faible       | Faible   | Avance technique de 3-5 ans, communauté à fédérer plutôt qu'à combattre              |
| Hardware/électricité instable                   | Élevée     | Moyen    | Travail offline-first, backups multiples (NAS local + cloud), groupe électrogène         |
| Connexion internet africaine                      | Élevée     | Moyen    | Code optimisé pour bande passante limitée, mirrors locaux Github                         |
| Cybersécurité (vol de code)                     | Faible       | Élevé  | Code public open-source = pas de secret à voler ; brand est la valeur                     |

---

<a id="9-financement"></a>

## 9. Besoins en financement

### Bootstrap (2026) — 0-5 k $

**Source actuelle** : revenus enseignement + dons SupporterScene + économies personnelles.
**Suffisant pour** : tenir 12-18 mois en mode solo lean.

### Seed (2027-2028) — 50-200 k $

**Usages** :

- Salaires : 1-2 dev senior (24 mois)
- Marketing : conférences africaines, communication
- Infrastructure : 1 VPS Hetzner + outils SaaS minimum
- Opérations : SARL fiscalité, juridique, comptabilité

**Sources potentielles** :

- Subvention recherche (PV3DE → bourse master/thèse + équipement)
- Subvention culture/numérique (Ministère de l'Économie Numérique Cameroun, Agence Universitaire de la Francophonie)
- Don philanthropique (Mo Ibrahim Foundation, Tony Elumelu Foundation)
- Programme accélérateur (Orange Digital Center, MEST Africa, AfricaInk)
- Revenus services + licences B2B

### Series A (2029-2030) — 500 k $ — 2 M $ (si pertinent)

**Usages** :

- Équipe 5-8 ETP
- Bureaux Yaoundé + Abidjan
- 2-3 produits commerciaux lancés
- Marketing pan-africain

**Sources potentielles** :

- VC africain (Partech Africa, TLcom Capital, Norrsken22)
- Fonds souverain ou stratégique (Smart Africa, AfDB)
- Co-investissement diaspora (LP via Africinvest)
- ⚠️ **Non recommandé** : VC Silicon Valley classique (incompatibilité philosophique)

### Long terme — auto-financement

**Objectif** : à partir de Y5 (2030+), être profitable et croître sur fonds propres ou JV stratégiques (pas de VC traditionnel).

---

## Annexe A — Indicateurs externes 2026 à suivre

- **Croissance tech africaine** : Disrupt Africa Funding Report (annuel)
- **Adoption mobile** : GSMA Mobile Economy Africa
- **Investissements VC Afrique** : Briter Bridges, Partech Africa Tech Venture Capital Report
- **Concurrents directs** : recherche annuelle "moteur 3D Afrique", "build system Africa"
- **Communauté tech** : Africa Tech Festival (Cape Town), DevFest Africa, GITEX Africa, AfricaInk

## Annexe B — Mantras opérationnels

- "Livrer chaque semaine. Pas chaque mois."
- "Documenter en français en priorité. L'anglais peut attendre."
- "Open-source est notre marketing."
- "Pas d'employé avant le revenu. Pas de bureau avant l'équipe."
- "Tenir 10 ans est notre moat."
- "Le succès se mesure en livrables, pas en ronds de jambes."

---

*Document vivant — mis à jour à chaque révision majeure de la stratégie.*

*Rédigé pour Rihen Universe par Rodolf Séderis TEUGUIA TADJUIDJE.*

*Yaoundé, Cameroun — 20 mai 2026.*
