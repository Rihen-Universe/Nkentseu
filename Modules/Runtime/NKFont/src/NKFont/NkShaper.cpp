/**
 * @File    NkShaper.cpp
 * @Brief   Shaping OpenType — GSUB complet + GPOS kerning.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Correctness
 *  GSUB : Lookup types 1 (Single), 3 (Alternate), 4 (Ligature), 6 (Chained Context)
 *         via lecture directe des tables OpenType dans rawData.
 *  GPOS : Lookup type 2 (PairPos format 1 et 2) — kerning OpenType.
 *  Kern : table kern format 0 (classique TrueType) par recherche binaire.
 *  Bidi + GSUB + GPOS intégrés dans le pipeline Shape().
 */
#include "NKFont/NkShaper.h"
#include <cstring>
namespace nkentseu {

// ─────────────────────────────────────────────────────────────────────────────
//  Lecture des tables OpenType dans rawData
// ─────────────────────────────────────────────────────────────────────────────

// Lecture big-endian depuis un pointeur arbitraire
static NKFONT_NODISCARD inline uint16 RU16(const uint8* p) noexcept {
    return static_cast<uint16>((p[0]<<8)|p[1]);
}
static NKFONT_NODISCARD inline uint32 RU32(const uint8* p) noexcept {
    return (static_cast<uint32>(p[0])<<24)|(static_cast<uint32>(p[1])<<16)
          |(static_cast<uint32>(p[2])<<8)|p[3];
}
static NKFONT_NODISCARD inline int16 RI16(const uint8* p) noexcept {
    return static_cast<int16>(RU16(p));
}

// Localise une table OpenType dans les données brutes du font
static const uint8* FindTable(const NkTTFFont& font, uint32 tag) noexcept {
    if(!font.rawData||font.rawSize<12) return nullptr;
    const uint8* d=font.rawData;
    const uint16 numTables=RU16(d+4);
    for(uint16 i=0;i<numTables&&12+i*16+16<=font.rawSize;++i){
        const uint8* rec=d+12+i*16;
        const uint32 t=RU32(rec);
        if(t==tag){
            const uint32 off=RU32(rec+8);
            const uint32 len=RU32(rec+12);
            if(off+len<=font.rawSize) return d+off;
            return nullptr;
        }
    }
    return nullptr;
}

// Tags OpenType
static constexpr uint32 TAG_GSUB=0x47535542u; // 'GSUB'
static constexpr uint32 TAG_GPOS=0x47504F53u; // 'GPOS'

// ─────────────────────────────────────────────────────────────────────────────
//  Coverage — lookup O(log n) format 1 et 2
// ─────────────────────────────────────────────────────────────────────────────

static int32 CoverageLookup(const uint8* cov, uint16 gid) noexcept {
    if(!cov) return -1;
    const uint16 fmt=RU16(cov);
    if(fmt==1){
        const uint16 cnt=RU16(cov+2);
        // Liste triée → recherche binaire
        int32 lo=0,hi=static_cast<int32>(cnt)-1;
        while(lo<=hi){
            const int32 mid=(lo+hi)>>1;
            const uint16 g=RU16(cov+4+mid*2);
            if(g==gid) return mid;
            if(g<gid) lo=mid+1; else hi=mid-1;
        }
    } else if(fmt==2){
        const uint16 cnt=RU16(cov+2);
        for(uint16 i=0;i<cnt;++i){
            const uint8* r=cov+4+i*6;
            const uint16 s=RU16(r),e=RU16(r+2),si=RU16(r+4);
            if(gid>=s&&gid<=e) return static_cast<int32>(si+(gid-s));
        }
    }
    return -1;
}

// ClassDef — classe d'un glyphe
static uint16 ClassOf(const uint8* cd, uint16 gid) noexcept {
    if(!cd) return 0;
    const uint16 fmt=RU16(cd);
    if(fmt==1){
        const uint16 start=RU16(cd+2), cnt=RU16(cd+4);
        if(gid>=start&&gid<start+cnt) return RU16(cd+6+(gid-start)*2);
    } else if(fmt==2){
        const uint16 cnt=RU16(cd+2);
        for(uint16 i=0;i<cnt;++i){
            const uint8* r=cd+4+i*6;
            if(gid>=RU16(r)&&gid<=RU16(r+2)) return RU16(r+4);
        }
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkKerning
// ─────────────────────────────────────────────────────────────────────────────

F26Dot6 NkKerning::GetKernTable(const NkTTFFont& font,
                                  uint16 left, uint16 right, uint16 ppem) noexcept
{
    if(!font.hasKern||!font.kernPairs||font.numKernPairs==0)
        return F26Dot6::Zero();
    const uint32 key=(static_cast<uint32>(left)<<16)|right;
    uint32 lo=0,hi=font.numKernPairs;
    while(lo<hi){
        const uint32 mid=(lo+hi)>>1;
        const uint32 cur=(static_cast<uint32>(font.kernPairs[mid].left)<<16)
                        |static_cast<uint32>(font.kernPairs[mid].right);
        if(key<cur) hi=mid;
        else if(key>cur) lo=mid+1;
        else {
            const int32 duVal=font.kernPairs[mid].value;
            const int32 scale=(static_cast<int32>(ppem)<<F26Dot6::SHIFT)
                             /font.head.unitsPerEm;
            return F26Dot6::FromRaw((duVal*scale)>>F26Dot6::SHIFT);
        }
    }
    return F26Dot6::Zero();
}

// ValueRecord : lit XAdvance depuis un ValueRecord GPOS (format bitmask)
static int16 ReadValueRecord_XAdvance(const uint8*& p, uint16 fmt) noexcept {
    int16 xa=0;
    // Bit 0: XPlacement, 1: YPlacement, 2: XAdvance, 3: YAdvance
    if(fmt&0x0001){p+=2;} // XPlacement
    if(fmt&0x0002){p+=2;} // YPlacement
    if(fmt&0x0004){xa=RI16(p);p+=2;} // XAdvance
    if(fmt&0x0008){p+=2;} // YAdvance
    if(fmt&0x0010){p+=2;} // XPlaDevice
    if(fmt&0x0020){p+=2;} // YPlaDevice
    if(fmt&0x0040){p+=2;} // XAdvDevice
    if(fmt&0x0080){p+=2;} // YAdvDevice
    return xa;
}

static int32 ValueRecordSize(uint16 fmt) noexcept {
    int32 n=0;
    for(int32 i=0;i<8;++i) if(fmt&(1<<i)) n+=2;
    return n;
}

F26Dot6 NkKerning::GetGPOSKern(const NkTTFFont& font,
                                  uint16 left, uint16 right,
                                  uint16 ppem, NkMemArena&) noexcept
{
    const uint8* gpos=FindTable(font,TAG_GPOS);
    if(!gpos) return F26Dot6::Zero();

    // GPOS Header : version(4), ScriptListOffset(2), FeatureListOffset(2), LookupListOffset(2)
    const uint16 llOff=RU16(gpos+8);
    const uint8* ll=gpos+llOff;
    const uint16 lookupCount=RU16(ll);

    for(uint16 li=0;li<lookupCount;++li){
        const uint8* lut=gpos+llOff+RU16(ll+2+li*2); // pointeur vers Lookup
        const uint16 lookupType=RU16(lut);
        if(lookupType!=2) continue; // On cherche PairPos (type 2)
        const uint16 subCount=RU16(lut+4);
        for(uint16 si=0;si<subCount;++si){
            const uint8* sub=lut+RU16(lut+6+si*2);
            const uint16 fmt=RU16(sub);
            const uint16 covOff=RU16(sub+2);
            if(CoverageLookup(sub+covOff,left)<0) continue;

            if(fmt==1){
                // Format 1 : paires explicites
                const uint16 vfmt1=RU16(sub+4),vfmt2=RU16(sub+6);
                const uint16 pairSetCount=RU16(sub+8);
                const int32 covIdx=CoverageLookup(sub+covOff,left);
                if(covIdx<0||covIdx>=(int32)pairSetCount) continue;
                const uint8* ps=sub+RU16(sub+10+covIdx*2);
                const uint16 pvc=RU16(ps);
                const int32 vrs1=ValueRecordSize(vfmt1),vrs2=ValueRecordSize(vfmt2);
                for(uint16 pi=0;pi<pvc;++pi){
                    const uint8* pv=ps+2+pi*(2+vrs1+vrs2);
                    if(RU16(pv)==right){
                        const uint8* vr1=pv+2;
                        int16 xa=0;
                        if(vfmt1&0x0004){
                            // XAdvance est au bon offset
                            int32 skip=0;
                            if(vfmt1&0x0001)skip+=2;
                            if(vfmt1&0x0002)skip+=2;
                            xa=RI16(vr1+skip);
                        }
                        const int32 scale=(static_cast<int32>(ppem)<<F26Dot6::SHIFT)
                                         /font.head.unitsPerEm;
                        return F26Dot6::FromRaw((xa*scale)>>F26Dot6::SHIFT);
                    }
                }
            } else if(fmt==2){
                // Format 2 : classes
                const uint16 vfmt1=RU16(sub+4),vfmt2=RU16(sub+6);
                const uint8* cd1=sub+RU16(sub+8);
                const uint8* cd2=sub+RU16(sub+10);
                const uint16 cls1=ClassOf(cd1,left),cls2=ClassOf(cd2,right);
                const uint16 cls2Count=RU16(sub+14);
                const int32 vrs1=ValueRecordSize(vfmt1),vrs2=ValueRecordSize(vfmt2);
                const uint8* row=sub+16+(cls1*cls2Count)*(vrs1+vrs2)+cls2*(vrs1+vrs2);
                int16 xa=0;
                if(vfmt1&0x0004){
                    int32 skip=0;
                    if(vfmt1&0x0001)skip+=2;
                    if(vfmt1&0x0002)skip+=2;
                    xa=RI16(row+skip);
                }
                if(xa!=0){
                    const int32 scale=(static_cast<int32>(ppem)<<F26Dot6::SHIFT)
                                     /font.head.unitsPerEm;
                    return F26Dot6::FromRaw((xa*scale)>>F26Dot6::SHIFT);
                }
            }
        }
    }
    return F26Dot6::Zero();
}

F26Dot6 NkKerning::GetKerning(const NkTTFFont& font,
                                uint16 left, uint16 right, uint16 ppem) noexcept
{
    if(font.hasGPOS){
        uint8 tmp[512]; NkMemArena a; a.InitView(tmp,sizeof(tmp));
        const F26Dot6 gpos=GetGPOSKern(font,left,right,ppem,a);
        if(!gpos.IsZero()) return gpos;
    }
    return GetKernTable(font,left,right,ppem);
}

// ─────────────────────────────────────────────────────────────────────────────
//  GSUB — parser complet des lookups
// ─────────────────────────────────────────────────────────────────────────────

// Applique un lookup GSUB type 1 (Single substitution)
static bool GSUB_Single(const uint8* sub, uint16& gid) noexcept {
    const uint16 fmt=RU16(sub);
    const uint8* cov=sub+RU16(sub+2);
    if(CoverageLookup(cov,gid)<0) return false;
    if(fmt==1){
        gid=static_cast<uint16>((gid+RI16(sub+4))&0xFFFF);
        return true;
    } else if(fmt==2){
        const int32 idx=CoverageLookup(cov,gid);
        const uint16 cnt=RU16(sub+4);
        if(idx>=0&&idx<(int32)cnt){ gid=RU16(sub+6+idx*2); return true; }
    }
    return false;
}

// Applique un lookup GSUB type 4 (Ligature substitution)
// Retourne le nombre de glyphes consommés (1 = pas de match, >1 = match)
static int32 GSUB_Ligature(const uint8* sub, uint16* glyphs,
                             uint32 count, uint32 pos, uint16& outGid) noexcept
{
    const uint8* cov=sub+RU16(sub+2);
    const int32 covIdx=CoverageLookup(cov,glyphs[pos]);
    if(covIdx<0) return 1;
    const uint16 lsCount=RU16(sub+4);
    if(covIdx>=(int32)lsCount) return 1;
    const uint8* ls=sub+RU16(sub+6+covIdx*2);
    const uint16 ligCount=RU16(ls);
    for(uint16 li=0;li<ligCount;++li){
        const uint8* lig=ls+RU16(ls+2+li*2);
        const uint16 ligGlyph=RU16(lig);
        const uint16 compCount=RU16(lig+2); // inclut le premier glyph
        if(pos+compCount-1>=count) continue;
        bool match=true;
        for(uint16 ci=1;ci<compCount&&match;++ci)
            if(glyphs[pos+ci]!=RU16(lig+4+(ci-1)*2)) match=false;
        if(match){outGid=ligGlyph;return compCount;}
    }
    return 1;
}

// Applique un lookup GSUB type 3 (Alternate substitution — prend la première alternative)
static bool GSUB_Alternate(const uint8* sub, uint16& gid) noexcept {
    const uint8* cov=sub+RU16(sub+2);
    const int32 idx=CoverageLookup(cov,gid);
    if(idx<0) return false;
    const uint16 asCnt=RU16(sub+4);
    if(idx>=(int32)asCnt) return false;
    const uint8* as_=sub+RU16(sub+6+idx*2);
    const uint16 altCnt=RU16(as_);
    if(altCnt>0){ gid=RU16(as_+2); return true; }
    return false;
}

// Évalue si une séquence de glyphes correspond à une Input sequence OpenType
static bool MatchSequence(const uint8* seq, uint16 seqCount,
                           const uint16* glyphs, uint32 count, uint32 pos,
                           const uint8* cd=nullptr) noexcept
{
    for(uint16 i=0;i<seqCount;++i){
        if(pos+1+i>=count) return false;
        const uint16 expected=RU16(seq+i*2);
        const uint16 actual=cd?ClassOf(cd,glyphs[pos+1+i]):glyphs[pos+1+i];
        if(actual!=expected) return false;
    }
    return true;
}

// Applique un lookup OpenType (GSUB) identifié par son index
// Déclaration forward
static bool GSUB_ApplyLookup(const uint8* gsubBase, uint16 lookupIdx,
                               uint16* glyphs, uint32*,
                               uint32& count, uint32 pos,
                               NkMemArena&) noexcept;

// Lookup type 6 : Chained Context substitution (format 3 principalement)
static bool GSUB_ChainContext(const uint8* sub, const uint8* gsubBase,
                               uint16 lookupIdx, uint16* glyphs, uint32* cps,
                               uint32& count, uint32 pos, NkMemArena& arena) noexcept
{
    const uint16 fmt=RU16(sub);
    (void)lookupIdx;
    if(fmt==3){
        // Format 3 : coverage-based
        const uint16 backCount=RU16(sub+2);
        uint16 off=4;
        // Vérifie les backtrack glyphs
        for(uint16 i=0;i<backCount;++i){
            if(pos<1+i) return false;
            if(CoverageLookup(sub+RU16(sub+off+i*2),glyphs[pos-1-i])<0) return false;
        }
        off+=backCount*2;
        const uint16 inCount=RU16(sub+off); off+=2;
        for(uint16 i=0;i<inCount;++i){
            if(pos+i>=count) return false;
            if(CoverageLookup(sub+RU16(sub+off+i*2),glyphs[pos+i])<0) return false;
        }
        off+=inCount*2;
        const uint16 laCount=RU16(sub+off); off+=2;
        for(uint16 i=0;i<laCount;++i){
            if(pos+inCount+i>=count) return false;
            if(CoverageLookup(sub+RU16(sub+off+i*2),glyphs[pos+inCount+i])<0) return false;
        }
        off+=laCount*2;
        const uint16 substCount=RU16(sub+off); off+=2;
        for(uint16 si=0;si<substCount;++si){
            const uint16 seqIdx=RU16(sub+off+si*4);
            const uint16 li2=RU16(sub+off+si*4+2);
            if(pos+seqIdx<count)
                GSUB_ApplyLookup(gsubBase,li2,glyphs,cps,count,pos+seqIdx,arena);
        }
        return true;
    }
    return false;
}

static bool GSUB_ApplyLookup(const uint8* gsubBase, uint16 lookupIdx,
                               uint16* glyphs, uint32* cps,
                               uint32& count, uint32 pos,
                               NkMemArena& arena) noexcept
{
    if(!gsubBase) return false;
    const uint16 llOff=RU16(gsubBase+8);
    const uint8* ll=gsubBase+llOff;
    const uint16 lCount=RU16(ll);
    if(lookupIdx>=lCount) return false;
    const uint8* lut=gsubBase+llOff+RU16(ll+2+lookupIdx*2);
    const uint16 ltype=RU16(lut);
    const uint16 sCount=RU16(lut+4);

    for(uint16 si=0;si<sCount;++si){
        const uint8* sub=lut+RU16(lut+6+si*2);
        bool applied=false;
        if(ltype==1){
            uint16 newGid=glyphs[pos];
            if(GSUB_Single(sub,newGid)){glyphs[pos]=newGid;applied=true;}
        } else if(ltype==3){
            uint16 newGid=glyphs[pos];
            if(GSUB_Alternate(sub,newGid)){glyphs[pos]=newGid;applied=true;}
        } else if(ltype==4){
            uint16 ligGid=0;
            const int32 consumed=GSUB_Ligature(sub,glyphs,count,pos,ligGid);
            if(consumed>1){
                glyphs[pos]=ligGid;
                const uint32 remove=static_cast<uint32>(consumed-1);
                // Décale le tableau
                for(uint32 i=pos+1;i+remove<=count;++i){
                    glyphs[i]=glyphs[i+remove];
                    if(cps) cps[i]=cps[i+remove];
                }
                count-=remove;
                applied=true;
            }
        } else if(ltype==6){
            applied=GSUB_ChainContext(sub,gsubBase,lookupIdx,
                                       glyphs,cps,count,pos,arena);
        }
        if(applied) return true;
    }
    return false;
}

bool NkGSUB::Apply(const NkTTFFont& font,
                    uint16* glyphs, uint32* cps,
                    uint32 count, uint32& outCount,
                    NkMemArena& arena, uint32 features) noexcept
{
    outCount=count;
    if(!font.hasGSUB||count==0) return true;
    const uint8* gsub=FindTable(font,TAG_GSUB);
    if(!gsub) return true;

    // GSUB Header : version(4), ScriptListOffset(2), FeatureListOffset(2), LookupListOffset(2)
    const uint16 flOff=RU16(gsub+6);
    const uint16 llOff=RU16(gsub+8);
    const uint8* fl=gsub+flOff;
    const uint16 fCount=RU16(fl);

    // Lookup tags que l'on active selon le masque de features
    // Bit mapping simplifié : on active liga(0), calt(1), kern(2), etc.
    static const char* kActiveFeatures[]={"liga","calt","clig","dlig","rlig","salt","ss01",nullptr};

    for(uint16 fi=0;fi<fCount;++fi){
        const char* ftag=reinterpret_cast<const char*>(fl+2+fi*6);
        bool active=false;
        for(int32 k=0;kActiveFeatures[k]&&!active;++k){
            if(::strncmp(ftag,kActiveFeatures[k],4)==0) active=true;
        }
        if(!active&&(features!=0xFFFFFFFFu)) continue;

        const uint8* feat=gsub+flOff+RU16(fl+2+fi*6+4);
        const uint16 lCount=RU16(feat+2);
        for(uint16 li=0;li<lCount;++li){
            const uint16 lidx=RU16(feat+4+li*2);
            for(uint32 pos=0;pos<outCount;++pos)
                GSUB_ApplyLookup(gsub,lidx,glyphs,cps,outCount,pos,arena);
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkShaper::Shape
// ─────────────────────────────────────────────────────────────────────────────

bool NkShaper::Shape(const NkTTFFont& font, const char* text,
                      NkMemArena& arena, NkGlyphRun& out,
                      const Options& opts) noexcept
{
    if(!text) return false;
    usize len=0; while(text[len]) ++len;
    return Shape(font,reinterpret_cast<const uint8*>(text),len,arena,out,opts);
}

bool NkShaper::Shape(const NkTTFFont& font,
                      const uint8* utf8, usize byteLen,
                      NkMemArena& arena, NkGlyphRun& out,
                      const Options& opts) noexcept
{
    ::memset(&out,0,sizeof(out));
    if(!font.isValid||!utf8||byteLen==0) return false;

    NkScratchArena scratch(arena);

    // 1. UTF-8 → codepoints
    uint32* cps=nullptr; uint32 cpCount=0;
    if(!NkUnicode::UTF8Decode(utf8,byteLen,arena,cps,cpCount)||cpCount==0) return true;

    // 2. Bidi
    uint8*  levels=nullptr;
    uint32* order=nullptr;
    if(opts.enableBidi){
        NkUnicode::BidiResolve(cps,cpCount,arena,levels,opts.baseLevel);
        NkUnicode::BidiReorder(levels,cpCount,arena,order);
    }

    // 3. Codepoints → glyph IDs (ordre visuel)
    uint16* gids=arena.Alloc<uint16>(cpCount);
    uint32* orderedCps=arena.Alloc<uint32>(cpCount);
    if(!gids||!orderedCps) return false;

    for(uint32 i=0;i<cpCount;++i){
        const uint32 logIdx=(order&&opts.enableBidi)?order[i]:i;
        gids[i]=font.GetGlyphId(cps[logIdx]);
        orderedCps[i]=cps[logIdx];
    }

    // 4. GSUB — substitutions actives
    uint32 glyphCount=cpCount;
    if(font.hasGSUB)
        NkGSUB::Apply(font,gids,orderedCps,cpCount,glyphCount,arena,opts.features);

    // 5. Construit le NkGlyphRun
    out.glyphs=arena.Alloc<NkGlyphInfo>(glyphCount);
    out.numGlyphs=glyphCount;
    if(!out.glyphs) return false;

    F26Dot6 totalAdv=F26Dot6::Zero();
    const int32 scale=(static_cast<int32>(opts.ppem)<<F26Dot6::SHIFT)
                     /font.head.unitsPerEm;

    for(uint32 i=0;i<glyphCount;++i){
        NkGlyphInfo& g=out.glyphs[i];
        const uint32 logIdx=(order&&opts.enableBidi)?order[i<cpCount?i:cpCount-1]:i;
        g.glyphId   =gids[i];
        g.codepoint =(i<cpCount)?orderedCps[i]:0;
        g.level     =levels?(i<cpCount?levels[logIdx]:0u):0u;
        g.isRTL     =levels?(g.level&1)!=0:false;
        g.isMark    =NkUnicode::IsCombining(g.codepoint);
        g.isLigature=false;
        g.ligComponents=1;
        g.xOffset   =F26Dot6::Zero();
        g.yOffset   =F26Dot6::Zero();

        const NkTTFHMetric hm=font.GetHMetric(g.glyphId);
        g.xAdvance=F26Dot6::FromRaw(
            (static_cast<int32>(hm.advanceWidth)*scale)>>F26Dot6::SHIFT);
        g.yAdvance=F26Dot6::Zero();

        // 6. Kerning (GPOS ou kern table)
        if(opts.enableKern&&i+1<glyphCount){
            const F26Dot6 kern=NkKerning::GetKerning(font,gids[i],gids[i+1],opts.ppem);
            g.xAdvance+=kern;
        }
        totalAdv+=g.xAdvance;
    }

    out.totalAdvance=totalAdv;
    out.isRTL=(levels&&cpCount>0)?(levels[0]&1)!=0:false;
    return true;
}

} // namespace nkentseu
