#pragma once
/**
 * @File    NkRichText.h
 * @Brief   Système de texte enrichi pour NkUI — support des styles, couleurs, 
 *          polices multiples et coloration syntaxique.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkRichText permet de créer des textes stylisés avec:
 *  - Plusieurs polices et tailles
 *  - Couleurs personnalisées
 *  - Gras, italique, souligné, barré
 *  - Exposant et indice
 *  - Listes à puces et numérotées
 *  - Coloration syntaxique pour le code
 *  - Support HTML simplifié
 */

#include "NKUI/NkUIFont.h"
#include "NKUI/NkUIInput.h"
#include "NKFont/NkMemArena.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace nkui {

        // ============================================================================
        //  Structures de base pour le texte enrichi
        // ============================================================================

        /**
         * @struct NkTextFragment
         * @brief Fragment de texte avec ses propriétés de style
         */
        struct NKUI_API NkTextFragment {
            const char* text = nullptr;     ///< Le texte (doit rester valide)
            NkColor     color = {255,255,255,255};  ///< Couleur du texte
            NkUIFont*   font = nullptr;     ///< Police (différente taille/style)
            float32     scaleX = 1.0f;      ///< Facteur d'échelle horizontal
            float32     scaleY = 1.0f;      ///< Facteur d'échelle vertical
            float32     letterSpacing = 0.0f; ///< Espacement entre lettres (pixels)
            float32     wordSpacing = 0.0f;   ///< Espacement entre mots (pixels)
            bool        underline = false;   ///< Souligné
            bool        strikethrough = false; ///< Barré
            bool        italic = false;      ///< Italique (utilise police italique si disponible)
            bool        bold = false;        ///< Gras (utilise police gras si disponible)
            float32     baselineShift = 0.0f; ///< Décalage vertical (exposant/indice)
            
            // Métadonnées pour les liens
            const char* linkUrl = nullptr;   ///< URL du lien (si non null)
        };

        /**
         * @struct NkTextStyle
         * @brief Style CSS-like pour le texte
         */
        struct NKUI_API NkTextStyle {
            NkUIFont*   font = nullptr;          ///< Police
            NkColor     color = {255,255,255,255}; ///< Couleur
            float32     fontSize = 12.0f;        ///< Taille de police
            bool        bold = false;             ///< Gras
            bool        italic = false;           ///< Italique
            bool        underline = false;        ///< Souligné
            bool        strikethrough = false;    ///< Barré
            float32     letterSpacing = 0.0f;     ///< Espacement entre lettres
            float32     wordSpacing = 0.0f;       ///< Espacement entre mots
            
            // Styles de liste
            enum ListType : uint8 { 
                LIST_NONE = 0,      ///< Pas de liste
                LIST_BULLET,        ///< Puces (•, ◦, ▪)
                LIST_NUMBERED,      ///< Numéroté (1., 2., 3.)
                LIST_ROMAN          ///< Chiffres romains (I., II., III.)
            } listType = LIST_NONE;
            int32       listLevel = 0;            ///< Niveau d'indentation
            bool        listItem = false;         ///< Est un élément de liste
            
            // Alignement
            enum TextAlign : uint8 {
                ALIGN_LEFT = 0,     ///< Aligné à gauche
                ALIGN_CENTER,       ///< Centré
                ALIGN_RIGHT,        ///< Aligné à droite
                ALIGN_JUSTIFY       ///< Justifié
            } alignment = ALIGN_LEFT;
            
            // Marges et padding
            float32 marginLeft = 0.0f;    ///< Marge gauche
            float32 marginRight = 0.0f;   ///< Marge droite
            float32 marginTop = 0.0f;     ///< Marge haute
            float32 marginBottom = 0.0f;  ///< Marge basse
            float32 paddingLeft = 0.0f;   ///< Padding gauche
            float32 paddingRight = 0.0f;  ///< Padding droit
            float32 indent = 0.0f;        ///< Indentation de première ligne
            
            // Fond
            NkColor     backgroundColor = {0,0,0,0};  ///< Couleur de fond (0 = transparent)
            float32     borderRadius = 0.0f;         ///< Rayon des coins
        };

        // ============================================================================
        //  Coloration syntaxique
        // ============================================================================

        /**
         * @struct NkSyntaxToken
         * @brief Token de code avec sa couleur
         */
        struct NKUI_API NkSyntaxToken {
            const char* text;           ///< Le texte du token
            NkColor     color;          ///< Couleur du token
            NkUIFont*   font;           ///< Police (monospace recommandée)
            uint32      line;           ///< Numéro de ligne (0-indexé)
            uint32      column;         ///< Colonne de début
        };

        /**
         * @struct NkSyntaxTheme
         * @brief Thème de coloration syntaxique
         */
        struct NKUI_API NkSyntaxTheme {
            NkColor keyword     = {86, 156, 214, 255};   // Bleu
            NkColor string      = {214, 157, 133, 255};  // Orange
            NkColor comment     = {106, 153, 85, 255};   // Vert
            NkColor number      = {181, 206, 168, 255};  // Vert clair
            NkColor operators    = {215, 186, 125, 255};  // Jaune
            NkColor preprocessor = {155, 155, 155, 255}; // Gris
            NkColor type        = {78, 201, 176, 255};   // Turquoise
            NkColor function    = {220, 220, 170, 255};  // Jaune clair
            NkColor variable    = {156, 220, 254, 255};  // Cyan
            NkColor constant    = {181, 206, 168, 255};  // Vert
            NkColor punctuation = {212, 212, 212, 255};  // Gris clair
            NkColor background  = {30, 30, 30, 255};     // Fond
            NkColor lineNumber  = {100, 100, 100, 255};  // Numéros de ligne
        };

        /**
         * @class NkSyntaxHighlighter
         * @brief Coloration syntaxique pour différents langages
         */
        class NKUI_API NkSyntaxHighlighter {
            public:
                enum Language : uint8 {
                    LANG_C,         ///< C/C++
                    LANG_CPP,       ///< C++
                    LANG_PYTHON,    ///< Python
                    LANG_JAVASCRIPT,///< JavaScript
                    LANG_JSON,      ///< JSON
                    LANG_XML,       ///< XML/HTML
                    LANG_GLSL,      ///< GLSL Shader
                    LANG_HLSL,      ///< HLSL Shader
                    LANG_TEXT       ///< Texte brut
                };
                
                /**
                 * @brief Colorise un texte selon le langage
                 * @param text Texte source
                 * @param lang Langage
                 * @param theme Thème de couleurs
                 * @param arena Arena pour allocations
                 * @return Vecteur de tokens colorisés
                 */
                static NkVector<NkSyntaxToken> Highlight(
                    const char* text,
                    Language lang,
                    const NkSyntaxTheme& theme = NkSyntaxTheme(),
                    NkMemArena* arena = nullptr
                ) noexcept;
                
                /**
                 * @brief Rendu de code colorisé
                 */
                static void RenderCode(
                    NkUIDrawList& dl,
                    const char* text,
                    Language lang,
                    NkVec2 pos,
                    NkUIFont* font,
                    const NkSyntaxTheme& theme = NkSyntaxTheme(),
                    float32 maxWidth = -1.f,
                    bool showLineNumbers = true
                ) noexcept;
                
            private:
                // Parsers par langage
                static void HighlightC(const char* text, NkVector<NkSyntaxToken>& out, const NkSyntaxTheme& theme, NkMemArena* arena) noexcept;
                static void HighlightPython(const char* text, NkVector<NkSyntaxToken>& out, const NkSyntaxTheme& theme, NkMemArena* arena) noexcept;
                static void HighlightJavaScript(const char* text, NkVector<NkSyntaxToken>& out, const NkSyntaxTheme& theme, NkMemArena* arena) noexcept;
                static void HighlightJSON(const char* text, NkVector<NkSyntaxToken>& out, const NkSyntaxTheme& theme, NkMemArena* arena) noexcept;
                static void HighlightGLSL(const char* text, NkVector<NkSyntaxToken>& out, const NkSyntaxTheme& theme, NkMemArena* arena) noexcept;
                
                // Utilitaires
                static bool IsWhitespace(char c) noexcept;
                static bool IsDigit(char c) noexcept;
                static bool IsAlpha(char c) noexcept;
                static bool IsAlphaNum(char c) noexcept;
                static bool IsKeyword(const char* word, const char** keywords, uint32 numKeywords) noexcept;
                static const char* ReadWord(const char* start, char* buffer, uint32 maxLen) noexcept;
        };

        // ============================================================================
        //  Classe NkRichText
        // ============================================================================

        /**
         * @class NkRichText
         * @brief Générateur de texte enrichi
         */
        class NKUI_API NkRichText {
            public:
                NkRichText() = default;
                ~NkRichText() = default;
                
                // ── Configuration ──────────────────────────────────────────────────────
                
                /**
                 * @brief Définit la police par défaut
                 */
                void SetDefaultFont(NkUIFont* font) noexcept { mDefaultFont = font; }
                
                /**
                 * @brief Définit le style par défaut
                 */
                void SetDefaultStyle(const NkTextStyle& style) noexcept { mDefaultStyle = style; }
                
                /**
                 * @brief Efface tous les fragments
                 */
                void Clear() noexcept { mFragments.Clear(); mHasLinks = false; }
                
                // ── Ajout de fragments simples ─────────────────────────────────────────
                
                /**
                 * @brief Ajoute du texte simple
                 */
                void AddText(const char* text, NkColor color = NkColor{255,255,255,255}) noexcept;
                
                /**
                 * @brief Ajoute du texte avec une police spécifique
                 */
                void AddText(const char* text, NkUIFont* font) noexcept;
                
                /**
                 * @brief Ajoute du texte gras
                 */
                void AddBold(const char* text) noexcept;
                
                /**
                 * @brief Ajoute du texte italique
                 */
                void AddItalic(const char* text) noexcept;
                
                /**
                 * @brief Ajoute du texte souligné
                 */
                void AddUnderline(const char* text) noexcept;
                
                /**
                 * @brief Ajoute du texte barré
                 */
                void AddStrikethrough(const char* text) noexcept;
                
                /**
                 * @brief Ajoute du texte en exposant
                 */
                void AddSuperscript(const char* text) noexcept;
                
                /**
                 * @brief Ajoute du texte en indice
                 */
                void AddSubscript(const char* text) noexcept;
                
                /**
                 * @brief Ajoute un lien hypertexte
                 */
                void AddLink(const char* text, const char* url) noexcept;
                
                /**
                 * @brief Ajoute du code colorisé
                 */
                void AddCode(const char* code, NkSyntaxHighlighter::Language lang,
                            NkUIFont* codeFont = nullptr) noexcept;
                
                // ── Ajout avec style personnalisé ──────────────────────────────────────
                
                /**
                 * @brief Ajoute du texte avec un style complet
                 */
                void AddStyledText(const char* text, const NkTextStyle& style) noexcept;
                
                /**
                 * @brief Commence un nouveau style pour les fragments suivants
                 */
                void BeginStyle(const NkTextStyle& style) noexcept;
                
                /**
                 * @brief Termine le style courant
                 */
                void EndStyle() noexcept;
                
                // ── Listes ─────────────────────────────────────────────────────────────
                
                /**
                 * @brief Commence une liste à puces
                 */
                void BeginBulletList(int32 level = 0) noexcept;
                
                /**
                 * @brief Commence une liste numérotée
                 */
                void BeginNumberedList(int32 level = 0) noexcept;
                
                /**
                 * @brief Termine la liste courante
                 */
                void EndList() noexcept;
                
                /**
                 * @brief Ajoute un élément de liste
                 */
                void AddListItem(const char* text) noexcept;
                
                // ── Rendu ──────────────────────────────────────────────────────────────
                
                /**
                 * @brief Calcule la hauteur totale du texte
                 */
                float32 MeasureHeight(float32 maxWidth) const noexcept;
                
                /**
                 * @brief Rendu du texte enrichi
                 * @param dl DrawList
                 * @param pos Position de départ
                 * @param maxWidth Largeur max (-1 = illimitée)
                 * @param hoverLink Retourne l'URL du lien survolé (optionnel)
                 * @return Position Y après le rendu
                 */
                float32 Render(NkUIDrawList& dl, NkVec2 pos, float32 maxWidth = -1.f, const char** hoverLink = nullptr) noexcept;
                
                /**
                 * @brief Rendu avec gestion des clics sur liens
                 */
                float32 RenderInteractive(NkUIDrawList& dl, NkVec2 pos, const NkUIInputState& input, float32 maxWidth = -1.f, const char** clickedLink = nullptr) noexcept;
                
                // ── Accès ──────────────────────────────────────────────────────────────
                
                /**
                 * @brief Retourne le nombre de fragments
                 */
                uint32 GetFragmentCount() const noexcept { return mFragments.Size(); }
                
                /**
                 * @brief Vérifie si le texte contient des liens
                 */
                bool HasLinks() const noexcept { return mHasLinks; }
                
            private:
                struct StyleStack {
                    NkTextStyle style;
                    StyleStack* prev;
                };
                
                NkVector<NkTextFragment> mFragments;
                NkTextStyle              mDefaultStyle;
                NkUIFont*                mDefaultFont = nullptr;
                StyleStack*              mCurrentStyle = nullptr;
                bool                     mHasLinks = false;
                
                // Pour les listes
                struct ListState {
                    NkTextStyle::ListType type;
                    int32 level;
                    int32 counter;
                    ListState* prev;
                };
                ListState* mCurrentList = nullptr;
                
                void AddFragment(const NkTextFragment& frag) noexcept;
                void ApplyStyle(NkTextFragment& frag, const NkTextStyle& style) noexcept;
                const char* GetBulletChar(NkTextStyle::ListType type, int32 counter, int32 level) const noexcept;
                float32 RenderFragment(NkUIDrawList& dl, NkTextFragment& frag, NkVec2 pos,
                                    float32& curX, float32& curY, float32 lineHeight,
                                    float32 maxWidth, bool interactive, const NkUIInputState* input,
                                    const char** hoverLink, const char** clickedLink) noexcept;
        };

        // ============================================================================
        //  Parseur HTML simplifié
        // ============================================================================

        /**
         * @class NkHtmlTextParser
         * @brief Parseur HTML simplifié pour texte enrichi
         * 
         * Supporte les balises:
         *   <b>...</b>       - Gras
         *   <i>...</i>       - Italique
         *   <u>...</u>       - Souligné
         *   <s>...</s>       - Barré
         *   <sup>...</sup>   - Exposant
         *   <sub>...</sub>   - Indice
         *   <color=#RRGGBB>  - Couleur
         *   <font=name>      - Police
         *   <a href="url">   - Lien
         *   <code lang="c">  - Code colorisé
         *   <br>             - Saut de ligne
         *   <hr>             - Séparateur
         *   <h1>...</h1>     - Titre
         *   <p>...</p>       - Paragraphe
         *   <ul>...</ul>     - Liste à puces
         *   <ol>...</ol>     - Liste numérotée
         *   <li>...</li>     - Élément de liste
         */
        class NKUI_API NkHtmlTextParser {
            public:
                /**
                 * @brief Parse du HTML et génère du texte enrichi
                 * @param html Code HTML
                 * @param defaultFont Police par défaut
                 * @param defaultColor Couleur par défaut
                 * @return Texte enrichi
                 */
                static NkRichText Parse(const char* html, 
                                        NkUIFont* defaultFont,
                                        NkColor defaultColor = NkColor{255,255,255,255}) noexcept;
                
                /**
                 * @brief Parse un fichier HTML
                 */
                static NkRichText ParseFile(const char* filepath,
                                            NkUIFont* defaultFont,
                                            NkColor defaultColor = NkColor{255,255,255,255}) noexcept;
                
            private:
                struct Tag {
                    char name[32];
                    char attributes[256];
                    bool isClosing;
                };
                
                static bool ParseTag(const char*& ptr, Tag& out) noexcept;
                static const char* ParseAttribute(const char* ptr, const char* name, char* value, uint32 maxLen) noexcept;
                static NkColor ParseColor(const char* str) noexcept;
                static NkSyntaxHighlighter::Language ParseLanguage(const char* str) noexcept;
        };

    } // namespace nkui
} // namespace nkentseu