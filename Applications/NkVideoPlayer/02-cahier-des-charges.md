# Cahier des Charges — Lecteur Audio/Vidéo Multiplateforme Intelligent

**Version :** 1.0
**Date :** 21 juillet 2026
**Documents liés :** `specification-produit-lecteur-av.md`, `01-roadmap-complete.md`

---

## 1. Contexte et présentation du projet

Le projet consiste à concevoir et développer un lecteur audio/vidéo capable de :
- diffuser des contenus en streaming à toute résolution (basse à très haute définition),
- gérer nativement le multilingue (audio et sous-titres),
- intégrer des fonctionnalités boostées à l'intelligence artificielle,
- permettre un visionnage collectif synchronisé à distance,
- fonctionner de manière adaptative sur tout type d'appareil (PC, smartphone, tablette),
- proposer, en option, un modèle de monétisation par segmentation temporelle ("mode Drama") sur les contenus du catalogue en ligne, sans jamais restreindre l'accès à la bibliothèque personnelle de l'utilisateur.

## 2. Objectifs du projet

| Réf. | Objectif |
|---|---|
| OBJ-01 | Offrir une expérience de lecture fluide et adaptative, quelle que soit la résolution source ou la bande passante disponible |
| OBJ-02 | Garantir un accès multilingue complet (pistes audio + sous-titres) |
| OBJ-03 | Différencier le produit par des fonctionnalités IA à forte valeur ajoutée |
| OBJ-04 | Permettre la diffusion en ligne (streaming) sans téléchargement obligatoire |
| OBJ-05 | Permettre un visionnage synchronisé à plusieurs, à distance, cross-device |
| OBJ-06 | Assurer une expérience unifiée et adaptée sur PC, mobile et tablette |
| OBJ-07 | Proposer un modèle de monétisation optionnel par segmentation temporelle sur le catalogue en ligne |

## 3. Parties prenantes

| Rôle | Responsabilité |
|---|---|
| Product Owner | Priorisation fonctionnelle, arbitrages produit |
| Équipe UX/UI | Conception des interfaces (voir cahiers de spécification d'interface) |
| Équipe Backend | Streaming, transcodage, IA, paiement, synchronisation temps réel |
| Équipe Frontend/Mobile/Desktop | Applications clientes multiplateformes |
| Équipe Data/IA | Modèles de sous-titrage, traduction, recommandation, upscaling |
| Équipe Juridique/Conformité | DRM, RGPD, paiement, fiscalité |
| Utilisateurs finaux | Testeurs et bénéficiaires du produit |

## 4. Périmètre du projet

### 4.1 Dans le périmètre (In scope)
- Lecteur audio/vidéo avec streaming adaptatif multi-résolution
- Gestion multilingue (audio + sous-titres, y compris générés par IA)
- Fonctionnalités IA : sous-titrage auto, traduction, recommandations, recherche sémantique, chapitrage, résumé, upscaling, doublage
- Visionnage collectif synchronisé à distance (watch party)
- Applications web, mobile (iOS/Android), desktop (Windows/macOS/Linux)
- Continuité d'usage cross-device
- Bibliothèque personnelle de contenus (fichiers propres à l'utilisateur)
- Mode "Drama" : segmentation temporelle, paiement à la minute, déblocage forfaitaire (catalogue en ligne uniquement)

### 4.2 Hors périmètre (Out of scope, sauf mention contraire ultérieure)
- Production ou acquisition de contenu (le projet fournit le lecteur/la plateforme, pas le contenu lui-même, sauf accords de diffusion à définir séparément)
- Support des TV connectées en phase MVP (prévu en phase avancée, cf. roadmap)
- Doublage IA en temps réel pour le direct/live streaming (non prioritaire au lancement)

## 5. Exigences fonctionnelles

### 5.1 Lecture et streaming

| Réf. | Exigence |
|---|---|
| EF-001 | Le système doit diffuser les contenus via streaming adaptatif (HLS et/ou DASH) |
| EF-002 | Le système doit proposer plusieurs profils de qualité (ex. basse, moyenne, haute, très haute résolution) |
| EF-003 | Le système doit ajuster automatiquement la qualité selon la bande passante disponible |
| EF-004 | L'utilisateur doit pouvoir sélectionner manuellement la qualité de lecture |
| EF-005 | Le système doit permettre la lecture audio seule (mode podcast/musique) sans charger de flux vidéo |
| EF-006 | Le système doit permettre la reprise de lecture au point exact où l'utilisateur s'est arrêté |
| EF-007 | Le système doit permettre la mise en cache partielle pour lecture en connexion instable |

### 5.2 Multilingue

| Réf. | Exigence |
|---|---|
| EF-010 | Le système doit permettre la sélection de la piste audio parmi les langues disponibles |
| EF-011 | Le système doit permettre la sélection de sous-titres parmi plusieurs langues |
| EF-012 | L'utilisateur doit pouvoir personnaliser l'affichage des sous-titres (taille, police, couleur, position) |
| EF-013 | Le système doit détecter automatiquement la langue préférée de l'utilisateur par défaut |

### 5.3 Intelligence artificielle

| Réf. | Exigence |
|---|---|
| EF-020 | Le système doit pouvoir générer automatiquement des sous-titres à partir de l'audio (speech-to-text) |
| EF-021 | Le système doit pouvoir traduire automatiquement les sous-titres générés ou existants |
| EF-022 | Le système doit proposer un doublage automatique par synthèse vocale (fonctionnalité optionnelle, activable) |
| EF-023 | Le système doit pouvoir améliorer la résolution perçue d'un contenu basse qualité (upscaling) |
| EF-024 | Le système doit pouvoir générer un résumé automatique d'un contenu |
| EF-025 | Le système doit permettre une recherche sémantique en langage naturel dans un contenu |
| EF-026 | Le système doit proposer des recommandations personnalisées basées sur l'historique |
| EF-027 | Le système doit pouvoir découper automatiquement un contenu en chapitres/scènes |
| EF-028 | Chaque fonctionnalité IA doit être activable/désactivable indépendamment par l'utilisateur |
| EF-029 | Les traitements IA doivent s'exécuter de manière asynchrone, sans bloquer la lecture |

### 5.4 Visionnage collectif (Watch Party)

| Réf. | Exigence |
|---|---|
| EF-030 | Le système doit permettre de créer une session de visionnage partagée |
| EF-031 | Le système doit synchroniser en temps réel les actions de lecture (play/pause/seek) entre tous les participants |
| EF-032 | Le système doit permettre à chaque participant de recevoir une qualité vidéo adaptée à sa propre connexion, sans désynchronisation |
| EF-033 | Le système doit proposer un chat ou des réactions en direct pendant le visionnage |
| EF-034 | Le système doit gérer un rôle "hôte" avec droits de contrôle de la lecture et un rôle "invité" |
| EF-035 | Le système doit permettre une reconnexion automatique en cas de coupure réseau d'un participant |

### 5.5 Multiplateforme

| Réf. | Exigence |
|---|---|
| EF-040 | L'interface doit s'adapter automatiquement au type d'appareil (PC, smartphone, tablette) |
| EF-041 | Le système doit être disponible en version web |
| EF-042 | Le système doit être disponible en application mobile (iOS et Android) |
| EF-043 | Le système doit être disponible en application desktop (Windows, macOS, Linux) |
| EF-044 | Le système doit synchroniser les préférences utilisateur (langue, sous-titres, qualité) entre tous les appareils |
| EF-045 | Les contrôles doivent s'adapter au mode d'interaction du dispositif (tactile, clavier/souris, télécommande) |

### 5.6 Bibliothèque personnelle

| Réf. | Exigence |
|---|---|
| EF-050 | L'utilisateur doit pouvoir importer/ajouter ses propres fichiers audio/vidéo |
| EF-051 | L'accès à la bibliothèque personnelle ne doit jamais être soumis au modèle de paiement du mode "Drama" |

### 5.7 Mode "Drama" (monétisation par segmentation)

| Réf. | Exigence |
|---|---|
| EF-060 | Le système doit permettre de définir, par contenu du catalogue, un segment initial gratuit de durée configurable |
| EF-061 | Le système doit proposer un paiement à la minute au-delà du segment gratuit |
| EF-062 | Le système doit proposer un déblocage forfaitaire complet du contenu |
| EF-063 | Le système doit afficher visuellement, dans la timeline, la portion gratuite et la portion payante avant lecture |
| EF-064 | Le système doit notifier l'utilisateur avant la fin du segment gratuit |
| EF-065 | Le système doit conserver un historique des contenus débloqués/achetés par l'utilisateur |
| EF-066 | Le système doit empêcher le contournement frauduleux du comptage du temps consommé |

## 6. Exigences non fonctionnelles

| Réf. | Exigence |
|---|---|
| ENF-001 | Le démarrage de la lecture doit intervenir en moins de 2 secondes dans des conditions réseau normales |
| ENF-002 | Le changement de qualité (ABR) ne doit pas provoquer de coupure perceptible |
| ENF-003 | La disponibilité du service de streaming doit être ≥ 99,5 % |
| ENF-004 | Les flux doivent être chiffrés ; un système DRM doit être disponible pour les contenus protégés |
| ENF-005 | L'interface doit respecter des standards d'accessibilité (sous-titres, contraste, navigation clavier, compatibilité lecteurs d'écran) |
| ENF-006 | Le consentement de l'utilisateur doit être recueilli explicitement pour toute donnée utilisée par les modèles IA |
| ENF-007 | Le système doit gérer une dégradation gracieuse en cas de bande passante faible |
| ENF-008 | Les transactions de paiement doivent être sécurisées et conformes aux standards en vigueur (ex. PCI-DSS) |
| ENF-009 | Le système doit être scalable pour supporter un nombre variable de participants en watch party |

## 7. Contraintes

### 7.1 Contraintes techniques
- Utilisation de protocoles de streaming standards (HLS/DASH) pour garantir la compatibilité large.
- Nécessité d'une infrastructure CDN pour la distribution mondiale.
- Pipeline IA devant fonctionner en asynchrone pour ne pas dégrader la latence de lecture.

### 7.2 Contraintes légales et réglementaires
- Conformité RGPD (ou équivalent local) pour les données personnelles et les usages IA.
- Gestion des droits de diffusion / DRM pour les contenus protégés.
- Conformité aux réglementations sur les paiements en ligne et micro-transactions selon les marchés visés.

### 7.3 Contraintes budgétaires et organisationnelles
- Coût du transcodage multi-résolution et des traitements IA à anticiper dès la Phase 0 (cf. roadmap).
- Ressources de développement multiplateforme à dimensionner (choix d'une base de code partagée recommandé).

## 8. Livrables attendus

- Cahier des charges (ce document)
- Roadmap complète (`01-roadmap-complete.md`)
- Cahier de spécification des interfaces — version humaine
- Cahier de spécification des interfaces — version progressive pour outil de design IA (Banani)
- Design system / maquettes validées
- Applications web, mobile, desktop fonctionnelles selon les phases de la roadmap
- Documentation technique d'architecture

## 9. Critères d'acceptation généraux

- Chaque exigence fonctionnelle (EF-xxx) doit être démontrable via un scénario de test reproductible.
- Chaque exigence non fonctionnelle (ENF-xxx) doit être mesurable via un indicateur défini (temps, taux, disponibilité...).
- Le mode "Drama" ne doit jamais restreindre l'accès à la bibliothèque personnelle : ce point doit faire l'objet d'un test de non-régression systématique.
- L'expérience watch party doit rester fonctionnelle même si les participants ont des qualités vidéo différentes.

## 10. Glossaire

| Terme | Définition |
|---|---|
| ABR | Adaptive Bitrate — ajustement automatique de la qualité de streaming selon la bande passante |
| HLS / DASH | Protocoles standards de streaming adaptatif |
| Watch Party | Session de visionnage collectif synchronisé à distance |
| Mode Drama | Modèle de monétisation par segmentation temporelle (segment gratuit + paiement à la minute ou déblocage) |
| STT | Speech-to-Text — reconnaissance vocale |
| Upscaling | Amélioration de la résolution perçue d'un contenu par IA |
| DRM | Digital Rights Management — gestion des droits numériques |
| Cross-device | Continuité d'usage entre plusieurs appareils différents |

---

*Ce cahier des charges doit être relu et validé formellement par l'ensemble des parties prenantes avant le démarrage de la Phase 0 de la roadmap.*
