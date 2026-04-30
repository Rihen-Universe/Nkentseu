#ifndef NKENTSEU_EVENT_NKEVENTSTATE_H
#define NKENTSEU_EVENT_NKEVENTSTATE_H

#pragma once

// =============================================================================
// Fichier : NkEventState.h
// =============================================================================
// Description :
//   État global consolidé du système d'événements et des entrées au sein du 
//   moteur NkEntseu. Ce fichier centralise deux catégories de fonctionnalités :
//
//   1. ÉTATS SÉMANTIQUES (classes légères) :
//      Petites structures décrivant l'état qualitatif d'un composant particulier :
//        - NkWindowState    : Cycle de vie d'une fenêtre
//        - NkRegionState    : Survol curseur / contact dans une zone
//        - NkConnectionState : Connexion d'un périphérique
//        - NkResizeState    : Nature d'un redimensionnement
//        - NkAxisState      : État qualitatif d'un axe analogique
//        - NkAxisDirection  : Direction sémantique d'un contrôle multi-axe
//        - NkStatusState    : Disponibilité générique d'une ressource
//        - NkDraggedState   : Phase d'une opération de drag & drop
//        - NkFocusState     : État du focus clavier d'une fenêtre
//
//   2. SNAPSHOTS D'ENTRÉE (structs de polling) :
//      Structures de snapshot mis à jour en temps réel par EventSystem :
//        - NkKeyboardInputState : Touches pressées, modificateurs, dernière touche
//        - NkMouseInputState    : Position, boutons, capture, survol
//        - NkTouchInputState    : Contacts tactiles actifs, centroïde
//        - NkGamepadInputState  : État d'une manette individuelle (boutons/axes)
//        - NkGamepadSetState    : Ensemble des slots de manettes (0..N-1)
//        - NkEventState         : Agrégat global de tous les snapshots ci-dessus
//
//      Ces snapshots sont mis à jour par EventSystem en réponse aux événements
//      et peuvent être lus à tout moment par le code applicatif sans créer de
//      couplage avec la boucle d'événements (pattern "polling sans callback").
//
// Dépendances :
//   - NkKeyboardEvent.h : NkKey, NkScancode, NkModifierState
//   - NkMouseEvent.h    : NkMouseButton, NkButtonState, NkMouseButtons
//   - NkTouchEvent.h    : NkTouchPoint, NK_MAX_TOUCH_POINTS
//   - NkGamepadEvent.h  : NkGamepadButton, NkGamepadAxis, NkGamepadInfo
//   - NKMemory/NkUtils.h : memory::NkMemSet pour les réinitialisations
//
// Thread-safety :
//   - Non thread-safe par défaut. Synchronisation externe requise si accès 
//     concurrent depuis plusieurs threads.
//   - Lecture seule recommandée depuis le code applicatif.
//   - Écriture réservée à EventSystem ou aux backends d'entrée.
//
// Auteur : Équipe NkEntseu
// Version : 1.0.0
// Date : 2026
// =============================================================================

    #include "NkKeyboardEvent.h"
    #include "NkMouseEvent.h"
    #include "NkTouchEvent.h"
    #include "NkGamepadEvent.h"
    #include "NKMemory/NkUtils.h"

    namespace nkentseu {
        // =========================================================================
        // Section : États sémantiques qualitatifs (classes légères)
        // =========================================================================

        // -------------------------------------------------------------------------
        // Structure : NkWindowState
        // -------------------------------------------------------------------------
        // Description :
        //   Décrit l'état courant du cycle de vie d'une fenêtre graphique.
        //   Utilisé dans les événements de fenêtre et pour le polling d'état.
        // Valeurs :
        //   - NK_WINDOW_UNDEFINED   : État non initialisé ou inconnu
        //   - NK_WINDOW_CREATED     : Fenêtre créée et visible à l'écran
        //   - NK_WINDOW_MINIMIZED   : Fenêtre réduite dans la barre des tâches
        //   - NK_WINDOW_MAXIMIZED   : Fenêtre agrandie pour occuper l'écran
        //   - NK_WINDOW_FULLSCREEN  : Fenêtre en mode plein écran exclusif
        //   - NK_WINDOW_HIDDEN      : Fenêtre masquée mais non détruite
        //   - NK_WINDOW_CLOSED      : Fenêtre fermée, handle système détruit
        // Utilisation :
        //   - Déclenchement d'actions conditionnelles selon l'état de la fenêtre.
        //   - Adaptation du rendu (pause en minimized, ajustement en fullscreen...).
        // -------------------------------------------------------------------------
        struct NkWindowState {
            using Code = uint32;

            enum Value : Code {
                NK_WINDOW_UNDEFINED = 0,
                NK_WINDOW_CREATED,
                NK_WINDOW_MINIMIZED,
                NK_WINDOW_MAXIMIZED,
                NK_WINDOW_FULLSCREEN,
                NK_WINDOW_HIDDEN,
                NK_WINDOW_CLOSED
            };

            static const char* ToString(Code v) noexcept {
                switch (v) {
                    case NK_WINDOW_CREATED:
                        return "Created";
                    case NK_WINDOW_MINIMIZED:
                        return "Minimized";
                    case NK_WINDOW_MAXIMIZED:
                        return "Maximized";
                    case NK_WINDOW_FULLSCREEN:
                        return "Fullscreen";
                    case NK_WINDOW_HIDDEN:
                        return "Hidden";
                    case NK_WINDOW_CLOSED:
                        return "Closed";
                    default:
                        return "Undefined";
                }
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkRegionState
        // -------------------------------------------------------------------------
        // Description :
        //   Indique la relation spatiale entre un curseur/contact et une zone 
        //   définie (bouton UI, zone de drop, hotspot interactif...).
        // Valeurs :
        //   - NK_REGION_UNDEFINED : Relation non initialisée ou inconnue
        //   - NK_REGION_ENTERED   : Le curseur vient juste d'entrer dans la zone
        //   - NK_REGION_INSIDE    : Le curseur est actuellement à l'intérieur
        //   - NK_REGION_EXITED    : Le curseur vient juste de quitter la zone
        // Utilisation :
        //   - Déclenchement d'effets hover/tooltip à l'entrée.
        //   - Gestion des états visuels des boutons (normal/hover/pressed).
        //   - Détection de drop targets pour le drag & drop.
        // -------------------------------------------------------------------------
        struct NkRegionState {
            using Code = uint32;

            enum Value : Code {
                NK_REGION_UNDEFINED = 0,
                NK_REGION_ENTERED,
                NK_REGION_INSIDE,
                NK_REGION_EXITED
            };

            static const char* ToString(Code v) noexcept {
                switch (v) {
                    case NK_REGION_ENTERED:
                        return "Entered";
                    case NK_REGION_INSIDE:
                        return "Inside";
                    case NK_REGION_EXITED:
                        return "Exited";
                    default:
                        return "Undefined";
                }
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkConnectionState
        // -------------------------------------------------------------------------
        // Description :
        //   Décrit l'état de connexion d'un périphérique d'entrée (manette, 
        //   HID générique, périphérique audio...) au système.
        // Valeurs :
        //   - NK_CONNECTION_UNDEFINED   : État non initialisé ou inconnu
        //   - NK_CONNECTION_ESTABLISHED : Périphérique connecté et opérationnel
        //   - NK_CONNECTION_TERMINATED  : Périphérique déconnecté ou retiré
        // Utilisation :
        //   - Mise à jour de l'interface selon la disponibilité des périphériques.
        //   - Gestion dynamique des slots joueurs pour les manettes.
        //   - Déclenchement de notifications de connexion/déconnexion.
        // -------------------------------------------------------------------------
        struct NkConnectionState {
            using Code = uint32;

            enum Value : Code {
                NK_CONNECTION_UNDEFINED = 0,
                NK_CONNECTION_ESTABLISHED,
                NK_CONNECTION_TERMINATED
            };

            static const char* ToString(Code v) noexcept {
                switch (v) {
                    case NK_CONNECTION_ESTABLISHED:
                        return "Established";
                    case NK_CONNECTION_TERMINATED:
                        return "Terminated";
                    default:
                        return "Undefined";
                }
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkResizeState
        // -------------------------------------------------------------------------
        // Description :
        //   Décrit la nature d'un changement de dimensions d'une fenêtre ou 
        //   d'un framebuffer de rendu.
        // Valeurs :
        //   - NK_RESIZE_UNDEFINED : Changement non initialisé ou inconnu
        //   - NK_RESIZE_EXPANDED  : Nouvelle dimension > ancienne (agrandissement)
        //   - NK_RESIZE_REDUCED   : Nouvelle dimension < ancienne (réduction)
        //   - NK_RESIZE_NO_CHANGE : Dimensions identiques (déplacement seulement)
        // Utilisation :
        //   - Optimisation du re-rendu selon la direction du redimensionnement.
        //   - Adaptation des effets de transition UI (zoom in/out).
        //   - Déclenchement de recalculs de layout conditionnels.
        // -------------------------------------------------------------------------
        struct NkResizeState {
            using Code = uint32;

            enum Value : Code {
                NK_RESIZE_UNDEFINED = 0,
                NK_RESIZE_EXPANDED,
                NK_RESIZE_REDUCED,
                NK_RESIZE_NO_CHANGE
            };

            static const char* ToString(Code v) noexcept {
                switch (v) {
                    case NK_RESIZE_EXPANDED:
                        return "Expanded";
                    case NK_RESIZE_REDUCED:
                        return "Reduced";
                    case NK_RESIZE_NO_CHANGE:
                        return "NoChange";
                    default:
                        return "Undefined";
                }
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkAxisState
        // -------------------------------------------------------------------------
        // Description :
        //   Classification qualitative de la valeur d'un axe analogique.
        //   Permet de déclencher des actions logiques sans seuils codés en dur.
        // Valeurs :
        //   - NK_AXIS_UNDEFINED : État non initialisé ou valeur invalide
        //   - NK_AXIS_NEUTRAL   : Valeur dans la zone morte (|value| < deadzone)
        //   - NK_AXIS_POSITIVE  : Valeur dépasse le seuil positif (+deadzone)
        //   - NK_AXIS_NEGATIVE  : Valeur dépasse le seuil négatif (-deadzone)
        // Méthode utilitaire :
        //   - Classify() : Classe une valeur normalisée [-1,1] avec deadzone configurable
        // Utilisation :
        //   - Détection de direction d'entrée sans gestion manuelle des seuils.
        //   - Simplification des conditions de gameplay (ex: "si stick à droite...").
        //   - Abstraction de la précision analogique pour des inputs binaires.
        // -------------------------------------------------------------------------
        struct NkAxisState {
            using Code = uint32;

            enum Value : Code {
                NK_AXIS_UNDEFINED = 0,
                NK_AXIS_NEUTRAL,
                NK_AXIS_POSITIVE,
                NK_AXIS_NEGATIVE
            };

            static const char* ToString(Code v) noexcept {
                switch (v) {
                    case NK_AXIS_NEUTRAL:
                        return "Neutral";
                    case NK_AXIS_POSITIVE:
                        return "Positive";
                    case NK_AXIS_NEGATIVE:
                        return "Negative";
                    default:
                        return "Undefined";
                }
            }

            static Value Classify(float value, float deadzone = 0.1f) noexcept {
                if (value > deadzone) {
                    return NK_AXIS_POSITIVE;
                }
                if (value < -deadzone) {
                    return NK_AXIS_NEGATIVE;
                }
                return NK_AXIS_NEUTRAL;
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkAxisDirection
        // -------------------------------------------------------------------------
        // Description :
        //   Décrit les dimensions spatiales d'un contrôle analogique multi-axe.
        //   Utile pour l'abstraction des schémas de contrôle complexes.
        // Valeurs :
        //   - NK_AXIS_DIR_UNDEFINED          : Direction non spécifiée
        //   - NK_AXIS_DIR_HORIZONTAL         : 1D horizontal (axe X seul)
        //   - NK_AXIS_DIR_VERTICAL           : 1D vertical (axe Y seul)
        //   - NK_AXIS_DIR_DEPTH              : 1D profondeur (axe Z seul)
        //   - NK_AXIS_DIR_HORIZONTAL_VERTICAL : 2D XY (stick analogique standard)
        //   - NK_AXIS_DIR_HORIZONTAL_DEPTH   : 2D XZ (contrôle de vol/simulation)
        //   - NK_AXIS_DIR_VERTICAL_DEPTH     : 2D YZ (contrôle spécialisé)
        //   - NK_AXIS_DIR_ALL                : 3D XYZ (contrôle spatial complet)
        // Utilisation :
        //   - Configuration dynamique des schémas de contrôle par type de périphérique.
        //   - Adaptation des prompts UI selon la dimensionalité des contrôles.
        //   - Validation des mappings axe → action dans les options de contrôle.
        // -------------------------------------------------------------------------
        struct NkAxisDirection {
            using Code = uint32;

            enum Value : Code {
                NK_AXIS_DIR_UNDEFINED = 0,
                NK_AXIS_DIR_HORIZONTAL,
                NK_AXIS_DIR_VERTICAL,
                NK_AXIS_DIR_DEPTH,
                NK_AXIS_DIR_HORIZONTAL_VERTICAL,
                NK_AXIS_DIR_HORIZONTAL_DEPTH,
                NK_AXIS_DIR_VERTICAL_DEPTH,
                NK_AXIS_DIR_ALL
            };

            static const char* ToString(Code v) noexcept {
                switch (v) {
                    case NK_AXIS_DIR_HORIZONTAL:
                        return "Horizontal";
                    case NK_AXIS_DIR_VERTICAL:
                        return "Vertical";
                    case NK_AXIS_DIR_DEPTH:
                        return "Depth";
                    case NK_AXIS_DIR_HORIZONTAL_VERTICAL:
                        return "HorizVert";
                    case NK_AXIS_DIR_HORIZONTAL_DEPTH:
                        return "HorizDepth";
                    case NK_AXIS_DIR_VERTICAL_DEPTH:
                        return "VertDepth";
                    case NK_AXIS_DIR_ALL:
                        return "All";
                    default:
                        return "Undefined";
                }
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkStatusState
        // -------------------------------------------------------------------------
        // Description :
        //   État générique de disponibilité d'un périphérique, service ou 
        //   ressource système au sein du moteur.
        // Valeurs :
        //   - NK_STATUS_UNDEFINED    : État non initialisé ou inconnu
        //   - NK_STATUS_CONNECTED    : Opérationnel et fonctionnel
        //   - NK_STATUS_DISCONNECTED : Déconnecté mais récupérable (reconnectable)
        //   - NK_STATUS_UNAVAILABLE  : Non disponible (erreur matérielle, driver absent...)
        // Utilisation :
        //   - Gestion robuste des ressources avec fallback en cas d'indisponibilité.
        //   - Affichage d'état dans les menus de configuration système.
        //   - Déclenchement de tentatives de reconnexion automatique.
        // -------------------------------------------------------------------------
        struct NkStatusState {
            using Code = uint32;

            enum Value : Code {
                NK_STATUS_UNDEFINED = 0,
                NK_STATUS_CONNECTED,
                NK_STATUS_DISCONNECTED,
                NK_STATUS_UNAVAILABLE
            };

            static const char* ToString(Code v) noexcept {
                switch (v) {
                    case NK_STATUS_CONNECTED:
                        return "Connected";
                    case NK_STATUS_DISCONNECTED:
                        return "Disconnected";
                    case NK_STATUS_UNAVAILABLE:
                        return "Unavailable";
                    default:
                        return "Undefined";
                }
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkDraggedState
        // -------------------------------------------------------------------------
        // Description :
        //   Phase d'une opération de glisser-déposer (drag & drop) dans l'interface.
        //   Permet de gérer le cycle de vie complet d'un drag operation.
        // Valeurs :
        //   - NK_DRAGGED_UNDEFINED : État non initialisé ou opération invalide
        //   - NK_DRAGGED_ACTIVE    : L'objet est en cours de glissement (drag en cours)
        //   - NK_DRAGGED_DROPPED   : L'objet a été déposé sur une cible valide
        //   - NK_DRAGGED_CANCELLED : L'opération a été annulée (Échap, clic droit...)
        // Utilisation :
        //   - Gestion des états visuels pendant le drag (opacité, curseur...).
        //   - Validation des drop targets selon l'état de l'opération.
        //   - Nettoyage des ressources temporaires en cas d'annulation.
        // -------------------------------------------------------------------------
        struct NkDraggedState {
            using Code = uint32;

            enum Value : Code {
                NK_DRAGGED_UNDEFINED = 0,
                NK_DRAGGED_ACTIVE,
                NK_DRAGGED_DROPPED,
                NK_DRAGGED_CANCELLED
            };

            static const char* ToString(Code v) noexcept {
                switch (v) {
                    case NK_DRAGGED_ACTIVE:
                        return "Active";
                    case NK_DRAGGED_DROPPED:
                        return "Dropped";
                    case NK_DRAGGED_CANCELLED:
                        return "Cancelled";
                    default:
                        return "Undefined";
                }
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkFocusState
        // -------------------------------------------------------------------------
        // Description :
        //   Indique si une fenêtre possède actuellement le focus clavier système.
        //   Critique pour la gestion des inputs et de la pause automatique.
        // Valeurs :
        //   - NK_FOCUS_UNDEFINED : État non initialisé ou inconnu
        //   - NK_FOCUS_GAINED    : La fenêtre vient d'obtenir le focus clavier
        //   - NK_FOCUS_LOST      : La fenêtre vient de perdre le focus clavier
        // Utilisation :
        //   - Pause automatique du jeu lors de la perte de focus.
        //   - Désactivation des raccourcis clavier quand la fenêtre n'est pas active.
        //   - Adaptation du rendu (réduction de la fréquence d'update en background).
        // -------------------------------------------------------------------------
        struct NkFocusState {
            using Code = uint32;

            enum Value : Code {
                NK_FOCUS_UNDEFINED = 0,
                NK_FOCUS_GAINED,
                NK_FOCUS_LOST
            };

            static const char* ToString(Code v) noexcept {
                switch (v) {
                    case NK_FOCUS_GAINED:
                        return "Gained";
                    case NK_FOCUS_LOST:
                        return "Lost";
                    default:
                        return "Undefined";
                }
            }
        };

        // =========================================================================
        // Section : Snapshots d'entrée pour le polling en temps réel
        // =========================================================================

        // -------------------------------------------------------------------------
        // Structure : NkKeyboardInputState
        // -------------------------------------------------------------------------
        // Description :
        //   Snapshot de l'état courant du clavier, mis à jour par EventSystem 
        //   en réponse aux événements NkKeyPressEvent / NkKeyReleaseEvent.
        // Organisation mémoire :
        //   - pressed[256]  : Bitset des touches actuellement enfoncées (indexé par NkKey)
        //   - repeated[256] : Bitset des touches en auto-repeat OS (subset de pressed)
        //   - modifiers     : État des modificateurs (Ctrl, Alt, Shift, Super)
        //   - lastKey/lastScancode : Dernière touche/scancode traité pour référence
        // Taille approximative : ~64 octets + overhead des modificateurs
        // Utilisation :
        //   - Polling d'état clavier sans dépendance aux callbacks d'événements.
        //   - Détection de combinaisons de touches (modificateurs + touche).
        //   - Implémentation de raccourcis clavier et de bindings configurables.
        // Notes :
        //   - Les tableaux sont indexés par la valeur uint32 de NkKey/NkScancode.
        //   - La méthode IsKeyPressed() inclut les vérifications de bounds.
        // -------------------------------------------------------------------------
        struct NkKeyboardInputState {
            static constexpr uint32 KEY_COUNT = 256;

            bool pressed[KEY_COUNT] = {};
            bool repeated[KEY_COUNT] = {};
            NkModifierState modifiers;
            NkKey lastKey = NkKey::NK_UNKNOWN;
            NkScancode lastScancode = NkScancode::NK_SC_UNKNOWN;

            bool IsKeyPressed(NkKey key) const noexcept {
                uint32 idx = static_cast<uint32>(key);
                return (idx < KEY_COUNT) ? pressed[idx] : false;
            }

            bool IsKeyRepeated(NkKey key) const noexcept {
                uint32 idx = static_cast<uint32>(key);
                return (idx < KEY_COUNT) ? repeated[idx] : false;
            }

            bool IsAnyKeyPressed() const noexcept {
                for (uint32 i = 0; i < KEY_COUNT; ++i) {
                    if (pressed[i]) {
                        return true;
                    }
                }
                return false;
            }

            bool IsCtrlDown() const noexcept {
                return modifiers.ctrl;
            }

            bool IsAltDown() const noexcept {
                return modifiers.alt;
            }

            bool IsShiftDown() const noexcept {
                return modifiers.shift;
            }

            bool IsSuperDown() const noexcept {
                return modifiers.super;
            }

            void OnKeyPress(
                NkKey key,
                NkScancode sc,
                const NkModifierState& mods
            ) noexcept {
                uint32 idx = static_cast<uint32>(key);
                if (idx < KEY_COUNT) {
                    pressed[idx] = true;
                }
                lastKey = key;
                lastScancode = sc;
                modifiers = mods;
            }

            void OnKeyRepeat(
                NkKey key,
                NkScancode sc,
                const NkModifierState& mods
            ) noexcept {
                uint32 idx = static_cast<uint32>(key);
                if (idx < KEY_COUNT) {
                    pressed[idx] = true;
                    repeated[idx] = true;
                }
                lastKey = key;
                lastScancode = sc;
                modifiers = mods;
            }

            void OnKeyRelease(
                NkKey key,
                const NkModifierState& mods
            ) noexcept {
                uint32 idx = static_cast<uint32>(key);
                if (idx < KEY_COUNT) {
                    pressed[idx] = false;
                    repeated[idx] = false;
                }
                modifiers = mods;
            }

            void Clear() noexcept {
                memory::NkMemSet(pressed, 0, sizeof(pressed));
                memory::NkMemSet(repeated, 0, sizeof(repeated));
                modifiers = {};
                lastKey = NkKey::NK_UNKNOWN;
                lastScancode = NkScancode::NK_SC_UNKNOWN;
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkMouseInputState
        // -------------------------------------------------------------------------
        // Description :
        //   Snapshot de l'état courant de la souris, avec positions relatives 
        //   et absolues, mouvements bruts et état des boutons.
        // Coordonnées :
        //   - x/y         : Position dans la zone client de la fenêtre [pixels]
        //   - screenX/Y   : Position absolue sur l'écran [pixels]
        //   - deltaX/Y    : Déplacement depuis le dernier poll (avec accélération OS)
        //   - rawDeltaX/Y : Mouvement brut sans accélération (pour FPS/aiming)
        // États :
        //   - buttons     : Bitset des boutons actuellement enfoncés
        //   - modifiers   : Modificateurs clavier au dernier événement souris
        //   - captured    : Capture souris active (SetCapture / grab cursor)
        //   - insideWindow : Curseur dans la zone client de la fenêtre
        //   - insideFrame  : Curseur dans le cadre de la fenêtre (titre, bordures...)
        // Utilisation :
        //   - Contrôle de caméra FPS avec rawDelta pour précision maximale.
        //   - Détection de drag/drop avec état des boutons et position.
        //   - Menus contextuels avec détection de clic droit + modificateurs.
        // Notes :
        //   - Toutes les coordonnées sont en pixels physiques (non DPI-scalés).
        //   - Le coin supérieur gauche de la zone client est l'origine (0,0).
        // -------------------------------------------------------------------------
        struct NkMouseInputState {
            int32 x = 0;
            int32 y = 0;
            int32 screenX = 0;
            int32 screenY = 0;
            int32 deltaX = 0;
            int32 deltaY = 0;
            int32 rawDeltaX = 0;
            int32 rawDeltaY = 0;

            NkMouseButtons buttons;
            NkModifierState modifiers;

            bool captured = false;
            bool insideWindow = false;
            bool insideFrame = false;

            bool IsButtonPressed(NkMouseButton btn) const noexcept {
                return buttons.IsDown(btn);
            }

            bool IsAnyButtonPressed() const noexcept {
                return buttons.Any();
            }

            bool IsLeftDown() const noexcept {
                return buttons.IsDown(NkMouseButton::NK_MB_LEFT);
            }

            bool IsRightDown() const noexcept {
                return buttons.IsDown(NkMouseButton::NK_MB_RIGHT);
            }

            bool IsMiddleDown() const noexcept {
                return buttons.IsDown(NkMouseButton::NK_MB_MIDDLE);
            }

            void OnMove(
                int32 nx,
                int32 ny,
                int32 nsx,
                int32 nsy
            ) noexcept {
                deltaX = nx - x;
                deltaY = ny - y;
                x = nx;
                y = ny;
                screenX = nsx;
                screenY = nsy;
            }

            void OnRaw(int32 rdx, int32 rdy) noexcept {
                rawDeltaX = rdx;
                rawDeltaY = rdy;
            }

            void OnButtonPress(
                NkMouseButton btn,
                const NkModifierState& mods
            ) noexcept {
                buttons.Set(btn);
                modifiers = mods;
            }

            void OnButtonRelease(
                NkMouseButton btn,
                const NkModifierState& mods
            ) noexcept {
                buttons.Clear(btn);
                modifiers = mods;
            }

            void OnEnter() noexcept {
                insideWindow = true;
            }

            void OnLeave() noexcept {
                insideWindow = false;
                buttons = {};
            }

            void Clear() noexcept {
                x = 0;
                y = 0;
                screenX = 0;
                screenY = 0;
                deltaX = 0;
                deltaY = 0;
                rawDeltaX = 0;
                rawDeltaY = 0;
                buttons = {};
                modifiers = {};
                captured = false;
                insideWindow = false;
                insideFrame = false;
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkTouchInputState
        // -------------------------------------------------------------------------
        // Description :
        //   Snapshot de l'état courant du multi-touch, gérant jusqu'à 
        //   NK_MAX_TOUCH_POINTS contacts simultanés avec identification stable.
        // Structure interne TouchSlot :
        //   - id       : Identifiant unique du contact (stable pendant sa durée de vie)
        //   - x/y      : Position du contact dans la zone client [pixels flottants]
        //   - pressure : Pression du contact [0.0 = aucun, 1.0 = maximal]
        //   - active   : Indicateur de validité du slot (true = contact actif)
        // Métriques globales :
        //   - activeCount : Nombre de contacts actifs actuellement
        //   - centroidX/Y : Position moyenne (barycentre) des contacts actifs
        // Méthodes utilitaires :
        //   - IsTouching()   : Retourne true si au moins un contact est actif
        //   - IsMultiTouch() : Retourne true si deux contacts ou plus sont actifs
        //   - FindById()     : Recherche un slot actif par identifiant (nullptr si absent)
        // Mise à jour :
        //   - OnBegin()  : Ajout d'un nouveau contact dans un slot libre
        //   - OnMove()   : Mise à jour de position/pression d'un contact existant
        //   - OnEnd()    : Libération d'un slot lors de la fin d'un contact
        //   - OnCancel() : Annulation de tous les contacts (événement système)
        //   - UpdateCentroid() : Recalcul du barycentre après modification
        // Utilisation :
        //   - Gestes multi-touch (pinch, rotate, swipe) via analyse des contacts.
        //   - Contrôle de caméra ou d'interface via gestes tactiles complexes.
        //   - Jeux mobiles avec inputs multi-doigts simultanés.
        // Notes :
        //   - NK_MAX_TOUCH_POINTS est défini dans NkTouchEvent.h (typiquement 10-20).
        //   - Les positions sont en flottants pour précision sous-pixel.
        // -------------------------------------------------------------------------
        struct NkTouchInputState {
            struct TouchSlot {
                uint64 id = 0;
                float32 x = 0.f;
                float32 y = 0.f;
                float32 pressure = 1.f;
                bool active = false;
            };

            TouchSlot slots[NK_MAX_TOUCH_POINTS] = {};
            uint32 activeCount = 0;
            float32 centroidX = 0.f;
            float32 centroidY = 0.f;

            bool IsTouching() const noexcept {
                return activeCount > 0;
            }

            bool IsMultiTouch() const noexcept {
                return activeCount >= 2;
            }

            const TouchSlot* FindById(uint64 id) const noexcept {
                for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
                    if (slots[i].active && slots[i].id == id) {
                        return &slots[i];
                    }
                }
                return nullptr;
            }

            void OnBegin(const NkTouchPoint& pt) noexcept {
                for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
                    if (!slots[i].active) {
                        slots[i] = { pt.id, pt.clientX, pt.clientY, pt.pressure, true };
                        ++activeCount;
                        UpdateCentroid();
                        return;
                    }
                }
            }

            void OnMove(const NkTouchPoint& pt) noexcept {
                for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
                    if (slots[i].active && slots[i].id == pt.id) {
                        slots[i].x = pt.clientX;
                        slots[i].y = pt.clientY;
                        slots[i].pressure = pt.pressure;
                        UpdateCentroid();
                        return;
                    }
                }
            }

            void OnEnd(uint64 id) noexcept {
                for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
                    if (slots[i].active && slots[i].id == id) {
                        slots[i] = {};
                        if (activeCount > 0) {
                            --activeCount;
                        }
                        UpdateCentroid();
                        return;
                    }
                }
            }

            void OnCancel() noexcept {
                Clear();
            }

            void UpdateCentroid() noexcept {
                if (activeCount == 0) {
                    centroidX = 0.f;
                    centroidY = 0.f;
                    return;
                }
                float32 sx = 0.f;
                float32 sy = 0.f;
                for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
                    if (slots[i].active) {
                        sx += slots[i].x;
                        sy += slots[i].y;
                    }
                }
                centroidX = sx / static_cast<float32>(activeCount);
                centroidY = sy / static_cast<float32>(activeCount);
            }

            void Clear() noexcept {
                for (auto& s : slots) {
                    s = {};
                }
                activeCount = 0;
                centroidX = 0.f;
                centroidY = 0.f;
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkGamepadInputState
        // -------------------------------------------------------------------------
        // Description :
        //   Snapshot de l'état d'une manette individuelle à un instant donné,
        //   indexé par les énumérations NkGamepadButton et NkGamepadAxis.
        // Organisation :
        //   - connected    : Indicateur de connexion de la manette au système
        //   - info         : Métadonnées de la manette (type, capacités, VID/PID...)
        //   - batteryLevel : Niveau de batterie [0.0-1.0] ou -1.0 (filaire/inconnu)
        //   - buttons[]    : Bitset des boutons actuellement enfoncés (indexé par NkGamepadButton)
        //   - axes[]       : Valeurs des axes après application de la deadzone (indexé par NkGamepadAxis)
        //   - prevAxes[]   : Valeurs des axes au poll précédent pour calcul de delta
        // Méthodes d'accès :
        //   - IsButtonDown() : Vérifie si un bouton spécifique est enfoncé
        //   - GetAxis()      : Retourne la valeur courante d'un axe
        //   - GetPrevAxis()  : Retourne la valeur précédente d'un axe
        //   - GetAxisState() : Classe qualitativement un axe via NkAxisState::Classify()
        //   - IsConnected()  : Vérifie si la manette est connectée
        //   - IsCharging()   : Placeholder pour état de charge (rempli par NkGamepadBatteryEvent)
        // Mise à jour :
        //   - OnButtonPress()  : Marque un bouton comme enfoncé
        //   - OnButtonRelease() : Marque un bouton comme relâché
        //   - OnAxisMove()     : Met à jour la valeur d'un axe avec conservation de prev
        //   - Clear()          : Réinitialise tous les champs à l'état par défaut
        // Notes importantes :
        //   - La deadzone est appliquée en amont par le backend : les valeurs dans axes[] 
        //     sont déjà filtrées (0.0 si |valeur| < deadzone).
        //   - BUTTON_COUNT et AXIS_COUNT sont dérivés des énumérations correspondantes.
        //   - Les tableaux utilisent l'index uint32 de l'enum pour l'accès direct.
        // -------------------------------------------------------------------------
        struct NkGamepadInputState {
            static constexpr uint32 BUTTON_COUNT = static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);
            static constexpr uint32 AXIS_COUNT = static_cast<uint32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX);

            bool connected = false;
            NkGamepadInfo info;
            float32 batteryLevel = -1.f;

            bool buttons[BUTTON_COUNT] = {};
            float32 axes[AXIS_COUNT] = {};
            float32 prevAxes[AXIS_COUNT] = {};

            bool IsButtonDown(NkGamepadButton btn) const noexcept {
                uint32 idx = static_cast<uint32>(btn);
                return (idx < BUTTON_COUNT) ? buttons[idx] : false;
            }

            float32 GetAxis(NkGamepadAxis ax) const noexcept {
                uint32 idx = static_cast<uint32>(ax);
                return (idx < AXIS_COUNT) ? axes[idx] : 0.f;
            }

            float32 GetPrevAxis(NkGamepadAxis ax) const noexcept {
                uint32 idx = static_cast<uint32>(ax);
                return (idx < AXIS_COUNT) ? prevAxes[idx] : 0.f;
            }

            NkAxisState::Value GetAxisState(
                NkGamepadAxis ax,
                float32 deadzone = 0.1f
            ) const noexcept {
                return NkAxisState::Classify(GetAxis(ax), deadzone);
            }

            bool IsConnected() const noexcept {
                return connected;
            }

            bool IsCharging() const noexcept {
                return false;
            }

            void OnButtonPress(NkGamepadButton btn) noexcept {
                uint32 idx = static_cast<uint32>(btn);
                if (idx < BUTTON_COUNT) {
                    buttons[idx] = true;
                }
            }

            void OnButtonRelease(NkGamepadButton btn) noexcept {
                uint32 idx = static_cast<uint32>(btn);
                if (idx < BUTTON_COUNT) {
                    buttons[idx] = false;
                }
            }

            void OnAxisMove(
                NkGamepadAxis ax,
                float32 value
            ) noexcept {
                uint32 idx = static_cast<uint32>(ax);
                if (idx < AXIS_COUNT) {
                    prevAxes[idx] = axes[idx];
                    axes[idx] = value;
                }
            }

            void Clear() noexcept {
                connected = false;
                batteryLevel = -1.f;
                memory::NkMemSet(buttons, 0, sizeof(buttons));
                memory::NkMemSet(axes, 0, sizeof(axes));
                memory::NkMemSet(prevAxes, 0, sizeof(prevAxes));
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkGamepadSetState
        // -------------------------------------------------------------------------
        // Description :
        //   Conteneur gérant jusqu'à NK_MAX_GAMEPADS_STATE manettes simultanées,
        //   avec accès indexé par slot joueur (0 = joueur 1, 1 = joueur 2, etc.).
        // Constantes :
        //   - NK_MAX_GAMEPADS_STATE = 8 : Nombre maximal de slots de manettes supportés
        // Organisation :
        //   - slots[] : Tableau fixe de NkGamepadInputState pour chaque slot
        //   - connectedCount : Compteur de manettes actuellement connectées
        // Méthodes d'accès :
        //   - GetSlot(idx) : Retourne un pointeur vers le slot demandé (nullptr si hors bounds)
        //   - IsButtonDown(idx, btn) : Vérifie un bouton sur une manette spécifique
        //   - GetAxis(idx, ax) : Retourne la valeur d'un axe sur une manette spécifique
        // Gestion de connexion :
        //   - OnConnect(idx, info) : Initialise un slot avec les métadonnées de la manette
        //   - OnDisconnect(idx)   : Réinitialise un slot et décrémente le compteur
        //   - Clear()             : Réinitialise tous les slots et le compteur
        // Utilisation :
        //   - Jeux multi-joueurs locaux avec plusieurs manettes simultanées.
        //   - Attribution dynamique des slots selon l'ordre de connexion.
        //   - Gestion des déconnexions/reconnexions sans réorganisation des slots.
        // Notes :
        //   - Les slots non connectés retournent des valeurs par défaut (0/false).
        //   - L'index idx est l'index du slot, pas l'identifiant système de la manette.
        // -------------------------------------------------------------------------
        static constexpr uint32 NK_MAX_GAMEPADS_STATE = 8;

        struct NkGamepadSetState {
            NkGamepadInputState slots[NK_MAX_GAMEPADS_STATE];
            uint32 connectedCount = 0;

            NkGamepadInputState* GetSlot(uint32 idx) noexcept {
                return (idx < NK_MAX_GAMEPADS_STATE) ? &slots[idx] : nullptr;
            }

            const NkGamepadInputState* GetSlot(uint32 idx) const noexcept {
                return (idx < NK_MAX_GAMEPADS_STATE) ? &slots[idx] : nullptr;
            }

            bool IsButtonDown(
                uint32 idx,
                NkGamepadButton btn
            ) const noexcept {
                const auto* s = GetSlot(idx);
                return s && s->IsButtonDown(btn);
            }

            float32 GetAxis(
                uint32 idx,
                NkGamepadAxis ax
            ) const noexcept {
                const auto* s = GetSlot(idx);
                return s ? s->GetAxis(ax) : 0.f;
            }

            void OnConnect(
                uint32 idx,
                const NkGamepadInfo& info
            ) noexcept {
                if (idx >= NK_MAX_GAMEPADS_STATE) {
                    return;
                }
                slots[idx].Clear();
                slots[idx].connected = true;
                slots[idx].info = info;
                ++connectedCount;
            }

            void OnDisconnect(uint32 idx) noexcept {
                if (idx >= NK_MAX_GAMEPADS_STATE) {
                    return;
                }
                slots[idx].Clear();
                if (connectedCount > 0) {
                    --connectedCount;
                }
            }

            void Clear() noexcept {
                for (auto& s : slots) {
                    s.Clear();
                }
                connectedCount = 0;
            }
        };

        // =========================================================================
        // Section : Classe agrégée NkEventState (point d'entrée principal)
        // =========================================================================

        // -------------------------------------------------------------------------
        // Classe : NkEventState
        // -------------------------------------------------------------------------
        // Description :
        //   Agrégat global de tous les snapshots d'entrée, maintenu comme instance 
        //   unique par EventSystem et mis à jour en temps réel à chaque frame.
        //   Point d'entrée principal pour le polling d'entrées dans le code applicatif.
        // Membres publics :
        //   - keyboard : Snapshot de l'état clavier (NkKeyboardInputState)
        //   - mouse    : Snapshot de l'état souris (NkMouseInputState)
        //   - touch    : Snapshot de l'état tactile (NkTouchInputState)
        //   - gamepads : Snapshot de l'ensemble des manettes (NkGamepadSetState)
        // Accès en lecture :
        //   - GetKeyboard() : Retourne une référence const au snapshot clavier
        //   - GetMouse()    : Retourne une référence const au snapshot souris
        //   - GetTouch()    : Retourne une référence const au snapshot tactile
        //   - GetGamepads() : Retourne une référence const au snapshot manettes
        // Accès en écriture (réservé à EventSystem) :
        //   - Versions non-const des getters ci-dessus pour mise à jour interne
        // Prédicats globaux :
        //   - IsAnyInputActive() : Retourne true si au moins une entrée est active
        // Réinitialisation :
        //   - Clear() : Remet à zéro tous les snapshots (focus change, reset...)
        // Modèle d'utilisation :
        //   - Lecture seule recommandée depuis le code applicatif
        //   - Mise à jour réservée à EventSystem ou aux backends d'entrée
        //   - Pas de synchronisation interne : externaliser si accès multi-threads
        // Exemple d'usage :
        //   @code
        //   const auto& state = EventSystem::Instance().GetInputState();
        //   
        //   // Clavier : détection de touche avec modificateur
        //   if (state.keyboard.IsKeyPressed(NkKey::NK_SPACE) 
        //       && state.keyboard.IsCtrlDown()) {
        //       ToggleDebugOverlay();
        //   }
        //   
        //   // Souris : drag avec bouton gauche
        //   if (state.mouse.IsLeftDown()) {
        //       StartDrag(state.mouse.x, state.mouse.y);
        //   }
        //   
        //   // Manette : tir avec bouton SOUTH sur joueur 1
        //   if (state.gamepads.IsButtonDown(0, NkGamepadButton::NK_GP_SOUTH)) {
        //       FireWeapon(0);
        //   }
        //   
        //   // Manette : mouvement avec stick gauche
        //   float lx = state.gamepads.GetAxis(0, NkGamepadAxis::NK_GP_AXIS_LX);
        //   float ly = state.gamepads.GetAxis(0, NkGamepadAxis::NK_GP_AXIS_LY);
        //   MovePlayer(0, lx, ly);
        //   @endcode
        // -------------------------------------------------------------------------
        class NkEventState {
            public:
                NkEventState() = default;

                const NkKeyboardInputState& GetKeyboard() const noexcept {
                    return keyboard;
                }

                const NkMouseInputState& GetMouse() const noexcept {
                    return mouse;
                }

                const NkTouchInputState& GetTouch() const noexcept {
                    return touch;
                }

                const NkGamepadSetState& GetGamepads() const noexcept {
                    return gamepads;
                }

                NkKeyboardInputState& GetKeyboard() noexcept {
                    return keyboard;
                }

                NkMouseInputState& GetMouse() noexcept {
                    return mouse;
                }

                NkTouchInputState& GetTouch() noexcept {
                    return touch;
                }

                NkGamepadSetState& GetGamepads() noexcept {
                    return gamepads;
                }

                bool IsAnyInputActive() const noexcept {
                    return keyboard.IsAnyKeyPressed()
                        || mouse.IsAnyButtonPressed()
                        || touch.IsTouching()
                        || gamepads.connectedCount > 0;
                }

                void Clear() noexcept {
                    keyboard.Clear();
                    mouse.Clear();
                    touch.Clear();
                    gamepads.Clear();
                }

                NkKeyboardInputState keyboard;
                NkMouseInputState mouse;
                NkTouchInputState touch;
                NkGamepadSetState gamepads;
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKEVENTSTATE_H

// =============================================================================
// Section : Exemples d'utilisation (à titre informatif - en commentaire)
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Polling d'entrées dans la boucle de jeu principale
// -----------------------------------------------------------------------------
void GameLoop::Update() {
    // Accès au snapshot global d'entrées (lecture seule)
    const auto& inputState = EventSystem::Instance().GetInputState();

    // --- Gestion du clavier ---
    // Déplacement du personnage avec touches directionnelles
    float moveX = 0.f;
    float moveY = 0.f;

    if (inputState.keyboard.IsKeyPressed(NkKey::NK_A)
        || inputState.keyboard.IsKeyPressed(NkKey::NK_LEFT)) {
        moveX = -1.f;
    }
    if (inputState.keyboard.IsKeyPressed(NkKey::NK_D)
        || inputState.keyboard.IsKeyPressed(NkKey::NK_RIGHT)) {
        moveX = +1.f;
    }
    if (inputState.keyboard.IsKeyPressed(NkKey::NK_W)
        || inputState.keyboard.IsKeyPressed(NkKey::NK_UP)) {
        moveY = +1.f;
    }
    if (inputState.keyboard.IsKeyPressed(NkKey::NK_S)
        || inputState.keyboard.IsKeyPressed(NkKey::NK_DOWN)) {
        moveY = -1.f;
    }

    // Application du mouvement si une direction est pressée
    if (moveX != 0.f || moveY != 0.f) {
        // Normalisation pour mouvement diagonal cohérent
        const float length = std::sqrt(moveX * moveX + moveY * moveY);
        if (length > 0.f) {
            moveX /= length;
            moveY /= length;
        }
        // PlayerController::Move(moveX, moveY);
    }

    // Action avec modificateur : sprint avec Shift
    if (inputState.keyboard.IsKeyPressed(NkKey::NK_LSHIFT)
        || inputState.keyboard.IsKeyPressed(NkKey::NK_RSHIFT)) {
        // PlayerController::EnableSprint(true);
    }

    // --- Gestion de la souris ---
    // Rotation de caméra FPS avec mouvement brut (sans accélération OS)
    if (inputState.mouse.IsLeftDown()) {
        constexpr float MOUSE_SENSITIVITY = 0.002f;
        float yaw   = -static_cast<float>(inputState.mouse.rawDeltaX) * MOUSE_SENSITIVITY;
        float pitch = -static_cast<float>(inputState.mouse.rawDeltaY) * MOUSE_SENSITIVITY;
        // CameraController::Rotate(yaw, pitch);
    }

    // Clic droit pour menu contextuel à la position du curseur
    if (inputState.mouse.IsButtonPressed(NkMouseButton::NK_MB_RIGHT)) {
        // UIManager::ShowContextMenu(inputState.mouse.x, inputState.mouse.y);
    }

    // --- Gestion des manettes ---
    // Support multi-joueurs avec itération sur les slots connectés
    for (uint32 playerIdx = 0; playerIdx < NK_MAX_GAMEPADS_STATE; ++playerIdx) {
        const auto* gamepad = inputState.gamepads.GetSlot(playerIdx);
        if (!gamepad || !gamepad->IsConnected()) {
            continue;
        }

        // Mouvement avec stick gauche
        float lx = gamepad->GetAxis(NkGamepadAxis::NK_GP_AXIS_LX);
        float ly = gamepad->GetAxis(NkGamepadAxis::NK_GP_AXIS_LY);
        // PlayerController::Move(playerIdx, lx, ly);

        // Action avec bouton SOUTH (A/Cross)
        if (gamepad->IsButtonDown(NkGamepadButton::NK_GP_SOUTH)) {
            // PlayerController::Jump(playerIdx);
        }

        // Vibration haptique en retour d'impact (événement sortant)
        // if (PlayerController::IsTakingDamage(playerIdx)) {
        //     NkGamepadRumbleEvent rumble(
        //         playerIdx,
        //         0.0f,    // motorLow
        //         0.9f,    // motorHigh: vibration intense
        //         0.0f,    // triggerLeft
        //         0.0f,    // triggerRight
        //         100      // durationMs
        //     );
        //     EventSystem::Instance().Dispatch(rumble);
        // }
    }

    // --- Gestion du tactile (mobile) ---
    if (inputState.touch.IsTouching()) {
        // Contrôle virtuel : premier contact = mouvement, second = action
        const auto* firstTouch = inputState.touch.FindById(0);
        if (firstTouch && firstTouch->active) {
            // Mapping de la position du doigt sur un joystick virtuel
            // VirtualJoystick::Update(firstTouch->x, firstTouch->y);
        }

        // Geste de pinch pour zoom caméra (si multi-touch)
        if (inputState.touch.IsMultiTouch()) {
            // GestureRecognizer::ProcessPinch(inputState.touch);
        }
    }
}

// -----------------------------------------------------------------------------
// Exemple 2 : Détection d'états qualitatifs avec NkAxisState
// -----------------------------------------------------------------------------
void HandleSteeringInput(const NkGamepadInputState& gamepad) {
    // Classification qualitative de l'axe de direction
    const auto axisState = gamepad.GetAxisState(
        NkGamepadAxis::NK_GP_AXIS_LX,
        0.15f  // deadzone de 15%
    );

    switch (axisState) {
        case NkAxisState::NK_AXIS_POSITIVE:
            // VehicleController::TurnRight();
            break;
        case NkAxisState::NK_AXIS_NEGATIVE:
            // VehicleController::TurnLeft();
            break;
        case NkAxisState::NK_AXIS_NEUTRAL:
        default:
            // VehicleController::CenterSteering();
            break;
    }
}

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des transitions de focus fenêtre
// -----------------------------------------------------------------------------
void OnWindowFocusChanged(const NkWindowFocusEvent& event) {
    const auto& inputState = EventSystem::Instance().GetInputState();

    if (event.GetFocusState() == NkFocusState::NK_FOCUS_LOST) {
        // Perte de focus : pause automatique et nettoyage des inputs
        // Game::Pause();
        inputState.Clear();  // Évite les inputs "fantômes" au retour de focus
        NK_LOG_INFO("Jeu en pause - fenêtre sans focus");
    } else {
        // Gain de focus : reprise si le jeu était en pause
        // if (Game::IsPausedByFocusLoss()) {
        //     Game::Resume();
        //     NK_LOG_INFO("Jeu repris - fenêtre active");
        // }
    }
}

// -----------------------------------------------------------------------------
// Exemple 4 : Détection de gestes multi-touch complexes
// -----------------------------------------------------------------------------
void ProcessTouchGestures(const NkTouchInputState& touchState) {
    // Détection de swipe : mouvement rapide avec un seul contact
    if (touchState.activeCount == 1) {
        const auto* touch = touchState.FindById(0);
        if (touch && touch->active) {
            static float32 lastX = touch->x;
            static float32 lastY = touch->y;
            static uint64 lastTime = 0;

            const uint64 currentTime = Platform::GetTimeMs();
            const float32 dx = touch->x - lastX;
            const float32 dy = touch->y - lastY;
            const uint64 dt = currentTime - lastTime;

            if (dt > 0 && dt < 200) {  // Swipe détecté si < 200ms
                const float32 speed = std::sqrt(dx * dx + dy * dy) / static_cast<float32>(dt);
                if (speed > 0.5f) {  // Seuil de vitesse pour swipe
                    if (std::abs(dx) > std::abs(dy)) {
                        // Swipe horizontal
                        // UIManager::NavigateHorizontally(dx > 0 ? 1 : -1);
                    } else {
                        // Swipe vertical
                        // UIManager::NavigateVertically(dy > 0 ? 1 : -1);
                    }
                }
            }

            lastX = touch->x;
            lastY = touch->y;
            lastTime = currentTime;
        }
    }

    // Détection de pinch/zoom : deux contacts qui s'éloignent ou se rapprochent
    if (touchState.activeCount >= 2) {
        const auto* t1 = touchState.FindById(0);
        const auto* t2 = touchState.FindById(1);
        if (t1 && t2 && t1->active && t2->active) {
            const float32 currentDist = std::sqrt(
                (t2->x - t1->x) * (t2->x - t1->x)
                + (t2->y - t1->y) * (t2->y - t1->y)
            );
            static float32 lastDist = currentDist;

            const float32 delta = currentDist - lastDist;
            if (std::abs(delta) > 10.f) {  // Seuil de 10 pixels pour zoom
                // CameraController::Zoom(delta > 0 ? 1.f : -1.f);
            }

            lastDist = currentDist;
        }
    }
}

// -----------------------------------------------------------------------------
// Exemple 5 : Réinitialisation conditionnelle des inputs
// -----------------------------------------------------------------------------
void ResetInputsOnSceneChange() {
    auto& inputState = EventSystem::Instance().GetInputState();

    // Conservation des états de connexion des manettes
    NkGamepadSetState preservedGamepads;
    for (uint32 i = 0; i < NK_MAX_GAMEPADS_STATE; ++i) {
        const auto* slot = inputState.gamepads.GetSlot(i);
        if (slot && slot->IsConnected()) {
            preservedGamepads.OnConnect(i, slot->info);
        }
    }

    // Réinitialisation complète
    inputState.Clear();

    // Restauration des connexions de manettes
    for (uint32 i = 0; i < NK_MAX_GAMEPADS_STATE; ++i) {
        const auto* preserved = preservedGamepads.GetSlot(i);
        if (preserved && preserved->IsConnected()) {
            inputState.gamepads.OnConnect(i, preserved->info);
        }
    }

    NK_LOG_DEBUG("Inputs réinitialisés, connexions manettes préservées");
}

// -----------------------------------------------------------------------------
// Exemple 6 : Debug overlay affichant l'état des entrées en temps réel
// -----------------------------------------------------------------------------
void RenderInputDebugOverlay() {
    const auto& state = EventSystem::Instance().GetInputState();

    // En-tête
    // DebugUI::BeginPanel("Input State");

    // Section clavier
    // DebugUI::Text("Keyboard:");
    if (state.keyboard.IsAnyKeyPressed()) {
        for (uint32 i = 0; i < NkKeyboardInputState::KEY_COUNT; ++i) {
            if (state.keyboard.pressed[i]) {
                // DebugUI::Text("  Key %u: PRESSED", i);
            }
        }
    } else {
        // DebugUI::Text("  (no keys pressed)");
    }

    // Section souris
    // DebugUI::Text("Mouse: pos=(%d,%d) delta=(%d,%d) buttons=0x%02X",
    //     state.mouse.x, state.mouse.y,
    //     state.mouse.deltaX, state.mouse.deltaY,
    //     state.mouse.buttons.Value());

    // Section manettes
    // DebugUI::Text("Gamepads: %u connected", state.gamepads.connectedCount);
    for (uint32 i = 0; i < NK_MAX_GAMEPADS_STATE; ++i) {
        const auto* gp = state.gamepads.GetSlot(i);
        if (gp && gp->IsConnected()) {
            // DebugUI::Text("  Slot %u: %s", i, gp->info.name);
            // DebugUI::Text("    Battery: %.0f%%", gp->batteryLevel * 100.f);
        }
    }

    // DebugUI::EndPanel();
}

// -----------------------------------------------------------------------------
// Exemple 7 : Binding dynamique des touches via configuration utilisateur
// -----------------------------------------------------------------------------
class InputBindingManager {
public:
    void Initialize() {
        // Chargement des bindings depuis la configuration utilisateur
        // auto config = ConfigLoader::Load("input/bindings.json");
        // for (const auto& binding : config.GetBindings()) {
        //     mBindings[binding.actionName] = binding.inputSpec;
        // }
    }

    bool IsActionTriggered(
        const NkEventState& inputState,
        const NkString& actionName
    ) const noexcept {
        const auto it = mBindings.find(actionName);
        if (it == mBindings.end()) {
            return false;
        }

        const InputSpec& spec = it->second;

        // Vérification clavier
        if (spec.type == InputType::KEYBOARD) {
            return inputState.keyboard.IsKeyPressed(spec.key);
        }

        // Vérification souris
        if (spec.type == InputType::MOUSE) {
            return inputState.mouse.IsButtonPressed(spec.mouseButton);
        }

        // Vérification manette
        if (spec.type == InputType::GAMEPAD) {
            return inputState.gamepads.IsButtonDown(
                spec.gamepadSlot,
                spec.gamepadButton
            );
        }

        return false;
    }

private:
    enum class InputType { KEYBOARD, MOUSE, GAMEPAD };

    struct InputSpec {
        InputType type = InputType::KEYBOARD;
        NkKey key = NkKey::NK_UNKNOWN;
        NkMouseButton mouseButton = NkMouseButton::NK_MB_UNKNOWN;
        uint32 gamepadSlot = 0;
        NkGamepadButton gamepadButton = NkGamepadButton::NK_GP_UNKNOWN;
    };

    std::unordered_map<NkString, InputSpec> mBindings;
};

// -----------------------------------------------------------------------------
// Exemple 8 : Test unitaire de NkEventState
// -----------------------------------------------------------------------------
void TestNkEventState() {
    NkEventState state;

    // Test initial : tous les inputs doivent être inactifs
    NK_ASSERT(!state.IsAnyInputActive());
    NK_ASSERT(!state.keyboard.IsKeyPressed(NkKey::NK_SPACE));
    NK_ASSERT(!state.mouse.IsLeftDown());
    NK_ASSERT(state.gamepads.connectedCount == 0);

    // Simulation d'appui sur une touche
    NkModifierState mods{};
    state.keyboard.OnKeyPress(NkKey::NK_SPACE, NkScancode::NK_SC_SPACE, mods);
    NK_ASSERT(state.keyboard.IsKeyPressed(NkKey::NK_SPACE));
    NK_ASSERT(state.IsAnyInputActive());

    // Simulation de mouvement de souris
    state.mouse.OnMove(100, 200, 1100, 300);
    NK_ASSERT(state.mouse.x == 100);
    NK_ASSERT(state.mouse.y == 200);
    NK_ASSERT(state.mouse.screenX == 1100);
    NK_ASSERT(state.mouse.screenY == 300);

    // Simulation de connexion d'une manette
    NkGamepadInfo gpInfo{};
    gpInfo.index = 0;
    strcpy_s(gpInfo.name, "Test Gamepad");
    state.gamepads.OnConnect(0, gpInfo);
    NK_ASSERT(state.gamepads.connectedCount == 1);
    NK_ASSERT(state.gamepads.GetSlot(0)->IsConnected());

    // Simulation d'appui sur un bouton de manette
    state.gamepads.GetSlot(0)->OnButtonPress(NkGamepadButton::NK_GP_SOUTH);
    NK_ASSERT(state.gamepads.IsButtonDown(0, NkGamepadButton::NK_GP_SOUTH));

    // Test de Clear()
    state.Clear();
    NK_ASSERT(!state.IsAnyInputActive());
    NK_ASSERT(state.gamepads.connectedCount == 0);

    NK_LOG_INFO("Tests de NkEventState passés avec succès");
}
*/