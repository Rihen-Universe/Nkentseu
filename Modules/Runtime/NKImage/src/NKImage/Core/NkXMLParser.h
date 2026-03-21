#pragma once
/**
 * @File    NkXMLParser.h
 * @Brief   Parser XML complet — tokenizer, DOM tree, namespaces, entités.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Ce module produit un arbre DOM minimal conforme aux besoins du SVG :
 *
 *  Fonctionnalités XML supportées :
 *    - Éléments, attributs, texte, CDATA, commentaires, PI
 *    - Namespaces (xmlns, xmlns:prefix)
 *    - Entités prédéfinies : &amp; &lt; &gt; &apos; &quot;
 *    - Entités numériques : &#NNN; &#xHH;
 *    - Balises auto-fermantes (<br/>)
 *    - DOCTYPE minimal (ignoré, pas de validation)
 *    - Encodage : UTF-8 uniquement (suffisant pour SVG)
 *
 *  Structure DOM :
 *    NkXMLDocument  — racine
 *    NkXMLNode      — nœud générique (élément, texte, commentaire, CDATA, PI)
 *    NkXMLAttr      — attribut d'un élément
 *    NkXMLNodeList  — liste de nœuds (vecteur simple)
 *
 *  API de recherche :
 *    getElementById(id)           — recherche récursive par attribut id
 *    getElementsByTagName(tag)    — tous les descendants avec ce tag
 *    getElementsByLocalName(name) — sans préfixe de namespace
 *    querySelector(selector)      — sélecteur CSS minimal (#id, .class, tag)
 *
 *  Gestion mémoire :
 *    Tous les nœuds sont alloués dans une NkMemArena fournie à l'init.
 *    Les chaînes sont internées (dupliquées dans l'arena).
 *    Pas de malloc/free individuel — tout libéré d'un coup avec l'arena.
 */

#include "NkImageExport.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace nkentseu {

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkXMLArena — allocateur linéaire interne (pas de dépendance NKFont)
    // ─────────────────────────────────────────────────────────────────────────────

    struct NKENTSEU_IMAGE_API NkXMLArena {
        uint8* base   = nullptr;
        usize  size   = 0;
        usize  offset = 0;
        bool   owning = false;

        bool Init(usize bytes) noexcept {
            base=static_cast<uint8*>(::malloc(bytes));
            if(!base) return false;
            size=bytes; offset=0; owning=true;
            return true;
        }
        void InitView(uint8* buf, usize bytes) noexcept {
            base=buf; size=bytes; offset=0; owning=false;
        }
        void Destroy() noexcept { if(owning&&base){::free(base);base=nullptr;} owning=false; }
        void Reset() noexcept { offset=0; }

        template<typename T> T* Alloc(int32 count=1) noexcept {
            const usize bytes=(sizeof(T)*count+7)&~7u;
            if(offset+bytes>size) return nullptr;
            T* p=reinterpret_cast<T*>(base+offset);
            ::memset(p,0,bytes);
            offset+=bytes;
            return p;
        }
        // Alloue et copie une chaîne (null-terminated)
        const char* Dup(const char* str, int32 len=-1) noexcept {
            if(!str) return nullptr;
            const int32 n=(len<0)?(int32)::strlen(str):len;
            char* dst=Alloc<char>(n+1);
            if(!dst) return nullptr;
            ::memcpy(dst,str,n); dst[n]=0;
            return dst;
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  Types de nœuds
    // ─────────────────────────────────────────────────────────────────────────────

    enum class NkXMLNodeType : uint8 {
        NK_ELEMENT   = 1,
        NK_TEXT      = 3,
        NK_CDATA     = 4,
        NK_COMMENT   = 8,
        NK_PI        = 7,  // Processing Instruction
        NK_DOCUMENT  = 9,
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkXMLAttr — attribut d'un élément
    // ─────────────────────────────────────────────────────────────────────────────

    struct NKENTSEU_IMAGE_API NkXMLAttr {
        const char* name    = nullptr; // nom complet (avec préfixe si namespace)
        const char* value   = nullptr; // valeur déjà décodée (entités résolues)
        const char* prefix  = nullptr; // préfixe de namespace (peut être nullptr)
        const char* localName = nullptr; // nom local sans préfixe
        const char* ns      = nullptr; // URI du namespace (peut être nullptr)
        NkXMLAttr*  next    = nullptr; // liste chaînée
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkXMLNode — nœud DOM
    // ─────────────────────────────────────────────────────────────────────────────

    struct NKENTSEU_IMAGE_API NkXMLNode {
        NkXMLNodeType type    = NkXMLNodeType::NK_ELEMENT;
        const char*   tagName = nullptr; // nom de l'élément (ou nullptr pour texte)
        const char*   prefix  = nullptr; // préfixe namespace
        const char*   localName = nullptr;
        const char*   ns      = nullptr; // URI namespace résolu
        const char*   text    = nullptr; // contenu texte (type Text/CDATA/Comment/PI)
        NkXMLAttr*    attrs   = nullptr; // liste des attributs
        int32         numAttrs= 0;
        NkXMLNode*    parent  = nullptr;
        NkXMLNode*    firstChild  = nullptr;
        NkXMLNode*    lastChild   = nullptr;
        NkXMLNode*    nextSibling = nullptr;
        NkXMLNode*    prevSibling = nullptr;
        int32         childCount  = 0;
        int32         lineNumber  = 0; // pour le débogage

        // ── Navigation ────────────────────────────────────────────────────────────

        NKIMG_NODISCARD const char* GetAttr(const char* name) const noexcept {
            for(NkXMLAttr* a=attrs;a;a=a->next)
                if(a->name&&::strcmp(a->name,name)==0) return a->value;
            return nullptr;
        }
        NKIMG_NODISCARD const char* GetAttrLocal(const char* localName_) const noexcept {
            for(NkXMLAttr* a=attrs;a;a=a->next)
                if(a->localName&&::strcmp(a->localName,localName_)==0) return a->value;
            return nullptr;
        }
        NKIMG_NODISCARD float GetAttrF(const char* name, float def=0.f) const noexcept {
            const char* v=GetAttr(name);
            if(!v) return def;
            char* end; float r=::strtof(v,&end);
            return (end>v)?r:def;
        }
        NKIMG_NODISCARD bool HasAttr(const char* name) const noexcept {
            return GetAttr(name)!=nullptr;
        }

        // Vérifie si le tagName (sans préfixe) correspond
        NKIMG_NODISCARD bool Is(const char* localTag) const noexcept {
            if(!localName) return false;
            return ::strcmp(localName,localTag)==0;
        }

        // Premier enfant élément avec ce tag local
        NKIMG_NODISCARD NkXMLNode* FirstChildNamed(const char* tag) const noexcept {
            for(NkXMLNode* c=firstChild;c;c=c->nextSibling)
                if(c->type==NkXMLNodeType::NK_ELEMENT&&c->Is(tag)) return c;
            return nullptr;
        }

        // Prochain frère avec ce tag
        NKIMG_NODISCARD NkXMLNode* NextSiblingNamed(const char* tag) const noexcept {
            for(NkXMLNode* s=nextSibling;s;s=s->nextSibling)
                if(s->type==NkXMLNodeType::NK_ELEMENT&&s->Is(tag)) return s;
            return nullptr;
        }

        // Contenu texte concaténé de tous les enfants texte
        void GetTextContent(char* buf, int32 bufLen) const noexcept {
            int32 pos=0;
            for(NkXMLNode* c=firstChild;c&&pos<bufLen-1;c=c->nextSibling){
                if(c->type==NkXMLNodeType::NK_TEXT||c->type==NkXMLNodeType::NK_CDATA){
                    const int32 len=c->text?(int32)::strlen(c->text):0;
                    const int32 rem=bufLen-1-pos;
                    const int32 n=len<rem?len:rem;
                    if(n>0){::memcpy(buf+pos,c->text,n);pos+=n;}
                }
            }
            buf[pos]=0;
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkXMLNamespaceCtx — contexte de résolution des namespaces
    //  (pile de bindings prefix → URI)
    // ─────────────────────────────────────────────────────────────────────────────

    struct NkXMLNSBinding {
        const char*     prefix; // nullptr = namespace par défaut
        const char*     uri;
        NkXMLNSBinding* prev;   // lien vers le binding précédent du même scope
    };

    struct NkXMLNamespaceCtx {
        NkXMLNSBinding* head = nullptr;

        void Push(const char* prefix, const char* uri, NkXMLArena& arena) noexcept {
            NkXMLNSBinding* b=arena.Alloc<NkXMLNSBinding>();
            if(!b) return;
            b->prefix=prefix; b->uri=uri; b->prev=head; head=b;
        }

        const char* Resolve(const char* prefix) const noexcept {
            for(NkXMLNSBinding* b=head;b;b=b->prev){
                const bool match=(!prefix&&!b->prefix)||
                                (prefix&&b->prefix&&::strcmp(prefix,b->prefix)==0);
                if(match) return b->uri;
            }
            return nullptr;
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkXMLDocument — racine du DOM
    // ─────────────────────────────────────────────────────────────────────────────

    struct NKENTSEU_IMAGE_API NkXMLDocument {
            NkXMLArena arena;
            NkXMLNode* root      = nullptr; // racine document (type Document)
            NkXMLNode* docElement= nullptr; // premier élément racine (ex: <svg>)
            bool       isValid   = false;
            char       errorMsg[128] = {};
            int32      errorLine = 0;

            void Init(usize arenaBytes=4*1024*1024) noexcept { arena.Init(arenaBytes); }
            void Destroy() noexcept { arena.Destroy(); root=nullptr; docElement=nullptr; isValid=false; }

            // ── Recherche ─────────────────────────────────────────────────────────────

            /// Trouve un élément par attribut id="..."
            NKIMG_NODISCARD NkXMLNode* GetElementById(const char* id) const noexcept {
                return docElement ? FindById(docElement,id) : nullptr;
            }

            /// Tous les descendants avec ce tag (résultat dans outNodes, max maxOut)
            int32 GetElementsByTagName(const char* tag,
                                        NkXMLNode** outNodes, int32 maxOut) const noexcept {
                int32 count=0;
                if(docElement) FindByTag(docElement,tag,outNodes,count,maxOut);
                return count;
            }

            /// Sélecteur CSS minimal : "#id", ".class", "tag", "tag.class"
            NKIMG_NODISCARD NkXMLNode* QuerySelector(const char* selector) const noexcept;

            // ── Itérateur ─────────────────────────────────────────────────────────────

            /// Visite tous les nœuds en profondeur d'abord (DFS)
            using VisitFn = bool(*)(NkXMLNode* node, void* userdata);
            void ForEach(VisitFn fn, void* ud) const noexcept {
                if(docElement) WalkDFS(docElement,fn,ud);
            }

        private:
            static NkXMLNode* FindById(NkXMLNode* node, const char* id) noexcept {
                if(node->type==NkXMLNodeType::NK_ELEMENT){
                    const char* v=node->GetAttr("id");
                    if(v&&::strcmp(v,id)==0) return node;
                }
                for(NkXMLNode* c=node->firstChild;c;c=c->nextSibling){
                    NkXMLNode* found=FindById(c,id);
                    if(found) return found;
                }
                return nullptr;
            }
            static void FindByTag(NkXMLNode* node, const char* tag,
                                NkXMLNode** out, int32& cnt, int32 max) noexcept {
                if(node->type==NkXMLNodeType::NK_ELEMENT&&
                node->localName&&(::strcmp(tag,"*")==0||::strcmp(tag,node->localName)==0)){
                    if(cnt<max) out[cnt++]=node;
                }
                for(NkXMLNode* c=node->firstChild;c;c=c->nextSibling)
                    FindByTag(c,tag,out,cnt,max);
            }
            static bool WalkDFS(NkXMLNode* node, VisitFn fn, void* ud) noexcept {
                if(!fn(node,ud)) return false;
                for(NkXMLNode* c=node->firstChild;c;c=c->nextSibling)
                    if(!WalkDFS(c,fn,ud)) return false;
                return true;
            }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkXMLParser — parse du XML vers NkXMLDocument
    // ─────────────────────────────────────────────────────────────────────────────

    class NKENTSEU_IMAGE_API NkXMLParser {
        public:
            /**
             * @Brief Parse un buffer XML UTF-8 et produit un NkXMLDocument.
             * @param xml     Données XML (pas forcément null-terminées).
             * @param size    Taille en octets.
             * @param doc     Document de sortie (doit être initialisé avec Init()).
             * @return true si parse réussi (même avec des warnings).
             */
            static bool Parse(const uint8* xml, usize size, NkXMLDocument& doc) noexcept;

            /// Version avec C-string null-terminée.
            static bool Parse(const char* xml, NkXMLDocument& doc) noexcept {
                return xml ? Parse(reinterpret_cast<const uint8*>(xml),::strlen(xml),doc) : false;
            }

            /// Parse un fichier.
            static bool ParseFile(const char* path, NkXMLDocument& doc) noexcept;

        private:
            // ── Tokenizer ─────────────────────────────────────────────────────────────

            struct Ctx {
                const uint8*     data;
                usize            size;
                usize            pos;
                int32            line;
                NkXMLDocument*   doc;
                NkXMLArena*      arena;
                NkXMLNode*       current; // nœud courant
                NkXMLNamespaceCtx nsCtx;
                bool             error;
                char             errorMsg[128];
            };

            static bool  ParseDocument   (Ctx& c) noexcept;
            static bool  ParseElement    (Ctx& c) noexcept;  // parse <tag ... > content </tag>
            static bool  ParseAttributes (Ctx& c, NkXMLNode* node) noexcept;
            static bool  ParseContent    (Ctx& c, NkXMLNode* parent) noexcept;
            static bool  ParseText       (Ctx& c, NkXMLNode* parent) noexcept;
            static bool  ParseComment    (Ctx& c) noexcept;
            static bool  ParseCDATA      (Ctx& c, NkXMLNode* parent) noexcept;
            static bool  ParsePI         (Ctx& c) noexcept;
            static bool  ParseDoctype    (Ctx& c) noexcept;

            // ── Helpers ───────────────────────────────────────────────────────────────

            static NKIMG_INLINE uint8 Peek(Ctx& c, int32 offset=0) noexcept {
                return (c.pos+offset<c.size)?c.data[c.pos+offset]:0;
            }
            static NKIMG_INLINE uint8 Consume(Ctx& c) noexcept {
                const uint8 b=(c.pos<c.size)?c.data[c.pos++]:0;
                if(b=='\n') ++c.line;
                return b;
            }
            static void SkipWS(Ctx& c) noexcept;
            static bool Match(Ctx& c, const char* str) noexcept;  // consomme si match
            static bool Expect(Ctx& c, char ch) noexcept;
            static const char* ReadName(Ctx& c) noexcept;         // retour dans arena
            static const char* ReadAttrValue(Ctx& c) noexcept;    // avec résolution entités
            static const char* ReadText(Ctx& c, bool inCDATA) noexcept;
            static const char* DecodeEntities(const char* raw, int32 len, NkXMLArena& arena) noexcept;

            // ── Résolution des namespaces ─────────────────────────────────────────────
            static void ResolveNamespaces(Ctx& c, NkXMLNode* node) noexcept;

            // ── Construction DOM ──────────────────────────────────────────────────────
            static NkXMLNode* AllocNode(Ctx& c, NkXMLNodeType type) noexcept;
            static NkXMLAttr* AllocAttr(Ctx& c) noexcept;
            static void AppendChild(NkXMLNode* parent, NkXMLNode* child) noexcept;
    };

} // namespace nkentseu
