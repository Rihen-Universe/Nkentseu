/**
 * @File    NkSVGRenderer.cpp
 * @Brief   Rasteriseur SVG complet connecté au DOM.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "NKImage/Codecs/SVG/NkSVGRenderer.h"
#include "NKImage/Codecs/SVG/NkXMLParser.h"
#include <cmath>
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKContainers/String/NkStringView.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {

using namespace nkentseu::memory;
using namespace nkentseu::math;

    // ─────────────────────────────────────────────────────────────────────────────
    //  Helpers
    // ─────────────────────────────────────────────────────────────────────────────

    static NKIMG_INLINE uint8 Clamp8(float v) noexcept {
        return static_cast<uint8>(v<0?0:v>255?255:v);
    }
    static NKIMG_INLINE float Clamp01(float v) noexcept { return v<0?0:v>1?1:v; }

    float NkSVGRenderer::ParseCoord(const char*& p) noexcept {
        while(*p==' '||*p==','||*p=='\t') ++p;
        char* e; float v=::strtof(p,&e);
        if(e>p) p=e; else ++p;
        return v;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  BlendPixel — alpha composite Porter-Duff (src over)
    // ─────────────────────────────────────────────────────────────────────────────

    void NkSVGRenderer::BlendPixel(NkImage& img, int32 x, int32 y,
                                    uint8 r, uint8 g, uint8 b, uint8 a) noexcept
    {
        if(x<0||y<0||x>=img.Width()||y>=img.Height()) return;
        uint8* p=img.RowPtr(y)+x*4;
        const float sa=a/255.f, da=p[3]/255.f;
        const float outA=sa+da*(1-sa);
        if(outA<1e-6f){p[0]=p[1]=p[2]=0;p[3]=0;return;}
        p[0]=Clamp8((r*sa+p[0]*da*(1-sa))/outA);
        p[1]=Clamp8((g*sa+p[1]*da*(1-sa))/outA);
        p[2]=Clamp8((b*sa+p[2]*da*(1-sa))/outA);
        p[3]=Clamp8(outA*255.f);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  SamplePaint — couleur en un point (direct ou gradient)
    // ─────────────────────────────────────────────────────────────────────────────

    void NkSVGRenderer::SamplePaint(const NkSVGPaint& paint,
                                    const NkSVGDefs& defs,
                                    float x, float y,
                                    float bx0,float by0,float bx1,float by1,
                                    float opacity,
                                    uint8& R,uint8& G,uint8& B,uint8& A) noexcept
    {
        if(paint.IsNone()){R=G=B=A=0;return;}
        if(paint.IsColor()){
            R=paint.r; G=paint.g; B=paint.b;
            A=Clamp8(paint.a*opacity);
            return;
        }
        if(paint.IsURL()){
            NkSVGGradient* grad=defs.FindGradient(paint.url);
            if(!grad){R=G=B=A=0;return;}
            float t=0;
            const float bw=bx1-bx0, bh=by1-by0;

            // Transforme le point dans l'espace du gradient
            float lx=x,ly=y;
            if(!grad->transform.IsIdentity()){
                NkSVGMatrix inv=grad->transform.Inverse();
                inv.Apply(lx,ly);
            }

            if(grad->units==NkSVGGradientUnits::NK_OBJECT_BOUNDING_BOX&&bw>0&&bh>0){
                lx=(lx-bx0)/bw; ly=(ly-by0)/bh;
            }

            if(grad->type==NkSVGGradientType::NK_LINEAR) t=grad->LinearT(lx,ly);
            else                                        t=grad->RadialT(lx,ly);

            grad->Sample(t,R,G,B,A);
            A=Clamp8(A*opacity);
            return;
        }
        R=G=B=A=0;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  FillPolygon — scanline AA avec super-sampling
    // ─────────────────────────────────────────────────────────────────────────────

    void NkSVGRenderer::FillPolygon(NkImage& img,
                                    const float* xs, const float* ys, int32 n,
                                    const NkSVGPaint& paint, const NkSVGDefs& defs,
                                    float bx0,float by0,float bx1,float by1,
                                    float opacity, NkSVGFillRule rule,
                                    int32 ss) noexcept
    {
        if(n<3||paint.IsNone()) return;
        const int32 W=img.Width(), H=img.Height();
        // Bounding box clampée
        float minY=ys[0],maxY=ys[0];
        for(int32 i=1;i<n;++i){if(ys[i]<minY)minY=ys[i];if(ys[i]>maxY)maxY=ys[i];}
        const int32 y0s=static_cast<int32>(minY);
        const int32 y1s=static_cast<int32>(maxY)+1;

        const int32 ssF=ss>0?ss:1; // super-sampling factor
        const float ssInv=1.f/ssF;

        for(int32 py=y0s;py<=y1s&&py<H;++py){
            if(py<0) continue;
            for(int32 px=0;px<W;++px){
                int32 hits=0;
                for(int32 sy=0;sy<ssF;++sy){
                    const float fy=py+(sy+0.5f)*ssInv;
                    // Accumule le winding number
                    int32 wind=0;
                    float isects[512]; int32 ic=0;
                    for(int32 i=0;i<n&&ic<511;++i){
                        const int32 j=(i+1)%n;
                        const float yi=ys[i],yj=ys[j];
                        if((yi<=fy&&yj>fy)||(yj<=fy&&yi>fy)){
                            const float t=(fy-yi)/(yj-yi);
                            isects[ic++]=xs[i]+t*(xs[j]-xs[i]);
                            // winding
                            if(yj>yi) ++wind; else --wind;
                            (void)wind;
                        }
                    }
                    // Tri des intersections
                    for(int32 a=1;a<ic;++a){
                        float k=isects[a]; int32 b=a-1;
                        while(b>=0&&isects[b]>k){isects[b+1]=isects[b];--b;}
                        isects[b+1]=k;
                    }
                    // Vérifie si px est à l'intérieur
                    for(int32 sx=0;sx<ssF;++sx){
                        const float fx=px+(sx+0.5f)*ssInv;
                        // Winding rule
                        int32 w2=0;
                        for(int32 i=0;i<n;++i){
                            const int32 j=(i+1)%n;
                            if((ys[i]<=fy&&ys[j]>fy)||(ys[j]<=fy&&ys[i]>fy)){
                                const float t2=(fy-ys[i])/(ys[j]-ys[i]);
                                const float xint=xs[i]+t2*(xs[j]-xs[i]);
                                if(xint>fx){if(ys[j]>ys[i])++w2;else --w2;}
                            }
                        }
                        const bool inside=(rule==NkSVGFillRule::NK_NON_ZERO)?(w2!=0):(w2&1);
                        if(inside) ++hits;
                    }
                }
                if(hits==0) continue;
                const float coverage=static_cast<float>(hits)/(ssF*ssF);
                uint8 r,g,b,a;
                SamplePaint(paint,defs,px+0.5f,py+0.5f,bx0,by0,bx1,by1,opacity*coverage,r,g,b,a);
                BlendPixel(img,px,py,r,g,b,a);
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  DrawLineAA — ligne épaisse avec Wu antialiasing
    // ─────────────────────────────────────────────────────────────────────────────

    void NkSVGRenderer::DrawLineAA(NkImage& img,
                                    float x0, float y0, float x1, float y1,
                                    uint8 r, uint8 g, uint8 b, uint8 a,
                                    float width) noexcept
    {
        const float dx=x1-x0, dy=y1-y0;
        const float len=NkSqrt(dx*dx+dy*dy);
        if(len<0.001f){
            BlendPixel(img,static_cast<int32>(x0),static_cast<int32>(y0),r,g,b,a);
            return;
        }
        // Vecteur normal normalisé
        const float nx=-dy/len, ny=dx/len;
        const float hw=width*0.5f;

        // Pour chaque pixel dans la bounding box de la ligne épaissie
        const float minX=::fminf(x0,x1)-hw-1, maxX=::fmaxf(x0,x1)+hw+1;
        const float minY=::fminf(y0,y1)-hw-1, maxY=::fmaxf(y0,y1)+hw+1;
        const int32 px0=static_cast<int32>(minX), px1=static_cast<int32>(maxX)+1;
        const int32 py0=static_cast<int32>(minY), py1=static_cast<int32>(maxY)+1;
        const int32 W=img.Width(), H=img.Height();

        for(int32 py=py0;py<=py1&&py<H;++py){
            if(py<0) continue;
            for(int32 px=px0;px<=px1&&px<W;++px){
                if(px<0) continue;
                const float fx=px+0.5f-x0, fy=py+0.5f-y0;
                // Distance signée le long de la ligne (t) et perpendiculaire (d)
                const float t=(fx*dx+fy*dy)/(len*len);
                const float d=NkFabs(fx*nx+fy*ny);
                // Point le plus proche sur le segment
                if(t>=0&&t<=1){
                    // Distance perpendiculaire
                    const float cover=Clamp01(hw+0.5f-d);
                    if(cover>0)
                        BlendPixel(img,px,py,r,g,b,Clamp8(a*cover));
                } else {
                    // Caps
                    const float capX=(t<0)?x0:x1, capY=(t<0)?y0:y1;
                    const float cdx=px+0.5f-capX, cdy=py+0.5f-capY;
                    const float cd=NkSqrt(cdx*cdx+cdy*cdy);
                    const float cover=Clamp01(hw+0.5f-cd);
                    if(cover>0)
                        BlendPixel(img,px,py,r,g,b,Clamp8(a*cover));
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  StrokePolyline — stroke avec dash et jointures
    // ─────────────────────────────────────────────────────────────────────────────

    void NkSVGRenderer::StrokePolyline(NkImage& img,
                                        const float* xs, const float* ys, int32 n,
                                        bool closed,
                                        const NkSVGPaint& paint, const NkSVGDefs& defs,
                                        float bx0,float by0,float bx1,float by1,
                                        float width, float opacity,
                                        NkSVGLineCap cap, NkSVGLineJoin join,
                                        float miterLimit,
                                        const float* dash, int32 dashCount, float dashOffset,
                                        int32 ss) noexcept
    {
        if(n<2||paint.IsNone()||width<=0) return;
        (void)cap;(void)join;(void)miterLimit;(void)ss;

        // Gestion des tirets (dash)
        bool useDash=(dashCount>0);
        float dashLen=0; for(int32 i=0;i<dashCount;++i) dashLen+=dash[i];

        for(int32 i=0;i<n-1+(closed?1:0);++i){
            const int32 j=(i+1)%n;
            float ax=xs[i],ay=ys[i],bx=xs[j],by=ys[j];

            if(useDash&&dashLen>0){
                // Décompose le segment selon le dash pattern
                const float segLen=NkSqrt((bx-ax)*(bx-ax)+(by-ay)*(by-ay));
                if(segLen<0.001f) continue;
                const float ddx=(bx-ax)/segLen, ddy=(by-ay)/segLen;
                float t=0, phase=::fmodf(dashOffset,dashLen);
                int32 di=0;
                while(phase>=dash[di]){phase-=dash[di];di=(di+1)%dashCount;}
                bool drawing=(di%2==0);
                float rem=dash[di]-phase;
                while(t<segLen){
                    const float step=::fminf(rem,segLen-t);
                    if(drawing){
                        const float sx=ax+t*ddx, sy=ay+t*ddy;
                        const float ex=ax+(t+step)*ddx, ey=ay+(t+step)*ddy;
                        uint8 r,g,b,a;
                        SamplePaint(paint,defs,(sx+ex)*0.5f,(sy+ey)*0.5f,
                                    bx0,by0,bx1,by1,opacity,r,g,b,a);
                        DrawLineAA(img,sx,sy,ex,ey,r,g,b,a,width);
                    }
                    t+=step; rem-=step;
                    if(rem<0.001f){ di=(di+1)%dashCount; rem=dash[di]; drawing=!drawing; }
                }
            } else {
                uint8 r,g,b,a;
                SamplePaint(paint,defs,(ax+bx)*0.5f,(ay+by)*0.5f,
                            bx0,by0,bx1,by1,opacity,r,g,b,a);
                DrawLineAA(img,ax,ay,bx,by,r,g,b,a,width);
            }
        }
        // Round joins approximatifs
        if(join==NkSVGLineJoin::NK_ROUND||cap==NkSVGLineCap::NK_ROUND){
            for(int32 i=closed?0:1;i<n-(closed?0:1);++i){
                uint8 r,g,b,a;
                SamplePaint(paint,defs,xs[i],ys[i],bx0,by0,bx1,by1,opacity,r,g,b,a);
                const int32 steps=static_cast<int32>(3.14159f*width)+4;
                for(int32 s=0;s<steps;++s){
                    const float ang=2.f*3.14159f*s/steps;
                    const float px=xs[i]+NkCos(ang)*width*0.5f;
                    const float py=ys[i]+NkSin(ang)*width*0.5f;
                    BlendPixel(img,static_cast<int32>(px),static_cast<int32>(py),r,g,b,a);
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Bézier flatten (Casteljau adaptatif)
    // ─────────────────────────────────────────────────────────────────────────────

    void NkSVGRenderer::BezierFlatten(float x0,float y0,float x1,float y1,
                                        float x2,float y2,float x3,float y3,
                                        float* xs, float* ys, int32& n, int32 max,
                                        int32 depth) noexcept
    {
        // Mesure la déviation
        const float d1x=x1-x0,d1y=y1-y0,d2x=x2-x3,d2y=y2-y3;
        const float d=NkSqrt(d1x*d1x+d1y*d1y)+NkSqrt(d2x*d2x+d2y*d2y);
        if(d<0.5f||depth>8){if(n<max){xs[n]=x3;ys[n]=y3;++n;}return;}
        const float mx=(x0+3*x1+3*x2+x3)/8,my=(y0+3*y1+3*y2+y3)/8;
        const float l1x=(x0+x1)/2,l1y=(y0+y1)/2;
        const float l2x=(l1x+(x1+x2)/2)/2,l2y=(l1y+(y1+y2)/2)/2;
        const float r2x=((x1+x2)/2+(x2+x3)/2)/2,r2y=((y1+y2)/2+(y2+y3)/2)/2;
        const float r1x=(x2+x3)/2,r1y=(y2+y3)/2;
        BezierFlatten(x0,y0,l1x,l1y,l2x,l2y,mx,my,xs,ys,n,max,depth+1);
        BezierFlatten(mx,my,r2x,r2y,r1x,r1y,x3,y3,xs,ys,n,max,depth+1);
    }

    void NkSVGRenderer::ArcToLines(float x1,float y1,float rx,float ry,
                                    float xAngle, bool la, bool sw,
                                    float x2, float y2,
                                    float* xs, float* ys, int32& n, int32 max) noexcept
    {
        if(rx<=0||ry<=0){if(n<max){xs[n]=x2;ys[n]=y2;++n;}return;}
        const float ca=NkCos(xAngle),sa=NkSin(xAngle);
        const float dxH=(x1-x2)*.5f,dyH=(y1-y2)*.5f;
        const float x1p= ca*dxH+sa*dyH, y1p=-sa*dxH+ca*dyH;
        float rx_=rx,ry_=ry;
        const float lambda=x1p*x1p/(rx_*rx_)+y1p*y1p/(ry_*ry_);
        if(lambda>1){const float sl=NkSqrt(lambda);rx_*=sl;ry_*=sl;}
        float sq=(rx_*rx_*ry_*ry_-rx_*rx_*y1p*y1p-ry_*ry_*x1p*x1p)/
                (rx_*rx_*y1p*y1p+ry_*ry_*x1p*x1p);
        sq=sq<0?0:NkSqrt(sq);
        if(la==sw) sq=-sq;
        const float cxp= sq*rx_*y1p/ry_, cyp=-sq*ry_*x1p/rx_;
        const float cx=ca*cxp-sa*cyp+(x1+x2)*.5f;
        const float cy=sa*cxp+ca*cyp+(y1+y2)*.5f;
        auto ang=[](float ux,float uy,float vx,float vy)->float{
            const float n2=NkSqrt(ux*ux+uy*uy)*NkSqrt(vx*vx+vy*vy);
            if(n2==0) return 0;
            float a=NkAcos(::fmaxf(-1.f,::fminf(1.f,(ux*vx+uy*vy)/n2)));
            if(ux*vy-uy*vx<0) a=-a; return a;
        };
        const float theta1=ang(1,0,(x1p-cxp)/rx_,(y1p-cyp)/ry_);
        float dTheta=ang((x1p-cxp)/rx_,(y1p-cyp)/ry_,(-x1p-cxp)/rx_,(-y1p-cyp)/ry_);
        if(!sw&&dTheta>0) dTheta-=2*3.14159265f;
        if( sw&&dTheta<0) dTheta+=2*3.14159265f;
        const int32 steps=static_cast<int32>(NkFabs(dTheta)*::fmaxf(rx_,ry_)*2.f)+4;
        for(int32 i=1;i<=steps&&n<max;++i){
            const float t=static_cast<float>(i)/steps;
            const float a=theta1+t*dTheta;
            xs[n]=cx+rx_*NkCos(a)*ca-ry_*NkSin(a)*sa;
            ys[n]=cy+rx_*NkCos(a)*sa+ry_*NkSin(a)*ca;
            ++n;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  ParsePath — parse SVG path data vers listes de contours
    // ─────────────────────────────────────────────────────────────────────────────

    bool NkSVGRenderer::ParsePath(const char* d,
                                    float* xs, float* ys,
                                    int32* cStart, int32* cLen,
                                    int32& numPts, int32& numContours,
                                    int32 maxPts, int32 maxCnt) noexcept
    {
        if(!d){numPts=0;numContours=0;return true;}
        const char* p=d;
        float cx=0,cy=0,sx=0,sy=0,cpx=0,cpy=0;
        char cmd=' ';
        int32 contourStart=0;
        numPts=0; numContours=0;

        auto addPt=[&](float x,float y){if(numPts<maxPts){xs[numPts]=x;ys[numPts]=y;++numPts;}};
        auto closePath=[&](){
            if(numContours<maxCnt){
                cStart[numContours]=contourStart;
                cLen[numContours]=numPts-contourStart;
                ++numContours;
            }
            contourStart=numPts;
        };
        auto parseF=[&]()->float{ return ParseCoord(p); };

        while(*p){
            while(*p==' '||*p=='\t'||*p=='\n'||*p=='\r') ++p;
            if(!*p) break;
            if((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')){cmd=*p++;while(*p==' ') ++p;}
            const bool rel=(cmd>='a'&&cmd<='z');
            const char uc=rel?(char)(cmd-32):cmd;

            if(uc=='M'){
                float x=parseF(),y=parseF();
                if(rel){x+=cx;y+=cy;}
                if(numPts>contourStart) closePath();
                cx=sx=x;cy=sy=y;addPt(cx,cy);cpx=cx;cpy=cy;
                cmd=rel?'l':'L';
            } else if(uc=='L'){
                float x=parseF(),y=parseF();
                if(rel){x+=cx;y+=cy;}
                cx=x;cy=y;addPt(cx,cy);cpx=cx;cpy=cy;
            } else if(uc=='H'){
                float x=parseF();cx=rel?cx+x:x;addPt(cx,cy);
            } else if(uc=='V'){
                float y=parseF();cy=rel?cy+y:y;addPt(cx,cy);
            } else if(uc=='C'){
                float x1=parseF(),y1=parseF(),x2=parseF(),y2=parseF(),ex=parseF(),ey=parseF();
                if(rel){x1+=cx;y1+=cy;x2+=cx;y2+=cy;ex+=cx;ey+=cy;}
                BezierFlatten(cx,cy,x1,y1,x2,y2,ex,ey,xs,ys,numPts,maxPts);
                cpx=x2;cpy=y2;cx=ex;cy=ey;
            } else if(uc=='S'){
                float x2=parseF(),y2=parseF(),ex=parseF(),ey=parseF();
                if(rel){x2+=cx;y2+=cy;ex+=cx;ey+=cy;}
                const float x1=2*cx-cpx,y1=2*cy-cpy;
                BezierFlatten(cx,cy,x1,y1,x2,y2,ex,ey,xs,ys,numPts,maxPts);
                cpx=x2;cpy=y2;cx=ex;cy=ey;
            } else if(uc=='Q'){
                float x1=parseF(),y1=parseF(),ex=parseF(),ey=parseF();
                if(rel){x1+=cx;y1+=cy;ex+=cx;ey+=cy;}
                const float cx1=cx+(x1-cx)*2/3,cy1=cy+(y1-cy)*2/3;
                const float cx2=ex+(x1-ex)*2/3,cy2=ey+(y1-ey)*2/3;
                BezierFlatten(cx,cy,cx1,cy1,cx2,cy2,ex,ey,xs,ys,numPts,maxPts);
                cpx=x1;cpy=y1;cx=ex;cy=ey;
            } else if(uc=='T'){
                float ex=parseF(),ey=parseF();
                if(rel){ex+=cx;ey+=cy;}
                const float x1=2*cx-cpx,y1=2*cy-cpy;
                const float cx1=cx+(x1-cx)*2/3,cy1=cy+(y1-cy)*2/3;
                const float cx2=ex+(x1-ex)*2/3,cy2=ey+(y1-ey)*2/3;
                BezierFlatten(cx,cy,cx1,cy1,cx2,cy2,ex,ey,xs,ys,numPts,maxPts);
                cpx=x1;cpy=y1;cx=ex;cy=ey;
            } else if(uc=='A'){
                float rx=parseF(),ry=parseF(),xa=parseF()*3.14159265f/180.f;
                const bool la=parseF()!=0,sw=parseF()!=0;
                float ex=parseF(),ey=parseF();
                if(rel){ex+=cx;ey+=cy;}
                ArcToLines(cx,cy,rx,ry,xa,la,sw,ex,ey,xs,ys,numPts,maxPts);
                cx=ex;cy=ey;cpx=cx;cpy=cy;
            } else if(uc=='Z'||uc=='z'){
                addPt(sx,sy);closePath();cx=sx;cy=sy;cmd=' ';++p;continue;
            } else {++p;continue;}
            while(*p==' '||*p==',') ++p;
        }
        if(numPts>contourStart) closePath();
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Formes → points
    // ─────────────────────────────────────────────────────────────────────────────

    void NkSVGRenderer::RectToPoints(float x,float y,float w,float h,
                                        float rx,float ry,
                                        float* xs,float* ys,int32& n,int32 max) noexcept
    {
        if(rx<=0&&ry<=0){
            // Rectangle simple
            if(n+4>max) return;
            xs[n]=x;ys[n]=y;++n;
            xs[n]=x+w;ys[n]=y;++n;
            xs[n]=x+w;ys[n]=y+h;++n;
            xs[n]=x;ys[n]=y+h;++n;
        } else {
            // Rectangle arrondi : décompose en 4 arcs
            if(rx>w/2)rx=w/2; if(ry>h/2)ry=h/2;
            // Coin supérieur gauche → en sens horaire
            xs[n]=x+rx;ys[n]=y;++n;        // top
            xs[n]=x+w-rx;ys[n]=y;++n;
            ArcToLines(x+w-rx,y,rx,ry,0,false,true,x+w,y+ry,xs,ys,n,max);
            xs[n]=x+w;ys[n]=y+h-ry;++n;
            ArcToLines(x+w,y+h-ry,rx,ry,0,false,true,x+w-rx,y+h,xs,ys,n,max);
            xs[n]=x+rx;ys[n]=y+h;++n;
            ArcToLines(x+rx,y+h,rx,ry,0,false,true,x,y+h-ry,xs,ys,n,max);
            xs[n]=x;ys[n]=y+ry;++n;
            ArcToLines(x,y+ry,rx,ry,0,false,true,x+rx,y,xs,ys,n,max);
        }
    }

    void NkSVGRenderer::CircleToPoints(float cx,float cy,float r,
                                        float* xs,float* ys,int32& n,int32 max) noexcept
    {
        const int32 steps=static_cast<int32>(2*3.14159265f*r/2.f)+8;
        for(int32 i=0;i<steps&&n<max;++i){
            const float a=2.f*3.14159265f*i/steps;
            xs[n]=cx+r*NkCos(a);ys[n]=cy+r*NkSin(a);++n;
        }
    }

    void NkSVGRenderer::EllipseToPoints(float cx,float cy,float rx,float ry,
                                        float* xs,float* ys,int32& n,int32 max) noexcept
    {
        const int32 steps=static_cast<int32>(2*3.14159265f*::fmaxf(rx,ry)/2.f)+8;
        for(int32 i=0;i<steps&&n<max;++i){
            const float a=2.f*3.14159265f*i/steps;
            xs[n]=cx+rx*NkCos(a);ys[n]=cy+ry*NkSin(a);++n;
        }
    }

    void NkSVGRenderer::PolygonFromAttr(const char* pts,
                                        float* xs,float* ys,int32& n,int32 max) noexcept
    {
        if(!pts) return;
        const char* p=pts;
        while(*p&&n<max){
            xs[n]=ParseCoord(p);ys[n]=ParseCoord(p);++n;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  ComputeBBox + ApplyCTM
    // ─────────────────────────────────────────────────────────────────────────────

    void NkSVGRenderer::ComputeBBox(Shape& s) noexcept {
        if(s.numPoints==0){s.bboxX0=s.bboxY0=s.bboxX1=s.bboxY1=0;return;}
        s.bboxX0=s.bboxX1=s.xs[0]; s.bboxY0=s.bboxY1=s.ys[0];
        for(int32 i=1;i<s.numPoints;++i){
            if(s.xs[i]<s.bboxX0)s.bboxX0=s.xs[i]; if(s.xs[i]>s.bboxX1)s.bboxX1=s.xs[i];
            if(s.ys[i]<s.bboxY0)s.bboxY0=s.ys[i]; if(s.ys[i]>s.bboxY1)s.bboxY1=s.ys[i];
        }
    }

    void NkSVGRenderer::ApplyCTM(Shape& s, const NkSVGMatrix& m) noexcept {
        if(m.IsIdentity()) return;
        for(int32 i=0;i<s.numPoints;++i) m.Apply(s.xs[i],s.ys[i]);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  FillShape + StrokeShape
    // ─────────────────────────────────────────────────────────────────────────────

    void NkSVGRenderer::FillShape(NkImage& img, const Shape& s,
                                    const NkSVGStyle& st, const NkSVGDefs& defs,
                                    int32 ss) noexcept
    {
        if(st.fill.IsNone()) return;
        const float op=st.opacity*st.fillOpacity;
        for(int32 ci=0;ci<s.numContours;++ci){
            FillPolygon(img,s.xs+s.contourStart[ci],s.ys+s.contourStart[ci],
                        s.contourLen[ci],st.fill,defs,
                        s.bboxX0,s.bboxY0,s.bboxX1,s.bboxY1,
                        op,st.fillRule,ss);
        }
    }

    void NkSVGRenderer::StrokeShape(NkImage& img, const Shape& s,
                                    const NkSVGStyle& st, const NkSVGDefs& defs,
                                    int32 ss) noexcept
    {
        if(st.stroke.IsNone()||st.strokeWidth<=0) return;
        const float op=st.opacity*st.strokeOpacity;
        for(int32 ci=0;ci<s.numContours;++ci){
            const bool closed=(s.contourLen[ci]>2&&
                s.xs[s.contourStart[ci]]==s.xs[s.contourStart[ci]+s.contourLen[ci]-1]&&
                s.ys[s.contourStart[ci]]==s.ys[s.contourStart[ci]+s.contourLen[ci]-1]);
            StrokePolyline(img,s.xs+s.contourStart[ci],s.ys+s.contourStart[ci],
                        s.contourLen[ci],closed,
                        st.stroke,defs,s.bboxX0,s.bboxY0,s.bboxX1,s.bboxY1,
                        st.strokeWidth,op,st.lineCap,st.lineJoin,st.strokeMiterLimit,
                        st.dashArray,st.dashCount,st.dashOffset,ss);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Helpers de rendu par type d'élément
    // ─────────────────────────────────────────────────────────────────────────────

    static constexpr int32 MAX_PTS=16384, MAX_CNT=512;

    // Crée une Shape à partir de tableaux temporaires
    static NkSVGRenderer::Shape MakeShape(float* xs,float* ys,int32* cs,int32* cl,
                                        int32 np,int32 nc) noexcept {
        NkSVGRenderer::Shape s;
        s.xs=xs;s.ys=ys;s.contourStart=cs;s.contourLen=cl;
        s.numPoints=np;s.numContours=nc;
        NkSVGRenderer::ComputeBBox(s);
        return s;
    }

    void NkSVGRenderer::RenderRect(const NkSVGElement* e, RCtx& ctx) noexcept {
        if(e->width<=0||e->height<=0) return;
        float xs[512],ys[512]; int32 cs[8],cl[8]; int32 np=0,nc=0;
        RectToPoints(e->x,e->y,e->width,e->height,e->rx,e->ry,xs,ys,np,512);
        cs[0]=0;cl[0]=np;nc=1;
        Shape s=MakeShape(xs,ys,cs,cl,np,nc);
        ApplyCTM(s,e->ctm); ComputeBBox(s);
        FillShape(*ctx.img,s,e->style,ctx.dom->defs,ctx.opts.superSample);
        StrokeShape(*ctx.img,s,e->style,ctx.dom->defs,ctx.opts.superSample);
    }

    void NkSVGRenderer::RenderCircle(const NkSVGElement* e, RCtx& ctx) noexcept {
        if(e->r<=0) return;
        float xs[1024],ys[1024]; int32 cs[8],cl[8]; int32 np=0;
        CircleToPoints(e->cx,e->cy,e->r,xs,ys,np,1024);
        cs[0]=0;cl[0]=np;
        Shape s=MakeShape(xs,ys,cs,cl,np,1);
        ApplyCTM(s,e->ctm); ComputeBBox(s);
        FillShape(*ctx.img,s,e->style,ctx.dom->defs,ctx.opts.superSample);
        StrokeShape(*ctx.img,s,e->style,ctx.dom->defs,ctx.opts.superSample);
    }

    void NkSVGRenderer::RenderEllipse(const NkSVGElement* e, RCtx& ctx) noexcept {
        if(e->rx2<=0||e->ry2<=0) return;
        float xs[1024],ys[1024]; int32 cs[8],cl[8]; int32 np=0;
        EllipseToPoints(e->cx,e->cy,e->rx2,e->ry2,xs,ys,np,1024);
        cs[0]=0;cl[0]=np;
        Shape s=MakeShape(xs,ys,cs,cl,np,1);
        ApplyCTM(s,e->ctm); ComputeBBox(s);
        FillShape(*ctx.img,s,e->style,ctx.dom->defs,ctx.opts.superSample);
        StrokeShape(*ctx.img,s,e->style,ctx.dom->defs,ctx.opts.superSample);
    }

    void NkSVGRenderer::RenderLine(const NkSVGElement* e, RCtx& ctx) noexcept {
        float xs[2]={e->x1,e->x2},ys[2]={e->y1,e->y2};
        e->ctm.Apply(xs[0],ys[0]); e->ctm.Apply(xs[1],ys[1]);
        if(e->style.stroke.IsNone()||e->style.strokeWidth<=0) return;
        uint8 r,g,b,a;
        NkSVGRenderer::SamplePaint(e->style.stroke,ctx.dom->defs,
                                    (xs[0]+xs[1])*.5f,(ys[0]+ys[1])*.5f,
                                    xs[0],ys[0],xs[1],ys[1],
                                    e->style.opacity*e->style.strokeOpacity,r,g,b,a);
        DrawLineAA(*ctx.img,xs[0],ys[0],xs[1],ys[1],r,g,b,a,e->style.strokeWidth);
    }

    void NkSVGRenderer::RenderPolyline(const NkSVGElement* e, RCtx& ctx, bool close) noexcept {
        float xs[2048],ys[2048]; int32 np=0;
        PolygonFromAttr(e->points,xs,ys,np,2048);
        if(np<2) return;
        for(int32 i=0;i<np;++i) e->ctm.Apply(xs[i],ys[i]);
        int32 cs[8]={0},cl[8]={np}; int32 nc=1;
        if(close&&np>=3){xs[np]=xs[0];ys[np]=ys[0];++np;cl[0]=np;}
        Shape s=MakeShape(xs,ys,cs,cl,np,nc);
        ComputeBBox(s);
        if(close) FillShape(*ctx.img,s,e->style,ctx.dom->defs,ctx.opts.superSample);
        StrokeShape(*ctx.img,s,e->style,ctx.dom->defs,ctx.opts.superSample);
    }

    void NkSVGRenderer::RenderPath(const NkSVGElement* e, RCtx& ctx) noexcept {
        if(!e->d[0]) return;
        float* xs=static_cast<float*>(NkAlloc(MAX_PTS*sizeof(float)));
        float* ys=static_cast<float*>(NkAlloc(MAX_PTS*sizeof(float)));
        int32* cs=static_cast<int32*>(NkAlloc(MAX_CNT*sizeof(int32)));
        int32* cl=static_cast<int32*>(NkAlloc(MAX_CNT*sizeof(int32)));
        if(!xs||!ys||!cs||!cl){NkFree(xs);NkFree(ys);NkFree(cs);NkFree(cl);return;}
        int32 np=0,nc=0;
        ParsePath(e->d,xs,ys,cs,cl,np,nc,MAX_PTS,MAX_CNT);
        for(int32 i=0;i<np;++i) e->ctm.Apply(xs[i],ys[i]);
        Shape s=MakeShape(xs,ys,cs,cl,np,nc);
        ComputeBBox(s);
        FillShape(*ctx.img,s,e->style,ctx.dom->defs,ctx.opts.superSample);
        StrokeShape(*ctx.img,s,e->style,ctx.dom->defs,ctx.opts.superSample);
        NkFree(xs);NkFree(ys);NkFree(cs);NkFree(cl);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  RenderElement — dispatch récursif
    // ─────────────────────────────────────────────────────────────────────────────

    void NkSVGRenderer::RenderElement(const NkSVGElement* e, RCtx& ctx) noexcept {
        if(!e||e->inDefs) return;
        if(!e->style.IsVisible()) return;

        const char* tag=e->Tag();
        if(!tag) goto children;

        if     (NkStringView(tag)=="rect")     RenderRect    (e,ctx);
        else if(NkStringView(tag)=="circle")   RenderCircle  (e,ctx);
        else if(NkStringView(tag)=="ellipse")  RenderEllipse (e,ctx);
        else if(NkStringView(tag)=="line")     RenderLine    (e,ctx);
        else if(NkStringView(tag)=="polyline") RenderPolyline(e,ctx,false);
        else if(NkStringView(tag)=="polygon")  RenderPolyline(e,ctx,true);
        else if(NkStringView(tag)=="path")     RenderPath    (e,ctx);
        // <g>, <svg>, <symbol> : seulement les enfants

        children:
        for(const NkSVGElement* c=e->firstChild;c;c=c->nextSibling)
            RenderElement(c,ctx);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Render — point d'entrée public
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkSVGRenderer::Render(const NkSVGDOM& dom,
                                    const NkSVGRenderOptions& opts) noexcept
    {
        if(!dom.isValid||!dom.root) return nullptr;
        const int32 W=static_cast<int32>(dom.outputW>0?dom.outputW:dom.viewportW);
        const int32 H=static_cast<int32>(dom.outputH>0?dom.outputH:dom.viewportH);
        if(W<=0||H<=0) return nullptr;

        NkImage* img=NkImage::Alloc(W,H,NkImagePixelFormat::NK_RGBA32);
        if(!img) return nullptr;

        // Fond
        if(opts.transparentBg){
            NkSet(img->Pixels(),0,img->TotalBytes());
        } else {
            for(int32 y=0;y<H;++y){
                uint8* row=img->RowPtr(y);
                for(int32 x=0;x<W;++x){
                    row[x*4+0]=opts.bgR; row[x*4+1]=opts.bgG;
                    row[x*4+2]=opts.bgB; row[x*4+3]=opts.bgA;
                }
            }
        }

        RCtx ctx{img,&dom,opts,W,H};
        RenderElement(dom.root,ctx);
        return img;
    }

    NkImage* NkSVGRenderer::RenderFromXML(const uint8* xml, usize size,
                                            int32 outW, int32 outH,
                                            const NkSVGRenderOptions& opts) noexcept
    {
        NkSVGDOM dom; dom.Init();
        if(!NkSVGDOMBuilder::BuildFromXML(xml,size,dom,
                                        static_cast<float>(outW),
                                        static_cast<float>(outH))){
            dom.Destroy(); return nullptr;
        }
        NkImage* img=Render(dom,opts);
        dom.Destroy();
        return img;
    }

    NkImage* NkSVGRenderer::RenderFromFile(const char* path,
                                            int32 outW, int32 outH,
                                            const NkSVGRenderOptions& opts) noexcept
    {
        NkSVGDOM dom; dom.Init();
        if(!NkSVGDOMBuilder::BuildFromFile(path,dom,
                                            static_cast<float>(outW),
                                            static_cast<float>(outH))){
            dom.Destroy(); return nullptr;
        }
        NkImage* img=Render(dom,opts);
        dom.Destroy();
        return img;
    }

} // namespace nkentseu