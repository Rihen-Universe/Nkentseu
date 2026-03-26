/**
 * @File    NkRichText.cpp
 * @Brief   Implémentation du système de texte enrichi
 */

#include "NkRichText.h"
#include "NKMemory/NkFunction.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cstdio>
#include <cmath>

namespace nkentseu {
    namespace nkui {

        // ============================================================================
        //  NkRichText - Gestion des fragments
        // ============================================================================

        void NkRichText::AddFragment(const NkTextFragment& frag) noexcept {
            if (!frag.text) return;
            mFragments.PushBack(frag);
            if (frag.linkUrl) mHasLinks = true;
        }

        void NkRichText::ApplyStyle(NkTextFragment& frag, const NkTextStyle& style) noexcept {
            if (style.font) frag.font = style.font;
            frag.color = style.color;
            if (style.bold) frag.bold = true;
            if (style.italic) frag.italic = true;
            if (style.underline) frag.underline = true;
            if (style.strikethrough) frag.strikethrough = true;
            frag.letterSpacing = style.letterSpacing;
            frag.wordSpacing = style.wordSpacing;
        }

        void NkRichText::AddText(const char* text, NkColor color) noexcept {
            NkTextFragment frag;
            frag.text = text;
            frag.color = color;
            frag.font = mDefaultFont;
            AddFragment(frag);
        }

        void NkRichText::AddText(const char* text, NkUIFont* font) noexcept {
            NkTextFragment frag;
            frag.text = text;
            frag.font = font;
            AddFragment(frag);
        }

        void NkRichText::AddBold(const char* text) noexcept {
            NkTextFragment frag;
            frag.text = text;
            frag.font = mDefaultFont;
            frag.bold = true;
            AddFragment(frag);
        }

        void NkRichText::AddItalic(const char* text) noexcept {
            NkTextFragment frag;
            frag.text = text;
            frag.font = mDefaultFont;
            frag.italic = true;
            AddFragment(frag);
        }

        void NkRichText::AddUnderline(const char* text) noexcept {
            NkTextFragment frag;
            frag.text = text;
            frag.font = mDefaultFont;
            frag.underline = true;
            AddFragment(frag);
        }

        void NkRichText::AddStrikethrough(const char* text) noexcept {
            NkTextFragment frag;
            frag.text = text;
            frag.font = mDefaultFont;
            frag.strikethrough = true;
            AddFragment(frag);
        }

        void NkRichText::AddSuperscript(const char* text) noexcept {
            NkTextFragment frag;
            frag.text = text;
            frag.font = mDefaultFont;
            frag.scaleY = 0.7f;
            frag.baselineShift = -0.3f;  // Décalage vers le haut
            AddFragment(frag);
        }

        void NkRichText::AddSubscript(const char* text) noexcept {
            NkTextFragment frag;
            frag.text = text;
            frag.font = mDefaultFont;
            frag.scaleY = 0.7f;
            frag.baselineShift = 0.3f;   // Décalage vers le bas
            AddFragment(frag);
        }

        void NkRichText::AddLink(const char* text, const char* url) noexcept {
            NkTextFragment frag;
            frag.text = text;
            frag.font = mDefaultFont;
            frag.color = {100, 150, 255, 255};
            frag.underline = true;
            frag.linkUrl = url;
            AddFragment(frag);
        }

        void NkRichText::AddCode(const char* code, NkSyntaxHighlighter::Language lang,
                                NkUIFont* codeFont) noexcept {
            if (!codeFont) codeFont = mDefaultFont;
            
            NkSyntaxTheme theme;
            NkVector<NkSyntaxToken> tokens = NkSyntaxHighlighter::Highlight(code, lang, theme);
            
            for (const auto& token : tokens) {
                NkTextFragment frag;
                frag.text = token.text;
                frag.color = token.color;
                frag.font = codeFont;
                AddFragment(frag);
            }
        }

        void NkRichText::AddStyledText(const char* text, const NkTextStyle& style) noexcept {
            NkTextFragment frag;
            frag.text = text;
            frag.font = style.font ? style.font : mDefaultFont;
            frag.color = style.color;
            frag.bold = style.bold;
            frag.italic = style.italic;
            frag.underline = style.underline;
            frag.strikethrough = style.strikethrough;
            frag.letterSpacing = style.letterSpacing;
            frag.wordSpacing = style.wordSpacing;
            AddFragment(frag);
        }

        void NkRichText::BeginStyle(const NkTextStyle& style) noexcept {
            StyleStack* stack = new StyleStack();
            stack->style = style;
            stack->prev = mCurrentStyle;
            mCurrentStyle = stack;
        }

        void NkRichText::EndStyle() noexcept {
            if (mCurrentStyle) {
                StyleStack* prev = mCurrentStyle->prev;
                delete mCurrentStyle;
                mCurrentStyle = prev;
            }
        }

        // ============================================================================
        //  NkRichText - Listes
        // ============================================================================

        const char* NkRichText::GetBulletChar(NkTextStyle::ListType type, int32 counter, int32 level) const noexcept {
            static const char* bullets[] = {"•", "◦", "▪", "▫"};
            static const char* roman[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
            
            switch (type) {
                case NkTextStyle::LIST_BULLET:
                    if (level < 4) return bullets[level];
                    return "•";
                case NkTextStyle::LIST_NUMBERED:
                {
                    static char num[16];
                    snprintf(num, sizeof(num), "%d.", counter);
                    return num;
                }
                case NkTextStyle::LIST_ROMAN:
                {
                    static char romanNum[16];
                    if (counter <= 10) {
                        snprintf(romanNum, sizeof(romanNum), "%s.", roman[counter - 1]);
                        return romanNum;
                    }
                    snprintf(romanNum, sizeof(romanNum), "%d.", counter);
                    return romanNum;
                }
                default:
                    return "";
            }
        }

        void NkRichText::BeginBulletList(int32 level) noexcept {
            ListState* state = new ListState();
            state->type = NkTextStyle::LIST_BULLET;
            state->level = level;
            state->counter = 0;
            state->prev = mCurrentList;
            mCurrentList = state;
        }

        void NkRichText::BeginNumberedList(int32 level) noexcept {
            ListState* state = new ListState();
            state->type = NkTextStyle::LIST_NUMBERED;
            state->level = level;
            state->counter = 0;
            state->prev = mCurrentList;
            mCurrentList = state;
        }

        void NkRichText::EndList() noexcept {
            if (mCurrentList) {
                ListState* prev = mCurrentList->prev;
                delete mCurrentList;
                mCurrentList = prev;
            }
        }

        void NkRichText::AddListItem(const char* text) noexcept {
            if (!mCurrentList) return;
            
            mCurrentList->counter++;
            
            // Ajouter le marqueur de liste
            const char* bullet = GetBulletChar(mCurrentList->type, mCurrentList->counter, mCurrentList->level);
            NkTextFragment bulletFrag;
            bulletFrag.text = bullet;
            bulletFrag.font = mDefaultFont;
            bulletFrag.color = mDefaultStyle.color;
            AddFragment(bulletFrag);
            
            // Ajouter l'espace après le marqueur
            NkTextFragment spaceFrag;
            spaceFrag.text = " ";
            spaceFrag.font = mDefaultFont;
            AddFragment(spaceFrag);
            
            // Ajouter le texte
            AddText(text);
            
            // Ajouter un saut de ligne
            NkTextFragment newlineFrag;
            newlineFrag.text = "\n";
            AddFragment(newlineFrag);
        }

        // ============================================================================
        //  NkRichText - Rendu
        // ============================================================================

        float32 NkRichText::MeasureHeight(float32 maxWidth) const noexcept {
            if (!mDefaultFont) return 0;
            
            float32 curX = 0;
            float32 curY = 0;
            float32 lineHeight = mDefaultFont->metrics.lineHeight;
            float32 maxLineHeight = lineHeight;
            
            for (const auto& frag : mFragments) {
                if (!frag.text) continue;
                
                NkUIFont* font = frag.font ? frag.font : mDefaultFont;
                float32 fontSize = font->size * frag.scaleY;
                float32 charWidth = fontSize * 0.55f;
                float32 fragWidth = 0;
                
                // Mesurer le texte
                for (const char* p = frag.text; *p; ++p) {
                    if (*p == ' ') {
                        fragWidth += font->metrics.spaceWidth + frag.wordSpacing;
                    } else if (*p == '\n') {
                        curX = 0;
                        curY += maxLineHeight;
                        maxLineHeight = lineHeight;
                        continue;
                    } else {
                        fragWidth += charWidth + frag.letterSpacing;
                    }
                }
                
                if (maxWidth > 0 && curX + fragWidth > maxWidth) {
                    curX = 0;
                    curY += maxLineHeight;
                    maxLineHeight = lineHeight;
                }
                
                maxLineHeight = NK_MAX(maxLineHeight, fontSize * 1.2f);
                curX += fragWidth;
            }
            
            return curY + maxLineHeight;
        }

        float32 NkRichText::RenderFragment(NkUIDrawList& dl, NkTextFragment& frag, NkVec2 pos,
                                            float32& curX, float32& curY, float32 lineHeight,
                                            float32 maxWidth, bool interactive, 
                                            const NkUIInputState* input,
                                            const char** hoverLink, const char** clickedLink) noexcept {
            if (!frag.text || !*frag.text) return lineHeight;
            
            NkUIFont* font = frag.font ? frag.font : mDefaultFont;
            if (!font) return lineHeight;
            
            float32 fontSize = font->size * frag.scaleY;
            float32 charWidth = fontSize * 0.55f;
            float32 lineH = NK_MAX(lineHeight, fontSize * 1.2f);
            float32 x = curX;
            float32 y = curY + frag.baselineShift * fontSize;
            
            const char* p = frag.text;
            const char* wordStart = p;
            float32 wordWidth = 0;
            
            while (*p) {
                if (*p == '\n') {
                    curX = 0;
                    curY += lineH;
                    lineH = fontSize * 1.2f;
                    p++;
                    wordStart = p;
                    wordWidth = 0;
                    continue;
                }
                
                float32 charAdvance;
                if (*p == ' ') {
                    charAdvance = font->metrics.spaceWidth + frag.wordSpacing;
                } else {
                    charAdvance = charWidth + frag.letterSpacing;
                }
                
                // Vérifier le wrapping
                if (maxWidth > 0 && x + wordWidth + charAdvance > pos.x + maxWidth && x > pos.x) {
                    curX = pos.x;
                    curY += lineH;
                    lineH = fontSize * 1.2f;
                    x = curX;
                    y = curY + frag.baselineShift * fontSize;
                    wordWidth = 0;
                    wordStart = p;
                    
                    if (*p == ' ') {
                        p++;
                        continue;
                    }
                }
                
                // Rendu du caractère
                if (*p != ' ') {
                    char temp[2] = {*p, 0};
                    NkColor renderColor = frag.color;
                    
                    // Vérifier les interactions (liens)
                    if (interactive && frag.linkUrl && input) {
                        float32 charEndX = x + charAdvance;
                        if (input->mousePos.x >= x && input->mousePos.x <= charEndX &&
                            input->mousePos.y >= y && input->mousePos.y <= y + fontSize) {
                            renderColor = {renderColor.r, renderColor.g, 255, 255};
                            if (hoverLink) *hoverLink = frag.linkUrl;
                            if (clickedLink && input->IsMouseClicked(0)) *clickedLink = frag.linkUrl;
                        }
                    }
                    
                    font->RenderText(dl, {x, y}, temp, renderColor);
                }
                
                wordWidth += charAdvance;
                x += charAdvance;
                p++;
            }
            
            // Dessiner le soulignement
            if (frag.underline && wordWidth > 0) {
                float32 underlineY = curY + fontSize + 2.0f;
                dl.AddLine({curX, underlineY}, {curX + wordWidth, underlineY}, frag.color, 1.0f);
            }
            
            // Dessiner le barré
            if (frag.strikethrough && wordWidth > 0) {
                float32 strikeY = curY + fontSize * 0.6f;
                dl.AddLine({curX, strikeY}, {curX + wordWidth, strikeY}, frag.color, 1.0f);
            }
            
            curX = x;
            return lineH;
        }

        float32 NkRichText::Render(NkUIDrawList& dl, NkVec2 pos, float32 maxWidth,
                                const char** hoverLink) noexcept {
            return RenderInteractive(dl, pos, NkUIInputState{}, maxWidth, hoverLink);
        }

        float32 NkRichText::RenderInteractive(NkUIDrawList& dl, NkVec2 pos, const NkUIInputState& input,
                                            float32 maxWidth, const char** clickedLink) noexcept {
            if (!mDefaultFont) return pos.y;
            
            float32 curX = pos.x;
            float32 curY = pos.y;
            float32 lineHeight = mDefaultFont->metrics.lineHeight;
            const char* hoverLinkResult = nullptr;
            const char* clickedLinkResult = nullptr;
            
            for (auto& frag : mFragments) {
                lineHeight = RenderFragment(dl, frag, pos, curX, curY, lineHeight, maxWidth,
                                            true, &input, &hoverLinkResult, &clickedLinkResult);
            }
            
            if (clickedLink && clickedLinkResult) *clickedLink = clickedLinkResult;
            return curY + lineHeight;
        }

        // ============================================================================
        //  NkSyntaxHighlighter
        // ============================================================================

        // Mots-clés C/C++
        static const char* cKeywords[] = {
            "auto", "break", "case", "char", "const", "continue", "default", "do",
            "double", "else", "enum", "extern", "float", "for", "goto", "if",
            "int", "long", "register", "return", "short", "signed", "sizeof", "static",
            "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while",
            "class", "namespace", "new", "delete", "virtual", "override", "final",
            "public", "private", "protected", "template", "typename", "using", "constexpr",
            "nullptr", "decltype", "noexcept", "static_assert", "thread_local"
        };
        static const uint32 cKeywordsCount = sizeof(cKeywords) / sizeof(cKeywords[0]);

        // Types C/C++
        static const char* cTypes[] = {
            "int8_t", "int16_t", "int32_t", "int64_t",
            "uint8_t", "uint16_t", "uint32_t", "uint64_t",
            "size_t", "ptrdiff_t", "intptr_t", "uintptr_t",
            "bool", "char", "wchar_t", "char16_t", "char32_t"
        };
        static const uint32 cTypesCount = sizeof(cTypes) / sizeof(cTypes[0]);

        bool NkSyntaxHighlighter::IsWhitespace(char c) noexcept {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }

        bool NkSyntaxHighlighter::IsDigit(char c) noexcept {
            return c >= '0' && c <= '9';
        }

        bool NkSyntaxHighlighter::IsAlpha(char c) noexcept {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        }

        bool NkSyntaxHighlighter::IsAlphaNum(char c) noexcept {
            return IsAlpha(c) || IsDigit(c);
        }

        bool NkSyntaxHighlighter::IsKeyword(const char* word, const char** keywords, uint32 numKeywords) noexcept {
            for (uint32 i = 0; i < numKeywords; ++i) {
                if (strcmp(word, keywords[i]) == 0) return true;
            }
            return false;
        }

        void NkSyntaxHighlighter::HighlightC(const char* text, NkVector<NkSyntaxToken>& out,
                                            const NkSyntaxTheme& theme, NkMemArena* arena) noexcept {
            enum State { NORMAL, COMMENT, STRING, CHAR, PREPROCESSOR };
            State state = NORMAL;
            const char* p = text;
            const char* tokenStart = p;
            uint32 line = 0;
            uint32 column = 0;
            
            auto emitToken = [&](NkColor color) {
                if (tokenStart < p) {
                    NkSyntaxToken token;
                    token.text = tokenStart;
                    token.color = color;
                    token.line = line;
                    token.column = column;
                    // Note: on ne copie pas le texte, on utilise les pointeurs
                    out.PushBack(token);
                    column += static_cast<uint32>(p - tokenStart);
                }
                tokenStart = p;
            };
            
            while (*p) {
                if (*p == '\n') {
                    emitToken(theme.punctuation);
                    line++;
                    column = 0;
                    tokenStart = p + 1;
                    p++;
                    continue;
                }
                
                switch (state) {
                    case NORMAL:
                        if (*p == '/' && *(p+1) == '/') {
                            emitToken(theme.punctuation);
                            state = COMMENT;
                            tokenStart = p;
                            p += 2;
                        } else if (*p == '/' && *(p+1) == '*') {
                            emitToken(theme.punctuation);
                            state = COMMENT;
                            tokenStart = p;
                            p += 2;
                        } else if (*p == '"') {
                            emitToken(theme.punctuation);
                            state = STRING;
                            tokenStart = p;
                            p++;
                        } else if (*p == '\'') {
                            emitToken(theme.punctuation);
                            state = CHAR;
                            tokenStart = p;
                            p++;
                        } else if (*p == '#') {
                            emitToken(theme.punctuation);
                            state = PREPROCESSOR;
                            tokenStart = p;
                            p++;
                        } else if (IsAlpha(*p)) {
                            const char* wordStart = p;
                            while (IsAlphaNum(*p)) p++;
                            char word[128];
                            uint32 len = static_cast<uint32>(p - wordStart);
                            if (len < sizeof(word)) {
                                memcpy(word, wordStart, len);
                                word[len] = 0;
                                
                                if (IsKeyword(word, cKeywords, cKeywordsCount)) {
                                    emitToken(theme.keyword);
                                } else if (IsKeyword(word, cTypes, cTypesCount)) {
                                    emitToken(theme.type);
                                } else {
                                    emitToken(theme.variable);
                                }
                            }
                            tokenStart = p;
                        } else if (IsDigit(*p)) {
                            while (IsDigit(*p) || *p == '.' || *p == 'x' || *p == 'X' ||
                                (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) p++;
                            emitToken(theme.number);
                        } else {
                            p++;
                        }
                        break;
                        
                    case COMMENT:
                        if ((*p == '/' && *(p-1) == '*') || (*p == '\n' && *(p-1) == '/')) {
                            p++;
                            emitToken(theme.comment);
                            state = NORMAL;
                        } else {
                            p++;
                        }
                        break;
                        
                    case STRING:
                        if (*p == '"' && *(p-1) != '\\') {
                            p++;
                            emitToken(theme.string);
                            state = NORMAL;
                        } else {
                            p++;
                        }
                        break;
                        
                    case CHAR:
                        if (*p == '\'' && *(p-1) != '\\') {
                            p++;
                            emitToken(theme.string);
                            state = NORMAL;
                        } else {
                            p++;
                        }
                        break;
                        
                    case PREPROCESSOR:
                        if (*p == '\n') {
                            emitToken(theme.preprocessor);
                            state = NORMAL;
                        } else {
                            p++;
                        }
                        break;
                }
            }
            
            emitToken(theme.punctuation);
        }

        NkVector<NkSyntaxToken> NkSyntaxHighlighter::Highlight(
            const char* text, Language lang, const NkSyntaxTheme& theme, NkMemArena* arena) noexcept {
            
            NkVector<NkSyntaxToken> tokens;
            if (!text) return tokens;
            
            switch (lang) {
                case LANG_C:
                case LANG_CPP:
                    HighlightC(text, tokens, theme, arena);
                    break;
                default:
                    // Texte brut
                    {
                        NkSyntaxToken token;
                        token.text = text;
                        token.color = theme.punctuation;
                        token.line = 0;
                        token.column = 0;
                        tokens.PushBack(token);
                    }
                    break;
            }
            
            return tokens;
        }

        void NkSyntaxHighlighter::RenderCode(
            NkUIDrawList& dl, const char* text, Language lang,
            NkVec2 pos, NkUIFont* font, const NkSyntaxTheme& theme,
            float32 maxWidth, bool showLineNumbers) noexcept {
            
            if (!font) return;
            
            NkVector<NkSyntaxToken> tokens = Highlight(text, lang, theme);
            float32 curX = pos.x;
            float32 curY = pos.y;
            float32 lineHeight = font->metrics.lineHeight;
            uint32 currentLine = 0;
            
            // Fond du bloc de code
            dl.AddRectFilled({pos.x, pos.y, maxWidth > 0 ? maxWidth : 800, 0}, theme.background, 4.f);
            
            for (const auto& token : tokens) {
                if (token.line != currentLine) {
                    curX = pos.x + (showLineNumbers ? 40 : 0);
                    curY += lineHeight;
                    currentLine = token.line;
                }
                
                if (showLineNumbers && token.column == 0) {
                    char lineNum[16];
                    snprintf(lineNum, sizeof(lineNum), "%3d ", token.line + 1);
                    font->RenderText(dl, {pos.x, curY}, lineNum, theme.lineNumber);
                    curX = pos.x + 40;
                }
                
                font->RenderText(dl, {curX, curY}, token.text, token.color);
                curX += font->MeasureWidth(token.text);
            }
        }

        // ============================================================================
        //  NkHtmlTextParser
        // ============================================================================

        bool NkHtmlTextParser::ParseTag(const char*& ptr, Tag& out) noexcept {
            if (*ptr != '<') return false;
            
            const char* start = ptr;
            ptr++; // Skip '<'
            
            out.isClosing = (*ptr == '/');
            if (out.isClosing) ptr++;
            
            // Lire le nom du tag
            const char* tagStart = ptr;
            while (*ptr && *ptr != '>' && *ptr != ' ' && *ptr != '/') ptr++;
            uint32 tagLen = static_cast<uint32>(ptr - tagStart);
            if (tagLen >= sizeof(out.name)) tagLen = sizeof(out.name) - 1;
            memcpy(out.name, tagStart, tagLen);
            out.name[tagLen] = 0;
            
            // Lire les attributs
            out.attributes[0] = 0;
            if (*ptr == ' ') {
                const char* attrStart = ptr;
                while (*ptr && *ptr != '>') ptr++;
                uint32 attrLen = static_cast<uint32>(ptr - attrStart);
                if (attrLen < sizeof(out.attributes)) {
                    memcpy(out.attributes, attrStart, attrLen);
                    out.attributes[attrLen] = 0;
                }
            }
            
            if (*ptr == '>') ptr++;
            else if (*ptr == '/') ptr += 2;
            
            return true;
        }

        const char* NkHtmlTextParser::ParseAttribute(const char* ptr, const char* name, 
                                                    char* value, uint32 maxLen) noexcept {
            char search[64];
            snprintf(search, sizeof(search), "%s=\"", name);
            const char* found = strstr(ptr, search);
            if (!found) return nullptr;
            
            found += strlen(search);
            const char* end = strchr(found, '"');
            if (!end) return nullptr;
            
            uint32 len = static_cast<uint32>(end - found);
            if (len >= maxLen) len = maxLen - 1;
            memcpy(value, found, len);
            value[len] = 0;
            
            return end + 1;
        }

        NkColor NkHtmlTextParser::ParseColor(const char* str) noexcept {
            NkColor color = {255, 255, 255, 255};
            if (!str || *str != '#') return color;
            
            uint32 hex = 0;
            sscanf(str + 1, "%06x", &hex);
            color.r = static_cast<uint8>((hex >> 16) & 0xFF);
            color.g = static_cast<uint8>((hex >> 8) & 0xFF);
            color.b = static_cast<uint8>(hex & 0xFF);
            return color;
        }

        NkSyntaxHighlighter::Language NkHtmlTextParser::ParseLanguage(const char* str) noexcept {
            if (!str) return NkSyntaxHighlighter::LANG_TEXT;
            if (strcmp(str, "c") == 0 || strcmp(str, "cpp") == 0) return NkSyntaxHighlighter::LANG_CPP;
            if (strcmp(str, "python") == 0 || strcmp(str, "py") == 0) return NkSyntaxHighlighter::LANG_PYTHON;
            if (strcmp(str, "javascript") == 0 || strcmp(str, "js") == 0) return NkSyntaxHighlighter::LANG_JAVASCRIPT;
            if (strcmp(str, "json") == 0) return NkSyntaxHighlighter::LANG_JSON;
            if (strcmp(str, "xml") == 0 || strcmp(str, "html") == 0) return NkSyntaxHighlighter::LANG_XML;
            if (strcmp(str, "glsl") == 0) return NkSyntaxHighlighter::LANG_GLSL;
            if (strcmp(str, "hlsl") == 0) return NkSyntaxHighlighter::LANG_HLSL;
            return NkSyntaxHighlighter::LANG_TEXT;
        }

        NkRichText NkHtmlTextParser::Parse(const char* html, NkUIFont* defaultFont,
                                            NkColor defaultColor) noexcept {
            NkRichText richText;
            richText.SetDefaultFont(defaultFont);
            
            if (!html) return richText;
            
            NkVector<NkTextStyle> styleStack;
            NkTextStyle currentStyle;
            currentStyle.color = defaultColor;
            currentStyle.font = defaultFont;
            
            const char* p = html;
            const char* textStart = p;
            
            while (*p) {
                if (*p == '<') {
                    // Émettre le texte courant
                    if (textStart < p) {
                        char* text = const_cast<char*>(textStart);
                        *const_cast<char*>(p) = 0; // Temporaire
                        richText.AddStyledText(textStart, currentStyle);
                        *const_cast<char*>(p) = '<';
                    }
                    
                    Tag tag;
                    if (ParseTag(p, tag)) {
                        if (tag.isClosing) {
                            // Fermeture de tag
                            if (styleStack.Size() > 0) {
                                currentStyle = styleStack.Back();
                                styleStack.PopBack();
                            }
                        } else {
                            // Ouverture de tag
                            styleStack.PushBack(currentStyle);
                            
                            if (strcmp(tag.name, "b") == 0) {
                                currentStyle.bold = true;
                            } else if (strcmp(tag.name, "i") == 0) {
                                currentStyle.italic = true;
                            } else if (strcmp(tag.name, "u") == 0) {
                                currentStyle.underline = true;
                            } else if (strcmp(tag.name, "s") == 0) {
                                currentStyle.strikethrough = true;
                            } else if (strcmp(tag.name, "sup") == 0) {
                                // TODO: Gérer exposant
                            } else if (strcmp(tag.name, "sub") == 0) {
                                // TODO: Gérer indice
                            } else if (strcmp(tag.name, "color") == 0) {
                                char colorStr[16];
                                if (ParseAttribute(tag.attributes, "color", colorStr, sizeof(colorStr))) {
                                    currentStyle.color = ParseColor(colorStr);
                                }
                            } else if (strcmp(tag.name, "font") == 0) {
                                // TODO: Changer de police
                            } else if (strcmp(tag.name, "a") == 0) {
                                // TODO: Gérer les liens
                            } else if (strcmp(tag.name, "code") == 0) {
                                char langStr[32];
                                NkSyntaxHighlighter::Language lang = NkSyntaxHighlighter::LANG_TEXT;
                                if (ParseAttribute(tag.attributes, "lang", langStr, sizeof(langStr))) {
                                    lang = ParseLanguage(langStr);
                                }
                                // Le code sera traité dans le texte suivant
                            } else if (strcmp(tag.name, "br") == 0) {
                                richText.AddStyledText("\n", currentStyle);
                            } else if (strcmp(tag.name, "hr") == 0) {
                                // TODO: Ajouter un séparateur
                            }
                        }
                    }
                    textStart = p;
                } else {
                    p++;
                }
            }
            
            // Dernier texte
            if (textStart < p) {
                richText.AddStyledText(textStart, currentStyle);
            }
            
            return richText;
        }

        NkRichText NkHtmlTextParser::ParseFile(const char* filepath, NkUIFont* defaultFont,
                                                NkColor defaultColor) noexcept {
            // TODO: Implémenter la lecture de fichier
            return Parse("", defaultFont, defaultColor);
        }

    } // namespace nkui
} // namespace nkentseu 