/**
 * @File    NkXMLParser.cpp
 * @Brief   Parser XML complet — tokenizer récursif + DOM.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "NKImage/NkXMLParser.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
namespace nkentseu {

// ─────────────────────────────────────────────────────────────────────────────
//  NkXMLDocument::QuerySelector — sélecteur CSS minimal
// ─────────────────────────────────────────────────────────────────────────────

NkXMLNode* NkXMLDocument::QuerySelector(const char* sel) const noexcept {
    if(!sel||!docElement) return nullptr;
    // Sélecteurs supportés : "#id", ".class", "tag", "tag.class", "tag#id"
    if(sel[0]=='#'){
        return GetElementById(sel+1);
    }
    // Recherche récursive avec critère combiné
    struct QCtx { const char* sel; NkXMLNode* result; };
    QCtx qc{sel,nullptr};
    ForEach([](NkXMLNode* node, void* ud)->bool{
        auto* q=static_cast<QCtx*>(ud);
        if(node->type!=NkXMLNodeType::Element) return true;
        const char* sel2=q->sel;
        // Parse le sélecteur
        char tag[64]={},cls[64]={},id2[64]={};
        const char* p=sel2;
        // Tag
        int32 ti=0; while(*p&&*p!='.'&&*p!='#') tag[ti++]=*p++;
        // .class ou #id
        while(*p){
            if(*p=='.'){++p;int32 ci=0;while(*p&&*p!='.'&&*p!='#') cls[ci++]=*p++;cls[ci]=0;}
            else if(*p=='#'){++p;int32 ii=0;while(*p&&*p!='.'&&*p!='#') id2[ii++]=*p++;id2[ii]=0;}
            else ++p;
        }
        bool match=true;
        if(tag[0]&&::strcmp(tag,"*")!=0)
            match=match&&node->localName&&::strcmp(node->localName,tag)==0;
        if(cls[0]){
            const char* cv=node->GetAttr("class");
            if(!cv){match=false;}
            else {
                // Cherche la classe dans la liste séparée par espaces
                bool found=false;
                const char* c2=cv;
                while(*c2){
                    while(*c2==' ') ++c2;
                    const char* e=c2; while(*e&&*e!=' ') ++e;
                    if(static_cast<int32>(e-c2)==(int32)::strlen(cls)&&
                       ::strncmp(c2,cls,e-c2)==0){found=true;break;}
                    c2=e;
                }
                match=match&&found;
            }
        }
        if(id2[0]){
            const char* iv=node->GetAttr("id");
            match=match&&iv&&::strcmp(iv,id2)==0;
        }
        if(match){q->result=node;return false;} // arrête la traversée
        return true;
    },&qc);
    return qc.result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers du tokenizer
// ─────────────────────────────────────────────────────────────────────────────

void NkXMLParser::SkipWS(Ctx& c) noexcept {
    while(c.pos<c.size){
        const uint8 b=c.data[c.pos];
        if(b==' '||b=='\t'||b=='\r'){++c.pos;}
        else if(b=='\n'){++c.pos;++c.line;}
        else break;
    }
}

bool NkXMLParser::Match(Ctx& c, const char* str) noexcept {
    const int32 len=static_cast<int32>(::strlen(str));
    if(c.pos+len>c.size) return false;
    if(::memcmp(c.data+c.pos,str,len)!=0) return false;
    for(int32 i=0;i<len;++i){if(c.data[c.pos+i]=='\n')++c.line;}
    c.pos+=len;
    return true;
}

bool NkXMLParser::Expect(Ctx& c, char ch) noexcept {
    SkipWS(c);
    if(c.pos>=c.size||c.data[c.pos]!=static_cast<uint8>(ch)){
        if(!c.error){
            c.error=true;
            ::snprintf(c.errorMsg,sizeof(c.errorMsg),
                "Expected '%c' at line %d (got '%c')",ch,c.line,
                c.pos<c.size?(char)c.data[c.pos]:'?');
            c.doc->errorLine=c.line;
        }
        return false;
    }
    if(ch=='\n') ++c.line;
    ++c.pos;
    return true;
}

// Lit un nom XML (NCName) : [a-zA-Z_:][a-zA-Z0-9_:.-]*
const char* NkXMLParser::ReadName(Ctx& c) noexcept {
    if(c.pos>=c.size) return nullptr;
    const uint8 first=c.data[c.pos];
    if(!((first>='a'&&first<='z')||(first>='A'&&first<='Z')||first=='_'||first==':'||first>127))
        return nullptr;
    const usize start=c.pos;
    while(c.pos<c.size){
        const uint8 b=c.data[c.pos];
        if((b>='a'&&b<='z')||(b>='A'&&b<='Z')||(b>='0'&&b<='9')||
           b=='_'||b==':'||b=='-'||b=='.'||b>127)
            ++c.pos;
        else break;
    }
    return c.arena->Dup(reinterpret_cast<const char*>(c.data+start),
                        static_cast<int32>(c.pos-start));
}

// Résout les entités XML dans une chaîne brute
const char* NkXMLParser::DecodeEntities(const char* raw, int32 len,
                                          NkXMLArena& arena) noexcept
{
    if(!raw) return nullptr;
    char* dst=arena.Alloc<char>(len+1);
    if(!dst) return raw;
    int32 di=0;
    for(int32 i=0;i<len;){
        if(raw[i]!='&'||i+1>=len){dst[di++]=raw[i++];continue;}
        // Entité
        int32 j=i+1;
        while(j<len&&raw[j]!=';'&&j-i<12) ++j;
        if(j>=len||raw[j]!=';'){dst[di++]=raw[i++];continue;}
        const char* ent=raw+i+1; const int32 elen=j-(i+1);
        i=j+1;
        // Entités prédéfinies
        if(elen==3&&::strncmp(ent,"amp",3)==0){dst[di++]='&';}
        else if(elen==2&&::strncmp(ent,"lt",2)==0){dst[di++]='<';}
        else if(elen==2&&::strncmp(ent,"gt",2)==0){dst[di++]='>';}
        else if(elen==4&&::strncmp(ent,"quot",4)==0){dst[di++]='"';}
        else if(elen==4&&::strncmp(ent,"apos",4)==0){dst[di++]='\'';}
        else if(elen==5&&::strncmp(ent,"nbsp",5)==0){dst[di++]=(char)0xC2;dst[di++]=(char)0xA0;} // U+00A0 UTF-8
        else if(ent[0]=='#'){
            // Entité numérique
            uint32 cp=0;
            if(ent[1]=='x'||ent[1]=='X'){
                // Hexadécimale
                for(int32 k=2;k<elen;++k){
                    const char hc=ent[k];
                    uint32 nib=(hc>='0'&&hc<='9')?hc-'0':(hc>='a'&&hc<='f')?hc-'a'+10:(hc>='A'&&hc<='F')?hc-'A'+10:0;
                    cp=(cp<<4)|nib;
                }
            } else {
                for(int32 k=1;k<elen;++k) cp=cp*10+(ent[k]-'0');
            }
            // Encode en UTF-8
            if(cp<0x80){dst[di++]=static_cast<char>(cp);}
            else if(cp<0x800){dst[di++]=static_cast<char>(0xC0|(cp>>6));dst[di++]=static_cast<char>(0x80|(cp&0x3F));}
            else if(cp<0x10000){dst[di++]=static_cast<char>(0xE0|(cp>>12));dst[di++]=static_cast<char>(0x80|((cp>>6)&0x3F));dst[di++]=static_cast<char>(0x80|(cp&0x3F));}
            else {dst[di++]=static_cast<char>(0xF0|(cp>>18));dst[di++]=static_cast<char>(0x80|((cp>>12)&0x3F));dst[di++]=static_cast<char>(0x80|((cp>>6)&0x3F));dst[di++]=static_cast<char>(0x80|(cp&0x3F));}
        } else {
            // Entité inconnue : reproduit littéralement
            dst[di++]='&';
            for(int32 k=0;k<elen;++k) dst[di++]=ent[k];
            dst[di++]=';';
        }
    }
    dst[di]=0;
    // Réalloue avec la taille exacte si économie importante
    return dst;
}

// Lit la valeur d'un attribut (entre guillemets simples ou doubles)
const char* NkXMLParser::ReadAttrValue(Ctx& c) noexcept {
    SkipWS(c);
    if(c.pos>=c.size) return "";
    const uint8 quote=c.data[c.pos];
    if(quote!='"'&&quote!='\'') return "";
    ++c.pos;
    const usize start=c.pos;
    while(c.pos<c.size&&c.data[c.pos]!=quote){
        if(c.data[c.pos]=='\n') ++c.line;
        ++c.pos;
    }
    const int32 len=static_cast<int32>(c.pos-start);
    if(c.pos<c.size) ++c.pos; // ferme le guillemet
    return DecodeEntities(reinterpret_cast<const char*>(c.data+start),len,*c.arena);
}

// Lit du texte jusqu'à '<'
const char* NkXMLParser::ReadText(Ctx& c, bool inCDATA) noexcept {
    const usize start=c.pos;
    if(inCDATA){
        while(c.pos+2<c.size){
            if(c.data[c.pos]==']'&&c.data[c.pos+1]==']'&&c.data[c.pos+2]=='>') break;
            if(c.data[c.pos]=='\n') ++c.line;
            ++c.pos;
        }
    } else {
        while(c.pos<c.size&&c.data[c.pos]!='<'){
            if(c.data[c.pos]=='\n') ++c.line;
            ++c.pos;
        }
    }
    const int32 len=static_cast<int32>(c.pos-start);
    if(!inCDATA)
        return DecodeEntities(reinterpret_cast<const char*>(c.data+start),len,*c.arena);
    return c.arena->Dup(reinterpret_cast<const char*>(c.data+start),len);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Construction DOM
// ─────────────────────────────────────────────────────────────────────────────

NkXMLNode* NkXMLParser::AllocNode(Ctx& c, NkXMLNodeType type) noexcept {
    NkXMLNode* n=c.arena->Alloc<NkXMLNode>();
    if(n){ n->type=type; n->lineNumber=c.line; }
    return n;
}

NkXMLAttr* NkXMLParser::AllocAttr(Ctx& c) noexcept {
    return c.arena->Alloc<NkXMLAttr>();
}

void NkXMLParser::AppendChild(NkXMLNode* parent, NkXMLNode* child) noexcept {
    child->parent=parent;
    if(!parent->firstChild){ parent->firstChild=parent->lastChild=child; }
    else {
        child->prevSibling=parent->lastChild;
        parent->lastChild->nextSibling=child;
        parent->lastChild=child;
    }
    ++parent->childCount;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Résolution des namespaces
// ─────────────────────────────────────────────────────────────────────────────

void NkXMLParser::ResolveNamespaces(Ctx& c, NkXMLNode* node) noexcept {
    // 1. Scanne les attributs xmlns et xmlns:prefix
    for(NkXMLAttr* a=node->attrs;a;a=a->next){
        if(!a->name) continue;
        if(::strcmp(a->name,"xmlns")==0){
            c.nsCtx.Push(nullptr,a->value,*c.arena);
        } else if(::strncmp(a->name,"xmlns:",6)==0){
            c.nsCtx.Push(c.arena->Dup(a->name+6),a->value,*c.arena);
        }
    }
    // 2. Résout le namespace de l'élément
    node->ns=c.nsCtx.Resolve(node->prefix);
    // 3. Résout les namespaces des attributs
    for(NkXMLAttr* a=node->attrs;a;a=a->next){
        if(a->prefix) a->ns=c.nsCtx.Resolve(a->prefix);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  ParseAttributes
// ─────────────────────────────────────────────────────────────────────────────

bool NkXMLParser::ParseAttributes(Ctx& c, NkXMLNode* node) noexcept {
    NkXMLAttr* lastAttr=nullptr;
    while(!c.error&&c.pos<c.size){
        SkipWS(c);
        if(c.pos>=c.size) break;
        const uint8 b=c.data[c.pos];
        if(b=='>'||b=='/') break; // fin des attributs
        // Nom de l'attribut
        const char* name=ReadName(c);
        if(!name) break;
        SkipWS(c);
        // = valeur (optionnel pour attributs booléens XML)
        const char* value="";
        if(c.pos<c.size&&c.data[c.pos]=='='){
            ++c.pos;
            SkipWS(c);
            value=ReadAttrValue(c);
        }
        NkXMLAttr* attr=AllocAttr(c);
        if(!attr) break;
        attr->name=name;
        attr->value=value?value:"";
        // Parse préfixe
        const char* colon=::strchr(name,':');
        if(colon){
            attr->prefix=c.arena->Dup(name,(int32)(colon-name));
            attr->localName=colon+1;
        } else {
            attr->prefix=nullptr;
            attr->localName=name;
        }
        // Chaîne dans la liste d'attributs
        if(!node->attrs) node->attrs=attr;
        else lastAttr->next=attr;
        lastAttr=attr;
        ++node->numAttrs;
    }
    return !c.error;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ParseComment
// ─────────────────────────────────────────────────────────────────────────────

bool NkXMLParser::ParseComment(Ctx& c) noexcept {
    // On est après "<!--"
    const usize start=c.pos;
    while(c.pos+2<c.size){
        if(c.data[c.pos]=='-'&&c.data[c.pos+1]=='-'&&c.data[c.pos+2]=='>'){c.pos+=3;break;}
        if(c.data[c.pos]=='\n') ++c.line;
        ++c.pos;
    }
    return true; // commentaires ignorés
}

// ─────────────────────────────────────────────────────────────────────────────
//  ParseCDATA
// ─────────────────────────────────────────────────────────────────────────────

bool NkXMLParser::ParseCDATA(Ctx& c, NkXMLNode* parent) noexcept {
    // On est après "<![CDATA["
    const char* text=ReadText(c,true);
    if(c.pos+2<c.size) c.pos+=3; // saute "]]>"
    NkXMLNode* node=AllocNode(c,NkXMLNodeType::CDATA);
    if(node){ node->text=text; AppendChild(parent,node); }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ParsePI — Processing Instruction
// ─────────────────────────────────────────────────────────────────────────────

bool NkXMLParser::ParsePI(Ctx& c) noexcept {
    // On est après "<?"
    while(c.pos+1<c.size){
        if(c.data[c.pos]=='?'&&c.data[c.pos+1]=='>'){c.pos+=2;break;}
        if(c.data[c.pos]=='\n') ++c.line;
        ++c.pos;
    }
    return true; // PI ignorées (<?xml ... ?> déjà sauté)
}

// ─────────────────────────────────────────────────────────────────────────────
//  ParseDoctype
// ─────────────────────────────────────────────────────────────────────────────

bool NkXMLParser::ParseDoctype(Ctx& c) noexcept {
    // On est après "<!DOCTYPE"
    // Ignore tout jusqu'au '>' de fermeture (en gérant les sous-structures [...])
    int32 depth=0;
    while(c.pos<c.size){
        const uint8 b=c.data[c.pos++];
        if(b=='\n') ++c.line;
        else if(b=='[') ++depth;
        else if(b==']') --depth;
        else if(b=='>'&&depth<=0) break;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ParseElement — nœud récursif principal
// ─────────────────────────────────────────────────────────────────────────────

bool NkXMLParser::ParseElement(Ctx& c) noexcept {
    // On est juste après '<' (vérifié par l'appelant)
    const char* name=ReadName(c);
    if(!name){
        // Essaie de sauter jusqu'à '>'
        while(c.pos<c.size&&c.data[c.pos]!='>') ++c.pos;
        if(c.pos<c.size) ++c.pos;
        return false;
    }

    NkXMLNode* node=AllocNode(c,NkXMLNodeType::Element);
    if(!node) return false;
    node->tagName=name;

    // Parse préfixe de namespace
    const char* colon=::strchr(name,':');
    if(colon){
        node->prefix=c.arena->Dup(name,(int32)(colon-name));
        node->localName=colon+1;
    } else {
        node->prefix=nullptr;
        node->localName=name;
    }

    // Parse les attributs
    ParseAttributes(c,node);
    if(c.error) return false;

    // Résout les namespaces (après avoir lu tous les xmlns)
    ResolveNamespaces(c,node);

    // Attach au parent courant
    NkXMLNode* parent=c.current;
    AppendChild(parent,node);
    if(!c.doc->docElement&&parent==c.doc->root)
        c.doc->docElement=node;

    SkipWS(c);

    if(c.pos<c.size&&c.data[c.pos]=='/'){
        // Balise auto-fermante
        ++c.pos;
        if(c.pos<c.size&&c.data[c.pos]=='>') ++c.pos;
        return true;
    }

    if(c.pos>=c.size||c.data[c.pos]!='>') return !c.error;
    ++c.pos; // consomme '>'

    // Parse le contenu (texte + enfants)
    c.current=node;
    if(!ParseContent(c,node)){c.current=parent;return false;}
    c.current=parent;

    return !c.error;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ParseText — nœud texte
// ─────────────────────────────────────────────────────────────────────────────

bool NkXMLParser::ParseText(Ctx& c, NkXMLNode* parent) noexcept {
    const char* text=ReadText(c,false);
    if(!text||text[0]==0) return true; // texte vide → ignore
    // Vérifie si le texte n'est pas que des espaces (optimisation)
    bool allWS=true;
    for(const char* p=text;*p&&allWS;++p)
        if(*p!=' '&&*p!='\t'&&*p!='\n'&&*p!='\r') allWS=false;
    if(allWS) return true; // texte blanc seul → ignore

    NkXMLNode* node=AllocNode(c,NkXMLNodeType::Text);
    if(node){ node->text=text; AppendChild(parent,node); }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ParseContent — contenu d'un élément (récursif)
// ─────────────────────────────────────────────────────────────────────────────

bool NkXMLParser::ParseContent(Ctx& c, NkXMLNode* parent) noexcept {
    while(!c.error&&c.pos<c.size){
        if(c.data[c.pos]!='<'){
            // Texte
            ParseText(c,parent);
            continue;
        }
        ++c.pos; // consomme '<'

        if(c.pos>=c.size) break;

        const uint8 b=c.data[c.pos];

        if(b=='/'){
            // Balise fermante </tag>
            ++c.pos;
            SkipWS(c);
            // Lit le nom (pour validation, mais on n'est pas strict)
            ReadName(c); // consomme le nom
            SkipWS(c);
            if(c.pos<c.size&&c.data[c.pos]=='>') ++c.pos;
            return true; // sort du contenu de l'élément courant
        }
        else if(b=='!'){
            ++c.pos;
            if(Match(c,"--")){
                ParseComment(c);
            } else if(Match(c,"[CDATA[")){
                ParseCDATA(c,parent);
            } else if(Match(c,"DOCTYPE")){
                ParseDoctype(c);
            } else {
                // Tag de déclaration inconnu → saute jusqu'à '>'
                while(c.pos<c.size&&c.data[c.pos]!='>') ++c.pos;
                if(c.pos<c.size) ++c.pos;
            }
        }
        else if(b=='?'){
            ++c.pos; ParsePI(c);
        }
        else {
            // Élément enfant
            ParseElement(c);
        }
    }
    return !c.error;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ParseDocument — entrée principale
// ─────────────────────────────────────────────────────────────────────────────

bool NkXMLParser::ParseDocument(Ctx& c) noexcept {
    // Crée la racine Document
    c.doc->root=AllocNode(c,NkXMLNodeType::Document);
    if(!c.doc->root) return false;
    c.current=c.doc->root;

    // Saute le BOM UTF-8 éventuel
    if(c.pos+2<c.size&&c.data[c.pos]==0xEF&&c.data[c.pos+1]==0xBB&&c.data[c.pos+2]==0xBF)
        c.pos+=3;

    // Prologue XML <?xml ... ?>
    SkipWS(c);
    if(c.pos+4<c.size&&c.data[c.pos]=='<'&&c.data[c.pos+1]=='?'){
        c.pos+=2; ParsePI(c);
        SkipWS(c);
    }

    // DOCTYPE éventuel
    if(c.pos+8<c.size&&::memcmp(c.data+c.pos,"<!DOCTYPE",9)==0){
        c.pos+=9; ParseDoctype(c); SkipWS(c);
    }
    // Commentaires avant la racine
    while(c.pos+3<c.size&&c.data[c.pos]=='<'&&c.data[c.pos+1]=='!'&&c.data[c.pos+2]=='-'){
        c.pos+=4; ParseComment(c); SkipWS(c);
    }

    // Parse le document (un seul élément racine + éventuellement du texte/PI)
    while(!c.error&&c.pos<c.size){
        SkipWS(c);
        if(c.pos>=c.size) break;
        if(c.data[c.pos]!='<') { ++c.pos; continue; }
        ++c.pos;
        if(c.pos>=c.size) break;
        const uint8 b=c.data[c.pos];
        if(b=='?'){++c.pos;ParsePI(c);}
        else if(b=='!'){
            ++c.pos;
            if(Match(c,"--")) ParseComment(c);
            else if(Match(c,"DOCTYPE")) ParseDoctype(c);
        } else {
            ParseElement(c);
            // Après l'élément racine : ignore le reste (commentaires, PIs)
        }
    }
    return !c.error;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkXMLParser::Parse — point d'entrée public
// ─────────────────────────────────────────────────────────────────────────────

bool NkXMLParser::Parse(const uint8* xml, usize size, NkXMLDocument& doc) noexcept {
    if(!xml||size==0) return false;

    Ctx c;
    c.data  = xml;
    c.size  = size;
    c.pos   = 0;
    c.line  = 1;
    c.doc   = &doc;
    c.arena = &doc.arena;
    c.current=nullptr;
    c.error = false;
    c.errorMsg[0]=0;

    const bool ok=ParseDocument(c);
    doc.isValid=ok&&doc.docElement!=nullptr;

    if(c.error){
        ::strncpy(doc.errorMsg,c.errorMsg,sizeof(doc.errorMsg)-1);
        doc.errorLine=c.line;
    }
    return doc.isValid;
}

bool NkXMLParser::ParseFile(const char* path, NkXMLDocument& doc) noexcept {
    FILE* f=::fopen(path,"rb");
    if(!f) return false;
    ::fseek(f,0,SEEK_END); const long sz=::ftell(f); ::fseek(f,0,SEEK_SET);
    if(sz<=0){::fclose(f);return false;}
    uint8* buf=static_cast<uint8*>(::malloc(sz));
    if(!buf){::fclose(f);return false;}
    const bool readOK=(::fread(buf,1,sz,f)==static_cast<usize>(sz));
    ::fclose(f);
    bool ok=false;
    if(readOK) ok=Parse(buf,sz,doc);
    ::free(buf);
    return ok;
}

} // namespace nkentseu
