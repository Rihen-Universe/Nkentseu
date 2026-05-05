#pragma once
// -----------------------------------------------------------------------------
// FICHIER: NkECS/Components/UI/NkUIComponent.h
// DESCRIPTION: Composants d'interface utilisateur 2D et 3D.
//
//  UI 2D (canvas screen-space) :
//    NkCanvas        — canvas racine (référentiel de l'UI 2D)
//    NkRectTransform — transform 2D ancré dans un canvas
//    NkUIImage       — image/sprite UI
//    NkUIText        — texte UI
//    NkUIButton      — bouton interactif
//    NkUISlider      — slider
//    NkUIProgressBar — barre de progression
//    NkUIScrollView  — zone scrollable
//    NkUIPanel       — panel/groupe
//    NkUIInputField  — champ de saisie texte
//    NkUIToggle      — case à cocher
//    NkUIDropdown    — liste déroulante
//
//  UI 3D (world-space) :
//    NkWorldCanvas   — canvas dans l'espace 3D (panneau holographique)
//    NkBillboard     — sprite qui fait face à la caméra
//    NkNameTag       — étiquette de nom au-dessus d'un personnage
//    NkHealthBar3D   — barre de vie en espace 3D
// -----------------------------------------------------------------------------

#include "../../NkECSDefines.h"
#include "../../Core/NkTypeRegistry.h"
#include "../Core/NkTransform.h"
#include <cstring>

namespace nkentseu { namespace ecs {

// =============================================================================
// Types communs UI
// =============================================================================

// Ancrage dans un rectangle parent (pivot + offset)
struct NkAnchor {
    float32 minX = 0.5f, minY = 0.5f; // [0..1] relatif au parent
    float32 maxX = 0.5f, maxY = 0.5f;
};

// Prédéfinitions d'ancrage standard
namespace NkAnchors {
    inline NkAnchor TopLeft()      { return {0,0,0,0}; }
    inline NkAnchor TopCenter()    { return {0.5f,0,0.5f,0}; }
    inline NkAnchor TopRight()     { return {1,0,1,0}; }
    inline NkAnchor MiddleLeft()   { return {0,0.5f,0,0.5f}; }
    inline NkAnchor MiddleCenter() { return {0.5f,0.5f,0.5f,0.5f}; }
    inline NkAnchor MiddleRight()  { return {1,0.5f,1,0.5f}; }
    inline NkAnchor BottomLeft()   { return {0,1,0,1}; }
    inline NkAnchor BottomCenter() { return {0.5f,1,0.5f,1}; }
    inline NkAnchor BottomRight()  { return {1,1,1,1}; }
    inline NkAnchor StretchFull()  { return {0,0,1,1}; }
    inline NkAnchor StretchH()     { return {0,0.5f,1,0.5f}; }
    inline NkAnchor StretchV()     { return {0.5f,0,0.5f,1}; }
}

// Alignement horizontal/vertical du texte
enum class NkTextAlign : uint8 {
    Left   = 0, Center = 1, Right = 2,
    Top    = 0, Middle = 1, Bottom = 2
};

enum class NkOverflow : uint8 { Visible = 0, Hidden = 1, Scroll = 2, Truncate = 3 };
enum class NkUIState  : uint8 { Normal = 0, Hovered = 1, Pressed = 2, Disabled = 3, Selected = 4 };

// Marges intérieures (padding) et extérieures (margin)
struct NkEdges {
    float32 left = 0, right = 0, top = 0, bottom = 0;
    NkEdges() = default;
    constexpr NkEdges(float32 all) : left(all),right(all),top(all),bottom(all) {}
    constexpr NkEdges(float32 h, float32 v) : left(h),right(h),top(v),bottom(v) {}
    constexpr NkEdges(float32 l, float32 r, float32 t, float32 b)
        : left(l),right(r),top(t),bottom(b) {}
};

// Couleur 4 RGBA (redéclarée ici pour éviter la dépendance à NkRenderer)
// Si NkRenderer.h est inclus, NkColor4 est déjà défini — on utilise celle-ci.

// =============================================================================
// NkCanvas — racine d'un arbre UI 2D
// =============================================================================

enum class NkCanvasMode : uint8 {
    ScreenSpace = 0,   // pixels écran
    WorldSpace  = 1,   // espace 3D (hologramme)
    CameraSpace = 2,   // relatif à une caméra
};

struct NkCanvas {
    NkCanvasMode  mode            = NkCanvasMode::ScreenSpace;
    float32       referenceWidth  = 1920.f;   // résolution de référence
    float32       referenceHeight = 1080.f;
    float32       pixelsPerUnit   = 100.f;    // WorldSpace : unités monde / pixel
    float32       planeDistance   = 1.f;      // CameraSpace : distance devant la caméra
    NkEntityId    cameraEntity    = NkEntityId::Invalid(); // caméra associée
    bool          scaleWithScreen = true;     // adapte au DPI/résolution
    float32       scaleFactor     = 1.f;      // calculé auto si scaleWithScreen
    uint32        sortingOrder    = 0;        // Z-order entre plusieurs canvas
    bool          visible         = true;
    bool          interactable    = true;
    uint32        targetTextureId = 0;        // 0 = rendu sur écran
};
NK_COMPONENT(NkCanvas)

// =============================================================================
// NkRectTransform — transform 2D dans un canvas
// =============================================================================

struct NkRectTransform {
    NkAnchor  anchor        = NkAnchors::MiddleCenter();
    NkVec2    pivot         = {0.5f, 0.5f};  // point d'origine local [0..1]
    NkVec2    anchoredPos   = {};             // offset depuis le point d'ancrage
    NkVec2    sizeDelta     = {100.f, 30.f}; // taille en pixels
    NkVec2    localScale    = {1.f, 1.f};
    float32   localRotation = 0.f;            // degrés (CCW)
    bool      visible       = true;

    // Rect calculé (en pixels écran) — mis à jour par NkUILayoutSystem
    float32   rectX = 0, rectY = 0;          // coin haut-gauche
    float32   rectW = 100, rectH = 30;       // dimensions finales

    [[nodiscard]] bool Contains(float32 px, float32 py) const noexcept {
        return px >= rectX && px <= rectX + rectW
            && py >= rectY && py <= rectY + rectH;
    }

    [[nodiscard]] NkVec2 Center() const noexcept {
        return {rectX + rectW * 0.5f, rectY + rectH * 0.5f};
    }
};
NK_COMPONENT(NkRectTransform)

// =============================================================================
// NkUIImage — image / sprite 2D en UI
// =============================================================================

enum class NkImageType : uint8 {
    Simple   = 0,  // image étirée
    Sliced   = 1,  // 9-slice (bords fixes)
    Tiled    = 2,  // répétée
    Filled   = 3,  // remplie progressivement (health bar style)
};

enum class NkFillMethod : uint8 { Horizontal = 0, Vertical = 1, Radial90 = 2, Radial180 = 3, Radial360 = 4 };

struct NkUIImage {
    uint32       textureId    = 0;
    NkColor4     color        = NkColor4::White();
    NkImageType  imageType    = NkImageType::Simple;
    bool         preserveAspect = false;
    bool         raycastTarget  = true;
    bool         visible        = true;

    // 9-slice
    NkEdges      border       = {};  // bordures de slice (pixels dans la texture)

    // Filled
    NkFillMethod fillMethod   = NkFillMethod::Horizontal;
    float32      fillAmount   = 1.f;  // [0..1]
    bool         fillClockwise = true;
    float32      fillOrigin   = 0.f;  // angle de départ (Radial)

    NkBlendMode  blendMode    = NkBlendMode::AlphaBlend;
    uint32       materialId   = 0;
};
NK_COMPONENT(NkUIImage)

// =============================================================================
// NkUIText — texte rendu en UI
// =============================================================================

struct NkUIText {
    static constexpr uint32 kMaxText = 1024u;

    char     text[kMaxText]   = {};
    uint32   fontId           = 0;
    float32  fontSize         = 14.f;
    NkColor4 color            = NkColor4::White();
    NkTextAlign alignH        = NkTextAlign::Left;
    NkTextAlign alignV        = NkTextAlign::Top;
    NkOverflow  overflow      = NkOverflow::Truncate;
    bool     bold             = false;
    bool     italic           = false;
    bool     underline        = false;
    bool     strikethrough    = false;
    bool     raycastTarget    = false;
    bool     visible          = true;
    float32  lineSpacing      = 1.2f;
    float32  characterSpacing = 0.f;
    NkColor4 outlineColor     = NkColor4::Black();
    float32  outlineWidth     = 0.f;
    NkColor4 shadowColor      = NkColor4{0,0,0,0.5f};
    NkVec2   shadowOffset     = {1.f, -1.f};
    bool     enableShadow     = false;

    void SetText(const char* t) noexcept {
        std::strncpy(text, t ? t : "", kMaxText - 1);
    }

    // Formatage simple printf-style
    void SetTextf(const char* fmt, ...) noexcept;
};
NK_COMPONENT(NkUIText)

// =============================================================================
// NkUIButton — bouton interactif
// =============================================================================

struct NkUIButtonColors {
    NkColor4 normal   = NkColor4::White();
    NkColor4 hovered  = NkColor4{0.9f, 0.9f, 0.9f, 1.f};
    NkColor4 pressed  = NkColor4{0.8f, 0.8f, 0.8f, 1.f};
    NkColor4 disabled = NkColor4{0.6f, 0.6f, 0.6f, 0.5f};
    NkColor4 selected = NkColor4{0.85f, 0.95f, 1.f, 1.f};
    float32  fadeDuration = 0.1f;
};

enum class NkButtonTransition : uint8 {
    None      = 0,  // pas d'animation
    ColorTint = 1,  // teinte de couleur
    SpriteSwap = 2, // échange de sprite
    Animation  = 3, // clip d'animation
};

struct NkUIButton {
    bool              interactable   = true;
    NkUIState         state          = NkUIState::Normal;
    NkButtonTransition transition    = NkButtonTransition::ColorTint;
    NkUIButtonColors  colors         = {};
    float32           stateTimer     = 0.f;   // animation de transition
    bool              isToggle       = false;
    bool              isOn           = false;

    // Sprite swap
    uint32  normalSprite   = 0;
    uint32  hoveredSprite  = 0;
    uint32  pressedSprite  = 0;
    uint32  disabledSprite = 0;
    uint32  selectedSprite = 0;

    // Sounds
    uint32  clickSoundId   = 0;
    uint32  hoverSoundId   = 0;

    // Event tags (l'EventBus dispatch OnButtonClicked avec cet ID)
    uint32  buttonId       = 0;
    char    buttonTag[64]  = {};

    void Click() noexcept {
        if (!interactable) return;
        state = NkUIState::Pressed;
        if (isToggle) isOn = !isOn;
    }
};
NK_COMPONENT(NkUIButton)

// =============================================================================
// NkUISlider — slider horizontal / vertical
// =============================================================================

struct NkUISlider {
    float32  value         = 0.f;   // valeur courante
    float32  minValue      = 0.f;
    float32  maxValue      = 1.f;
    bool     wholeNumbers  = false;
    bool     vertical      = false;
    bool     interactable  = true;
    NkUIState state        = NkUIState::Normal;

    uint32   backgroundId  = 0;   // image de fond
    uint32   fillId        = 0;   // image de remplissage
    uint32   handleId      = 0;   // handle draggable

    NkColor4 fillColor     = NkColor4{0.2f, 0.6f, 1.f, 1.f};
    NkColor4 handleColor   = NkColor4::White();
    float32  handleSize    = 20.f;

    [[nodiscard]] float32 Normalized() const noexcept {
        if (maxValue <= minValue) return 0.f;
        return (value - minValue) / (maxValue - minValue);
    }

    void SetNormalized(float32 t) noexcept {
        value = minValue + (maxValue - minValue) * (t < 0.f ? 0.f : t > 1.f ? 1.f : t);
        if (wholeNumbers) value = std::round(value);
    }
};
NK_COMPONENT(NkUISlider)

// =============================================================================
// NkUIProgressBar — barre de progression (non interactive)
// =============================================================================

struct NkUIProgressBar {
    float32  value         = 0.f;   // [0..1]
    bool     vertical      = false;
    bool     reverse       = false; // de droite à gauche
    NkColor4 backgroundColor = NkColor4{0.2f, 0.2f, 0.2f, 1.f};
    NkColor4 fillColor       = NkColor4{0.2f, 0.6f, 1.f, 1.f};
    NkColor4 textColor       = NkColor4::White();
    bool     showLabel     = true;
    char     labelFormat[32] = "{0:.0f}%"; // format : {0} = valeur 0..100
    bool     animate       = false;   // smooth animation
    float32  animSpeed     = 2.f;     // unités par seconde
    float32  displayedValue = 0.f;    // valeur animée courante
    bool     visible       = true;
};
NK_COMPONENT(NkUIProgressBar)

// =============================================================================
// NkUIScrollView — zone défilable
// =============================================================================

struct NkUIScrollView {
    bool     horizontal    = false;
    bool     vertical      = true;
    float32  scrollSensitivity = 1.f;
    NkVec2   normalizedPosition = {};  // [0..1] position du scroll
    float32  contentHeight = 0.f;     // hauteur du contenu (calculé)
    float32  contentWidth  = 0.f;
    bool     inertia       = true;    // rebond / momentum
    float32  decelerationRate = 0.135f;
    NkVec2   velocity      = {};      // vélocité courante du scroll
    bool     showScrollbarH = true;
    bool     showScrollbarV = true;
    float32  scrollbarSize = 8.f;
    NkColor4 scrollbarColor = NkColor4{0.5f,0.5f,0.5f,0.7f};
    bool     visible       = true;
    bool     interactable  = true;
};
NK_COMPONENT(NkUIScrollView)

// =============================================================================
// NkUIPanel — conteneur visuel (fond, bordure)
// =============================================================================

struct NkUIPanel {
    NkColor4  backgroundColor = NkColor4{0.1f,0.1f,0.15f,0.9f};
    uint32    backgroundTextureId = 0;
    NkEdges   padding         = NkEdges(8.f);
    float32   cornerRadius    = 4.f;
    NkColor4  borderColor     = NkColor4{0.3f,0.3f,0.4f,1.f};
    float32   borderWidth     = 1.f;
    bool      visible         = true;
    bool      raycastTarget   = false;
    bool      clipContent     = true;  // coupe le contenu aux bords
};
NK_COMPONENT(NkUIPanel)

// =============================================================================
// NkUIInputField — champ de saisie texte
// =============================================================================

enum class NkInputContentType : uint8 {
    Standard    = 0,
    Integer     = 1,
    Decimal     = 2,
    Alphanumeric = 3,
    Password    = 4,
    Email       = 5,
    Custom      = 6,
};

struct NkUIInputField {
    static constexpr uint32 kMaxText = 512u;
    char    text[kMaxText]      = {};
    char    placeholder[128]    = "Enter text...";
    char    passwordMask        = '*';
    NkInputContentType contentType = NkInputContentType::Standard;
    uint32  maxLength           = 256u;
    bool    multiline           = false;
    bool    interactable        = true;
    bool    isFocused           = false;
    uint32  caretPos            = 0;
    bool    caretVisible        = false;
    float32 caretBlinkRate      = 0.85f;
    float32 caretTimer          = 0.f;
    NkColor4 textColor          = NkColor4::White();
    NkColor4 placeholderColor   = NkColor4{0.6f,0.6f,0.6f,1.f};
    NkColor4 caretColor         = NkColor4::White();
    NkColor4 selectionColor     = NkColor4{0.2f,0.5f,1.f,0.5f};
    uint32  fontId              = 0;
    float32 fontSize            = 14.f;
    bool    visible             = true;

    void SetText(const char* t) noexcept {
        std::strncpy(text, t ? t : "", kMaxText - 1);
        caretPos = static_cast<uint32>(std::strlen(text));
    }

    [[nodiscard]] bool IsEmpty() const noexcept { return text[0] == '\0'; }
};
NK_COMPONENT(NkUIInputField)

// =============================================================================
// NkUIToggle — case à cocher
// =============================================================================

struct NkUIToggle {
    bool     isOn          = false;
    bool     interactable  = true;
    NkUIState state        = NkUIState::Normal;
    NkColor4 normalColor   = NkColor4::White();
    NkColor4 checkedColor  = NkColor4{0.2f,0.6f,1.f,1.f};
    NkColor4 hoveredColor  = NkColor4{0.9f,0.9f,0.9f,1.f};
    uint32   checkmarkId   = 0;   // sprite de la coche
    uint32   toggleGroupId = 0;   // pour exclusion mutuelle (radio buttons)
    char     label[128]    = {};
    bool     visible       = true;
};
NK_COMPONENT(NkUIToggle)

// =============================================================================
// NkUIDropdown — liste déroulante
// =============================================================================

struct NkUIDropdown {
    static constexpr uint32 kMaxOptions = 64u;
    char    options[kMaxOptions][128] = {};
    uint32  optionCount = 0;
    int32   selectedIndex = 0;
    bool    isOpen       = false;
    bool    interactable = true;
    NkUIState state      = NkUIState::Normal;
    float32  maxHeight   = 300.f;  // hauteur max de la liste
    uint32   itemHeight  = 30u;
    NkColor4 itemHoveredColor = NkColor4{0.2f,0.5f,1.f,1.f};
    NkColor4 itemSelectedColor = NkColor4{0.15f,0.4f,0.8f,1.f};
    bool     visible     = true;

    void AddOption(const char* opt) noexcept {
        if (optionCount < kMaxOptions)
            std::strncpy(options[optionCount++], opt, 127);
    }

    [[nodiscard]] const char* GetSelected() const noexcept {
        if (selectedIndex >= 0 && selectedIndex < (int32)optionCount)
            return options[selectedIndex];
        return "";
    }
};
NK_COMPONENT(NkUIDropdown)

// =============================================================================
// NkUILayout — layout automatique des enfants (horizontal/vertical/grid)
// =============================================================================

enum class NkUILayoutType : uint8 {
    None       = 0,
    Horizontal = 1,
    Vertical   = 2,
    Grid       = 3,
};

enum class NkUIAlignment : uint8 {
    Start  = 0, Center = 1, End = 2, SpaceBetween = 3, SpaceAround = 4
};

struct NkUILayout {
    NkUILayoutType  type          = NkUILayoutType::Vertical;
    NkUIAlignment   alignment     = NkUIAlignment::Start;
    NkEdges         padding       = NkEdges(4.f);
    float32         spacing       = 4.f;
    bool            controlWidth  = false;
    bool            controlHeight = false;
    bool            forceExpand   = false;
    uint32          columns       = 2u;   // Grid : nb de colonnes
    NkVec2          cellSize      = {100.f, 100.f}; // Grid : taille des cellules
    NkVec2          cellSpacing   = {4.f, 4.f};
    bool            reverseArrangement = false;
};
NK_COMPONENT(NkUILayout)

// =============================================================================
// NkUITooltip — info-bulle
// =============================================================================

struct NkUITooltip {
    char     text[512]     = {};
    float32  delay         = 0.5f;  // secondes avant affichage
    float32  timer         = 0.f;   // temps de survol courant
    bool     visible       = false;
    NkColor4 backgroundColor = NkColor4{0.1f,0.1f,0.15f,0.95f};
    NkColor4 textColor       = NkColor4::White();
    float32  maxWidth      = 200.f;
    float32  padding       = 8.f;
};
NK_COMPONENT(NkUITooltip)

// =============================================================================
// UI 3D — World-Space
// =============================================================================

// NkBillboard — sprite qui fait toujours face à la caméra
enum class NkBillboardMode : uint8 {
    FullFacing    = 0,  // tourne sur X et Y
    VerticalOnly  = 1,  // tourne sur Y uniquement (debout)
    Horizontal    = 2,  // plan horizontal (toujours à plat)
};

struct NkBillboard {
    NkBillboardMode mode      = NkBillboardMode::FullFacing;
    bool    lockScale         = false;   // taille constante quelle que soit la distance
    float32 screenSize        = 100.f;  // pixels si lockScale
    NkVec2  offset            = {};     // décalage en pixels
    bool    visible           = true;
    uint32  textureId         = 0;
    NkColor4 color            = NkColor4::White();
    NkVec2  size              = {1.f, 1.f};
};
NK_COMPONENT(NkBillboard)

// NkWorldCanvas — canvas 3D dans l'espace monde
struct NkWorldCanvas {
    float32  width            = 2.f;    // largeur en unités monde
    float32  height           = 1.f;
    float32  pixelsPerUnit    = 100.f;
    bool     faceCamera       = false;  // si true → billboard
    bool     visible          = true;
    uint32   targetTextureId  = 0;      // rendu dans cette texture
    uint32   renderTextureW   = 512u;
    uint32   renderTextureH   = 256u;
};
NK_COMPONENT(NkWorldCanvas)

// NkNameTag — étiquette de nom flottante au-dessus d'une entité
struct NkNameTag {
    char     displayName[128] = {};
    NkColor4 nameColor        = NkColor4::White();
    NkColor4 backgroundColor  = NkColor4{0,0,0,0.6f};
    float32  yOffset          = 2.f;   // décalage vertical (unités monde)
    float32  scale            = 1.f;
    float32  visibleDistance  = 20.f;  // au-delà = invisible
    bool     visible          = true;
    bool     alwaysOnTop      = false; // rendu par-dessus la géométrie
    uint32   fontId           = 0;
    float32  fontSize         = 14.f;
};
NK_COMPONENT(NkNameTag)

// NkHealthBar3D — barre de vie dans l'espace 3D
struct NkHealthBar3D {
    float32  value            = 1.f;    // [0..1]
    float32  targetValue      = 1.f;    // valeur cible (avec animation)
    float32  animSpeed        = 3.f;
    NkColor4 fullColor        = NkColor4{0.2f, 0.8f, 0.2f, 1.f};
    NkColor4 emptyColor       = NkColor4{0.8f, 0.1f, 0.1f, 1.f};
    NkColor4 backgroundColor  = NkColor4{0,0,0,0.7f};
    float32  width            = 1.f;
    float32  height           = 0.1f;
    float32  yOffset          = 2.2f;
    bool     visible          = true;
    bool     hideWhenFull     = true;
    bool     faceCamera       = true;
};
NK_COMPONENT(NkHealthBar3D)

// NkUIMarker3D — marqueur de position (waypoint, objectif)
struct NkUIMarker3D {
    uint32   iconId           = 0;
    NkColor4 color            = NkColor4::Yellow();
    float32  visibleDistance  = 100.f;
    float32  minScreenSize    = 16.f;   // taille min sur l'écran (pixels)
    float32  maxScreenSize    = 64.f;
    bool     visible          = true;
    bool     clampToScreen    = true;   // si hors écran, se place sur le bord
    float32  clampPadding     = 32.f;   // pixels du bord
    char     label[64]        = {};
    float32  distance         = 0.f;    // calculé chaque frame
    bool     showDistance     = true;
    char     distanceUnit[8]  = "m";    // ex: "m", "km", "ft"
};
NK_COMPONENT(NkUIMarker3D)

// =============================================================================
// NkHUD — heads-up display (overlay 2D sur la vue 3D)
// =============================================================================

struct NkHUDCrosshair {
    bool    visible        = true;
    uint32  textureId      = 0;    // 0 = dessiné procéduralement
    NkColor4 color         = NkColor4::White();
    float32 size           = 16.f;
    float32 gap            = 4.f;  // espace central
    float32 thickness      = 2.f;
    bool    dynamicSpread  = false; // s'écarte selon la vélocité
    float32 spreadAmount   = 0.f;  // écart courant
};
NK_COMPONENT(NkHUDCrosshair)

struct NkHUDMinimap {
    bool    visible        = true;
    uint32  mapTextureId   = 0;
    float32 size           = 150.f;  // pixels
    float32 range          = 50.f;   // unités monde affichées
    NkVec2  screenPos      = {10.f, 10.f}; // coin de l'écran
    bool    rotate         = true;   // pivote avec le joueur
    uint32  playerIconId   = 0;
    NkColor4 borderColor   = NkColor4{0.3f,0.3f,0.3f,0.8f};
    float32 zoom           = 1.f;
    bool    circular       = true;   // forme circulaire
};
NK_COMPONENT(NkHUDMinimap)

// =============================================================================
// Events UI — dispatché par l'UISystem
// =============================================================================

struct NkOnButtonClicked  { NkEntityId button; uint32 buttonId; char tag[64]; };
struct NkOnSliderChanged  { NkEntityId slider; float32 value;   float32 normalized; };
struct NkOnToggleChanged  { NkEntityId toggle; bool isOn; };
struct NkOnInputChanged   { NkEntityId field;  char text[512]; };
struct NkOnInputSubmitted { NkEntityId field;  char text[512]; };
struct NkOnDropdownChanged { NkEntityId dropdown; int32 index; char option[128]; };
struct NkOnScrollChanged  { NkEntityId scroll; NkVec2 position; };

}} // namespace nkentseu::ecs
