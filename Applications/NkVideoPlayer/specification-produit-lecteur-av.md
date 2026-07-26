# Spécification Produit — Lecteur Audio/Vidéo Multiplateforme Intelligent

**Version :** 0.2 (ajout du mode "Drama")
**Date :** 21 juillet 2026
**Statut :** Draft à valider

---

## 1. Vision du produit

Créer un lecteur audio/vidéo moderne, capable de diffuser du contenu en streaming à toute qualité (basse à très haute résolution), en plusieurs langues, sur tout type d'appareil (PC, smartphone, tablette, TV connectée), avec des fonctionnalités boostées à l'IA, et permettant le visionnage collectif à distance (les utilisateurs regardent ensemble en synchronisation, même sur des appareils et dans des lieux différents).

**Proposition de valeur en une phrase :**
> Un seul lecteur, n'importe quel appareil, n'importe où, ensemble — avec l'IA qui améliore l'expérience.

---

## 2. Objectifs produit

| Objectif | Description |
|---|---|
| O1 | Lecture fluide et adaptative de contenus audio/vidéo, quelle que soit la résolution source |
| O2 | Support natif du multilingue (audio + sous-titres) |
| O3 | Fonctionnalités IA à valeur ajoutée (voir §4.3) |
| O4 | Diffusion en ligne (streaming) sans nécessiter de téléchargement complet |
| O5 | Visionnage synchronisé à plusieurs, à distance, cross-device |
| O6 | Expérience unifiée et adaptative sur PC, mobile, tablette (responsive + multiplateforme) |

---

## 3. Public cible

- Particuliers voulant regarder du contenu personnel ou streamé (films, séries, cours, podcasts vidéo)
- Groupes d'amis / familles voulant regarder ensemble à distance ("watch party")
- Créateurs/plateformes voulant intégrer un lecteur robuste et multilingue
- Utilisateurs internationaux nécessitant plusieurs langues audio/sous-titres
- Utilisateurs avec des connexions internet et appareils hétérogènes (du smartphone bas de gamme au PC haut de gamme)

---

## 4. Fonctionnalités clés

### 4.1 Lecture multi-résolution et streaming adaptatif

- **Streaming adaptatif (ABR)** : ajustement automatique de la qualité (résolution/bitrate) selon la bande passante et les capacités de l'appareil, via des protocoles standards :
  - HLS (HTTP Live Streaming)
  - DASH (Dynamic Adaptive Streaming over HTTP)
- **Multi-résolution disponible** : de 240p/360p (basse résolution, faible bande passante) jusqu'à 4K/8K HDR selon la source et l'appareil.
- **Transcodage à la volée ou pré-encodage** côté serveur pour générer plusieurs profils de qualité.
- **Sélection manuelle ou automatique** de la qualité par l'utilisateur.
- **Lecture audio pure** (mode podcast/musique) avec gestion optimisée de la bande passante (pas de flux vidéo inutile).
- **Mise en cache / lecture hors-ligne** partielle pour les zones à connexion instable.

### 4.2 Multi-langue

- **Pistes audio multiples** : sélection de la langue de la piste audio (doublage) si disponible dans le contenu source.
- **Sous-titres multiples** : plusieurs langues de sous-titres, activables/désactivables, avec personnalisation (taille, police, couleur, position).
- **Détection automatique de la langue préférée** de l'utilisateur (paramètres profil / langue du système).
- **Sous-titres générés automatiquement par IA** (voir §4.3) pour les contenus n'ayant pas de sous-titres natifs.

### 4.3 Fonctionnalités boostées à l'IA

| Fonctionnalité | Description |
|---|---|
| Sous-titres automatiques | Génération de sous-titres via reconnaissance vocale (speech-to-text) dans la langue d'origine |
| Traduction automatique des sous-titres | Traduction des sous-titres générés ou existants vers la langue préférée de l'utilisateur |
| Doublage IA (optionnel, phase avancée) | Synthèse vocale pour doubler automatiquement un contenu dans une autre langue |
| Amélioration de qualité (upscaling) | Amélioration de la résolution perçue d'une vidéo basse qualité via un modèle d'upscaling |
| Résumé de contenu | Génération d'un résumé automatique (ex. résumé d'un épisode, chapitrage intelligent) |
| Recherche sémantique dans la vidéo | Recherche d'un passage précis via une requête en langage naturel ("montre-moi le moment où...") |
| Recommandations personnalisées | Suggestions de contenu basées sur l'historique et les préférences |
| Détection de scène / chapitrage automatique | Découpage automatique en chapitres/scènes |
| Modération de contenu | Détection automatique de contenu sensible (à des fins de filtrage parental, par exemple) |

> Ces fonctionnalités IA doivent être **modulaires** : activables/désactivables individuellement, et pensées pour fonctionner en tâche de fond (asynchrone) sans bloquer la lecture.

### 4.4 Visionnage en ligne (streaming)

- Lecture directe depuis un serveur/CDN, sans téléchargement préalable obligatoire.
- Reprise de lecture (résumer où l'utilisateur s'est arrêté), synchronisée entre appareils via le compte utilisateur.
- Gestion de la mise en mémoire tampon (buffering) intelligente et prédictive.

### 4.5 Visionnage à plusieurs (watch party / co-visionnage à distance)

- **Sessions partagées** : création d'une "salle" de visionnage accessible via un lien ou un code.
- **Synchronisation en temps réel** de la lecture (play/pause/seek) entre tous les participants, indépendamment de leur localisation ou de leur appareil.
- **Chat / réactions en direct** pendant le visionnage (texte, émojis, éventuellement audio/vidéo des participants en incrustation).
- **Tolérance à l'hétérogénéité des appareils** : chaque participant peut recevoir une qualité différente adaptée à sa connexion, tout en restant synchronisé dans le temps de lecture.
- **Gestion des rôles** : hôte (contrôle la lecture) vs invités (lecture seule ou droits partagés, configurable).
- **Reconnexion automatique** en cas de coupure réseau d'un participant.

### 4.6 Multiplateforme et adaptatif

- **Responsive design** : l'interface s'adapte automatiquement à la taille d'écran et au type d'appareil (PC, smartphone, tablette, TV).
- **Applications cibles** :
  - Web (navigateur, via une PWA si possible pour limiter la duplication de code)
  - Application mobile (iOS / Android)
  - Application desktop (Windows / macOS / Linux)
  - Support TV connectée (optionnel, phase ultérieure)
- **Continuité d'usage cross-device** : reprise de lecture et synchronisation des préférences (langue, sous-titres, qualité) via un compte utilisateur cloud.
- **Contrôles adaptés au dispositif** : tactile sur mobile/tablette, clavier/souris sur PC, télécommande sur TV.

### 4.7 Mode "Drama" — découpage temporel avec paiement à la minute (option)

Ce mode est une **option de monétisation par contenu**, indépendante et complémentaire du reste du produit. Elle ne concerne que certains contenus (typiquement des films/séries récents proposés par la plateforme) et **ne remplace jamais** la lecture libre des fichiers audio/vidéo personnels de l'utilisateur (stockés localement ou dans son espace personnel) décrite dans les sections précédentes, qui reste entièrement disponible sans restriction de ce type.

**Principe général :**
- Chaque contenu éligible à ce mode est découpé en segments temporels.
- Un premier segment (ex. les **12 premières minutes** — valeur configurable par contenu, définie par l'éditeur/la plateforme, pas une constante figée) est **gratuit** et accessible à tout utilisateur, sans compte payant.
- Au-delà de ce segment gratuit, deux options d'accès sont proposées :
  1. **Paiement à la minute / au segment consommé** : l'utilisateur paie uniquement pour les minutes supplémentaires qu'il visionne réellement (micro-paiement à l'usage).
  2. **Déblocage complet du contenu** : l'utilisateur paie un montant forfaitaire unique pour débloquer l'intégralité du film/de la vidéo au-delà du segment gratuit, sans limite de visionnage ultérieur.

**Caractéristiques fonctionnelles :**

| Élément | Description |
|---|---|
| Segment gratuit configurable | Durée définie au cas par cas par contenu (ex. 12 min, 5 min, 20 min...), paramétrable côté back-office |
| Compteur de consommation | Suivi précis du temps réellement visionné au-delà du seuil gratuit (pour le paiement à la minute) |
| Palier de paiement à la minute | Grille tarifaire par minute ou par tranche de minutes (ex. tous les X minutes = un micro-paiement) |
| Déblocage total | Achat unique donnant un accès illimité et permanent (ou pour une durée définie, type location) au contenu complet |
| Prévisualisation claire | Indicateur visuel dans la timeline montrant la portion gratuite vs. la portion payante avant même de lancer la lecture |
| Alerte de fin de gratuité | Notification/overlay à l'approche de la fin du segment gratuit, proposant les options de paiement |
| Reprise après paiement | Une fois payé (à la minute ou en déblocage complet), la lecture continue sans interruption |
| Historique d'achats | L'utilisateur retrouve les contenus déjà débloqués/payés dans son profil, sans repayer |
| Indépendance du contenu personnel | Ce mécanisme ne s'applique jamais aux fichiers/contenus personnels de l'utilisateur (bibliothèque privée, mémoire locale/cloud personnelle) |

**Cas d'usage type :**
> Un utilisateur veut regarder un film récemment sorti disponible sur la plateforme en mode "Drama". Il visionne gratuitement les 12 premières minutes. À la fin de ce segment, un écran lui propose soit de continuer en payant minute par minute, soit de débloquer le film entier pour un prix fixe. Pendant ce temps, il peut toujours, sans aucune restriction, lire ses propres films et musiques stockés dans sa bibliothèque personnelle.

**Points d'attention :**
- Nécessite un système de paiement intégré (micro-transactions + achats uniques), avec gestion des moyens de paiement, de la facturation et des remboursements.
- Le découpage doit être défini au niveau du **catalogue/contenu**, pas au niveau du lecteur lui-même : le lecteur doit simplement savoir lire les métadonnées de segmentation et de prix fournies par le backend.
- Cette fonctionnalité concerne uniquement le **catalogue en ligne / contenus de la plateforme** (ex. derniers films sortis), pas le contenu personnel de l'utilisateur.
- Prévoir une politique de test A/B sur la durée du segment gratuit et la tarification, ces paramètres étant amenés à évoluer par contenu ou par campagne commerciale.

---

## 5. Architecture technique (vue d'ensemble)

```
[Client Web / Mobile / Desktop]
        │
        ▼
[API Gateway / BFF] ──── Auth & gestion des comptes
        │
        ├──► [Service Streaming] ── CDN / origine média (HLS/DASH)
        ├──► [Service Watch Party] ── WebSocket / temps réel (sync lecture, chat)
        ├──► [Service IA] ── STT, traduction, upscaling, recommandations
        ├──► [Service Transcodage] ── pipeline d'encodage multi-résolution
        ├──► [Service Catalogue & Segmentation] ── métadonnées de découpage, seuils gratuits, tarifs
        ├──► [Service Paiement] ── micro-paiements à la minute, achats uniques, historique
        └──► [Base de données] ── profils, historique, préférences, métadonnées, bibliothèque personnelle
```

> Le contenu personnel de l'utilisateur (bibliothèque privée) ne transite jamais par le Service Catalogue & Segmentation ni par le Service Paiement : ces services ne s'appliquent qu'au contenu proposé par la plateforme en mode "Drama".

**Points d'attention techniques :**
- Protocole de streaming : HLS/DASH pour compatibilité large et ABR natif.
- Synchronisation temps réel : WebSocket ou WebRTC pour la faible latence du watch party.
- Player vidéo : s'appuyer sur des lecteurs éprouvés (ex. moteur type `hls.js`/`dash.js`/`ExoPlayer`/`AVPlayer` selon plateforme) plutôt que réinventer un moteur de décodage.
- Pipeline IA : traitement asynchrone (queue de tâches) pour ne pas impacter la latence de lecture.
- CDN pour la distribution du contenu à l'échelle mondiale.

---

## 6. Exigences non fonctionnelles

| Catégorie | Exigence |
|---|---|
| Performance | Démarrage de lecture < 2s, changement de qualité fluide sans coupure perceptible |
| Scalabilité | Support de sessions de watch party avec un nombre variable de participants |
| Disponibilité | Cible de disponibilité du service de streaming ≥ 99,5% |
| Sécurité | Authentification sécurisée, chiffrement des flux (DRM optionnel selon contenu protégé) |
| Accessibilité | Sous-titres, contraste réglable, navigation clavier, compatibilité lecteurs d'écran |
| Confidentialité | Consentement explicite pour les données utilisées par les modèles IA (ex. recommandations) |
| Compatibilité | Fonctionnement sur connexions de bande passante variable (dégradation gracieuse) |
| Fiabilité des paiements | Comptage précis et infalsifiable du temps consommé (mode Drama), transactions sécurisées (conformité type PCI-DSS) |

---

## 7. Parcours utilisateur (exemples)

1. **Visionnage solo** : l'utilisateur ouvre l'app → sélectionne un contenu → le lecteur choisit automatiquement la meilleure qualité disponible → l'utilisateur peut changer la langue audio/sous-titres à tout moment.
2. **Reprise cross-device** : l'utilisateur arrête un film sur son smartphone → le reprend automatiquement au même endroit sur sa tablette.
3. **Watch party** : un utilisateur crée une session → partage un lien → ses amis rejoignent depuis leurs propres appareils, où qu'ils soient → la lecture est synchronisée pour tous, avec chat en direct.
4. **Sous-titres IA** : l'utilisateur regarde un contenu sans sous-titres → active "sous-titres automatiques" → l'IA génère et traduit les sous-titres en temps quasi réel.
5. **Mode Drama** : l'utilisateur choisit un film récent du catalogue → regarde gratuitement le segment initial (ex. 12 min) → à la fin de ce segment, il choisit de payer à la minute ou de débloquer le film entier → la lecture continue sans coupure. En parallèle, sa bibliothèque personnelle reste accessible librement, sans aucun paiement.

---

## 8. Roadmap indicative (par phases)

**Phase 1 — MVP**
- Lecture vidéo/audio en streaming adaptatif (ABR)
- Support multi-résolution basique
- Sous-titres et pistes audio multiples (contenu déjà multilingue)
- Application web responsive

**Phase 2 — Multiplateforme**
- Applications mobile et desktop natives
- Synchronisation cross-device (reprise de lecture, préférences)

**Phase 3 — Social**
- Watch party (synchronisation multi-utilisateurs + chat)
- Gestion des rôles (hôte/invités)

**Phase 4 — IA**
- Sous-titres automatiques + traduction
- Recommandations personnalisées
- Chapitrage automatique / résumés

**Phase 5 — Avancé**
- Upscaling IA
- Doublage IA
- Support TV connectée / autres écrans

**Phase 6 — Monétisation "Drama"**
- Service Catalogue & Segmentation (définition des seuils gratuits/payants par contenu)
- Service Paiement (micro-paiement à la minute + déblocage forfaitaire)
- Indicateurs visuels de segmentation dans la timeline du lecteur
- Historique des contenus débloqués dans le profil utilisateur

---

## 9. Risques et contraintes identifiés

- **Coût du transcodage et de l'IA** : le traitement multi-résolution et les modèles IA (STT, traduction, upscaling) sont coûteux en calcul — nécessite une stratégie d'infrastructure claire (cloud, coûts variables).
- **Droits de contenu** : gestion des DRM si diffusion de contenus protégés.
- **Latence du watch party** : garantir une synchronisation acceptable malgré des connexions hétérogènes.
- **Complexité multiplateforme** : maintenir la cohérence UX sur PC/mobile/tablette avec des équipes ou du temps de dev limités (envisager une base de code partagée : PWA, Flutter, React Native, etc. — à trancher lors du choix technique).
- **Mode Drama** : risque de friction utilisateur si la limite gratuite/tarification est mal calibrée ; nécessite une gestion rigoureuse de la fraude (contournement du compteur de minutes) et une conformité aux réglementations locales sur les paiements et abonnements.

---

## 10. Métriques de succès (KPIs à définir précisément)

- Temps de démarrage moyen de la lecture
- Taux de rebuffering (interruptions)
- Taux d'utilisation des fonctionnalités IA
- Nombre moyen de participants par session watch party
- Taux de rétention cross-device
- Satisfaction utilisateur (NPS)

---

## 11. Prochaines étapes

- [ ] Valider le périmètre du MVP avec les parties prenantes
- [ ] Choisir la stack technique définitive (front multiplateforme, moteur de streaming, fournisseur IA)
- [ ] Définir le modèle économique (gratuit/payant, quotas IA, etc.)
- [ ] Réaliser des maquettes UX pour chaque type d'appareil
- [ ] Prioriser les fonctionnalités IA selon effort/valeur

---

*Ce document est un premier brouillon destiné à être affiné avec les parties prenantes (produit, tech, design) avant le lancement du développement.*
