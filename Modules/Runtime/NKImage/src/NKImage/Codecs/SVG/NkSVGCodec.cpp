/**
 * @File    NkSVGCodec.cpp
 * @Brief   Parser SVG + rasteriseur.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "NKImage/Codecs/SVG/NkSVGCodec.h"
#include "NKImage/Codecs/PNG/NkPNGCodec.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKContainers/String/NkStringView.h"
#include "NKMath/NkFunctions.h"
namespace nkentseu {

using namespace nkentseu::memory;
using namespace nkentseu::math;

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGColor::Parse
// ─────────────────────────────────────────────────────────────────────────────

static const struct { const char* name; uint32 rgb; } kColorNames[] = {
    {"none",0},{"transparent",0},{"black",0x000000},{"white",0xFFFFFF},
    {"red",0xFF0000},{"green",0x008000},{"blue",0x0000FF},{"yellow",0xFFFF00},
    {"cyan",0x00FFFF},{"magenta",0xFF00FF},{"orange",0xFFA500},{"pink",0xFFC0CB},
    {"purple",0x800080},{"brown",0xA52A2A},{"gray",0x808080},{"grey",0x808080},
    {"silver",0xC0C0C0},{"lime",0x00FF00},{"maroon",0x800000},{"navy",0x000080},
    {"olive",0x808000},{"teal",0x008080},{"aqua",0x00FFFF},{"fuchsia",0xFF00FF},
    {"coral",0xFF7F50},{"salmon",0xFA8072},{"gold",0xFFD700},{"khaki",0xF0E68C},
    {"indigo",0x4B0082},{"violet",0xEE82EE},{"crimson",0xDC143C},{"turquoise",0x40E0D0},
    {"beige",0xF5F5DC},{"ivory",0xFFFFF0},{"lavender",0xE6E6FA},{"tan",0xD2B48C},
    {nullptr,0}
};

NkSVGColor NkSVGColor::Parse(const char* str) noexcept {
    if(!str||!*str) return None();
    while(*str==' ') ++str;
    if(NkStringView(str)=="none"||NkStringView(str)=="transparent") return None();
    // Cherche dans les noms
    for(int32 i=0;kColorNames[i].name;++i){
        if(NkStringView(str)==NkStringView(kColorNames[i].name)){
            if(kColorNames[i].rgb==0&&(str[0]!='b')) return None();
            return {static_cast<uint8>(kColorNames[i].rgb>>16),
                    static_cast<uint8>(kColorNames[i].rgb>>8),
                    static_cast<uint8>(kColorNames[i].rgb),255,false};
        }
    }
    // #RRGGBB ou #RGB
    if(str[0]=='#'){
        ++str;
        auto hex=[](char c)->uint8{
            if(c>='0'&&c<='9')return static_cast<uint8>(c-'0');
            if(c>='a'&&c<='f')return static_cast<uint8>(c-'a'+10);
            if(c>='A'&&c<='F')return static_cast<uint8>(c-'A'+10);
            return 0;};
        const usize len=NkStringView(str).Length();
        if(len>=6){
            return {static_cast<uint8>((hex(str[0])<<4)|hex(str[1])),
                    static_cast<uint8>((hex(str[2])<<4)|hex(str[3])),
                    static_cast<uint8>((hex(str[4])<<4)|hex(str[5])),255,false};
        } else if(len>=3){
            return {static_cast<uint8>((hex(str[0])<<4)|hex(str[0])),
                    static_cast<uint8>((hex(str[1])<<4)|hex(str[1])),
                    static_cast<uint8>((hex(str[2])<<4)|hex(str[2])),255,false};
        }
    }
    // rgb(r,g,b) ou rgba(r,g,b,a)
    if(::strncmp(str,"rgb",3)==0){
        int32 r=0,g=0,b=0; float32 a=1.f;
        ::sscanf(str+4,"%d,%d,%d,%f",&r,&g,&b,&a);
        return {static_cast<uint8>(r),static_cast<uint8>(g),static_cast<uint8>(b),
                static_cast<uint8>(a*255),false};
    }
    return Black();
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGTransform
// ─────────────────────────────────────────────────────────────────────────────

NkSVGTransform NkSVGTransform::Rotate(float32 deg) noexcept {
    const float32 r=deg*math::constants::kPiF/180.f;
    const float32 c=NkCos(r),s=NkSin(r);
    return {c,s,-s,c,0,0};
}

NkSVGTransform NkSVGTransform::operator*(const NkSVGTransform& o) const noexcept {
    return {a*o.a+c*o.b, b*o.a+d*o.b,
            a*o.c+c*o.d, b*o.c+d*o.d,
            a*o.e+c*o.f+e, b*o.e+d*o.f+f};
}

NkSVGTransform NkSVGTransform::Parse(const char* str) noexcept {
    NkSVGTransform t=Identity();
    if(!str||!*str) return t;
    const char* p=str;
    while(*p){
        while(*p==' '||*p==',') ++p;
        if(::strncmp(p,"matrix(",7)==0){
            float32 v[6]={1,0,0,1,0,0};
            ::sscanf(p+7,"%f,%f,%f,%f,%f,%f",v,v+1,v+2,v+3,v+4,v+5);
            t=t*NkSVGTransform{v[0],v[1],v[2],v[3],v[4],v[5]};
        } else if(::strncmp(p,"translate(",10)==0){
            float32 tx=0,ty=0; ::sscanf(p+10,"%f,%f",&tx,&ty);
            t=t*Translate(tx,ty);
        } else if(::strncmp(p,"scale(",6)==0){
            float32 sx=1,sy=1; ::sscanf(p+6,"%f,%f",&sx,&sy);
            if(sy==1&&sx!=1) sy=sx;
            t=t*Scale(sx,sy);
        } else if(::strncmp(p,"rotate(",7)==0){
            float32 angle=0,cx=0,cy=0; ::sscanf(p+7,"%f,%f,%f",&angle,&cx,&cy);
            if(cx!=0||cy!=0) t=t*Translate(cx,cy)*Rotate(angle)*Translate(-cx,-cy);
            else t=t*Rotate(angle);
        }
        // Avance jusqu'au prochain ')'
        while(*p&&*p!=')') ++p;
        if(*p==')') ++p;
    }
    return t;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Parser XML minimal
// ─────────────────────────────────────────────────────────────────────────────

const char* NkSVGCodec::SkipWS(const char* p) noexcept {
    while(*p==' '||*p=='\t'||*p=='\n'||*p=='\r') ++p; return p;
}

float32 NkSVGCodec::ParseFloat(const char* s, const char** end) noexcept {
    if(!s) return 0.f;
    while(*s==' ') ++s;
    char* e; float32 v=::strtof(s,&e);
    // Ignore % (traité comme valeur brute)
    if(*e=='%'){++e;v/=100.f;}
    if(end) *end=e;
    return v;
}

float32 NkSVGCodec::GetAttr(const Attr* attrs, int32 n, const char* name, float32 defVal) noexcept {
    for(int32 i=0;i<n;++i)
        if(NkStringView(attrs[i].name)==NkStringView(name)) return ParseFloat(attrs[i].value);
    return defVal;
}

const char* NkSVGCodec::GetAttrStr(const Attr* attrs, int32 n, const char* name) noexcept {
    for(int32 i=0;i<n;++i)
        if(NkStringView(attrs[i].name)==NkStringView(name)) return attrs[i].value;
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Style parser
// ─────────────────────────────────────────────────────────────────────────────

void NkSVGCodec::ParseInlineStyle(const char* css, NkSVGStyle& out) noexcept {
    if(!css) return;
    char prop[64],val[128];
    const char* p=css;
    while(*p){
        p=SkipWS(p);
        // Lit nom de propriété
        int32 pi=0;
        while(*p&&*p!=':'&&*p!=';') prop[pi++]=*p++;
        prop[pi]=0; if(*p==':') ++p;
        p=SkipWS(p);
        // Lit valeur
        int32 vi=0;
        while(*p&&*p!=';') val[vi++]=*p++;
        val[vi]=0; if(*p==';') ++p;
        // Trim
        while(vi>0&&(val[vi-1]==' '||val[vi-1]=='\t')) val[--vi]=0;
        // Applique
        if(NkStringView(prop)=="fill")         out.fill=NkSVGColor::Parse(val);
        else if(NkStringView(prop)=="stroke")  out.stroke=NkSVGColor::Parse(val);
        else if(NkStringView(prop)=="stroke-width") out.strokeWidth=ParseFloat(val);
        else if(NkStringView(prop)=="opacity")      out.opacity=ParseFloat(val);
        else if(NkStringView(prop)=="fill-opacity") out.fillOpacity=ParseFloat(val);
        else if(NkStringView(prop)=="stroke-opacity") out.strokeOpacity=ParseFloat(val);
        else if(NkStringView(prop)=="fill-rule")
            out.fillEvenOdd=(::strncmp(val,"evenodd",7)==0);
        else if(NkStringView(prop)=="display"||NkStringView(prop)=="visibility")
            out.visible=(NkStringView(val)!="none"&&NkStringView(val)!="hidden");
        else if(NkStringView(prop)=="stroke-linecap"){
            if(NkStringView(val)=="round")  out.linecap=1;
            else if(NkStringView(val)=="square") out.linecap=2;
            else out.linecap=0;
        }
    }
}

NkSVGStyle NkSVGCodec::ParseStyle(const Attr* attrs, int32 n, const NkSVGStyle& parent) noexcept {
    NkSVGStyle s=parent;
    // Attributs directs
    const char* fill=GetAttrStr(attrs,n,"fill");
    if(fill) s.fill=NkSVGColor::Parse(fill);
    const char* stroke=GetAttrStr(attrs,n,"stroke");
    if(stroke) s.stroke=NkSVGColor::Parse(stroke);
    const char* sw=GetAttrStr(attrs,n,"stroke-width");
    if(sw) s.strokeWidth=ParseFloat(sw);
    const char* op=GetAttrStr(attrs,n,"opacity");
    if(op) s.opacity=ParseFloat(op);
    const char* fo=GetAttrStr(attrs,n,"fill-opacity");
    if(fo) s.fillOpacity=ParseFloat(fo);
    const char* fr=GetAttrStr(attrs,n,"fill-rule");
    if(fr) s.fillEvenOdd=(::strncmp(fr,"evenodd",7)==0);
    const char* style=GetAttrStr(attrs,n,"style");
    if(style) ParseInlineStyle(style,s);
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Primitives de dessin
// ─────────────────────────────────────────────────────────────────────────────

void NkSVGCodec::BlendPixel(NkImage& img, int32 x, int32 y,
                              NkSVGColor col, float32 alpha) noexcept
{
    if(x<0||y<0||x>=img.Width()||y>=img.Height()) return;
    const float32 a=static_cast<float32>(col.a)/255.f*alpha;
    uint8* p=img.RowPtr(y)+x*4;
    p[0]=static_cast<uint8>(p[0]*(1-a)+col.r*a);
    p[1]=static_cast<uint8>(p[1]*(1-a)+col.g*a);
    p[2]=static_cast<uint8>(p[2]*(1-a)+col.b*a);
    p[3]=static_cast<uint8>(p[3]+(255-p[3])*a);
}

// Algorithme de Wu pour lignes antialiasées
void NkSVGCodec::DrawAALine(NkImage& img, float32 x0, float32 y0,
                              float32 x1, float32 y1,
                              NkSVGColor col, float32 width) noexcept
{
    // Pour les lignes épaisses, on dessine plusieurs lignes parallèles
    const int32 passes=static_cast<int32>(width+0.5f);
    const float32 dx=x1-x0, dy=y1-y0;
    const float32 len=NkSqrt(dx*dx+dy*dy);
    if(len<0.001f){BlendPixel(img,static_cast<int32>(x0),static_cast<int32>(y0),col,1.f);return;}
    const float32 nx=-dy/len*0.5f, ny=dx/len*0.5f;

    for(int32 p=0;p<=passes;++p){
        const float32 off=static_cast<float32>(p)-width*0.5f;
        const float32 ax=x0+nx*off*2, ay=y0+ny*off*2;
        const float32 bx=x1+nx*off*2, by=y1+ny*off*2;
        // Bresenham avec antialiasing Wu
        float32 sx=ax,sy=ay,ex=bx,ey=by;
        const float32 ddx=ex-sx,ddy=ey-sy;
        const bool steep=NkFabs(ddy)>NkFabs(ddx);
        if(steep){float32 t=sx;sx=sy;sy=t;t=ex;ex=ey;ey=t;}
        if(sx>ex){float32 t=sx;sx=ex;ex=t;t=sy;sy=ey;ey=t;}
        const float32 grad=ddx!=0?(steep?ddx/ddy:ddy/ddx):1.f;
        const int32 x0i=static_cast<int32>(sx),x1i=static_cast<int32>(ex);
        float32 iy=sy;
        for(int32 xi=x0i;xi<=x1i;++xi,iy+=grad){
            const int32 yi=static_cast<int32>(iy);
            const float32 frac=iy-yi;
            if(steep){BlendPixel(img,yi,xi,col,1.f-frac);BlendPixel(img,yi+1,xi,col,frac);}
            else      {BlendPixel(img,xi,yi,col,1.f-frac);BlendPixel(img,xi,yi+1,col,frac);}
        }
    }
}

void NkSVGCodec::FillPolygon(NkImage& img, const float32* xs, const float32* ys,
                               int32 n, NkSVGColor col, bool evenOdd) noexcept
{
    if(n<3||col.none) return;
    // Bounding box
    float32 minY=ys[0],maxY=ys[0];
    for(int32 i=1;i<n;++i){if(ys[i]<minY)minY=ys[i];if(ys[i]>maxY)maxY=ys[i];}
    const int32 y0=static_cast<int32>(minY),y1=static_cast<int32>(maxY)+1;
    const int32 imgH=img.Height(), imgW=img.Width();

    for(int32 y=y0;y<=y1&&y<imgH;++y){
        if(y<0) continue;
        const float32 fy=static_cast<float32>(y)+0.5f;
        // Calcule les intersections
        float32 xs2[512]; int32 cnt=0;
        for(int32 i=0;i<n&&cnt<511;++i){
            const int32 j=(i+1)%n;
            if((ys[i]<=fy&&ys[j]>fy)||(ys[j]<=fy&&ys[i]>fy)){
                const float32 t=(fy-ys[i])/(ys[j]-ys[i]);
                xs2[cnt++]=xs[i]+t*(xs[j]-xs[i]);
            }
        }
        // Tri insertion
        for(int32 i=1;i<cnt;++i){float32 k=xs2[i];int32 j=i-1;while(j>=0&&xs2[j]>k){xs2[j+1]=xs2[j];--j;}xs2[j+1]=k;}
        // Remplit les spans
        for(int32 i=0;i+1<cnt;i+=2){
            if(evenOdd&&(i/2)%2==1) continue;
            const int32 xa=static_cast<int32>(xs2[i]+0.5f),xb=static_cast<int32>(xs2[i+1]+0.5f);
            for(int32 x=xa;x<xb&&x<imgW;++x) if(x>=0) BlendPixel(img,x,y,col,1.f);
        }
    }
}

void NkSVGCodec::StrokePolyline(NkImage& img, const float32* xs, const float32* ys,
                                  int32 n, NkSVGColor col, float32 width,
                                  bool closed, uint8 cap, uint8 join) noexcept
{
    if(n<2||col.none) return;
    (void)cap; (void)join;
    for(int32 i=0;i<n-1;++i)
        DrawAALine(img,xs[i],ys[i],xs[i+1],ys[i+1],col,width);
    if(closed&&n>2)
        DrawAALine(img,xs[n-1],ys[n-1],xs[0],ys[0],col,width);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Path parser
// ─────────────────────────────────────────────────────────────────────────────

void NkSVGCodec::BezierFlatten(float32 x0,float32 y0,float32 x1,float32 y1,
                                 float32 x2,float32 y2,float32 x3,float32 y3,
                                 float32* xs,float32* ys,int32& n,int32 maxN,int32 depth) noexcept
{
    // Test d'aplatissement : déviation maximale des points de contrôle
    const float32 dx1=x1-x0,dy1=y1-y0,dx2=x2-x3,dy2=y2-y3;
    const float32 d=NkSqrt(dx1*dx1+dy1*dy1)+NkSqrt(dx2*dx2+dy2*dy2);
    if(d<0.5f||depth>8){
        if(n<maxN){xs[n]=x3;ys[n]=y3;++n;} return;
    }
    // Subdivision de Casteljau
    const float32 m01x=(x0+x1)*.5f,m01y=(y0+y1)*.5f;
    const float32 m12x=(x1+x2)*.5f,m12y=(y1+y2)*.5f;
    const float32 m23x=(x2+x3)*.5f,m23y=(y2+y3)*.5f;
    const float32 m012x=(m01x+m12x)*.5f,m012y=(m01y+m12y)*.5f;
    const float32 m123x=(m12x+m23x)*.5f,m123y=(m12y+m23y)*.5f;
    const float32 midx=(m012x+m123x)*.5f,midy=(m012y+m123y)*.5f;
    BezierFlatten(x0,y0,m01x,m01y,m012x,m012y,midx,midy,xs,ys,n,maxN,depth+1);
    BezierFlatten(midx,midy,m123x,m123y,m23x,m23y,x3,y3,xs,ys,n,maxN,depth+1);
}

void NkSVGCodec::ArcToLines(float32 x1,float32 y1,float32 rx,float32 ry,
                              float32 xAngle,bool largeArc,bool sweep,
                              float32 x2,float32 y2,
                              float32* xs,float32* ys,int32& n,int32 maxN) noexcept
{
    // Conversion arc → Bézier (W3C SVG spec endpoint parameterization)
    if(rx<=0||ry<=0){if(n<maxN){xs[n]=x2;ys[n]=y2;++n;}return;}
    const float32 ca=NkCos(xAngle),sa=NkSin(xAngle);
    const float32 dx=(x1-x2)*.5f,dy=(y1-y2)*.5f;
    const float32 x1p= ca*dx+sa*dy, y1p=-sa*dx+ca*dy;
    float32 x1p2=x1p*x1p,y1p2=y1p*y1p,rx2=rx*rx,ry2=ry*ry;
    // Ajuste rx,ry si nécessaire
    const float32 lambda=x1p2/rx2+y1p2/ry2;
    if(lambda>1){const float32 sl=NkSqrt(lambda);rx*=sl;ry*=sl;rx2=rx*rx;ry2=ry*ry;}
    float32 sq=(rx2*ry2-rx2*y1p2-ry2*x1p2)/(rx2*y1p2+ry2*x1p2);
    sq=sq<0?0:NkSqrt(sq);
    if(largeArc==sweep) sq=-sq;
    const float32 cxp= sq*rx*y1p/ry, cyp=-sq*ry*x1p/rx;
    const float32 cx=ca*cxp-sa*cyp+(x1+x2)*.5f;
    const float32 cy=sa*cxp+ca*cyp+(y1+y2)*.5f;
    // Calcule les angles de départ et d'arc
    auto angle=[](float32 ux,float32 uy,float32 vx,float32 vy)->float32{
        const float32 n2=NkSqrt(ux*ux+uy*uy)*NkSqrt(vx*vx+vy*vy);
        if(n2==0) return 0;
        float32 a=NkAcos((ux*vx+uy*vy)/n2);
        if(ux*vy-uy*vx<0) a=-a;
        return a;
    };
    float32 theta1=angle(1,0,(x1p-cxp)/rx,(y1p-cyp)/ry);
    float32 dTheta=angle((x1p-cxp)/rx,(y1p-cyp)/ry,(-x1p-cxp)/rx,(-y1p-cyp)/ry);
    if(!sweep&&dTheta>0) dTheta-=2*3.14159265f;
    if( sweep&&dTheta<0) dTheta+=2*3.14159265f;
    // Génère des points sur l'arc
    const int32 steps=static_cast<int32>(NkFabs(dTheta)*::fmaxf(rx,ry)/2.f)+4;
    for(int32 i=1;i<=steps&&n<maxN;++i){
        const float32 t=static_cast<float32>(i)/steps;
        const float32 a=theta1+t*dTheta;
        xs[n]=cx+rx*NkCos(a)*ca-ry*NkSin(a)*sa;
        ys[n]=cy+rx*NkCos(a)*sa+ry*NkSin(a)*ca;
        ++n;
    }
}

const char* NkSVGCodec::ParsePathData(
    const char* d,
    float32* xs, float32* ys,
    int32* contourStarts, int32* contourLens,
    int32& numPts, int32& numContours,
    int32 maxPts, int32 maxContours) noexcept
{
    if(!d) return d;
    float32 cx=0,cy=0,sx=0,sy=0; // current, subpath start
    float32 cpx=0,cpy=0; // control point (pour S/T)
    char cmd=' ';
    int32 contourStart=0;
    numPts=0; numContours=0;

    auto addPt=[&](float32 x,float32 y){if(numPts<maxPts){xs[numPts]=x;ys[numPts]=y;++numPts;}};
    auto closePath=[&](){
        if(numContours<maxContours){
            contourStarts[numContours]=contourStart;
            contourLens[numContours]=numPts-contourStart;
            ++numContours;
        }
        contourStart=numPts;
    };

    const char* p=d;
    while(*p){
        p=SkipWS(p);
        if(!*p) break;
        // Commande ou continuation
        if((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')){cmd=*p++;p=SkipWS(p);}
        const bool rel=(cmd>='a'&&cmd<='z');
        const char ucmd=rel?static_cast<char>(cmd-32):cmd;

        if(ucmd=='M'){
            const float32 x=ParseFloat(p,&p); p=SkipWS(p); if(*p==',')++p;
            const float32 y=ParseFloat(p,&p);
            cx=rel?cx+x:x; cy=rel?cy+y:y;
            if(numPts>contourStart) closePath();
            sx=cx;sy=cy;
            addPt(cx,cy); cpx=cx;cpy=cy;
            cmd=rel?'l':'L'; // suite implicite
        } else if(ucmd=='L'){
            const float32 x=ParseFloat(p,&p); p=SkipWS(p); if(*p==',')++p;
            const float32 y=ParseFloat(p,&p);
            cx=rel?cx+x:x; cy=rel?cy+y:y; addPt(cx,cy); cpx=cx;cpy=cy;
        } else if(ucmd=='H'){
            const float32 x=ParseFloat(p,&p); cx=rel?cx+x:x; addPt(cx,cy);
        } else if(ucmd=='V'){
            const float32 y=ParseFloat(p,&p); cy=rel?cy+y:y; addPt(cx,cy);
        } else if(ucmd=='C'){
            float32 x1=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 y1=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 x2=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 y2=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 ex=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 ey=ParseFloat(p,&p);
            if(rel){x1+=cx;y1+=cy;x2+=cx;y2+=cy;ex+=cx;ey+=cy;}
            BezierFlatten(cx,cy,x1,y1,x2,y2,ex,ey,xs,ys,numPts,maxPts);
            cpx=x2;cpy=y2; cx=ex;cy=ey;
        } else if(ucmd=='S'){
            float32 x2=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 y2=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 ex=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 ey=ParseFloat(p,&p);
            if(rel){x2+=cx;y2+=cy;ex+=cx;ey+=cy;}
            const float32 x1=2*cx-cpx,y1=2*cy-cpy;
            BezierFlatten(cx,cy,x1,y1,x2,y2,ex,ey,xs,ys,numPts,maxPts);
            cpx=x2;cpy=y2; cx=ex;cy=ey;
        } else if(ucmd=='Q'){
            float32 x1=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 y1=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 ex=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 ey=ParseFloat(p,&p);
            if(rel){x1+=cx;y1+=cy;ex+=cx;ey+=cy;}
            // Q → C conversion : 2/3 interpolation
            const float32 cx1=cx+(x1-cx)*2/3,cy1=cy+(y1-cy)*2/3;
            const float32 cx2=ex+(x1-ex)*2/3,cy2=ey+(y1-ey)*2/3;
            BezierFlatten(cx,cy,cx1,cy1,cx2,cy2,ex,ey,xs,ys,numPts,maxPts);
            cpx=x1;cpy=y1; cx=ex;cy=ey;
        } else if(ucmd=='A'){
            float32 rx=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 ry=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 xa=ParseFloat(p,&p)*3.14159265f/180.f; p=SkipWS(p);if(*p==',')++p;
            const bool la=ParseFloat(p,&p)!=0; p=SkipWS(p);if(*p==',')++p;
            const bool sw=ParseFloat(p,&p)!=0; p=SkipWS(p);if(*p==',')++p;
            float32 ex=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
            float32 ey=ParseFloat(p,&p);
            if(rel){ex+=cx;ey+=cy;}
            ArcToLines(cx,cy,rx,ry,xa,la,sw,ex,ey,xs,ys,numPts,maxPts);
            cx=ex;cy=ey; cpx=cx;cpy=cy;
        } else if(ucmd=='Z'||ucmd=='z'){
            addPt(sx,sy); closePath(); cx=sx;cy=sy;
            cmd=' '; ++p; continue;
        } else { ++p; continue; }
        p=SkipWS(p); if(*p==',') ++p;
    }
    if(numPts>contourStart) closePath();
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Rendu des éléments
// ─────────────────────────────────────────────────────────────────────────────

static void ApplyStyleAlpha(NkSVGColor& c, float32 base, float32 extra) noexcept {
    c.a=static_cast<uint8>(c.a*base*extra);
}

void NkSVGCodec::RenderRect(RenderCtx& ctx, const Attr* a, int32 n) noexcept {
    float32 x=GetAttr(a,n,"x"),y=GetAttr(a,n,"y");
    float32 w=GetAttr(a,n,"width"),h=GetAttr(a,n,"height");
    float32 rx=GetAttr(a,n,"rx"),ry=GetAttr(a,n,"ry");
    if(w<=0||h<=0) return;
    ctx.ctm.Apply(x,y);
    // Simplifié : rectangle sans coins arrondis
    // (rx/ry ignorés pour la version de base)
    (void)rx;(void)ry;
    const float32 pxs[4]={x,x+w,x+w,x}, pys[4]={y,y,y+h,y+h};
    NkSVGStyle s=ctx.style;
    NkSVGColor fill=s.fill; ApplyStyleAlpha(fill,s.opacity,s.fillOpacity);
    NkSVGColor stroke=s.stroke; ApplyStyleAlpha(stroke,s.opacity,s.strokeOpacity);
    if(!fill.none) FillPolygon(*ctx.img,pxs,pys,4,fill,s.fillEvenOdd);
    if(!stroke.none) StrokePolyline(*ctx.img,pxs,pys,4,stroke,s.strokeWidth,true,s.linecap,s.linejoin);
}

void NkSVGCodec::RenderCircle(RenderCtx& ctx, const Attr* a, int32 n) noexcept {
    float32 cx=GetAttr(a,n,"cx"),cy=GetAttr(a,n,"cy"),r=GetAttr(a,n,"r");
    if(r<=0) return;
    ctx.ctm.Apply(cx,cy);
    const int32 steps=static_cast<int32>(2*math::constants::kPiF*r/2.f)+8;
    float32* xs=static_cast<float32*>(NkAlloc(sizeof(float32)*steps));
    float32* ys=static_cast<float32*>(NkAlloc(sizeof(float32)*steps));
    if(!xs||!ys){NkFree(xs);NkFree(ys);return;}
    for(int32 i=0;i<steps;++i){
        const float32 t=2*math::constants::kPiF*i/steps;
        xs[i]=cx+r*NkCos(t); ys[i]=cy+r*NkSin(t);
    }
    NkSVGStyle s=ctx.style;
    NkSVGColor fill=s.fill; ApplyStyleAlpha(fill,s.opacity,s.fillOpacity);
    NkSVGColor stroke=s.stroke; ApplyStyleAlpha(stroke,s.opacity,s.strokeOpacity);
    if(!fill.none) FillPolygon(*ctx.img,xs,ys,steps,fill,s.fillEvenOdd);
    if(!stroke.none) StrokePolyline(*ctx.img,xs,ys,steps,stroke,s.strokeWidth,true,s.linecap,s.linejoin);
    NkFree(xs);NkFree(ys);
}

void NkSVGCodec::RenderEllipse(RenderCtx& ctx, const Attr* a, int32 n) noexcept {
    float32 cx=GetAttr(a,n,"cx"),cy=GetAttr(a,n,"cy");
    float32 rx=GetAttr(a,n,"rx"),ry=GetAttr(a,n,"ry");
    if(rx<=0||ry<=0) return;
    ctx.ctm.Apply(cx,cy);
    const int32 steps=static_cast<int32>(2*math::constants::kPiF*::fmaxf(rx,ry)/2.f)+8;
    float32* xs=static_cast<float32*>(NkAlloc(sizeof(float32)*steps));
    float32* ys=static_cast<float32*>(NkAlloc(sizeof(float32)*steps));
    if(!xs||!ys){NkFree(xs);NkFree(ys);return;}
    for(int32 i=0;i<steps;++i){
        const float32 t=2*math::constants::kPiF*i/steps;
        xs[i]=cx+rx*NkCos(t); ys[i]=cy+ry*NkSin(t);
    }
    NkSVGStyle s=ctx.style;
    NkSVGColor fill=s.fill; ApplyStyleAlpha(fill,s.opacity,s.fillOpacity);
    NkSVGColor stroke=s.stroke; ApplyStyleAlpha(stroke,s.opacity,s.strokeOpacity);
    if(!fill.none) FillPolygon(*ctx.img,xs,ys,steps,fill,s.fillEvenOdd);
    if(!stroke.none) StrokePolyline(*ctx.img,xs,ys,steps,stroke,s.strokeWidth,true,s.linecap,s.linejoin);
    NkFree(xs);NkFree(ys);
}

void NkSVGCodec::RenderLine(RenderCtx& ctx, const Attr* a, int32 n) noexcept {
    float32 x1=GetAttr(a,n,"x1"),y1=GetAttr(a,n,"y1");
    float32 x2=GetAttr(a,n,"x2"),y2=GetAttr(a,n,"y2");
    ctx.ctm.Apply(x1,y1); ctx.ctm.Apply(x2,y2);
    NkSVGColor stroke=ctx.style.stroke;
    if(stroke.none) return;
    ApplyStyleAlpha(stroke,ctx.style.opacity,ctx.style.strokeOpacity);
    DrawAALine(*ctx.img,x1,y1,x2,y2,stroke,ctx.style.strokeWidth);
}

void NkSVGCodec::RenderPolyline(RenderCtx& ctx, const Attr* a, int32 n, bool close) noexcept {
    const char* pts=GetAttrStr(a,n,"points");
    if(!pts) return;
    float32 xs[1024],ys[1024]; int32 cnt=0;
    const char* p=pts;
    while(*p&&cnt<1023){
        xs[cnt]=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
        ys[cnt]=ParseFloat(p,&p); p=SkipWS(p);if(*p==',')++p;
        ctx.ctm.Apply(xs[cnt],ys[cnt]); ++cnt;
    }
    NkSVGStyle s=ctx.style;
    NkSVGColor fill=s.fill; ApplyStyleAlpha(fill,s.opacity,s.fillOpacity);
    NkSVGColor stroke=s.stroke; ApplyStyleAlpha(stroke,s.opacity,s.strokeOpacity);
    if(close&&!fill.none) FillPolygon(*ctx.img,xs,ys,cnt,fill,s.fillEvenOdd);
    if(!stroke.none) StrokePolyline(*ctx.img,xs,ys,cnt,stroke,s.strokeWidth,close,s.linecap,s.linejoin);
}

void NkSVGCodec::RenderPath(RenderCtx& ctx, const Attr* a, int32 n) noexcept {
    const char* d=GetAttrStr(a,n,"d");
    if(!d) return;
    static const int32 MAX_PTS=8192,MAX_CNT=256;
    float32* xs=static_cast<float32*>(NkAlloc(MAX_PTS*sizeof(float32)));
    float32* ys=static_cast<float32*>(NkAlloc(MAX_PTS*sizeof(float32)));
    int32* cs=static_cast<int32*>(NkAlloc(MAX_CNT*sizeof(int32)));
    int32* cl=static_cast<int32*>(NkAlloc(MAX_CNT*sizeof(int32)));
    if(!xs||!ys||!cs||!cl){NkFree(xs);NkFree(ys);NkFree(cs);NkFree(cl);return;}
    int32 numPts=0,numContours=0;
    ParsePathData(d,xs,ys,cs,cl,numPts,numContours,MAX_PTS,MAX_CNT);
    // Applique la transformation courante
    for(int32 i=0;i<numPts;++i) ctx.ctm.Apply(xs[i],ys[i]);
    // Dessine chaque contour
    NkSVGStyle st=ctx.style;
    NkSVGColor fill=st.fill; ApplyStyleAlpha(fill,st.opacity,st.fillOpacity);
    NkSVGColor stroke=st.stroke; ApplyStyleAlpha(stroke,st.opacity,st.strokeOpacity);
    for(int32 c=0;c<numContours;++c){
        const int32 start=cs[c],len=cl[c];
        if(!fill.none) FillPolygon(*ctx.img,xs+start,ys+start,len,fill,st.fillEvenOdd);
        if(!stroke.none) StrokePolyline(*ctx.img,xs+start,ys+start,len,stroke,st.strokeWidth,false,st.linecap,st.linejoin);
    }
    NkFree(xs);NkFree(ys);NkFree(cs);NkFree(cl);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Parser XML principal
// ─────────────────────────────────────────────────────────────────────────────

void NkSVGCodec::RenderElement(const char* tag, const Attr* attrs, int32 numAttrs,
                                 RenderCtx& ctx) noexcept
{
    if(!ctx.style.visible) return;

    // Sauvegarde/restaure le style et la transformation
    NkSVGStyle savedStyle=ctx.style;
    NkSVGTransform savedCTM=ctx.ctm;

    ctx.style=ParseStyle(attrs,numAttrs,ctx.style);

    // Transformation locale
    const char* transform=GetAttrStr(attrs,numAttrs,"transform");
    if(transform) ctx.ctm=ctx.ctm*NkSVGTransform::Parse(transform);

    if     (NkStringView(tag)=="rect")     RenderRect    (ctx,attrs,numAttrs);
    else if(NkStringView(tag)=="circle")   RenderCircle  (ctx,attrs,numAttrs);
    else if(NkStringView(tag)=="ellipse")  RenderEllipse (ctx,attrs,numAttrs);
    else if(NkStringView(tag)=="line")     RenderLine    (ctx,attrs,numAttrs);
    else if(NkStringView(tag)=="polyline") RenderPolyline(ctx,attrs,numAttrs,false);
    else if(NkStringView(tag)=="polygon")  RenderPolyline(ctx,attrs,numAttrs,true);
    else if(NkStringView(tag)=="path")     RenderPath    (ctx,attrs,numAttrs);

    ctx.style=savedStyle;
    ctx.ctm=savedCTM;
}

void NkSVGCodec::ParseAndRender(const char* xml, usize xmlLen,
                                  RenderCtx& ctx) noexcept
{
    const char* p=xml;
    const char* end=xml+xmlLen;
    char tagBuf[64]; Attr attrsBuf[32]; char attrNames[32][64]; char attrVals[32][256];

    while(p<end){
        // Cherche '<'
        while(p<end&&*p!='<') ++p;
        if(p>=end) break;
        ++p;
        if(*p=='/'){ while(p<end&&*p!='>') ++p; ++p; continue; } // closing tag
        if(*p=='!'||*p=='?'){ while(p<end&&*p!='>') ++p; ++p; continue; } // comment/PI

        // Lit le tag
        int32 ti=0;
        while(p<end&&*p!=' '&&*p!='>'&&*p!='/'&&ti<63) tagBuf[ti++]=*p++;
        tagBuf[ti]=0;

        // Lit les attributs
        int32 numAttrs=0;
        while(p<end&&*p!='>'&&*p!='/'){
            p=SkipWS(p); if(*p=='>'||*p=='/') break;
            int32 ni=0;
            while(p<end&&*p!='='&&*p!=' '&&*p!='>'&&ni<63) attrNames[numAttrs][ni++]=*p++;
            attrNames[numAttrs][ni]=0;
            if(*p=='='){++p;
                const char q=(*p=='"'||*p=='\'')?*p++:' ';
                int32 vi=0;
                while(p<end&&*p!=q&&*p!='>'&&vi<255) attrVals[numAttrs][vi++]=*p++;
                attrVals[numAttrs][vi]=0;
                if(*p==q) ++p;
            } else { attrVals[numAttrs][0]=0; }
            attrsBuf[numAttrs].name=attrNames[numAttrs];
            attrsBuf[numAttrs].value=attrVals[numAttrs];
            if(numAttrs<31) ++numAttrs;
        }
        const bool selfClose=(*p=='/');
        while(p<end&&*p!='>') ++p;
        if(p<end) ++p;

        RenderElement(tagBuf,attrsBuf,numAttrs,ctx);
        (void)selfClose;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Decode
// ─────────────────────────────────────────────────────────────────────────────

NkImage* NkSVGCodec::Decode(const uint8* data, usize size,
                              int32 outW, int32 outH) noexcept
{
    if(!data||size<5) return nullptr;
    const char* xml=reinterpret_cast<const char*>(data);

    // Parse viewBox et dimensions depuis <svg>
    float32 vbX=0,vbY=0,vbW=800,vbH=600;
    float32 svgW=0,svgH=0;
    const char* p=xml;
    // Cherche la balise <svg>
    while(*p&&!(p[0]=='<'&&p[1]=='s'&&p[2]=='v'&&p[3]=='g')) ++p;
    if(*p){
        // Parse quelques attributs
        const char* svgEnd=p;
        while(*svgEnd&&*svgEnd!='>') ++svgEnd;
        char tmp[512]={};
        NkCopy(tmp,p,static_cast<usize>(::fminf(static_cast<float32>(svgEnd-p),511.f)));
        // viewBox
        const char* vb=::strstr(tmp,"viewBox");
        if(vb){
            vb+=7; while(*vb==' '||*vb=='='||*vb=='"')++vb;
            vbX=ParseFloat(vb,&vb); while(*vb==' '||*vb==',')++vb;
            vbY=ParseFloat(vb,&vb); while(*vb==' '||*vb==',')++vb;
            vbW=ParseFloat(vb,&vb); while(*vb==' '||*vb==',')++vb;
            vbH=ParseFloat(vb,&vb);
        }
        const char* sw=::strstr(tmp,"width");
        if(sw){sw+=5;while(*sw==' '||*sw=='='||*sw=='"')++sw;svgW=ParseFloat(sw);}
        const char* sh=::strstr(tmp,"height");
        if(sh){sh+=6;while(*sh==' '||*sh=='='||*sh=='"')++sh;svgH=ParseFloat(sh);}
    }
    if(vbW<=0) vbW=svgW>0?svgW:800;
    if(vbH<=0) vbH=svgH>0?svgH:600;
    if(outW<=0) outW=static_cast<int32>(svgW>0?svgW:vbW);
    if(outH<=0) outH=static_cast<int32>(svgH>0?svgH:vbH);
    if(outW<=0) outW=800;
    if(outH<=0) outH=600;

    NkImage* img=NkImage::Alloc(outW,outH,NkImagePixelFormat::NK_RGBA32);
    if(!img) return nullptr;
    // Fond blanc par défaut
    for(int32 y=0;y<outH;++y){
        uint8* row=img->RowPtr(y);
        for(int32 x=0;x<outW;++x){row[x*4]=255;row[x*4+1]=255;row[x*4+2]=255;row[x*4+3]=255;}
    }

    RenderCtx ctx;
    ctx.img=img;
    ctx.scaleX=static_cast<float32>(outW)/vbW;
    ctx.scaleY=static_cast<float32>(outH)/vbH;
    ctx.viewX=vbX; ctx.viewY=vbY; ctx.viewW=vbW; ctx.viewH=vbH;
    ctx.ctm=NkSVGTransform::Scale(ctx.scaleX,ctx.scaleY)*
             NkSVGTransform::Translate(-vbX,-vbY);
    ctx.style.fill=NkSVGColor::Black();
    ctx.style.stroke=NkSVGColor::None();
    ctx.style.strokeWidth=1.f;
    ctx.style.opacity=1.f;
    ctx.style.fillOpacity=1.f;
    ctx.style.strokeOpacity=1.f;
    ctx.style.visible=true;

    ParseAndRender(xml,size,ctx);
    return img;
}

NkImage* NkSVGCodec::DecodeFromFile(const char* path, int32 outW, int32 outH) noexcept {
    FILE* f=::fopen(path,"rb");
    if(!f) return nullptr;
    ::fseek(f,0,SEEK_END); const long sz=::ftell(f); ::fseek(f,0,SEEK_SET);
    if(sz<=0){::fclose(f);return nullptr;}
    uint8* buf=static_cast<uint8*>(NkAlloc(sz+1));
    if(!buf){::fclose(f);return nullptr;}
    ::fread(buf,1,sz,f); buf[sz]=0; ::fclose(f);
    NkImage* img=Decode(buf,sz,outW,outH);
    NkFree(buf);
    return img;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Encode — encapsule l'image dans un SVG avec <image> base64 PNG
// ─────────────────────────────────────────────────────────────────────────────

usize NkSVGCodec::Base64Encode(const uint8* in, usize inLen,
                                 char* out, usize outLen) noexcept
{
    static const char kChars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    usize pos=0;
    for(usize i=0;i<inLen&&pos+4<=outLen;i+=3){
        const uint32 b=static_cast<uint32>(in[i])<<16|
                        (i+1<inLen?static_cast<uint32>(in[i+1])<<8:0)|
                        (i+2<inLen?static_cast<uint32>(in[i+2]):0);
        out[pos++]=kChars[(b>>18)&0x3F];
        out[pos++]=kChars[(b>>12)&0x3F];
        out[pos++]=(i+1<inLen)?kChars[(b>>6)&0x3F]:'=';
        out[pos++]=(i+2<inLen)?kChars[b&0x3F]:'=';
    }
    if(pos<outLen) out[pos]=0;
    return pos;
}

bool NkSVGCodec::Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept {
    if(!img.IsValid()) return false;
    // Encode l'image en PNG
    uint8* png=nullptr; usize pngSize=0;
    if(!NkPNGCodec::Encode(img,png,pngSize)) return false;
    // Base64 du PNG
    const usize b64Len=((pngSize+2)/3)*4+1;
    char* b64=static_cast<char*>(NkAlloc(b64Len));
    if(!b64){NkFree(png);return false;}
    Base64Encode(png,pngSize,b64,b64Len);
    NkFree(png);
    // Génère le SVG
    NkImageStream s;
    char header[256];
    ::snprintf(header,sizeof(header),
        "<?xml version=\"1.0\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
        "width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n"
        "<image width=\"%d\" height=\"%d\" "
        "xlink:href=\"data:image/png;base64,",
        img.Width(),img.Height(),img.Width(),img.Height(),
        img.Width(),img.Height());
    s.WriteBytes(reinterpret_cast<const uint8*>(header),NkStringView(header).Length());
    s.WriteBytes(reinterpret_cast<const uint8*>(b64),b64Len-1);
    const char* footer="\"/>\n</svg>\n";
    s.WriteBytes(reinterpret_cast<const uint8*>(footer),NkStringView(footer).Length());
    NkFree(b64);
    return s.TakeBuffer(out,outSize);
}

bool NkSVGCodec::EncodeToFile(const NkImage& img, const char* path) noexcept {
    uint8* d=nullptr; usize sz=0;
    if(!Encode(img,d,sz)) return false;
    FILE* f=::fopen(path,"wb");
    if(!f){NkFree(d);return false;}
    const bool ok=(::fwrite(d,1,sz,f)==sz);
    ::fclose(f); NkFree(d); return ok;
}

} // namespace nkentseu
