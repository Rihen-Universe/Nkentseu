/**
 * @File    NkUILayout2.cpp
 * @Brief   SaveLayout/LoadLayout, ColorPicker complet, NKFont intégration,
 *          NkUIOpenGLRenderer.
 */

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: OpenGL renderer adapter implementation.
 * Main data: GL pipeline state, texture upload, clip/scissor draw loop.
 * Change this file when: OpenGL backend artifacts or text/texture issues appear.
 */
#include "NKUI/NkUILayout2.h"
#include "NKUI/NkUIWidgets.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>

#ifdef NKUI_WITH_OPENGL
    #include <GL/gl.h>
#endif

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUILayoutSerializer
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUILayoutSerializer::Save(const NkUIWindowManager& wm,
                                        const NkUIDockManager& dm,
                                        const char* path) noexcept
        {
            char* json=nullptr; usize len=0;
            if(!SaveToMemory(wm,dm,json,len)||!json) return false;
            FILE* f=::fopen(path,"wb");
            if(!f){memory::NkFree(json);return false;}
            ::fwrite(json,1,len,f); ::fclose(f); memory::NkFree(json);
            return true;
        }

        bool NkUILayoutSerializer::SaveToMemory(const NkUIWindowManager& wm,
                                                const NkUIDockManager& dm,
                                                char*& outJson,usize& outLen) noexcept
        {
            // Alloue un buffer suffisant
            const usize cap=64*1024;
            char* buf=static_cast<char*>(memory::NkAlloc(cap));
            if(!buf) return false;
            int32 pos=0;

            auto W=[&](const char* s){ while(*s&&pos<(int32)cap-2) buf[pos++]=*s++; };
            auto F=[&](float32 v){ char tmp[32]; ::snprintf(tmp,sizeof(tmp),"%.2f",v); W(tmp); };
            auto I=[&](int32 v)  { char tmp[32]; ::snprintf(tmp,sizeof(tmp),"%d",v);   W(tmp); };
            auto B=[&](bool v)   { W(v?"true":"false"); };
            auto S=[&](const char* s){ W("\""); W(s); W("\""); };
            auto K=[&](const char* k){ S(k); W(":"); };

            W("{\n  \"windows\":[\n");
            for(int32 i=0;i<wm.numWindows;++i){
                const NkUIWindowState& ws=wm.windows[i];
                W("    {");
                K("name"); S(ws.name); W(",");
                K("x"); F(ws.pos.x); W(",");
                K("y"); F(ws.pos.y); W(",");
                K("w"); F(ws.size.x); W(",");
                K("h"); F(ws.size.y); W(",");
                K("open"); B(ws.isOpen); W(",");
                K("collapsed"); B(ws.isCollapsed); W(",");
                K("maximized"); B(false); W(",");
                K("scrollY"); F(ws.scrollY);
                W(i<wm.numWindows-1?"  },\n":"  }\n");
            }
            W("  ],\n  \"dock\":{\n    \"root\":"); I(dm.rootIdx);
            W(",\n    \"nodes\":[\n");
            for(int32 i=0;i<dm.numNodes;++i){
                const NkUIDockNode& n=dm.nodes[i];
                W("      {");
                K("idx"); I(i); W(",");
                K("type"); I(static_cast<int32>(n.type)); W(",");
                K("ratio"); F(n.splitRatio); W(",");
                K("c0"); I(n.children[0]); W(",");
                K("c1"); I(n.children[1]); W(",");
                K("parent"); I(n.parent); W(",");
                K("active"); I(n.activeTab); W(",");
                K("wins"); W("[");
                for(int32 j=0;j<n.numWindows;++j){
                    // Trouve le nom de la fenêtre
                    const NkUIWindowState* ws=const_cast<NkUIWindowManager&>(wm).Find(n.windows[j]);
                    if(ws){ S(ws->name); } else W("\"\"");
                    if(j<n.numWindows-1) W(",");
                }
                W("]}");
                if(i<dm.numNodes-1) W(",\n");
            }
            W("\n    ]\n  }\n}\n");
            buf[pos]=0;
            outJson=buf; outLen=static_cast<usize>(pos);
            return true;
        }

        bool NkUILayoutSerializer::Load(NkUIWindowManager& wm,NkUIDockManager& dm,
                                        const char* path) noexcept
        {
            FILE* f=::fopen(path,"rb"); if(!f) return false;
            ::fseek(f,0,SEEK_END); const long sz=::ftell(f); ::fseek(f,0,SEEK_SET);
            if(sz<=0){::fclose(f);return false;}
            char* buf=static_cast<char*>(memory::NkAlloc(sz+1));
            if(!buf){::fclose(f);return false;}
            ::fread(buf,1,sz,f); buf[sz]=0; ::fclose(f);
            const bool ok=LoadFromMemory(wm,dm,buf,sz);
            memory::NkFree(buf); return ok;
        }

        bool NkUILayoutSerializer::LoadFromMemory(NkUIWindowManager& wm,NkUIDockManager& dm,
                                                    const char* json,usize /*len*/) noexcept
        {
            if(!json) return false;
            // Parser JSON minimal — cherche les clés connues
            auto findStr=[](const char* js,const char* key,char* out,int32 maxOut)->const char*{
                char kk[64]; ::snprintf(kk,sizeof(kk),"\"%s\"",key);
                const char* p=::strstr(js,kk); if(!p) return nullptr;
                p+=::strlen(kk); while(*p==' '||*p==':') ++p;
                if(*p!='"') return nullptr; ++p;
                int32 i=0; while(*p&&*p!='"'&&i<maxOut-1) out[i++]=*p++;
                out[i]=0; return p+1;
            };
            auto findFloat=[](const char* js,const char* key,float32& out)->const char*{
                char kk[64]; ::snprintf(kk,sizeof(kk),"\"%s\"",key);
                const char* p=::strstr(js,kk); if(!p) return nullptr;
                p+=::strlen(kk); while(*p==' '||*p==':') ++p;
                char* e; out=::strtof(p,&e); return e>p?e:nullptr;
            };
            auto findBool=[](const char* js,const char* key,bool& out)->const char*{
                char kk[64]; ::snprintf(kk,sizeof(kk),"\"%s\"",key);
                const char* p=::strstr(js,kk); if(!p) return nullptr;
                p+=::strlen(kk); while(*p==' '||*p==':') ++p;
                if(::strncmp(p,"true",4)==0){out=true;return p+4;}
                if(::strncmp(p,"false",5)==0){out=false;return p+5;}
                return nullptr;
            };

            // Parse les fenêtres
            const char* p=::strstr(json,"\"windows\"");
            if(p) p=::strstr(p,"[");
            while(p&&*p){
                const char* obj=::strstr(p,"{"); if(!obj) break;
                const char* end=::strstr(obj,"}"); if(!end) break;
                // Copie l'objet
                char objBuf[512]={};
                memory::NkCopy(objBuf,obj,end-obj<511?(end-obj):511);
                char name[64]={}; findStr(objBuf,"name",name,64);
                if(!name[0]){p=end+1;continue;}
                float32 x=100,y=100,w=400,h=300,sy=0;
                bool open=true,col=false,max=false;
                findFloat(objBuf,"x",x);findFloat(objBuf,"y",y);
                findFloat(objBuf,"w",w);findFloat(objBuf,"h",h);
                findFloat(objBuf,"scrollY",sy);
                findBool(objBuf,"open",open);findBool(objBuf,"collapsed",col);findBool(objBuf,"maximized",max);
                NkUIWindowState* ws=wm.FindOrCreate(name,{x,y},{w,h},NkUIWindowFlags::NK_NONE);
                if(ws){ws->pos={x,y};ws->size={w,h};ws->isOpen=open;ws->isCollapsed=col;ws->scrollY=sy;}
                p=end+1;
                if(*p==']') break;
            }
            // Le dock est plus complexe à parser — on recrée simplement la structure depuis les fenêtres
            // (parsing complet du dock JSON serait une phase bonus)
            (void)dm; // dock left as-is
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUIColorPickerFull
        // ─────────────────────────────────────────────────────────────────────────────

        NkUIColorPickerFull::State NkUIColorPickerFull::sStates[16]={};
        int32 NkUIColorPickerFull::sNumStates=0;

        NkUIColorPickerFull::State* NkUIColorPickerFull::FindOrCreate(NkUIID id) noexcept {
            for(int32 i=0;i<sNumStates;++i) if(sStates[i].id==id) return &sStates[i];
            if(sNumStates>=16) return &sStates[0];
            State& s=sStates[sNumStates++]; s.id=id; s.h=0;s.s=1;s.v=1;s.a=1;
            ::strcpy(s.hexBuf,"#FFFFFFFF"); return &s;
        }

        void NkUIColorPickerFull::RGBtoHSV(uint8 r,uint8 g,uint8 b,
                                            float32& h,float32& s,float32& v) noexcept
        {
            const float32 rf=r/255.f,gf=g/255.f,bf=b/255.f;
            const float32 mx=rf>gf?rf:gf; const float32 mx2=mx>bf?mx:bf;
            const float32 mn=rf<gf?rf:gf; const float32 mn2=mn<bf?mn:bf;
            v=mx2;
            const float32 delta=mx2-mn2;
            s=(mx2>0)?delta/mx2:0;
            if(delta<0.0001f){h=0;return;}
            if(mx2==rf)      h=(gf-bf)/delta+(gf<bf?6:0);
            else if(mx2==gf) h=(bf-rf)/delta+2;
            else             h=(rf-gf)/delta+4;
            h/=6.f;
        }

        void NkUIColorPickerFull::HSVtoRGB(float32 h,float32 s,float32 v,
                                            uint8& r,uint8& g,uint8& b) noexcept
        {
            if(s<=0){r=g=b=static_cast<uint8>(v*255);return;}
            h*=6; const int32 i=static_cast<int32>(h); const float32 f=h-i;
            const float32 p=v*(1-s),q=v*(1-s*f),t2=v*(1-s*(1-f));
            float32 rf,gf,bf;
            switch(i%6){
                case 0:rf=v;gf=t2;bf=p;break; case 1:rf=q;gf=v;bf=p;break;
                case 2:rf=p;gf=v;bf=t2;break; case 3:rf=p;gf=q;bf=v;break;
                case 4:rf=t2;gf=p;bf=v;break; default:rf=v;gf=p;bf=q;break;
            }
            r=static_cast<uint8>(rf*255); g=static_cast<uint8>(gf*255); b=static_cast<uint8>(bf*255);
        }

        void NkUIColorPickerFull::DrawSVSquare(NkUIContext& ctx,NkUIDrawList& dl,
                                                NkRect r,float32 h,float32& s,float32& v) noexcept
        {
            // Dessine le carré SV avec dégradé : blanc→couleur (X) et →noir (Y)
            const int32 steps=16;
            for(int32 si=0;si<steps;++si){
                for(int32 vi2=0;vi2<steps;++vi2){
                    const float32 sf=static_cast<float32>(si)/steps;
                    const float32 vf=1.f-static_cast<float32>(vi2)/steps;
                    uint8 cr,cg,cb; HSVtoRGB(h,sf,vf,cr,cg,cb);
                    const NkRect cell={r.x+sf*r.w,r.y+(1-vf)*r.h,r.w/steps+1,r.h/steps+1};
                    dl.AddRectFilled(cell,{cr,cg,cb,255});
                }
            }
            // Curseur
            const NkVec2 cursor={r.x+s*r.w,r.y+(1-v)*r.h};
            dl.AddCircle(cursor,6.f,NkColor::White,2.f);
            dl.AddCircle(cursor,7.f,NkColor::Black,1.f);
            // Interaction
            const NkUIID id=NkHashPtr(&s,0xAB);
            if(ctx.IsHovered(r)&&ctx.ConsumeMouseClick(0)) ctx.SetActive(id);
            if(ctx.IsActive(id)){
                s=(ctx.input.mousePos.x-r.x)/r.w;
                v=1.f-(ctx.input.mousePos.y-r.y)/r.h;
                s=s<0?0:s>1?1:s; v=v<0?0:v>1?1:v;
                if(!ctx.input.mouseDown[0]) ctx.ClearActive();
            }
        }

        void NkUIColorPickerFull::DrawHueBar(NkUIContext& ctx,NkUIDrawList& dl,
                                            NkRect r,float32& h) noexcept
        {
            const int32 steps=18;
            for(int32 i=0;i<steps;++i){
                const float32 hf=static_cast<float32>(i)/steps;
                uint8 cr,cg,cb; HSVtoRGB(hf,1,1,cr,cg,cb);
                dl.AddRectFilled({r.x,r.y+hf*r.h,r.w,r.h/steps+1},{cr,cg,cb,255});
            }
            // Curseur
            const float32 cy=r.y+h*r.h;
            dl.AddRect({r.x-1,cy-3,r.w+2,6},NkColor::White,2.f);
            // Interaction
            const NkUIID id=NkHashPtr(&h,0xCD);
            if(ctx.IsHovered(r)&&ctx.ConsumeMouseClick(0)) ctx.SetActive(id);
            if(ctx.IsActive(id)){
                h=(ctx.input.mousePos.y-r.y)/r.h;
                h=h<0?0:h>1?1:h;
                if(!ctx.input.mouseDown[0]) ctx.ClearActive();
            }
        }

        void NkUIColorPickerFull::DrawAlphaBar(NkUIContext& ctx,NkUIDrawList& dl,
                                                NkRect r,float32 h,float32 s,float32 v,
                                                float32& a) noexcept
        {
            // Damier
            const int32 cs=8;
            for(int32 y=0;y<static_cast<int32>(r.h/cs)+1;++y)
                for(int32 x=0;x<static_cast<int32>(r.w/cs)+1;++x){
                    const NkColor c=((x+y)%2)?NkColor::GrayValue(200):NkColor::GrayValue(160);
                    dl.AddRectFilled({r.x+x*cs,r.y+y*cs,static_cast<float32>(cs),static_cast<float32>(cs)},c);
                }
            // Dégradé couleur→transparent
            uint8 cr,cg,cb; HSVtoRGB(h,s,v,cr,cg,cb);
            dl.AddRectFilledMultiColor(r,{cr,cg,cb,0},{cr,cg,cb,255},{cr,cg,cb,255},{cr,cg,cb,0});
            // Curseur
            const float32 cx=r.x+a*r.w;
            dl.AddRect({cx-3,r.y-1,6,r.h+2},NkColor::White,2.f);
            const NkUIID id=NkHashPtr(&a,0xEF);
            if(ctx.IsHovered(r)&&ctx.ConsumeMouseClick(0)) ctx.SetActive(id);
            if(ctx.IsActive(id)){
                a=(ctx.input.mousePos.x-r.x)/r.w;
                a=a<0?0:a>1?1:a;
                if(!ctx.input.mouseDown[0]) ctx.ClearActive();
            }
        }

        void NkUIColorPickerFull::DrawPreview(NkUIDrawList& dl,NkRect r,
                                                NkColor oldC,NkColor newC) noexcept
        {
            dl.AddRectFilled({r.x,r.y,r.w*0.5f,r.h},oldC,4.f);
            dl.AddRectFilled({r.x+r.w*0.5f,r.y,r.w*0.5f,r.h},newC,4.f);
            dl.AddRect(r,NkColor::GrayValue(180),1.f,4.f);
        }

        bool NkUIColorPickerFull::Draw(NkUIContext& ctx,NkUIDrawList& dl,
                                        NkUIFont& font,NkUILayoutStack& ls,
                                        const char* id,NkColor& color) noexcept
        {
            const NkUIID uid=ctx.GetID(id);
            State* st=FindOrCreate(uid);
            if(!st) return false;

            // Initialise depuis la couleur si première utilisation
            if(st->h==0&&st->s==1&&st->v==1){
                RGBtoHSV(color.r,color.g,color.b,st->h,st->s,st->v);
                st->a=color.a/255.f;
            }

            const NkColor oldColor=color;
            const float32 totalW=230.f, sqW=170.f, stripW=18.f, gap=6.f;
            const float32 sqH=170.f, stripH=sqH;
            const NkRect sqR={ctx.cursor.x,ctx.cursor.y,sqW,sqH};
            const NkRect hueR={sqR.x+sqW+gap,sqR.y,stripW,stripH};
            const NkRect alphaR={sqR.x,sqR.y+sqH+gap,sqW+stripW+gap,12.f};
            const NkRect prevR={sqR.x,alphaR.y+12+gap,totalW,22.f};

            DrawSVSquare(ctx,dl,sqR,st->h,st->s,st->v);
            DrawHueBar(ctx,dl,hueR,st->h);
            DrawAlphaBar(ctx,dl,alphaR,st->h,st->s,st->v,st->a);

            // Construit la couleur courante
            uint8 cr,cg,cb; HSVtoRGB(st->h,st->s,st->v,cr,cg,cb);
            NkColor newColor={cr,cg,cb,static_cast<uint8>(st->a*255)};
            DrawPreview(dl,prevR,oldColor,newColor);

            // Sliders RGBA
            float32 nextY=prevR.y+prevR.h+gap;
            NkUILayoutNode* n=ls.Top();
            if(n) n->cursor.y=nextY;
            else ctx.cursor.y=nextY;

            const char* chNames[]={"R##cp","G##cp","B##cp","A##cp"};
            float32 chVals[4]={cr/255.f,cg/255.f,cb/255.f,st->a};
            float32 chOut[4]; bool slChanged=false;
            for(int32 i=0;i<4;++i){
                chOut[i]=chVals[i];
                NkUI::SliderFloat(ctx,ls,dl,font,chNames[i],chOut[i],0.f,1.f,"%.2f",totalW-30);
                if(chOut[i]!=chVals[i]) slChanged=true;
            }
            if(slChanged){
                const uint8 nr=static_cast<uint8>(chOut[0]*255),ng=static_cast<uint8>(chOut[1]*255),
                            nb=static_cast<uint8>(chOut[2]*255);
                RGBtoHSV(nr,ng,nb,st->h,st->s,st->v);
                st->a=chOut[3];
                newColor={nr,ng,nb,static_cast<uint8>(st->a*255)};
            }

            // Input hex
            ::snprintf(st->hexBuf,sizeof(st->hexBuf),"#%02X%02X%02X%02X",
                    newColor.r,newColor.g,newColor.b,newColor.a);
            NkUI::InputText(ctx,ls,dl,font,"##hex",st->hexBuf,sizeof(st->hexBuf),totalW);
            // Parse hex si modifié
            if(::strlen(st->hexBuf)==9&&st->hexBuf[0]=='#'){
                auto hv=[](char c)->uint8{
                    if(c>='0'&&c<='9')return c-'0';
                    if(c>='a'&&c<='f')return c-'a'+10;
                    if(c>='A'&&c<='F')return c-'A'+10; return 0;};
                newColor.r=static_cast<uint8>((hv(st->hexBuf[1])<<4)|hv(st->hexBuf[2]));
                newColor.g=static_cast<uint8>((hv(st->hexBuf[3])<<4)|hv(st->hexBuf[4]));
                newColor.b=static_cast<uint8>((hv(st->hexBuf[5])<<4)|hv(st->hexBuf[6]));
                newColor.a=static_cast<uint8>((hv(st->hexBuf[7])<<4)|hv(st->hexBuf[8]));
                RGBtoHSV(newColor.r,newColor.g,newColor.b,st->h,st->s,st->v);
                st->a=newColor.a/255.f;
            }

            const bool changed=(newColor.r!=color.r||newColor.g!=color.g||
                                newColor.b!=color.b||newColor.a!=color.a);
            if(changed) color=newColor;
            return changed;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUIFontNKFont — intégration NKFont
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIFontNKFont::Load(const uint8*,usize,float32,NkUIFontAtlas&,NkUIFont&) noexcept { return false; }
        bool NkUIFontNKFont::LoadFile(const char*,float32,NkUIFontAtlas&,NkUIFont&) noexcept { return false; }
        bool NkUIFontNKFont::AddGlyphRange(NkUIFont&,NkUIFontAtlas&,uint32,uint32) noexcept { return false; }

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUIOpenGLRenderer
        // ─────────────────────────────────────────────────────────────────────────────

        const char* NkUIOpenGLRenderer::kVertSrc = R"(
        #version 330 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec2 aUV;
        layout(location=2) in vec4 aCol;
        uniform vec2 uViewport;
        out vec2 vUV;
        out vec4 vCol;
        void main(){
            vUV=aUV; vCol=aCol;
            vec2 pos=aPos/uViewport*2.0-1.0;
            gl_Position=vec4(pos.x,-pos.y,0,1);
        })";

        const char* NkUIOpenGLRenderer::kFragSrc = R"(
        #version 330 core
        in vec2 vUV;
        in vec4 vCol;
        uniform sampler2D uTex;
        out vec4 fragColor;
        void main(){
            vec4 tc=(vUV.x>0.0||vUV.y>0.0)?texture(uTex,vUV):vec4(1.0);
            fragColor=vCol*tc;
        })";

        #ifdef NKUI_WITH_OPENGL

        uint32 NkUIOpenGLRenderer::CompileShader(uint32 type,const char* src) noexcept {
            const uint32 s=glCreateShader(type);
            glShaderSource(s,1,&src,nullptr);
            glCompileShader(s);
            int32 ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
            if(!ok){glDeleteShader(s);return 0;}
            return s;
        }

        uint32 NkUIOpenGLRenderer::LinkProgram(uint32 vert,uint32 frag) noexcept {
            const uint32 p=glCreateProgram();
            glAttachShader(p,vert); glAttachShader(p,frag);
            glLinkProgram(p);
            int32 ok=0; glGetProgramiv(p,GL_LINK_STATUS,&ok);
            if(!ok){glDeleteProgram(p);return 0;}
            return p;
        }

        bool NkUIOpenGLRenderer::Init(int32 w,int32 h) noexcept {
            mW=w; mH=h;
            const uint32 v=CompileShader(GL_VERTEX_SHADER,kVertSrc);
            const uint32 f=CompileShader(GL_FRAGMENT_SHADER,kFragSrc);
            if(!v||!f) return false;
            mShader=LinkProgram(v,f);
            glDeleteShader(v); glDeleteShader(f);
            if(!mShader) return false;

            glGenVertexArrays(1,&mVAO);
            glGenBuffers(1,&mVBO);
            glGenBuffers(1,&mEBO);
            glBindVertexArray(mVAO);
            glBindBuffer(GL_ARRAY_BUFFER,mVBO);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(NkUIVertex),(void*)offsetof(NkUIVertex,pos));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(NkUIVertex),(void*)offsetof(NkUIVertex,uv));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(NkUIVertex),(void*)offsetof(NkUIVertex,col));
            glBindVertexArray(0);

            // Texture blanche 1x1 par défaut
            glGenTextures(1,&mFontTex);
            glBindTexture(GL_TEXTURE_2D,mFontTex);
            const uint8 white[4]={255,255,255,255};
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,white);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

            mInitialized=true;
            return true;
        }

        void NkUIOpenGLRenderer::Destroy() noexcept {
            if(mVAO){glDeleteVertexArrays(1,&mVAO);mVAO=0;}
            if(mVBO){glDeleteBuffers(1,&mVBO);mVBO=0;}
            if(mEBO){glDeleteBuffers(1,&mEBO);mEBO=0;}
            if(mShader){glDeleteProgram(mShader);mShader=0;}
            if(mFontTex){glDeleteTextures(1,&mFontTex);mFontTex=0;}
            mInitialized=false;
        }

        void NkUIOpenGLRenderer::BeginFrame(int32 w,int32 h) noexcept {
            mW=w; mH=h;
            glViewport(0,0,w,h);
            glClearColor(0,0,0,0);
            glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_SCISSOR_TEST);
        }

        void NkUIOpenGLRenderer::SubmitDrawList(const NkUIDrawList& dl,int32 fbW,int32 fbH) noexcept {
            if(!dl.vtx||!dl.idx||dl.cmdCount==0) return;
            glBindVertexArray(mVAO);
            glBindBuffer(GL_ARRAY_BUFFER,mVBO);
            glBufferData(GL_ARRAY_BUFFER,dl.vtxCount*sizeof(NkUIVertex),dl.vtx,GL_STREAM_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,dl.idxCount*sizeof(uint32),dl.idx,GL_STREAM_DRAW);

            glUseProgram(mShader);
            glUniform2f(glGetUniformLocation(mShader,"uViewport"),
                        static_cast<float32>(fbW),static_cast<float32>(fbH));
            glUniform1i(glGetUniformLocation(mShader,"uTex"),0);
            glActiveTexture(GL_TEXTURE0);

            for(uint32 ci=0;ci<dl.cmdCount;++ci){
                const NkUIDrawCmd& cmd=dl.cmds[ci];
                if(cmd.type==NkUIDrawCmdType::NK_CLIP_RECT){
                    const NkRect& cr=cmd.clipRect;
                    glScissor(static_cast<int32>(cr.x),
                            fbH-static_cast<int32>(cr.y+cr.h),
                            static_cast<int32>(cr.w),
                            static_cast<int32>(cr.h));
                    continue;
                }
                glBindTexture(GL_TEXTURE_2D,cmd.texId>0?cmd.texId:mFontTex);
                glDrawElementsBaseVertex(GL_TRIANGLES,cmd.idxCount,GL_UNSIGNED_INT,
                                        reinterpret_cast<void*>(cmd.idxOffset*sizeof(uint32)),0);
            }
            glBindVertexArray(0);
        }

        void NkUIOpenGLRenderer::Submit(const NkUIContext& ctx) noexcept {
            for(int32 i=0;i<NkUIContext::LAYER_COUNT;++i)
                SubmitDrawList(ctx.layers[i],mW,mH);
        }

        void NkUIOpenGLRenderer::EndFrame() noexcept {
            glDisable(GL_SCISSOR_TEST);
        }

        uint32 NkUIOpenGLRenderer::UploadTexture(const uint8* pix,int32 w,int32 h,int32 ch) noexcept {
            uint32 t=0; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
            const uint32 fmt=ch==1?GL_RED:(ch==3?GL_RGB:GL_RGBA);
            glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,pix);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            return t;
        }
        void NkUIOpenGLRenderer::FreeTexture(uint32 id) noexcept { glDeleteTextures(1,&id); }

        #else
        // Stubs quand OpenGL n'est pas disponible
        bool  NkUIOpenGLRenderer::Init(int32,int32)noexcept{return false;}
        void  NkUIOpenGLRenderer::Destroy()noexcept{}
        void  NkUIOpenGLRenderer::BeginFrame(int32,int32)noexcept{}
        void  NkUIOpenGLRenderer::Submit(const NkUIContext&)noexcept{}
        void  NkUIOpenGLRenderer::EndFrame()noexcept{}
        uint32 NkUIOpenGLRenderer::UploadTexture(const uint8*,int32,int32,int32)noexcept{return 0;}
        void  NkUIOpenGLRenderer::FreeTexture(uint32)noexcept{}
        void  NkUIOpenGLRenderer::SubmitDrawList(const NkUIDrawList&,int32,int32)noexcept{}
        uint32 NkUIOpenGLRenderer::CompileShader(uint32,const char*)noexcept{return 0;}
        uint32 NkUIOpenGLRenderer::LinkProgram(uint32,uint32)noexcept{return 0;}
        #endif
    }
} // namespace nkentseu
