# Roadmap Complète du Système — Lecteur Audio/Vidéo Multiplateforme Intelligent

**Version :** 1.0
**Date :** 21 juillet 2026
**Document lié :** `specification-produit-lecteur-av.md`, `02-cahier-des-charges.md`

---

## 1. Objectif du document

Cette roadmap découpe l'ensemble du projet en phases séquentielles et/ou parallélisables, avec pour chacune : objectifs, livrables, jalons de validation, dépendances et estimation d'effort relative (indicative, à affiner avec l'équipe technique).

Échelle d'effort utilisée : **XS / S / M / L / XL** (relative, pas en jours fixes — à caler lors du cadrage).

---

## 2. Vue d'ensemble des phases

| Phase | Nom | Objectif principal | Effort |
|---|---|---|---|
| 0 | Cadrage & Design | Valider le périmètre, choisir la stack, poser les fondations UX/UI | S |
| 1 | MVP Streaming | Lecture adaptative multi-résolution + multilingue de base | L |
| 2 | Multiplateforme | Applications mobile, desktop, continuité cross-device | L |
| 3 | Social / Watch Party | Visionnage synchronisé à plusieurs, à distance | M |
| 4 | Intelligence Artificielle | Sous-titres auto, traduction, recommandations, recherche sémantique | L |
| 5 | Fonctionnalités avancées | Upscaling IA, doublage IA, support TV connectée | L |
| 6 | Monétisation "Drama" | Segmentation payante, micro-paiements, déblocage | M |
| 7 | Scale & International | Optimisation infra, multi-région, conformité légale globale | M |
| 8 | Amélioration continue | Itérations post-lancement basées sur les données d'usage | Continu |

---

## 3. Détail par phase

### Phase 0 — Cadrage & Design
**Objectifs :**
- Valider le périmètre fonctionnel du MVP
- Choisir la stack technique (front multiplateforme, moteur de streaming, hébergement, fournisseur IA)
- Réaliser les maquettes UX pour chaque type d'appareil (PC, mobile, tablette)
- Définir l'architecture technique cible

**Livrables :**
- Cahier des charges validé
- Maquettes basse et moyenne fidélité (wireframes) des écrans principaux
- Choix technologique documenté (ADR — Architecture Decision Records)
- Design system initial (couleurs, typographie, composants de base)

**Jalon de sortie :** Validation du périmètre MVP par les parties prenantes.

**Dépendances :** Aucune (point de départ).

---

### Phase 1 — MVP Streaming
**Objectifs :**
- Lecture vidéo/audio en streaming adaptatif (ABR — HLS/DASH)
- Support multi-résolution (basse à haute définition selon la source)
- Pistes audio et sous-titres multiples (pour contenu déjà multilingue nativement)
- Application web responsive (PC + mobile en navigateur)
- Lecture de la bibliothèque personnelle (fichiers de l'utilisateur)

**Livrables :**
- Lecteur web fonctionnel avec ABR
- Pipeline de transcodage multi-résolution basique
- Gestion de compte utilisateur (inscription, connexion, profil)
- Bibliothèque personnelle (upload / import de fichiers personnels)

**Jalon de sortie :** Démo fonctionnelle — lecture fluide d'un contenu en plusieurs qualités et langues, sur navigateur web.

**Dépendances :** Phase 0.

---

### Phase 2 — Multiplateforme
**Objectifs :**
- Application mobile native (iOS / Android) ou PWA avancée
- Application desktop (Windows / macOS / Linux)
- Continuité d'usage cross-device (reprise de lecture, synchronisation des préférences)

**Livrables :**
- Apps mobile et desktop publiées (bêta interne)
- Synchronisation cloud des profils, historique, préférences (langue, sous-titres, qualité)
- Contrôles adaptés à chaque type d'appareil (tactile, clavier/souris, télécommande à prévoir en phase 5)

**Jalon de sortie :** Un utilisateur peut démarrer un contenu sur un appareil et le reprendre exactement où il s'est arrêté sur un autre.

**Dépendances :** Phase 1.

---

### Phase 3 — Social / Watch Party
**Objectifs :**
- Création de sessions de visionnage partagées ("salles")
- Synchronisation temps réel de la lecture (play/pause/seek) entre participants distants
- Chat / réactions en direct
- Gestion des rôles (hôte / invités)

**Livrables :**
- Service de synchronisation temps réel (WebSocket/WebRTC)
- Interface de création/rejoindre une salle
- Chat intégré au lecteur
- Gestion de la reconnexion automatique

**Jalon de sortie :** Plusieurs utilisateurs, sur des appareils et réseaux différents, regardent le même contenu en restant synchronisés à quelques centaines de millisecondes près.

**Dépendances :** Phase 1, Phase 2 (pour la synchronisation cross-device).

---

### Phase 4 — Intelligence Artificielle
**Objectifs :**
- Génération automatique de sous-titres (speech-to-text)
- Traduction automatique des sous-titres
- Recommandations personnalisées
- Recherche sémantique dans la vidéo
- Chapitrage / détection de scène automatique
- Résumé de contenu

**Livrables :**
- Pipeline IA asynchrone (file de traitement, ne bloque pas la lecture)
- Intégration des sous-titres générés dans le lecteur
- Moteur de recommandation basé sur l'historique
- Interface de recherche sémantique ("trouver le moment où...")

**Jalon de sortie :** Un contenu sans sous-titres natifs peut être visionné avec des sous-titres générés et traduits automatiquement, avec une latence acceptable.

**Dépendances :** Phase 1 (contenu et infrastructure de lecture disponibles).

---

### Phase 5 — Fonctionnalités avancées
**Objectifs :**
- Amélioration de qualité vidéo par IA (upscaling)
- Doublage automatique par IA (synthèse vocale multilingue)
- Support des TV connectées et grands écrans

**Livrables :**
- Module d'upscaling intégré au pipeline de transcodage
- Module de doublage IA (expérimental, opt-in)
- Application ou interface adaptée TV (navigation télécommande)

**Jalon de sortie :** Un contenu basse résolution peut être visionné en qualité améliorée ; un contenu peut être doublé automatiquement dans une langue non disponible nativement.

**Dépendances :** Phase 4 (briques IA), Phase 2 (multiplateforme).

---

### Phase 6 — Monétisation "Drama"
**Objectifs :**
- Segmentation temporelle des contenus du catalogue (seuil gratuit configurable)
- Paiement à la minute au-delà du seuil gratuit
- Déblocage forfaitaire complet d'un contenu
- Indicateurs visuels de segmentation dans le lecteur

**Livrables :**
- Service Catalogue & Segmentation (back-office pour définir seuils et tarifs par contenu)
- Service Paiement (micro-paiement + achat unique, historique des achats)
- UI de transition fin-de-gratuité (overlay de proposition de paiement)
- Système anti-fraude sur le comptage du temps consommé

**Jalon de sortie :** Un utilisateur peut regarder gratuitement un segment initial d'un contenu du catalogue, puis payer à la minute ou débloquer l'intégralité, sans impact sur l'accès à sa bibliothèque personnelle.

**Dépendances :** Phase 1 (lecteur), Phase 2 (comptes utilisateurs multiplateformes).

---

### Phase 7 — Scale & International
**Objectifs :**
- Optimisation de l'infrastructure de diffusion (CDN multi-région)
- Conformité légale par région (RGPD, taxes, moyens de paiement locaux)
- Support de langues d'interface supplémentaires (localisation de l'app elle-même, au-delà du contenu)

**Livrables :**
- Déploiement multi-région
- Localisation complète de l'interface (i18n)
- Conformité juridique validée par marché cible

**Jalon de sortie :** Le produit est accessible et conforme dans plusieurs marchés/régions cibles.

**Dépendances :** Phases 1 à 6 stabilisées.

---

### Phase 8 — Amélioration continue
**Objectifs :**
- Analyse des données d'usage (rétention, rebuffering, usage IA, conversion Drama)
- Itérations UX basées sur les retours utilisateurs
- Optimisation continue des coûts IA et infrastructure

**Livrables :**
- Tableaux de bord analytics
- Cycles d'itération réguliers (sprints post-lancement)

**Jalon de sortie :** Processus d'amélioration continue en place (pas de fin de phase — récurrent).

**Dépendances :** Lancement public (fin Phase 6/7 a minima).

---

## 4. Vue chronologique indicative (Gantt simplifié)

```
Phase 0  |███|
Phase 1  |    ███████|
Phase 2  |           ██████|
Phase 3  |                 █████|
Phase 4  |           ████████████|
Phase 5  |                       ██████|
Phase 6  |                 ██████████|
Phase 7  |                             █████|
Phase 8  |                                   ████████████████ (continu)
```
*Diagramme indicatif — les phases 4 et 6 peuvent démarrer en parallèle de la phase 3 dès que la Phase 1 est stabilisée, sous réserve des ressources disponibles.*

---

## 5. Jalons majeurs (Milestones)

| Jalon | Description | Phase associée |
|---|---|---|
| M1 | Premier streaming adaptatif fonctionnel (démo interne) | Fin Phase 1 |
| M2 | Application mobile/desktop en bêta fermée | Fin Phase 2 |
| M3 | Première watch party réussie entre testeurs externes | Fin Phase 3 |
| M4 | Sous-titres IA générés et traduits en production | Fin Phase 4 |
| M5 | Premier contenu "Drama" testé avec paiement réel | Fin Phase 6 |
| M6 | Lancement public multi-région | Fin Phase 7 |

---

## 6. Risques transverses impactant la roadmap

- Dépendance à des fournisseurs IA tiers (coût, disponibilité, latence) pouvant retarder la Phase 4.
- Complexité réglementaire des paiements (Phase 6) pouvant varier fortement selon les marchés visés (Phase 7).
- Charge de maintenance multiplateforme (Phase 2) pouvant ralentir les phases suivantes si la base de code n'est pas suffisamment mutualisée.

---

*Cette roadmap est indicative et doit être affinée avec des estimations de charge réelles une fois l'équipe et la stack technique définies (voir Phase 0).*
