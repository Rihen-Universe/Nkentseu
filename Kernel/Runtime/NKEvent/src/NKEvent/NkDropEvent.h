#ifndef NKENTSEU_EVENT_NKDROPEVENT_H
#define NKENTSEU_EVENT_NKDROPEVENT_H

#pragma once

// =============================================================================
// Fichier : NkDropEvent.h
// =============================================================================
// Description :
//   Définition complète des événements Drag & Drop pour le moteur NkEntseu.
//   Permet la gestion des opérations de glisser-déposer de fichiers, texte 
//   et images depuis des sources externes vers l'application.
//
// Catégorie : NK_CAT_DROP
//
// Responsabilités :
//   - Fournir des événements typés pour chaque phase du drag & drop.
//   - Transporter les métadonnées des éléments déposés (chemins, contenu...).
//   - Supporter les formats courants : fichiers, texte UTF-8, images, URLs.
//   - Intégration avec le système d'événements global via NkEvent.
//
// Types d'événements définis :
//   - NkDropEnterEvent   : Un objet glissé entre dans la zone de la fenêtre
//   - NkDropOverEvent    : Un objet glissé survole la fenêtre (position mise à jour)
//   - NkDropLeaveEvent   : Un objet glissé quitte la zone de la fenêtre
//   - NkDropFileEvent    : Un ou plusieurs fichiers ont été déposés
//   - NkDropTextEvent    : Du texte a été déposé
//   - NkDropImageEvent   : Une image a été déposée (données ou URI)
//
// Structures de données :
//   - NkDropEnterData  : Métadonnées d'entrée d'un objet glissé
//   - NkDropOverData   : Position courante d'un objet glissé
//   - NkDropLeaveData  : Marqueur de sortie d'objet glissé
//   - NkDropFilePath   : Chemin unique d'un fichier déposé
//   - NkDropFileData   : Collection de fichiers déposés avec position
//   - NkDropTextData   : Texte déposé avec type MIME et position
//   - NkDropImageData  : Image déposée avec métadonnées et pixels optionnels
//
// Énumérations :
//   - NkDropType : Classification du type de contenu déposé
//
// Auteur : Équipe NkEntseu
// Version : 1.0.0
// Date : 2026
// =============================================================================

    #include "NkEvent.h"
    #include "NkEventState.h"
    #include "NKContainers/String/NkStringUtils.h"

    #include <string>
    #include <vector>
    #include <cstring>

    namespace nkentseu {
        // =========================================================================
        // Section : Énumérations pour la classification des types de drop
        // =========================================================================

        // -------------------------------------------------------------------------
        // Énumération : NkDropType
        // -------------------------------------------------------------------------
        // Description :
        //   Classification du type de contenu transporté lors d'une opération 
        //   de drag & drop pour un routage approprié des événements.
        // Valeurs :
        //   - NK_DROP_TYPE_UNKNOWN  : Type non identifié ou non supporté
        //   - NK_DROP_TYPE_FILE     : Un ou plusieurs chemins de fichiers système
        //   - NK_DROP_TYPE_TEXT     : Chaîne de texte encodée en UTF-8
        //   - NK_DROP_TYPE_IMAGE    : Image sous forme de données brutes ou URI
        //   - NK_DROP_TYPE_URL      : URL web ou URI de ressource distante
        //   - NK_DROP_TYPE_MAX      : Valeur sentinelle pour itérations
        // Utilisation :
        //   - Filtrage des événements selon le type de contenu attendu.
        //   - Adaptation de l'interface selon le type de drop accepté.
        //   - Validation des opérations de drop avant traitement.
        // -------------------------------------------------------------------------
        enum class NkDropType : uint32 {
            NK_DROP_TYPE_UNKNOWN = 0,
            NK_DROP_TYPE_FILE,
            NK_DROP_TYPE_TEXT,
            NK_DROP_TYPE_IMAGE,
            NK_DROP_TYPE_URL,
            NK_DROP_TYPE_MAX
        };

        // =========================================================================
        // Section : Structures de données pour les événements de drop
        // =========================================================================

        // -------------------------------------------------------------------------
        // Structure : NkDropEnterData
        // -------------------------------------------------------------------------
        // Description :
        //   Métadonnées transmises lorsqu'un objet glissé entre dans la zone 
        //   de la fenêtre cible, avant acceptation ou refus de l'opération.
        // Champs :
        //   - x, y      : Position du curseur dans la zone client [pixels]
        //   - numFiles  : Nombre de fichiers proposés dans l'opération
        //   - hasText   : Indicateur de présence de texte dans le drop
        //   - hasImage  : Indicateur de présence d'image dans le drop
        // Utilisation :
        //   - Décision d'accepter ou refuser le drop selon les capacités.
        //   - Affichage de feedback visuel (curseur autorisé/interdit).
        //   - Pré-chargement ou pré-validation des ressources proposées.
        // Méthodes :
        //   - ToString() : Formatage lisible pour logging et debugging.
        // -------------------------------------------------------------------------
        struct NkDropEnterData {
            int32 x = 0;
            int32 y = 0;
            uint32 numFiles = 0;
            bool hasText = false;
            bool hasImage = false;

            NkString ToString() const {
                NkString s = NkString::Fmt("DropEnter(at {0},{1}", x, y);
                if (numFiles) {
                    s += NkString::Fmt(" files={0}", numFiles);
                }
                if (hasText) {
                    s += " text";
                }
                if (hasImage) {
                    s += " image";
                }
                s += ")";
                return s;
            }
        };

        // -------------------------------------------------------------------------
        // Classe : NkDropEnterEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'un objet glissé entre pour la première fois 
        //   dans la zone de la fenêtre cible.
        // Déclenchement :
        //   - Premier survol d'un drag operation sur la fenêtre.
        //   - Ré-entrée après une sortie temporaire (si supporté par la plateforme).
        // Données transportées :
        //   - data : Structure NkDropEnterData avec métadonnées du drop.
        // Utilisation typique :
        //   - Mise en surbrillance de la zone de drop acceptante.
        //   - Changement du curseur pour indiquer l'acceptation/refus.
        //   - Pré-validation du contenu avant acceptation définitive.
        // -------------------------------------------------------------------------
        class NkDropEnterEvent final : public NkEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_DROP_ENTER)

                NkDropEnterData data;

                explicit NkDropEnterEvent(
                    const NkDropEnterData& d = {},
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , data(d) {
                }

                NkEvent* Clone() const override {
                    return new NkDropEnterEvent(*this);
                }

                NkString ToString() const override {
                    return "NkDropEnterEvent(" + data.ToString() + ")";
                }
        };

        // -------------------------------------------------------------------------
        // Structure : NkDropOverData
        // -------------------------------------------------------------------------
        // Description :
        //   Données de position transmises lors du survol continu d'un objet 
        //   glissé au-dessus de la fenêtre cible.
        // Champs :
        //   - x, y : Position courante du curseur dans la zone client [pixels]
        // Utilisation :
        //   - Mise à jour du feedback visuel en temps réel.
        //   - Détection de zones de drop spécifiques dans la fenêtre.
        //   - Calcul de trajectoire pour effets d'animation.
        // Notes :
        //   - Cet événement peut être émis très fréquemment (chaque frame).
        //   - Optimiser le traitement pour éviter les impacts performance.
        // -------------------------------------------------------------------------
        struct NkDropOverData {
            int32 x = 0;
            int32 y = 0;

            NkString ToString() const {
                return NkString::Fmt("DropOver({0},{1})", x, y);
            }
        };

        // -------------------------------------------------------------------------
        // Classe : NkDropOverEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lors du déplacement continu d'un objet glissé 
        //   au-dessus de la fenêtre cible.
        // Déclenchement :
        //   - Mouvement du curseur pendant une opération de drag active.
        //   - Mise à jour périodique de la position par le système hôte.
        // Données transportées :
        //   - data : Structure NkDropOverData avec position courante.
        // Utilisation typique :
        //   - Mise à jour de la position d'un indicateur de drop.
        //   - Détection de zones de drop dynamiques (grid, liste...).
        //   - Animation de prévisualisation du contenu à déposer.
        // Notes :
        //   - Fréquence d'émission dépendante de la plateforme et du driver.
        //   - Peut être throttled/débounced côté application si nécessaire.
        // -------------------------------------------------------------------------
        class NkDropOverEvent final : public NkEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_DROP_OVER)

                NkDropOverData data;

                explicit NkDropOverEvent(
                    const NkDropOverData& d = {},
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , data(d) {
                }

                NkEvent* Clone() const override {
                    return new NkDropOverEvent(*this);
                }

                NkString ToString() const override {
                    return "NkDropOverEvent(" + data.ToString() + ")";
                }
        };

        // -------------------------------------------------------------------------
        // Structure : NkDropLeaveData
        // -------------------------------------------------------------------------
        // Description :
        //   Marqueur de sortie d'un objet glissé hors de la zone de la fenêtre.
        //   Structure minimaliste car l'information principale est l'événement lui-même.
        // Utilisation :
        //   - Nettoyage du feedback visuel de drop.
        //   - Annulation des pré-chargements ou pré-validations.
        //   - Réinitialisation de l'état interne de la zone de drop.
        // -------------------------------------------------------------------------
        struct NkDropLeaveData {
            NkString ToString() const {
                return "DropLeave";
            }
        };

        // -------------------------------------------------------------------------
        // Classe : NkDropLeaveEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'un objet glissé quitte la zone de la fenêtre 
        //   cible, annulant potentiellement l'opération de drop.
        // Déclenchement :
        //   - Sortie du curseur de la zone client de la fenêtre.
        //   - Annulation explicite de l'opération par l'utilisateur (Échap).
        //   - Interruption du drag par le système hôte.
        // Données transportées :
        //   - Aucune donnée supplémentaire (événement auto-descriptif).
        // Utilisation typique :
        //   - Masquage des indicateurs visuels de drop.
        //   - Libération des ressources allouées pour la prévisualisation.
        //   - Retour à l'état normal de l'interface utilisateur.
        // -------------------------------------------------------------------------
        class NkDropLeaveEvent final : public NkEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_DROP_LEAVE)

                explicit NkDropLeaveEvent(uint64 windowId = 0) noexcept
                    : NkEvent(windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkDropLeaveEvent(*this);
                }

                NkString ToString() const override {
                    return "NkDropLeaveEvent()";
                }
        };

        // -------------------------------------------------------------------------
        // Structure : NkDropFilePath
        // -------------------------------------------------------------------------
        // Description :
        //   Conteneur pour un chemin de fichier unique déposé via drag & drop.
        //   Encapsule un buffer fixe pour éviter les allocations dynamiques.
        // Champs :
        //   - path[512] : Buffer statique pour le chemin du fichier (encodage plateforme)
        // Constructeurs :
        //   - Défaut : initialise un chemin vide.
        //   - Paramétré : copie le chemin fourni avec troncation sécurisée.
        // Sécurité :
        //   - Copie limitée à sizeof(path)-1 pour garantir la terminaison nulle.
        //   - Gestion des pointeurs nuls en entrée.
        // Utilisation :
        //   - Stockage temporaire dans NkDropFileData.
        //   - Conversion vers NkString via ToString() pour traitement applicatif.
        // -------------------------------------------------------------------------
        struct NkDropFilePath {
            char path[512] = {};

            NkDropFilePath() = default;

            explicit NkDropFilePath(const char* p) {
                if (p) {
                    strncpy(path, p, sizeof(path) - 1);
                }
            }

            NkString ToString() const {
                return NkString(path);
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkDropFileData
        // -------------------------------------------------------------------------
        // Description :
        //   Conteneur pour les données d'un événement de drop de fichiers.
        //   Gère une collection de chemins avec la position de dépose.
        // Champs :
        //   - x, y   : Position de dépose dans la zone client [pixels]
        //   - paths  : Vecteur de chemins de fichiers déposés (NkString)
        // Méthodes :
        //   - AddPath()    : Ajoute un chemin à la collection.
        //   - Count()      : Retourne le nombre de fichiers déposés.
        //   - ToString()   : Formatage lisible pour logging et debugging.
        // Utilisation :
        //   - Transport des chemins de fichiers depuis le backend plateforme.
        //   - Itération applicative pour traitement des fichiers déposés.
        // Notes :
        //   - Les chemins sont dans l'encodage natif de la plateforme.
        //   - Conversion vers UTF-8 si nécessaire via NkStringUtils.
        // -------------------------------------------------------------------------
        struct NkDropFileData {
            int32 x = 0;
            int32 y = 0;
            NkVector<NkString> paths;

            NkDropFileData() = default;

            void AddPath(const NkString& p) {
                paths.PushBack(p);
            }

            uint32 Count() const {
                return static_cast<uint32>(paths.size());
            }

            NkString ToString() const {
                return NkString::Fmt("DropFile({0} file(s) at {1},{2})", Count(), x, y);
            }
        };

        // -------------------------------------------------------------------------
        // Classe : NkDropFileEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'un ou plusieurs fichiers sont déposés 
        //   dans la zone de la fenêtre cible.
        // Déclenchement :
        //   - Relâchement du bouton de souris après un drag de fichiers.
        //   - Validation de l'opération de drop par le système hôte.
        // Données transportées :
        //   - data : Structure NkDropFileData avec chemins et position.
        // Utilisation typique :
        //   - Ouverture des fichiers déposés dans l'application.
        //   - Import de ressources (textures, modèles, configurations...).
        //   - Gestion de batch de fichiers pour traitement en série.
        // Notes :
        //   - Les chemins peuvent être absolus ou relatifs selon la source.
        //   - Validation des permissions d'accès recommandée avant traitement.
        // -------------------------------------------------------------------------
        class NkDropFileEvent final : public NkEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_DROP_FILE)

                NkDropFileData data;

                explicit NkDropFileEvent(
                    const NkDropFileData& d = {},
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , data(d) {
                }

                NkEvent* Clone() const override {
                    return new NkDropFileEvent(*this);
                }

                NkString ToString() const override {
                    return "NkDropFileEvent(" + data.ToString() + ")";
                }
        };

        // -------------------------------------------------------------------------
        // Structure : NkDropTextData
        // -------------------------------------------------------------------------
        // Description :
        //   Conteneur pour les données d'un événement de drop de texte.
        //   Transporte le contenu textuel avec métadonnées de type.
        // Champs :
        //   - x, y      : Position de dépose dans la zone client [pixels]
        //   - text      : Contenu textuel encodé en UTF-8
        //   - mimeType  : Type MIME du texte (ex: "text/plain", "text/html")
        // Méthodes :
        //   - ToString() : Formatage avec aperçu tronqué pour logging.
        // Utilisation :
        //   - Insertion de texte depuis des applications externes.
        //   - Support du copier-coller enrichi via drag & drop.
        //   - Traitement différencié selon le type MIME (HTML vs plain text).
        // Notes :
        //   - L'aperçu dans ToString() est limité à 40 caractères.
        //   - Le texte complet est accessible via le champ 'text'.
        // -------------------------------------------------------------------------
        struct NkDropTextData {
            int32 x = 0;
            int32 y = 0;
            NkString text;
            NkString mimeType;

            NkString ToString() const {
                NkString preview = text.Size() > 40
                    ? text.SubStr(0, 40)
                    : text;
                return "DropText(\""
                    + preview
                    + (text.Size() > 40 ? "..." : "")
                    + "\")";
            }
        };

        // -------------------------------------------------------------------------
        // Classe : NkDropTextEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'un contenu textuel est déposé 
        //   dans la zone de la fenêtre cible.
        // Déclenchement :
        //   - Relâchement d'un drag de texte depuis une application source.
        //   - Validation de l'opération par le système de drag & drop hôte.
        // Données transportées :
        //   - data : Structure NkDropTextData avec contenu et type MIME.
        // Utilisation typique :
        //   - Insertion de texte dans des champs éditables.
        //   - Parsing de contenu HTML ou markdown selon le mimeType.
        //   - Recherche ou filtrage basé sur le texte déposé.
        // Notes :
        //   - Le texte est toujours en UTF-8 indépendamment de la source.
        //   - Le mimeType permet d'adapter le traitement (sanitization HTML...).
        // -------------------------------------------------------------------------
        class NkDropTextEvent final : public NkEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_DROP_TEXT)

                NkDropTextData data;

                explicit NkDropTextEvent(
                    const NkDropTextData& d = {},
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , data(d) {
                }

                NkEvent* Clone() const override {
                    return new NkDropTextEvent(*this);
                }

                NkString ToString() const override {
                    return "NkDropTextEvent(" + data.ToString() + ")";
                }
        };

        // -------------------------------------------------------------------------
        // Structure : NkDropImageData
        // -------------------------------------------------------------------------
        // Description :
        //   Conteneur pour les données d'un événement de drop d'image.
        //   Supporte à la fois les images par URI et les données pixel brutes.
        // Champs :
        //   - x, y        : Position de dépose dans la zone client [pixels]
        //   - sourceUri   : URI de la source de l'image (fichier, data:, http://...)
        //   - mimeType    : Type MIME de l'image ("image/png", "image/jpeg"...)
        //   - width/height: Dimensions de l'image en pixels (si disponibles)
        //   - pixels      : Données RGBA8 brutes (optionnel, peut être vide)
        // Méthodes :
        //   - HasPixels() : Retourne true si les données pixel sont disponibles.
        //   - ToString()  : Formatage lisible avec dimensions et type MIME.
        // Utilisation :
        //   - Import d'images depuis le gestionnaire de fichiers.
        //   - Collage d'images depuis d'autres applications (capture d'écran...).
        //   - Prévisualisation immédiate via données pixel si disponibles.
        // Notes :
        //   - Le format pixels est RGBA8 (4 octets par pixel, ordre R-G-B-A).
        //   - Si pixels est vide, utiliser sourceUri pour chargement différé.
        //   - Les dimensions peuvent être 0 si non disponibles dans les métadonnées.
        // -------------------------------------------------------------------------
        struct NkDropImageData {
            int32 x = 0;
            int32 y = 0;
            NkString sourceUri;
            NkString mimeType;
            uint32 width = 0;
            uint32 height = 0;
            NkVector<uint8> pixels;

            bool HasPixels() const {
                return !pixels.empty();
            }

            NkString ToString() const {
                return NkString::Fmt("DropImage({0}x{1} {2})", width, height, mimeType);
            }
        };

        // -------------------------------------------------------------------------
        // Classe : NkDropImageEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'une image est déposée dans la zone 
        //   de la fenêtre cible.
        // Déclenchement :
        //   - Relâchement d'un drag d'image depuis une source externe.
        //   - Validation de l'opération par le système de drag & drop hôte.
        // Données transportées :
        //   - data : Structure NkDropImageData avec métadonnées et pixels optionnels.
        // Utilisation typique :
        //   - Import d'images dans un éditeur ou visualiseur.
        //   - Création de textures GPU à partir des données pixel.
        //   - Affichage de prévisualisation immédiate si pixels disponibles.
        // Notes :
        //   - Vérifier HasPixels() avant d'accéder au vecteur pixels.
        //   - Utiliser sourceUri comme fallback si pixels n'est pas disponible.
        //   - Le mimeType guide le décodage si un rechargement est nécessaire.
        // -------------------------------------------------------------------------
        class NkDropImageEvent final : public NkEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_DROP_IMAGE)

                NkDropImageData data;

                explicit NkDropImageEvent(
                    const NkDropImageData& d = {},
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , data(d) {
                }

                NkEvent* Clone() const override {
                    return new NkDropImageEvent(*this);
                }

                NkString ToString() const override {
                    return "NkDropImageEvent(" + data.ToString() + ")";
                }
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKDROPEVENT_H

// =============================================================================
// Section : Exemples d'utilisation (à titre informatif - en commentaire)
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Gestion basique d'un drop de fichiers
// -----------------------------------------------------------------------------
void OnDropFileEvent(const NkDropFileEvent& event) {
    // Récupération des chemins de fichiers déposés
    const auto& fileData = event.data;
    const uint32 fileCount = fileData.Count();

    NK_LOG_INFO(
        "Fichiers déposés : {} fichier(s) à la position ({},{})",
        fileCount,
        fileData.x,
        fileData.y
    );

    // Traitement de chaque fichier déposé
    for (uint32 i = 0; i < fileCount; ++i) {
        const NkString& filePath = fileData.paths[i];

        // Vérification de l'existence du fichier
        // if (!FileSystem::FileExists(filePath)) {
        //     NK_LOG_WARN("Fichier introuvable : {}", filePath);
        //     continue;
        // }

        // Détermination du type de fichier par extension
        // const NkString extension = FileSystem::GetExtension(filePath);
        // if (extension == ".png" || extension == ".jpg") {
        //     // Import d'image
        //     // TextureLoader::LoadAsync(filePath);
        // } else if (extension == ".obj" || extension == ".fbx") {
        //     // Import de modèle 3D
        //     // ModelLoader::LoadAsync(filePath);
        // } else if (extension == ".json" || extension == ".xml") {
        //     // Import de configuration
        //     // ConfigLoader::Load(filePath);
        // } else {
        //     NK_LOG_DEBUG("Type de fichier non supporté : {}", extension);
        // }
    }
}

// -----------------------------------------------------------------------------
// Exemple 2 : Gestion d'un drop de texte avec type MIME
// -----------------------------------------------------------------------------
void OnDropTextEvent(const NkDropTextEvent& event) {
    const auto& textData = event.data;

    NK_LOG_DEBUG(
        "Texte déposé : type={}, contenu=\"{}\"",
        textData.mimeType,
        textData.text.Size() > 100 
            ? textData.text.SubStr(0, 100) + "..." 
            : textData.text
    );

    // Traitement selon le type MIME
    if (textData.mimeType == "text/plain") {
        // Insertion de texte brut dans un champ éditable
        // EditTextWidget::InsertAt(textData.x, textData.y, textData.text);
    } else if (textData.mimeType == "text/html") {
        // Parsing et sanitization du HTML avant insertion
        // NkString sanitized = HtmlSanitizer::Sanitize(textData.text);
        // RichTextWidget::InsertHtml(sanitized);
    } else if (textData.mimeType == "text/uri-list") {
        // Liste d'URLs : extraction et traitement individuel
        // NkVector<NkString> urls = NkStringUtils::Split(textData.text, "\r\n");
        // for (const auto& url : urls) {
        //     if (!url.Empty() && url[0] != '#') {
        //         // UrlHandler::Open(url);
        //     }
        // }
    } else {
        // Fallback : traitement comme texte brut
        // EditTextWidget::InsertAt(textData.x, textData.y, textData.text);
    }
}

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion d'un drop d'image avec fallback URI/pixels
// -----------------------------------------------------------------------------
void OnDropImageEvent(const NkDropImageEvent& event) {
    const auto& imageData = event.data;

    NK_LOG_INFO(
        "Image déposée : {}x{} {}, URI={}",
        imageData.width,
        imageData.height,
        imageData.mimeType,
        imageData.sourceUri
    );

    // Priorité aux données pixel si disponibles pour affichage immédiat
    if (imageData.HasPixels()) {
        // Création de texture GPU directe depuis les pixels RGBA8
        // auto texture = Texture2D::CreateFromRGBA8(
        //     imageData.width,
        //     imageData.height,
        //     imageData.pixels.Data()
        // );
        // ImageViewer::SetTexture(texture);
    } else {
        // Fallback : chargement asynchrone depuis l'URI
        // if (!imageData.sourceUri.Empty()) {
        //     ImageLoader::LoadAsync(
        //         imageData.sourceUri,
        //         [](const ImageData& loaded) {
        //             // ImageViewer::SetTexture(loaded.CreateTexture());
        //         }
        //     );
        // }
    }
}

// -----------------------------------------------------------------------------
// Exemple 4 : Feedback visuel pendant le drag (enter/over/leave)
// -----------------------------------------------------------------------------
class DropZoneWidget {
public:
    void OnDropEnter(const NkDropEnterEvent& event) {
        const auto& data = event.data;

        // Accepter uniquement les fichiers et images
        if (data.numFiles > 0 || data.hasImage) {
            mIsDropTarget = true;
            mDropPosition = { data.x, data.y };
            // SetVisualState(DropState::ACCEPTING);
            NK_LOG_DEBUG("Zone prête à accepter le drop");
        } else {
            mIsDropTarget = false;
            // SetVisualState(DropState::REJECTING);
            NK_LOG_DEBUG("Type de drop non supporté");
        }
    }

    void OnDropOver(const NkDropOverEvent& event) {
        if (!mIsDropTarget) {
            return;
        }

        // Mise à jour de la position pour le feedback visuel
        mDropPosition = { event.data.x, event.data.y };
        // UpdateDropIndicator(mDropPosition);
    }

    void OnDropLeave(const NkDropLeaveEvent& event) {
        mIsDropTarget = false;
        // SetVisualState(DropState::NORMAL);
        // HideDropIndicator();
    }

private:
    bool mIsDropTarget = false;
    NkPoint mDropPosition;
};

// -----------------------------------------------------------------------------
// Exemple 5 : Enregistrement des listeners d'événements de drop
// -----------------------------------------------------------------------------
void RegisterDropEventListeners(NkEventManager& eventManager, DropZoneWidget* dropZone) {
    // Événements de phase de drag (enter/over/leave)
    eventManager.Subscribe<NkDropEnterEvent>(
        [dropZone](const NkDropEnterEvent& e) {
            if (dropZone) {
                dropZone->OnDropEnter(e);
            }
        }
    );

    eventManager.Subscribe<NkDropOverEvent>(
        [dropZone](const NkDropOverEvent& e) {
            if (dropZone) {
                dropZone->OnDropOver(e);
            }
        }
    );

    eventManager.Subscribe<NkDropLeaveEvent>(
        [dropZone](const NkDropLeaveEvent& e) {
            if (dropZone) {
                dropZone->OnDropLeave(e);
            }
        }
    );

    // Événements de drop effectif (fichiers, texte, image)
    eventManager.Subscribe<NkDropFileEvent>(
        [](const NkDropFileEvent& e) {
            OnDropFileEvent(e);
        }
    );

    eventManager.Subscribe<NkDropTextEvent>(
        [](const NkDropTextEvent& e) {
            OnDropTextEvent(e);
        }
    );

    eventManager.Subscribe<NkDropImageEvent>(
        [](const NkDropImageEvent& e) {
            OnDropImageEvent(e);
        }
    );
}

// -----------------------------------------------------------------------------
// Exemple 6 : Activation du support drag & drop sur une fenêtre
// -----------------------------------------------------------------------------
void EnableDropSupportForWindow(void* nativeWindowHandle) {
    // Activation du backend de drop pour la fenêtre native
    // NkEnableDropTarget(nativeWindowHandle);

    NK_LOG_INFO("Support drag & drop activé pour la fenêtre");

    // Optionnel : configuration des types acceptés (si supporté par le backend)
    // NkDropTargetConfig config;
    // config.AcceptFiles(true);
    // config.AcceptText(true);
    // config.AcceptImages(true);
    // NkConfigureDropTarget(nativeWindowHandle, config);
}

// -----------------------------------------------------------------------------
// Exemple 7 : Validation des fichiers déposés avant traitement
// -----------------------------------------------------------------------------
bool ValidateDroppedFiles(const NkDropFileData& fileData) {
    // Vérification du nombre de fichiers (limite optionnelle)
    constexpr uint32 MAX_BATCH_SIZE = 10;
    if (fileData.Count() > MAX_BATCH_SIZE) {
        NK_LOG_WARN(
            "Trop de fichiers déposés : {} (max: {})",
            fileData.Count(),
            MAX_BATCH_SIZE
        );
        return false;
    }

    // Vérification de chaque fichier
    for (uint32 i = 0; i < fileData.Count(); ++i) {
        const NkString& path = fileData.paths[i];

        // Existence et accessibilité
        // if (!FileSystem::FileExists(path) || !FileSystem::IsReadable(path)) {
        //     NK_LOG_ERROR("Fichier inaccessible : {}", path);
        //     return false;
        // }

        // Extension supportée
        // const NkString ext = FileSystem::GetExtension(path);
        // if (!SUPPORTED_EXTENSIONS.Contains(ext)) {
        //     NK_LOG_WARN("Extension non supportée : {} pour {}", ext, path);
        //     return false;
        // }

        // Taille raisonnable (optionnel)
        // const uint64 size = FileSystem::GetFileSize(path);
        // constexpr uint64 MAX_FILE_SIZE = 100 * 1024 * 1024; // 100 MB
        // if (size > MAX_FILE_SIZE) {
        //     NK_LOG_WARN("Fichier trop volumineux : {} ({:.1f} MB)", path, size / 1e6);
        //     return false;
        // }
    }

    return true;
}

// -----------------------------------------------------------------------------
// Exemple 8 : Test unitaire des structures de données de drop
// -----------------------------------------------------------------------------
void TestDropEventDataStructures() {
    // Test NkDropEnterData
    nkentseu::NkDropEnterData enterData;
    enterData.x = 100;
    enterData.y = 200;
    enterData.numFiles = 3;
    enterData.hasText = true;
    NK_LOG_DEBUG("EnterData: {}", enterData.ToString());
    // Attendu: "DropEnter(at 100,200 files=3 text)"

    // Test NkDropFileData
    nkentseu::NkDropFileData fileData;
    fileData.x = 50;
    fileData.y = 75;
    fileData.AddPath("/path/to/file1.txt");
    fileData.AddPath("/path/to/file2.png");
    NK_ASSERT(fileData.Count() == 2);
    NK_LOG_DEBUG("FileData: {}", fileData.ToString());
    // Attendu: "DropFile(2 file(s) at 50,75)"

    // Test NkDropTextData avec texte long
    nkentseu::NkDropTextData textData;
    textData.text = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
                    "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
    textData.mimeType = "text/plain";
    NK_LOG_DEBUG("TextData: {}", textData.ToString());
    // Attendu: aperçu tronqué avec "..."

    // Test NkDropImageData avec pixels
    nkentseu::NkDropImageData imageData;
    imageData.width = 1920;
    imageData.height = 1080;
    imageData.mimeType = "image/png";
    imageData.pixels.Resize(1920 * 1080 * 4); // RGBA8
    NK_ASSERT(imageData.HasPixels() == true);
    NK_LOG_DEBUG("ImageData: {}", imageData.ToString());
    // Attendu: "DropImage(1920x1080 image/png)"

    NK_LOG_INFO("Tests des structures de drop passés avec succès");
}
*/