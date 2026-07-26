# Cahier de Spécification des Interfaces — Version Humaine

**Version :** 1.0
**Date :** 21 juillet 2026
**Documents liés :** `specification-produit-lecteur-av.md`, `02-cahier-des-charges.md`
**Public visé :** équipe produit, UX/UI, développement front/mobile/desktop

---

## 1. Objectif du document

Ce document décrit, écran par écran, dialogue par dialogue, menu par menu, l'ensemble de l'interface du produit : son objectif, ses éléments constitutifs, les actions possibles, les états et la navigation associée. Il sert de référence fonctionnelle pour la conception UI et le développement front-end, toutes plateformes confondues (web, mobile, desktop).

**Principe d'adaptation :** chaque écran est pensé "responsive/adaptatif" par défaut. Les variations spécifiques par type d'appareil (PC, smartphone, tablette) sont précisées quand elles diffèrent significativement de la version de référence.

---

## 2. Cartographie générale des écrans

```
Démarrage
 ├─ Splash / Onboarding
 ├─ Authentification (Connexion / Inscription / Mot de passe oublié)
 └─ Accueil (Home)
     ├─ Recherche
     ├─ Fiche Contenu
     │   └─ Lecteur (Player)
     │       ├─ Menu Qualité
     │       ├─ Menu Langue & Sous-titres
     │       ├─ Menu Vitesse de lecture
     │       ├─ Menu Chapitres
     │       ├─ Menu IA (sous-titres auto, résumé, recherche sémantique)
     │       ├─ Overlay Mode Drama (fin de segment gratuit)
     │       └─ Panneau Watch Party (chat, participants)
     ├─ Watch Party (créer / rejoindre une salle)
     ├─ Bibliothèque personnelle
     ├─ Profil & Paramètres
     │   ├─ Compte
     │   ├─ Préférences de lecture
     │   ├─ Accessibilité
     │   ├─ Moyens de paiement & historique
     │   └─ Notifications
     └─ Notifications (centre de notifications)
```

---

## 3. Écran — Splash / Onboarding

**Objectif :** Présenter brièvement le produit à un nouvel utilisateur et orienter vers l'inscription/connexion.

**Éléments UI :**
- Logo et nom du produit
- 2 à 4 slides d'introduction (streaming multi-résolution, multilingue, watch party, mode Drama)
- Boutons "Créer un compte" / "Se connecter"
- Lien "Continuer sans compte" (si navigation limitée autorisée)

**Actions :**
- Passer les slides (swipe / flèches / points de pagination)
- Aller vers Authentification

**États :**
- Première visite (onboarding complet affiché)
- Visite ultérieure (onboarding ignoré, redirection directe vers Accueil ou Authentification)

**Navigation :** → Authentification ou → Accueil

**Variations par device :**
- Mobile/Tablette : swipe horizontal entre slides
- PC : navigation par flèches/boutons, layout plus large avec illustrations

---

## 4. Écran — Authentification

### 4.1 Connexion
**Éléments UI :** champ identifiant/email, champ mot de passe, bouton "Se connecter", lien "Mot de passe oublié", lien "Créer un compte", boutons de connexion via services tiers (optionnel).

### 4.2 Inscription
**Éléments UI :** champ nom/pseudo, email, mot de passe (+ confirmation), case à cocher CGU/RGPD, bouton "S'inscrire".

### 4.3 Mot de passe oublié
**Éléments UI :** champ email, bouton "Envoyer le lien de réinitialisation", message de confirmation.

**Actions communes :** validation de formulaire, gestion des erreurs (champ requis, format invalide, identifiants incorrects).

**États :** vide / en saisie / erreur / chargement / succès

**Navigation :** → Accueil (après succès) ou reste sur l'écran (en cas d'erreur)

---

## 5. Écran — Accueil (Home)

**Objectif :** Point d'entrée principal, présente le contenu du catalogue et la bibliothèque personnelle.

**Éléments UI :**
- Barre de navigation principale (Accueil, Recherche, Bibliothèque, Watch Party, Profil)
- Bandeau "Continuer à regarder" (reprise cross-device)
- Sections de recommandations (générées par IA)
- Sections par catégorie/genre
- Mise en avant des contenus "Drama" récents (badge "12 min gratuites" ou équivalent configurable)
- Accès rapide à la bibliothèque personnelle

**Actions :**
- Cliquer sur une vignette → Fiche Contenu
- Cliquer sur "Continuer à regarder" → reprise directe dans le Lecteur
- Accéder au menu profil / notifications

**États :**
- Chargement des recommandations (skeleton loader)
- Aucun historique (nouvel utilisateur — affichage de suggestions génériques)
- Hors-ligne (accès limité à la bibliothèque personnelle uniquement)

**Navigation :** hub central vers tous les autres écrans

**Variations par device :**
- PC : grille large multi-colonnes, navigation latérale possible
- Mobile : défilement vertical, navigation basse (tab bar)
- Tablette : grille intermédiaire, navigation latérale ou basse selon orientation

---

## 6. Écran — Recherche

**Éléments UI :**
- Champ de recherche texte
- Suggestions/autocomplétion
- Filtres (genre, langue disponible, durée, type — vidéo/audio, contenu personnel/catalogue)
- Résultats sous forme de grille/liste
- Historique de recherche récent

**Actions :**
- Saisir une requête, appliquer des filtres, sélectionner un résultat

**États :**
- Recherche vide (affiche historique + suggestions populaires)
- Aucun résultat
- Chargement
- Résultats affichés

**Navigation :** → Fiche Contenu

---

## 7. Écran — Fiche Contenu

**Objectif :** Présenter les informations d'un contenu avant lecture et permettre de lancer le visionnage.

**Éléments UI :**
- Image/affiche, titre, description, durée, genre, année
- Langues audio disponibles, langues de sous-titres disponibles
- Indicateur qualité maximale disponible
- Bouton principal "Regarder" / "Écouter"
- Bouton "Ajouter à ma liste"
- Si contenu en mode Drama : indicateur "X minutes gratuites" + information sur le prix de déblocage
- Bouton "Créer une Watch Party avec ce contenu"
- Contenus similaires / recommandés

**Actions :**
- Lancer la lecture → Lecteur
- Ajouter/retirer de la liste
- Créer une session Watch Party directement depuis cette fiche

**États :**
- Contenu jamais visionné / en cours / terminé
- Contenu déjà débloqué (Drama) vs non débloqué

**Navigation :** → Lecteur, → Watch Party (création)

---

## 8. Écran — Lecteur (Player)

**Objectif :** Écran central de lecture, avec l'ensemble des contrôles et menus associés.

### 8.1 Zone de lecture principale
- Vidéo (ou visualisation audio pour le mode audio pur)
- Barre de progression (timeline) avec indicateur de segment gratuit/payant si mode Drama actif
- Contrôles principaux : lecture/pause, avance/retour rapide, volume, plein écran

### 8.2 Barre de contrôles secondaire
- Bouton Qualité → **Menu Qualité**
- Bouton Langue/Sous-titres → **Menu Langue & Sous-titres**
- Bouton Vitesse → **Menu Vitesse de lecture**
- Bouton Chapitres → **Menu Chapitres**
- Bouton IA → **Menu IA**
- Bouton Watch Party (si session active) → **Panneau Watch Party**
- Bouton Paramètres d'affichage (taille sous-titres, etc.)

### 8.3 Menu Qualité
**Éléments :** liste des résolutions disponibles (ex. Auto, 360p, 720p, 1080p, 4K), indicateur de la qualité active.
**Action :** sélection → changement de flux ABR manuel.

### 8.4 Menu Langue & Sous-titres
**Éléments :** liste des pistes audio disponibles, liste des sous-titres disponibles (+ option "Désactivé"), sous-menu personnalisation sous-titres (taille, police, couleur, fond, position).
**Action :** sélection → changement immédiat sans interruption de lecture.

### 8.5 Menu Vitesse de lecture
**Éléments :** liste de vitesses (0.5x, 0.75x, 1x, 1.25x, 1.5x, 2x).

### 8.6 Menu Chapitres
**Éléments :** liste des chapitres/scènes détectés (auto ou manuels), avec vignette et timestamp.
**Action :** sélection → saut direct au chapitre.

### 8.7 Menu IA
**Éléments :**
- Toggle "Sous-titres automatiques" (avec sélection de langue cible)
- Bouton "Résumé du contenu" (ouvre un panneau avec le résumé généré)
- Champ "Rechercher dans la vidéo" (recherche sémantique en langage naturel, retourne une liste de moments avec vignettes)
- Toggle "Amélioration de qualité (upscaling)" si disponible pour ce contenu
- Toggle "Doublage IA" (si activé, sélection de la langue cible)

**États :** traitement IA en cours (indicateur de progression), traitement terminé, erreur de traitement (avec message clair et option de réessayer).

### 8.8 Overlay Mode Drama
**Déclenchement :** approche ou atteinte de la fin du segment gratuit.
**Éléments :**
- Message "Votre segment gratuit se termine dans X secondes" (avertissement, non bloquant)
- À la fin du segment : écran de blocage avec deux options claires :
  - "Continuer en payant à la minute" (avec tarif affiché)
  - "Débloquer le film entier" (avec prix affiché)
- Bouton retour à la Fiche Contenu

**États :** avertissement / blocage / paiement en cours / paiement réussi (reprise automatique de la lecture) / paiement échoué (message d'erreur, réessai possible)

### 8.9 Panneau Watch Party (intégré au lecteur)
**Éléments :**
- Liste des participants connectés (avatar, statut de connexion)
- Zone de chat (messages texte, réactions rapides/émojis)
- Indicateur "Hôte" sur le participant qui contrôle la lecture
- Bouton "Quitter la session" / "Inviter d'autres personnes" (partage de lien)

**Actions (selon rôle) :**
- Hôte : contrôle play/pause/seek, peut transférer le rôle d'hôte
- Invité : lecture seule des contrôles (sauf si droits partagés activés par l'hôte)

**Variations par device pour le Lecteur :**
- PC : contrôles visibles en permanence ou au survol, panneau Watch Party en latéral
- Mobile : contrôles tactiles avec auto-masquage, panneau Watch Party en tiroir (bottom sheet)
- Tablette : hybride, orientation paysage recommandée pour affichage optimal

---

## 9. Écran — Watch Party (création / accueil de salle)

**Objectif :** Permettre de créer ou rejoindre une session de visionnage collectif.

**Éléments UI :**
- Bouton "Créer une salle" (sélection du contenu si non pré-sélectionné depuis la Fiche Contenu)
- Champ "Rejoindre une salle via un code/lien"
- Salle d'attente : liste des participants qui rejoignent, statut "en attente de l'hôte"
- Bouton "Démarrer la session" (visible pour l'hôte uniquement)

**États :**
- Salle en attente
- Salle en cours (redirige vers le Lecteur avec panneau Watch Party actif)
- Salle fermée / expirée

**Navigation :** → Lecteur (avec session active)

---

## 10. Écran — Bibliothèque personnelle

**Objectif :** Gérer les contenus personnels de l'utilisateur (toujours accessibles librement, sans lien avec le mode Drama).

**Éléments UI :**
- Bouton "Importer un fichier" (audio/vidéo)
- Grille/liste des fichiers importés (avec vignette, durée, taille)
- Organisation (dossiers/tags optionnels)
- Barre de recherche interne à la bibliothèque

**Actions :**
- Import, suppression, renommage, lecture directe
- Partage (si activé) pour Watch Party

**États :**
- Bibliothèque vide (message d'invitation à importer)
- Import en cours (barre de progression)
- Erreur d'import (format non supporté, etc.)

---

## 11. Écran — Profil & Paramètres

### 11.1 Compte
- Informations personnelles (nom, email, avatar)
- Changement de mot de passe
- Suppression de compte

### 11.2 Préférences de lecture
- Langue audio par défaut
- Langue de sous-titres par défaut
- Qualité de lecture par défaut (Auto / manuelle)
- Activation par défaut des fonctionnalités IA (sous-titres auto, upscaling, etc.)

### 11.3 Accessibilité
- Taille et style des sous-titres par défaut
- Contraste élevé
- Navigation clavier (PC) / options d'assistance

### 11.4 Moyens de paiement & historique
- Liste des moyens de paiement enregistrés
- Ajout/suppression d'un moyen de paiement
- Historique des transactions (mode Drama : minutes payées, contenus débloqués)

### 11.5 Notifications
- Activation/désactivation par type de notification (reprise de lecture disponible, invitation Watch Party, fin de segment gratuit, etc.)

**Navigation :** accessible depuis l'icône profil, présente sur tous les écrans principaux (barre de navigation)

---

## 12. Centre de notifications

**Éléments UI :**
- Liste chronologique des notifications (invitations Watch Party, nouveautés catalogue, alertes de paiement, etc.)
- Indicateur de notifications non lues

**Actions :** cliquer une notification → redirection vers l'écran concerné (ex. rejoindre une Watch Party)

---

## 13. Dialogues et messages transverses

| Dialogue | Déclencheur | Contenu |
|---|---|---|
| Reprise cross-device | Ouverture d'un contenu déjà visionné ailleurs | "Reprendre où vous étiez ?" (Oui/Non — recommencer) |
| Erreur réseau | Perte de connexion pendant la lecture | Message + tentative de reconnexion automatique |
| Buffering prolongé | Bande passante insuffisante | Indicateur de chargement + proposition de baisser la qualité |
| Confirmation de paiement | Achat mode Drama | Récapitulatif du montant, moyen de paiement, bouton de confirmation |
| Erreur de paiement | Échec transaction | Message clair, bouton "Réessayer" ou "Changer de moyen de paiement" |
| Consentement IA | Première activation d'une fonctionnalité IA utilisant des données utilisateur | Explication courte + acceptation |
| Fin de session Watch Party | L'hôte quitte la session | Notification aux participants restants, proposition de reprendre en solo |

---

## 14. Principes transverses d'adaptation par appareil

| Aspect | PC | Smartphone | Tablette |
|---|---|---|---|
| Navigation principale | Barre latérale ou haute | Barre basse (tab bar) | Barre latérale ou basse selon orientation |
| Contrôles lecteur | Visibles au survol souris | Tactiles, auto-masqués | Tactiles, hybrides |
| Grille de contenus | Multi-colonnes larges | Défilement vertical, 1-2 colonnes | Grille intermédiaire |
| Watch Party (chat) | Panneau latéral fixe | Tiroir (bottom sheet) | Panneau latéral ou tiroir selon orientation |
| Saisie | Clavier/souris | Clavier virtuel tactile | Clavier virtuel tactile |

---

*Ce document doit être utilisé conjointement avec le design system (à produire en Phase 0 de la roadmap) pour la réalisation des maquettes haute-fidélité.*
