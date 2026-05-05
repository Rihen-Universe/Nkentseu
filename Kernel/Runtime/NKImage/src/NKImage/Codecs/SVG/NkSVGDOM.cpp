/**
 * @File    NkSVGDOM.cpp
 * @Brief   SVG DOM — parsing, cascade CSS, gradients, defs/use/symbol.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "NKImage/Codecs/SVG/NkSVGDOM.h"
#include <cstdio>
#include <cmath>
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKContainers/String/NkStringView.h"
#include "NKMath/NkFunctions.h"
namespace nkentseu {

using namespace nkentseu::memory;
using namespace nkentseu::math;

// ─────────────────────────────────────────────────────────────────────────────
//  Table de noms de couleurs CSS (140 couleurs)
// ─────────────────────────────────────────────────────────────────────────────

static const struct { const char* name; uint32 rgb; } kCSSColors[] = {
    {"aliceblue",0xF0F8FF},{"antiquewhite",0xFAEBD7},{"aqua",0x00FFFF},
    {"aquamarine",0x7FFFD4},{"azure",0xF0FFFF},{"beige",0xF5F5DC},
    {"bisque",0xFFE4C4},{"black",0x000000},{"blanchedalmond",0xFFEBCD},
    {"blue",0x0000FF},{"blueviolet",0x8A2BE2},{"brown",0xA52A2A},
    {"burlywood",0xDEB887},{"cadetblue",0x5F9EA0},{"chartreuse",0x7FFF00},
    {"chocolate",0xD2691E},{"coral",0xFF7F50},{"cornflowerblue",0x6495ED},
    {"cornsilk",0xFFF8DC},{"crimson",0xDC143C},{"cyan",0x00FFFF},
    {"darkblue",0x00008B},{"darkcyan",0x008B8B},{"darkgoldenrod",0xB8860B},
    {"darkgray",0xA9A9A9},{"darkgreen",0x006400},{"darkgrey",0xA9A9A9},
    {"darkkhaki",0xBDB76B},{"darkmagenta",0x8B008B},{"darkolivegreen",0x556B2F},
    {"darkorange",0xFF8C00},{"darkorchid",0x9932CC},{"darkred",0x8B0000},
    {"darksalmon",0xE9967A},{"darkseagreen",0x8FBC8F},{"darkslateblue",0x483D8B},
    {"darkslategray",0x2F4F4F},{"darkturquoise",0x00CED1},{"darkviolet",0x9400D3},
    {"deeppink",0xFF1493},{"deepskyblue",0x00BFFF},{"dimgray",0x696969},
    {"dodgerblue",0x1E90FF},{"firebrick",0xB22222},{"floralwhite",0xFFFAF0},
    {"forestgreen",0x228B22},{"fuchsia",0xFF00FF},{"gainsboro",0xDCDCDC},
    {"ghostwhite",0xF8F8FF},{"gold",0xFFD700},{"goldenrod",0xDAA520},
    {"gray",0x808080},{"green",0x008000},{"greenyellow",0xADFF2F},
    {"grey",0x808080},{"honeydew",0xF0FFF0},{"hotpink",0xFF69B4},
    {"indianred",0xCD5C5C},{"indigo",0x4B0082},{"ivory",0xFFFFF0},
    {"khaki",0xF0E68C},{"lavender",0xE6E6FA},{"lavenderblush",0xFFF0F5},
    {"lawngreen",0x7CFC00},{"lemonchiffon",0xFFFACD},{"lightblue",0xADD8E6},
    {"lightcoral",0xF08080},{"lightcyan",0xE0FFFF},{"lightgoldenrodyellow",0xFAFAD2},
    {"lightgray",0xD3D3D3},{"lightgreen",0x90EE90},{"lightgrey",0xD3D3D3},
    {"lightpink",0xFFB6C1},{"lightsalmon",0xFFA07A},{"lightseagreen",0x20B2AA},
    {"lightskyblue",0x87CEFA},{"lightslategray",0x778899},{"lightsteelblue",0xB0C4DE},
    {"lightyellow",0xFFFFE0},{"lime",0x00FF00},{"limegreen",0x32CD32},
    {"linen",0xFAF0E6},{"magenta",0xFF00FF},{"maroon",0x800000},
    {"mediumaquamarine",0x66CDAA},{"mediumblue",0x0000CD},{"mediumorchid",0xBA55D3},
    {"mediumpurple",0x9370DB},{"mediumseagreen",0x3CB371},{"mediumslateblue",0x7B68EE},
    {"mediumspringgreen",0x00FA9A},{"mediumturquoise",0x48D1CC},{"mediumvioletred",0xC71585},
    {"midnightblue",0x191970},{"mintcream",0xF5FFFA},{"mistyrose",0xFFE4E1},
    {"moccasin",0xFFE4B5},{"navajowhite",0xFFDEAD},{"navy",0x000080},
    {"oldlace",0xFDF5E6},{"olive",0x808000},{"olivedrab",0x6B8E23},
    {"orange",0xFFA500},{"orangered",0xFF4500},{"orchid",0xDA70D6},
    {"palegoldenrod",0xEEE8AA},{"palegreen",0x98FB98},{"paleturquoise",0xAFEEEE},
    {"palevioletred",0xDB7093},{"papayawhip",0xFFEFD5},{"peachpuff",0xFFDAB9},
    {"peru",0xCD853F},{"pink",0xFFC0CB},{"plum",0xDDA0DD},{"powderblue",0xB0E0E6},
    {"purple",0x800080},{"rebeccapurple",0x663399},{"red",0xFF0000},
    {"rosybrown",0xBC8F8F},{"royalblue",0x4169E1},{"saddlebrown",0x8B4513},
    {"salmon",0xFA8072},{"sandybrown",0xF4A460},{"seagreen",0x2E8B57},
    {"seashell",0xFFF5EE},{"sienna",0xA0522D},{"silver",0xC0C0C0},
    {"skyblue",0x87CEEB},{"slateblue",0x6A5ACD},{"slategray",0x708090},
    {"slategrey",0x708090},{"snow",0xFFFAFA},{"springgreen",0x00FF7F},
    {"steelblue",0x4682B4},{"tan",0xD2B48C},{"teal",0x008080},
    {"thistle",0xD8BFD8},{"tomato",0xFF6347},{"turquoise",0x40E0D0},
    {"violet",0xEE82EE},{"wheat",0xF5DEB3},{"white",0xFFFFFF},
    {"whitesmoke",0xF5F5F5},{"yellow",0xFFFF00},{"yellowgreen",0x9ACD32},
    {nullptr,0}
};

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGPaint::Parse
// ─────────────────────────────────────────────────────────────────────────────

NkSVGPaint NkSVGPaint::Parse(const char* str, const NkSVGPaint& currentColor) noexcept {
    if(!str||!*str) return NkSVGPaint::None();
    while(*str==' ') ++str;

    if(NkStringView(str)=="none"||NkStringView(str)=="transparent")
        return NkSVGPaint::None();
    if(NkStringView(str)=="inherit") return NkSVGPaint::Inherit();
    if(NkStringView(str)=="currentColor") return currentColor;

    if(::strncmp(str,"url(#",5)==0||::strncmp(str,"url(\"#",6)==0){
        NkSVGPaint p; p.type=Type::URL;
        const char* start=str+5; if(*start=='"') ++start;
        int32 i=0;
        while(*start&&*start!=')'&&*start!='"'&&i<63) p.url[i++]=*start++;
        p.url[i]=0;
        return p;
    }

    if(str[0]=='#'){
        ++str;
        auto h=[](char c)->uint8{
            if(c>='0'&&c<='9')return (uint8)(c-'0');
            if(c>='a'&&c<='f')return (uint8)(c-'a'+10);
            if(c>='A'&&c<='F')return (uint8)(c-'A'+10);
            return 0;};
        const int32 len=(int32)NkStringView(str).Length();
        NkSVGPaint p; p.type=Type::Color;
        if(len>=8){
            p.r=static_cast<uint8>((h(str[0])<<4)|h(str[1]));
            p.g=static_cast<uint8>((h(str[2])<<4)|h(str[3]));
            p.b=static_cast<uint8>((h(str[4])<<4)|h(str[5]));
            p.a=static_cast<uint8>((h(str[6])<<4)|h(str[7]));
        } else if(len>=6){
            p.r=static_cast<uint8>((h(str[0])<<4)|h(str[1]));
            p.g=static_cast<uint8>((h(str[2])<<4)|h(str[3]));
            p.b=static_cast<uint8>((h(str[4])<<4)|h(str[5]));
            p.a=255;
        } else if(len>=3){
            p.r=static_cast<uint8>((h(str[0])<<4)|h(str[0]));
            p.g=static_cast<uint8>((h(str[1])<<4)|h(str[1]));
            p.b=static_cast<uint8>((h(str[2])<<4)|h(str[2]));
            p.a=255;
        }
        return p;
    }

    if(::strncmp(str,"rgb(",4)==0||::strncmp(str,"rgba(",5)==0){
        const bool hasA=(str[3]=='a');
        const char* p2=str+(hasA?5:4);
        int32 r=0,g=0,b=0; float a=1.f;
        // Supporte les % et les valeurs entières
        auto parseComp=[&](const char*& s)->float{
            while(*s==' ') ++s;
            float v=::strtof(s,(char**)&s);
            while(*s==' ') ++s;
            if(*s=='%'){++s;v=v*255.f/100.f;}
            while(*s==','||*s==' ') ++s;
            return v;
        };
        r=(int32)parseComp(p2); g=(int32)parseComp(p2); b=(int32)parseComp(p2);
        if(hasA) a=parseComp(p2);
        NkSVGPaint paint; paint.type=Type::Color;
        paint.r=static_cast<uint8>(r<0?0:r>255?255:r);
        paint.g=static_cast<uint8>(g<0?0:g>255?255:g);
        paint.b=static_cast<uint8>(b<0?0:b>255?255:b);
        paint.a=static_cast<uint8>(a<0?0:a>1?(int32)(a):(int32)(a*255));
        return paint;
    }

    if(::strncmp(str,"hsl(",4)==0){
        float hue=0,sat=0,lgt=0;
        ::sscanf(str+4,"%f,%f%%,%f%%",&hue,&sat,&lgt);
        sat/=100.f; lgt/=100.f;
        const float c=(1.f-NkFabs(2*lgt-1))*sat;
        const float x=c*(1.f-NkFabs(::fmodf(hue/60.f,2.f)-1.f));
        const float m=lgt-c/2.f;
        float r,g,b;
        if(hue<60){r=c;g=x;b=0;}else if(hue<120){r=x;g=c;b=0;}
        else if(hue<180){r=0;g=c;b=x;}else if(hue<240){r=0;g=x;b=c;}
        else if(hue<300){r=x;g=0;b=c;}else{r=c;g=0;b=x;}
        NkSVGPaint paint; paint.type=Type::Color;
        paint.r=static_cast<uint8>((r+m)*255);
        paint.g=static_cast<uint8>((g+m)*255);
        paint.b=static_cast<uint8>((b+m)*255);
        paint.a=255;
        return paint;
    }

    // Noms CSS
    for(int32 i=0;kCSSColors[i].name;++i){
        if(NkStringView(str)==NkStringView(kCSSColors[i].name)){
            NkSVGPaint p; p.type=Type::Color;
            p.r=(kCSSColors[i].rgb>>16)&0xFF;
            p.g=(kCSSColors[i].rgb>>8)&0xFF;
            p.b=kCSSColors[i].rgb&0xFF;
            p.a=255;
            return p;
        }
    }
    return NkSVGPaint::None();
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGMatrix
// ─────────────────────────────────────────────────────────────────────────────

NkSVGMatrix NkSVGMatrix::Rotate(float deg,float cx,float cy) noexcept {
    const float rad=deg*math::constants::kPiF/180.f;
    const float co=NkCos(rad), si=NkSin(rad);
    NkSVGMatrix r{co,si,-si,co,0,0};
    if(cx!=0||cy!=0){
        r.e=cx*(1-co)+cy*si;
        r.f=cy*(1-co)-cx*si;
    }
    return r;
}
NkSVGMatrix NkSVGMatrix::SkewX(float deg) noexcept {
    return {1,0,::tanf(deg*3.14159265f/180.f),1,0,0};
}
NkSVGMatrix NkSVGMatrix::SkewY(float deg) noexcept {
    return {1,::tanf(deg*3.14159265f/180.f),0,1,0,0};
}
NkSVGMatrix NkSVGMatrix::Inverse() const noexcept {
    const float det=a*d-b*c;
    if(NkFabs(det)<1e-10f) return Identity();
    const float id=1.f/det;
    return {d*id,-b*id,-c*id,a*id,(c*f-d*e)*id,(b*e-a*f)*id};
}

NkSVGMatrix NkSVGMatrix::Parse(const char* str) noexcept {
    NkSVGMatrix t=Identity();
    if(!str||!*str) return t;
    const char* p=str;
    auto skipWS=[&]{while(*p==' '||*p=='\t'||*p=='\n'||*p=='\r') ++p;};
    auto parseF=[&]()->float{
        skipWS(); char* end; float v=::strtof(p,&end);
        if(end>p) p=end; else ++p; skipWS();
        if(*p==','||*p==' ') ++p;
        return v;
    };

    while(*p){
        skipWS();
        if(!*p) break;
        if(::strncmp(p,"matrix(",7)==0){p+=7;
            float v[6]; for(int i=0;i<6;++i) v[i]=parseF();
            t=t*NkSVGMatrix{v[0],v[1],v[2],v[3],v[4],v[5]};
        } else if(::strncmp(p,"translate(",10)==0){p+=10;
            const float tx=parseF(), ty=(*p==')'||*p==',')?parseF():0;
            t=t*Translate(tx,ty);
        } else if(::strncmp(p,"scale(",6)==0){p+=6;
            const float sx=parseF();
            const float sy=(*p!=')')?parseF():sx;
            t=t*Scale(sx,sy);
        } else if(::strncmp(p,"rotate(",7)==0){p+=7;
            const float angle=parseF();
            float cx=0,cy=0;
            if(*p!=')'){cx=parseF();cy=parseF();}
            t=t*Rotate(angle,cx,cy);
        } else if(::strncmp(p,"skewX(",6)==0){p+=6;t=t*SkewX(parseF());}
        else if(::strncmp(p,"skewY(",6)==0){p+=6;t=t*SkewY(parseF());}
        // Avance jusqu'au prochain ')'
        while(*p&&*p!=')') ++p;
        if(*p==')') ++p;
        skipWS(); if(*p==',') ++p;
    }
    return t;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGViewBox
// ─────────────────────────────────────────────────────────────────────────────

NkSVGViewBox NkSVGViewBox::Parse(const char* str) noexcept {
    NkSVGViewBox vb; if(!str||!*str) return vb;
    const char* p=str;
    auto f=[&]()->float{
        while(*p==' '||*p==','||*p=='\t') ++p;
        char* e; float v=::strtof(p,&e);
        if(e>p) p=e; else ++p; return v;
    };
    vb.x=f(); vb.y=f(); vb.w=f(); vb.h=f();
    vb.valid=(vb.w>0&&vb.h>0);
    return vb;
}

NkSVGViewBox::Align NkSVGViewBox::ParseAlign(const char* str) noexcept {
    if(!str) return Align::XMidYMid;
    if(::strncmp(str,"none",4)==0) return Align::None;
    if(::strncmp(str,"xMinYMin",8)==0) return Align::XMinYMin;
    if(::strncmp(str,"xMidYMin",8)==0) return Align::XMidYMin;
    if(::strncmp(str,"xMaxYMin",8)==0) return Align::XMaxYMin;
    if(::strncmp(str,"xMinYMid",8)==0) return Align::XMinYMid;
    if(::strncmp(str,"xMidYMid",8)==0) return Align::XMidYMid;
    if(::strncmp(str,"xMaxYMid",8)==0) return Align::XMaxYMid;
    if(::strncmp(str,"xMinYMax",8)==0) return Align::XMinYMax;
    if(::strncmp(str,"xMidYMax",8)==0) return Align::XMidYMax;
    if(::strncmp(str,"xMaxYMax",8)==0) return Align::XMaxYMax;
    return Align::XMidYMid;
}

NkSVGMatrix NkSVGViewBox::ToMatrix(float vpW, float vpH) const noexcept {
    if(!valid||w<=0||h<=0) return NkSVGMatrix::Identity();
    const float sx=vpW/w, sy=vpH/h;
    float scale,tx=0,ty=0;
    if(align==Align::None){ scale=1; return NkSVGMatrix::Scale(sx,sy)*NkSVGMatrix::Translate(-x,-y); }
    scale=meet? ::fminf(sx,sy) : ::fmaxf(sx,sy);
    const float scaledW=w*scale, scaledH=h*scale;
    switch(align){
        case Align::XMinYMin: tx=0;              ty=0;              break;
        case Align::XMidYMin: tx=(vpW-scaledW)/2;ty=0;              break;
        case Align::XMaxYMin: tx=vpW-scaledW;    ty=0;              break;
        case Align::XMinYMid: tx=0;              ty=(vpH-scaledH)/2;break;
        case Align::XMidYMid: tx=(vpW-scaledW)/2;ty=(vpH-scaledH)/2;break;
        case Align::XMaxYMid: tx=vpW-scaledW;    ty=(vpH-scaledH)/2;break;
        case Align::XMinYMax: tx=0;              ty=vpH-scaledH;    break;
        case Align::XMidYMax: tx=(vpW-scaledW)/2;ty=vpH-scaledH;    break;
        case Align::XMaxYMax: tx=vpW-scaledW;    ty=vpH-scaledH;    break;
        default: tx=(vpW-scaledW)/2; ty=(vpH-scaledH)/2; break;
    }
    return NkSVGMatrix::Translate(tx,ty)*NkSVGMatrix::Scale(scale)*NkSVGMatrix::Translate(-x,-y);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Parsing des longueurs CSS
// ─────────────────────────────────────────────────────────────────────────────

float NkSVGStyle::ParseLength(const char* str, float vpW, float vpH,
                                float fontSize) noexcept
{
    if(!str||!*str) return 0.f;
    char* end; float v=::strtof(str,&end);
    if(end==str) return 0.f;
    const char* unit=end;
    while(*unit==' ') ++unit;
    if(*unit==0)          return v;           // px implicite
    if(NkStringView(unit)=="px")  return v;
    if(NkStringView(unit)=="pt")  return v*1.3333f;   // 1pt = 1.333px à 96dpi
    if(NkStringView(unit)=="mm")  return v*3.7795f;   // 1mm = 3.7795px à 96dpi
    if(NkStringView(unit)=="cm")  return v*37.795f;
    if(NkStringView(unit)=="in")  return v*96.f;
    if(NkStringView(unit)=="em")  return v*fontSize;
    if(NkStringView(unit)=="ex")  return v*fontSize*0.5f;
    if(NkStringView(unit)=="rem") return v*16.f;
    if(*unit=='%'){
        // Dépend du contexte — on utilise la diagonale pour les cas ambigus
        const float diag=NkSqrt(vpW*vpW+vpH*vpH)/NkSqrt(2.f);
        return v*diag/100.f;
    }
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGStyle::ApplyProperty — applique une propriété CSS à un style
// ─────────────────────────────────────────────────────────────────────────────

void NkSVGStyle::ApplyProperty(const char* prop, const char* value,
                                 NkSVGStyle& out, float vpW, float vpH) noexcept
{
    if(!prop||!value) return;
    // Trim value
    while(*value==' ') ++value;

    if(NkStringView(prop)=="fill"){
        out.fill=NkSVGPaint::Parse(value);
    } else if(NkStringView(prop)=="stroke"){
        out.stroke=NkSVGPaint::Parse(value);
    } else if(NkStringView(prop)=="stroke-width"){
        out.strokeWidth=ParseLength(value,vpW,vpH,out.fontSize);
    } else if(NkStringView(prop)=="stroke-miterlimit"){
        out.strokeMiterLimit=::strtof(value,nullptr);
    } else if(NkStringView(prop)=="opacity"){
        out.opacity=::strtof(value,nullptr);
    } else if(NkStringView(prop)=="fill-opacity"){
        out.fillOpacity=::strtof(value,nullptr);
    } else if(NkStringView(prop)=="stroke-opacity"){
        out.strokeOpacity=::strtof(value,nullptr);
    } else if(NkStringView(prop)=="fill-rule"){
        out.fillRule=(::strncmp(value,"evenodd",7)==0)?NkSVGFillRule::NK_EVEN_ODD:NkSVGFillRule::NK_NON_ZERO;
    } else if(NkStringView(prop)=="stroke-linecap"){
        if(NkStringView(value)=="round")  out.lineCap=NkSVGLineCap::NK_ROUND;
        else if(NkStringView(value)=="square") out.lineCap=NkSVGLineCap::NK_SQUARE;
        else out.lineCap=NkSVGLineCap::NK_BUTT;
    } else if(NkStringView(prop)=="stroke-linejoin"){
        if(NkStringView(value)=="round") out.lineJoin=NkSVGLineJoin::NK_ROUND;
        else if(NkStringView(value)=="bevel") out.lineJoin=NkSVGLineJoin::NK_BEVEL;
        else out.lineJoin=NkSVGLineJoin::NK_MITER;
    } else if(NkStringView(prop)=="stroke-dasharray"){
        if(NkStringView(value)=="none"){out.dashCount=0;return;}
        const char* p=value; out.dashCount=0;
        while(*p&&out.dashCount<8){
            while(*p==' '||*p==',') ++p;
            char* e; float v=::strtof(p,&e);
            if(e==p) break;
            out.dashArray[out.dashCount++]=v; p=e;
        }
    } else if(NkStringView(prop)=="stroke-dashoffset"){
        out.dashOffset=ParseLength(value,vpW,vpH,out.fontSize);
    } else if(NkStringView(prop)=="font-size"){
        out.fontSize=ParseLength(value,vpW,vpH,out.fontSize);
    } else if(NkStringView(prop)=="font-family"){
        ::strncpy(out.fontFamily,value,sizeof(out.fontFamily)-1);
    } else if(NkStringView(prop)=="font-weight"){
        if(NkStringView(value)=="bold"||NkStringView(value)=="bolder") out.fontWeight=700;
        else if(NkStringView(value)=="normal") out.fontWeight=400;
        else out.fontWeight=::strtof(value,nullptr);
    } else if(NkStringView(prop)=="display"){
        out.display=(NkStringView(value)=="none")?NkSVGDisplay::NK_NONE:NkSVGDisplay::NK_DISPLAY_INLINE;
    } else if(NkStringView(prop)=="visibility"){
        if(NkStringView(value)=="hidden")   out.visibility=NkSVGVisibility::NK_HIDDEN;
        else if(NkStringView(value)=="collapse") out.visibility=NkSVGVisibility::NK_COLLAPSE;
        else out.visibility=NkSVGVisibility::NK_VISIBLE;
    } else if(NkStringView(prop)=="clip-path"){
        // url(#id)
        if(::strncmp(value,"url(#",5)==0){
            const char* id=value+5; int32 i=0;
            while(*id&&*id!=')'&&i<63) out.clipPathId[i++]=*id++;
            out.clipPathId[i]=0;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGStyle::ParseInlineStyle
// ─────────────────────────────────────────────────────────────────────────────

void NkSVGStyle::ParseInlineStyle(const char* css, NkSVGStyle& out,
                                    float vpW, float vpH) noexcept
{
    if(!css||!*css) return;
    char prop[64], val[256];
    const char* p=css;
    while(*p){
        while(*p==' '||*p=='\t'||*p=='\n') ++p;
        int32 pi=0; while(*p&&*p!=':'&&*p!=';') { if(pi<63) prop[pi++]=*p; ++p; }
        prop[pi]=0;
        // Trim trailing spaces from prop
        while(pi>0&&prop[pi-1]==' ') prop[--pi]=0;
        if(*p==':') ++p;
        while(*p==' ') ++p;
        int32 vi=0; while(*p&&*p!=';') { if(vi<255) val[vi++]=*p; ++p; }
        val[vi]=0;
        while(vi>0&&(val[vi-1]==' '||val[vi-1]=='\t')) val[--vi]=0;
        if(*p==';') ++p;
        if(prop[0]) ApplyProperty(prop,val,out,vpW,vpH);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGStyle::Compute — cascade CSS complète
// ─────────────────────────────────────────────────────────────────────────────

NkSVGStyle NkSVGStyle::Compute(const NkSVGStyle& parent,
                                  NkXMLNode* node,
                                  float vpW, float vpH) noexcept
{
    NkSVGStyle s=parent; // hérite tout
    if(!node) return s;

    // 1. Attributs de présentation (faible priorité)
    static const char* kPresentationAttrs[]={
        "fill","stroke","stroke-width","stroke-miterlimit","stroke-linecap",
        "stroke-linejoin","stroke-dasharray","stroke-dashoffset",
        "fill-opacity","stroke-opacity","opacity","fill-rule",
        "display","visibility","font-size","font-family","font-weight",
        "clip-path",nullptr
    };
    for(int32 i=0;kPresentationAttrs[i];++i){
        const char* v=node->GetAttr(kPresentationAttrs[i]);
        if(v) ApplyProperty(kPresentationAttrs[i],v,s,vpW,vpH);
    }

    // 2. Attribut style= (priorité haute)
    const char* styleAttr=node->GetAttr("style");
    if(styleAttr) ParseInlineStyle(styleAttr,s,vpW,vpH);

    // 3. Résolution inherit
    // (déjà géré par l'héritage depuis parent — si "inherit" est explicite on recopie)
    if(s.fill.IsInherit())   s.fill   = parent.fill;
    if(s.stroke.IsInherit()) s.stroke = parent.stroke;

    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGGradient::Sample
// ─────────────────────────────────────────────────────────────────────────────

void NkSVGGradient::Sample(float t, uint8& R,uint8& G,uint8& B,uint8& A) const noexcept {
    if(!firstStop||numStops==0){R=G=B=0;A=255;return;}
    // Gestion du spread method
    if(spread==NkSVGSpreadMethod::NK_REPEAT){
        t=t-NkFloor(t);
    } else if(spread==NkSVGSpreadMethod::NK_REFLECT){
        t=t-NkFloor(t);
        if(static_cast<int32>(NkFloor(t/0.5f))&1) t=1.f-t;
        t=t-NkFloor(t);
    } else {
        if(t<0) t=0; if(t>1) t=1;
    }

    // Trouve les deux stops encadrants
    NkSVGGradientStop* s0=firstStop, *s1=firstStop;
    NkSVGGradientStop* cur=firstStop;
    while(cur){
        if(cur->offset<=t) s0=cur;
        if(cur->offset>=t&&s1==firstStop) s1=cur;
        cur=cur->next;
    }
    if(s0->offset==s1->offset){
        R=s0->color.r; G=s0->color.g; B=s0->color.b;
        A=static_cast<uint8>(s0->color.a*s0->opacity);
        return;
    }
    // Interpolation linéaire
    const float range=s1->offset-s0->offset;
    const float alpha=(range>0)?(t-s0->offset)/range:0.f;
    R=static_cast<uint8>(s0->color.r*(1-alpha)+s1->color.r*alpha);
    G=static_cast<uint8>(s0->color.g*(1-alpha)+s1->color.g*alpha);
    B=static_cast<uint8>(s0->color.b*(1-alpha)+s1->color.b*alpha);
    const float a0=s0->color.a/255.f*s0->opacity;
    const float a1=s1->color.a/255.f*s1->opacity;
    A=static_cast<uint8>((a0*(1-alpha)+a1*alpha)*255);
}

float NkSVGGradient::LinearT(float x, float y) const noexcept {
    const float dx=x2-x1, dy=y2-y1;
    const float len2=dx*dx+dy*dy;
    if(len2<1e-10f) return 0.f;
    return ((x-x1)*dx+(y-y1)*dy)/len2;
}

float NkSVGGradient::RadialT(float x, float y) const noexcept {
    // Distance au focal point normalisée par le rayon
    const float dx=x-fx, dy=y-fy;
    return NkSqrt(dx*dx+dy*dy)/r;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGDefs
// ─────────────────────────────────────────────────────────────────────────────

void NkSVGDefs::RegisterGradient(const char* id, NkSVGGradient* g) noexcept {
    if(!id||!g||!arena) return;
    const uint32 h=Hash(id);
    GradEntry* e=arena->Alloc<GradEntry>();
    if(!e) return;
    ::strncpy(e->id,id,63); e->grad=g;
    e->next=grads[h]; grads[h]=e;
}
void NkSVGDefs::RegisterClipPath(const char* id, NkSVGClipPath* c) noexcept {
    if(!id||!c||!arena) return;
    const uint32 h=Hash(id);
    ClipEntry* e=arena->Alloc<ClipEntry>();
    if(!e) return;
    ::strncpy(e->id,id,63); e->clip=c;
    e->next=clips[h]; clips[h]=e;
}
void NkSVGDefs::RegisterElement(const char* id, NkXMLNode* node) noexcept {
    if(!id||!node||!arena) return;
    const uint32 h=Hash(id);
    ElemEntry* e=arena->Alloc<ElemEntry>();
    if(!e) return;
    ::strncpy(e->id,id,63); e->node=node;
    e->next=elems[h]; elems[h]=e;
}
NkSVGGradient* NkSVGDefs::FindGradient(const char* id) const noexcept {
    if(!id) return nullptr;
    for(GradEntry* e=grads[Hash(id)];e;e=e->next)
        if(NkStringView(e->id)==NkStringView(id)) return e->grad;
    return nullptr;
}
NkSVGClipPath* NkSVGDefs::FindClipPath(const char* id) const noexcept {
    if(!id) return nullptr;
    for(ClipEntry* e=clips[Hash(id)];e;e=e->next)
        if(NkStringView(e->id)==NkStringView(id)) return e->clip;
    return nullptr;
}
NkXMLNode* NkSVGDefs::FindElement(const char* id) const noexcept {
    if(!id) return nullptr;
    for(ElemEntry* e=elems[Hash(id)];e;e=e->next)
        if(NkStringView(e->id)==NkStringView(id)) return e->node;
    return nullptr;
}

void NkSVGDefs::ResolveHrefs() noexcept {
    // Résout les gradients qui héritent d'un autre via href="#id"
    for(int32 i=0;i<HASH_SIZE;++i){
        for(GradEntry* e=grads[i];e;e=e->next){
            NkSVGGradient* g=e->grad;
            if(!g->href[0]) continue;
            NkSVGGradient* parent=FindGradient(g->href);
            if(!parent) continue;
            // Hérite des stops si g n'en a pas
            if(g->numStops==0){
                g->firstStop=parent->firstStop;
                g->numStops=parent->numStops;
            }
            // Hérite de la géométrie si non spécifiée
            if(g->type==NkSVGGradientType::NK_LINEAR&&g->x1==0&&g->y1==0&&g->x2==0&&g->y2==0){
                g->x1=parent->x1; g->y1=parent->y1;
                g->x2=parent->x2; g->y2=parent->y2;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGDOM
// ─────────────────────────────────────────────────────────────────────────────

void NkSVGDOM::Init(usize arenaBytes) noexcept {
    arena.Init(arenaBytes);
    xmlDoc.Init(arenaBytes/2);
    defs.Init(&arena);
    isValid=false;
}
void NkSVGDOM::Destroy() noexcept {
    xmlDoc.Destroy();
    arena.Destroy();
    isValid=false;
}

static const char* ExtractURL(const char* ref) noexcept {
    if(!ref) return nullptr;
    if(::strncmp(ref,"url(#",5)==0){
        static char buf[64];
        const char* p=ref+5; int32 i=0;
        while(*p&&*p!=')'&&i<63) buf[i++]=*p++;
        buf[i]=0; return buf;
    }
    if(ref[0]=='#') return ref+1;
    return ref;
}

NkSVGGradient* NkSVGDOM::FindGradient(const char* ref) const noexcept {
    const char* id=ExtractURL(ref); if(!id) return nullptr;
    return defs.FindGradient(id);
}
NkSVGClipPath* NkSVGDOM::FindClipPath(const char* ref) const noexcept {
    const char* id=ExtractURL(ref); if(!id) return nullptr;
    return defs.FindClipPath(id);
}
NkXMLNode* NkSVGDOM::FindDefElement(const char* id) const noexcept {
    return defs.FindElement(id);
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGDOMBuilder — helpers
// ─────────────────────────────────────────────────────────────────────────────

void NkSVGDOMBuilder::AppendChild(NkSVGElement* parent, NkSVGElement* child) noexcept {
    child->parent=parent;
    if(!parent->firstChild) parent->firstChild=parent->lastChild=child;
    else { child->prevSibling=parent->lastChild; parent->lastChild->nextSibling=child; parent->lastChild=child; }
}

// Parse une valeur SVG avec unités
static float ParseSVGLength(const char* str, float vpW, float vpH, float fs=16.f) noexcept {
    return NkSVGStyle::ParseLength(str,vpW,vpH,fs);
}

void NkSVGDOMBuilder::ParseGeometry(NkXMLNode* node, NkSVGElement* elem,
                                      float vpW, float vpH) noexcept
{
    if(!node||!elem) return;
    auto f=[&](const char* n,float def=0.f)->float{
        const char* v=node->GetAttr(n);
        return v?ParseSVGLength(v,vpW,vpH):def;
    };
    auto s=[&](const char* n,char* dst,int32 dstLen){
        const char* v=node->GetAttr(n);
        if(v) ::strncpy(dst,v,dstLen-1);
    };
    const char* tag=node->localName;
    if(!tag) return;

    if(NkStringView(tag)=="rect"){
        elem->x=f("x");elem->y=f("y");elem->width=f("width");elem->height=f("height");
        elem->rx=f("rx");elem->ry=f("ry");
        if(elem->rx>0&&elem->ry==0) elem->ry=elem->rx;
        if(elem->ry>0&&elem->rx==0) elem->rx=elem->ry;
    } else if(NkStringView(tag)=="circle"){
        elem->cx=f("cx");elem->cy=f("cy");elem->r=f("r");
    } else if(NkStringView(tag)=="ellipse"){
        elem->cx=f("cx");elem->cy=f("cy");elem->rx2=f("rx");elem->ry2=f("ry");
    } else if(NkStringView(tag)=="line"){
        elem->x1=f("x1");elem->y1=f("y1");elem->x2=f("x2");elem->y2=f("y2");
    } else if(NkStringView(tag)=="polyline"||NkStringView(tag)=="polygon"){
        s("points",elem->points,sizeof(elem->points));
    } else if(NkStringView(tag)=="path"){
        s("d",elem->d,sizeof(elem->d));
    } else if(NkStringView(tag)=="svg"||NkStringView(tag)=="symbol"||
              NkStringView(tag)=="image"||NkStringView(tag)=="use"){
        elem->x=f("x");elem->y=f("y");
        // width/height en % ou valeur absolue
        const char* wv=node->GetAttr("width");
        const char* hv=node->GetAttr("height");
        if(wv){
            char* e; float v=::strtof(wv,&e);
            elem->width=(*e=='%')?v*vpW/100.f:ParseSVGLength(wv,vpW,vpH);
        }
        if(hv){
            char* e; float v=::strtof(hv,&e);
            elem->height=(*e=='%')?v*vpH/100.f:ParseSVGLength(hv,vpW,vpH);
        }
    } else if(NkStringView(tag)=="text"){
        elem->x=f("x");elem->y=f("y");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  BuildDefs — parse <defs> et enregistre les ressources
// ─────────────────────────────────────────────────────────────────────────────

void NkSVGDOMBuilder::BuildGradient(NkXMLNode* node, BuildCtx& ctx) noexcept {
    NkSVGGradient* g=ctx.arena->Alloc<NkSVGGradient>();
    if(!g) return;
    const char* tag=node->localName;
    g->type=(tag&&NkStringView(tag)=="radialGradient")?
            NkSVGGradientType::NK_RADIAL:NkSVGGradientType::NK_LINEAR;

    // ID
    const char* id=node->GetAttr("id");
    if(id) ::strncpy(g->id,id,63);

    // href / xlink:href
    const char* href=node->GetAttr("xlink:href");
    if(!href) href=node->GetAttr("href");
    if(href){
        const char* ref=(href[0]=='#')?href+1:href;
        ::strncpy(g->href,ref,63);
    }

    // gradientUnits
    const char* gu=node->GetAttr("gradientUnits");
    g->units=(gu&&NkStringView(gu)=="userSpaceOnUse")?
             NkSVGGradientUnits::NK_USER_SPACE:NkSVGGradientUnits::NK_OBJECT_BOUNDING_BOX;

    // spreadMethod
    const char* sm=node->GetAttr("spreadMethod");
    if(sm){
        if(NkStringView(sm)=="reflect") g->spread=NkSVGSpreadMethod::NK_REFLECT;
        else if(NkStringView(sm)=="repeat") g->spread=NkSVGSpreadMethod::NK_REPEAT;
    }

    // gradientTransform
    const char* gt=node->GetAttr("gradientTransform");
    if(gt) g->transform=NkSVGMatrix::Parse(gt);

    // Géométrie
    auto gf=[&](const char* n,float def)->float{
        const char* v=node->GetAttr(n); return v?::strtof(v,nullptr):def;};
    if(g->type==NkSVGGradientType::NK_LINEAR){
        const char* v=node->GetAttr("x1"); if(v){char*e;float f=::strtof(v,&e);g->x1=(*e=='%')?f/100.f:f;}else g->x1=0.f;
        const char* v2=node->GetAttr("y1");if(v2){char*e;float f=::strtof(v2,&e);g->y1=(*e=='%')?f/100.f:f;}else g->y1=0.f;
        const char* v3=node->GetAttr("x2");if(v3){char*e;float f=::strtof(v3,&e);g->x2=(*e=='%')?f/100.f:f;}else g->x2=1.f;
        const char* v4=node->GetAttr("y2");if(v4){char*e;float f=::strtof(v4,&e);g->y2=(*e=='%')?f/100.f:f;}else g->y2=0.f;
    } else {
        const char* v=node->GetAttr("cx");if(v){char*e;float f=::strtof(v,&e);g->cx=(*e=='%')?f/100.f:f;}else g->cx=0.5f;
        const char* v2=node->GetAttr("cy");if(v2){char*e;float f=::strtof(v2,&e);g->cy=(*e=='%')?f/100.f:f;}else g->cy=0.5f;
        const char* vr=node->GetAttr("r");if(vr){char*e;float f=::strtof(vr,&e);g->r=(*e=='%')?f/100.f:f;}else g->r=0.5f;
        const char* vfx=node->GetAttr("fx");g->fx=vfx?gf("fx",g->cx):g->cx;
        const char* vfy=node->GetAttr("fy");g->fy=vfy?gf("fy",g->cy):g->cy;
    }
    (void)gf;

    // Stops
    NkSVGGradientStop* lastStop=nullptr;
    for(NkXMLNode* c=node->firstChild;c;c=c->nextSibling){
        if(c->type!=NkXMLNodeType::NK_ELEMENT||!c->localName) continue;
        if(NkStringView(c->localName)!="stop") continue;
        NkSVGGradientStop* stop=ctx.arena->Alloc<NkSVGGradientStop>();
        if(!stop) break;
        // offset
        const char* ov=c->GetAttr("offset");
        if(ov){ char*e;float f=::strtof(ov,&e); stop->offset=(*e=='%')?f/100.f:f; }
        // stop-color et stop-opacity depuis style= ou attributs
        NkSVGStyle ss; ss.fill=NkSVGPaint::Black(); ss.fillOpacity=1.f;
        const char* styleStr=c->GetAttr("style");
        const char* scAttr=c->GetAttr("stop-color");
        const char* soAttr=c->GetAttr("stop-opacity");
        if(styleStr) NkSVGStyle::Compute(ss,c,ctx.vpW,ctx.vpH); // réutilise la cascade
        if(scAttr) stop->color=NkSVGPaint::Parse(scAttr);
        else { const char* v=c->GetAttr("stop-color"); if(v) stop->color=NkSVGPaint::Parse(v); }
        stop->opacity=soAttr?::strtof(soAttr,nullptr):1.f;
        (void)styleStr; // déjà traité
        if(!g->firstStop) g->firstStop=lastStop=stop;
        else { lastStop->next=stop; lastStop=stop; }
        ++g->numStops;
    }

    if(id) ctx.defs->RegisterGradient(id,g);
}

void NkSVGDOMBuilder::BuildClipPath(NkXMLNode* node, BuildCtx& ctx) noexcept {
    const char* id=node->GetAttr("id");
    if(!id) return;
    NkSVGClipPath* cp=ctx.arena->Alloc<NkSVGClipPath>();
    if(!cp) return;
    ::strncpy(cp->id,id,63);
    cp->node=node;
    ctx.defs->RegisterClipPath(id,cp);
}

void NkSVGDOMBuilder::BuildDefs(NkXMLNode* defsNode, BuildCtx& ctx) noexcept {
    for(NkXMLNode* c=defsNode->firstChild;c;c=c->nextSibling){
        if(c->type!=NkXMLNodeType::NK_ELEMENT||!c->localName) continue;
        const char* tag=c->localName;
        if(NkStringView(tag)=="linearGradient"||NkStringView(tag)=="radialGradient")
            BuildGradient(c,ctx);
        else if(NkStringView(tag)=="clipPath")
            BuildClipPath(c,ctx);
        // Tout élément avec un id peut être référencé par <use>
        const char* id=c->GetAttr("id");
        if(id) ctx.defs->RegisterElement(id,c);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  BuildElement — récursif
// ─────────────────────────────────────────────────────────────────────────────

NkSVGElement* NkSVGDOMBuilder::BuildElement(NkXMLNode* node, NkSVGElement* parent,
                                              BuildCtx& ctx) noexcept
{
    if(!node||node->type!=NkXMLNodeType::NK_ELEMENT) return nullptr;
    const char* tag=node->localName;
    if(!tag) return nullptr;

    // <defs> : traitement spécial
    if(NkStringView(tag)=="defs"){
        BuildCtx defsCtx=ctx; defsCtx.inDefs=true;
        BuildDefs(node,defsCtx);
        // Enregistre aussi les éléments avec id directement dans le corps
        return nullptr; // les defs ne sont pas rendus
    }

    // Ignore les éléments non-rendus
    if(NkStringView(tag)=="title"||NkStringView(tag)=="desc"||
       NkStringView(tag)=="metadata") return nullptr;

    NkSVGElement* elem=AllocElem(*ctx.arena);
    if(!elem) return nullptr;
    elem->xmlNode=node;
    elem->inDefs=ctx.inDefs;

    // Calcule le style en cascade
    elem->style=NkSVGStyle::Compute(ctx.currentStyle,node,ctx.vpW,ctx.vpH);

    // Calcule la transformation locale
    const char* tfStr=node->GetAttr("transform");
    elem->localTransform=tfStr?NkSVGMatrix::Parse(tfStr):NkSVGMatrix::Identity();
    elem->ctm=ctx.currentCTM*elem->localTransform;

    // Parse le viewBox (pour svg, symbol, marker)
    const char* vbStr=node->GetAttr("viewBox");
    if(vbStr) elem->viewBox=NkSVGViewBox::Parse(vbStr);
    const char* parStr=node->GetAttr("preserveAspectRatio");
    if(parStr&&elem->viewBox.valid){
        // Parse "xMidYMid meet" ou "xMidYMid slice"
        const char* meet=::strstr(parStr,"meet");
        const char* slice=::strstr(parStr,"slice");
        elem->viewBox.meet=!slice||(meet&&meet<slice);
        elem->viewBox.align=NkSVGViewBox::ParseAlign(parStr);
    }

    // Parse la géométrie spécifique à l'élément
    ParseGeometry(node,elem,ctx.vpW,ctx.vpH);

    // Enregistre dans les defs si id présent (pour <use>)
    const char* id=node->GetAttr("id");
    if(id) ctx.defs->RegisterElement(id,node);

    // Attache au parent
    if(parent) AppendChild(parent,elem);

    // Contexte pour les enfants
    BuildCtx childCtx=ctx;
    childCtx.currentStyle=elem->style;
    childCtx.currentCTM=elem->ctm;

    // Ajuste le viewport pour svg/symbol imbriqués
    if(NkStringView(tag)=="svg"||NkStringView(tag)=="symbol"){
        if(elem->width>0) childCtx.vpW=elem->width;
        if(elem->height>0) childCtx.vpH=elem->height;
        // Applique le viewBox si présent
        if(elem->viewBox.valid){
            const NkSVGMatrix vbm=elem->viewBox.ToMatrix(childCtx.vpW,childCtx.vpH);
            childCtx.currentCTM=childCtx.currentCTM*vbm;
        }
    }

    // <use> : instancie l'élément référencé
    if(NkStringView(tag)=="use"){
        const char* href=node->GetAttr("xlink:href");
        if(!href) href=node->GetAttr("href");
        if(href){
            const char* refId=(href[0]=='#')?href+1:href;
            NkXMLNode* target=ctx.defs->FindElement(refId);
            if(target){
                // Crée un enfant virtuel avec une translation supplémentaire
                BuildCtx useCtx=childCtx;
                const NkSVGMatrix trans=NkSVGMatrix::Translate(elem->x,elem->y);
                useCtx.currentCTM=childCtx.currentCTM*trans;
                BuildElement(target,elem,useCtx);
            }
        }
    }

    // Récurse sur les enfants
    for(NkXMLNode* c=node->firstChild;c;c=c->nextSibling)
        BuildElement(c,elem,childCtx);

    return elem;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ParseSVGRoot — dimensions de la racine <svg>
// ─────────────────────────────────────────────────────────────────────────────

void NkSVGDOMBuilder::ParseSVGRoot(NkXMLNode* svg, NkSVGDOM& dom) noexcept {
    // viewBox
    const char* vbStr=svg->GetAttr("viewBox");
    if(vbStr) dom.viewBox=NkSVGViewBox::Parse(vbStr);

    // Dimensions explicites
    const char* wStr=svg->GetAttr("width");
    const char* hStr=svg->GetAttr("height");

    float svgW=dom.viewBox.valid?dom.viewBox.w:800.f;
    float svgH=dom.viewBox.valid?dom.viewBox.h:600.f;

    if(wStr){ char*e;float v=::strtof(wStr,&e); if(*e=='%') v=v/100.f*dom.outputW; svgW=(v>0)?v:svgW; }
    if(hStr){ char*e;float v=::strtof(hStr,&e); if(*e=='%') v=v/100.f*dom.outputH; svgH=(v>0)?v:svgH; }

    dom.viewportW=svgW;
    dom.viewportH=svgH;
    if(dom.outputW<=0) dom.outputW=svgW;
    if(dom.outputH<=0) dom.outputH=svgH;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSVGDOMBuilder::Build
// ─────────────────────────────────────────────────────────────────────────────

bool NkSVGDOMBuilder::Build(NkXMLDocument& xmlDoc, NkSVGDOM& dom,
                              float outW, float outH) noexcept
{
    if(!xmlDoc.isValid||!xmlDoc.docElement) return false;

    // Trouve l'élément <svg> racine
    NkXMLNode* svgNode=xmlDoc.docElement;
    // Parfois le document commence par une déclaration : cherche <svg>
    if(!svgNode->Is("svg")){
        NkXMLNode* found=nullptr;
        xmlDoc.ForEach([](NkXMLNode* n, void* ud)->bool{
            if(n->type==NkXMLNodeType::NK_ELEMENT&&n->Is("svg")){
                *static_cast<NkXMLNode**>(ud)=n; return false;}
            return true;},&found);
        if(!found) return false;
        svgNode=found;
    }

    dom.outputW=outW; dom.outputH=outH;
    ParseSVGRoot(svgNode,dom);

    // Premier scan pour les <defs> globaux (avant de construire l'arbre visuel)
    for(NkXMLNode* c=svgNode->firstChild;c;c=c->nextSibling){
        if(c->type==NkXMLNodeType::NK_ELEMENT&&c->Is("defs")){
            BuildCtx preCtx;
            preCtx.defs=&dom.defs; preCtx.arena=&dom.arena;
            preCtx.vpW=dom.viewportW; preCtx.vpH=dom.viewportH;
            preCtx.inDefs=true;
            BuildDefs(c,preCtx);
        }
    }

    // Résout les hrefs entre gradients
    dom.defs.ResolveHrefs();

    // Construit l'arbre visuel
    BuildCtx ctx;
    ctx.defs=&dom.defs; ctx.arena=&dom.arena;
    ctx.vpW=dom.viewportW; ctx.vpH=dom.viewportH;
    ctx.inDefs=false;
    // Style par défaut
    ctx.currentStyle.fill=NkSVGPaint::Black();
    ctx.currentStyle.stroke=NkSVGPaint::None();
    ctx.currentStyle.strokeWidth=1.f;
    ctx.currentStyle.opacity=1.f;
    ctx.currentStyle.fillOpacity=1.f;
    ctx.currentStyle.strokeOpacity=1.f;
    ctx.currentStyle.fontSize=16.f;
    ctx.currentStyle.display=NkSVGDisplay::NK_DISPLAY_INLINE;
    ctx.currentStyle.visibility=NkSVGVisibility::NK_VISIBLE;

    // CTM racine : applique viewBox si présent
    ctx.currentCTM=NkSVGMatrix::Identity();
    if(dom.viewBox.valid){
        ctx.currentCTM=dom.viewBox.ToMatrix(dom.outputW,dom.outputH);
    } else if(dom.outputW>0&&dom.viewportW>0&&dom.outputW!=dom.viewportW){
        const float sx=dom.outputW/dom.viewportW, sy=dom.outputH/dom.viewportH;
        ctx.currentCTM=NkSVGMatrix::Scale(sx,sy);
    }

    dom.root=BuildElement(svgNode,nullptr,ctx);
    dom.isValid=(dom.root!=nullptr);
    return dom.isValid;
}

bool NkSVGDOMBuilder::BuildFromXML(const uint8* xml, usize size, NkSVGDOM& dom,
                                     float outW, float outH) noexcept
{
    if(!NkXMLParser::Parse(xml,size,dom.xmlDoc)) return false;
    return Build(dom.xmlDoc,dom,outW,outH);
}

bool NkSVGDOMBuilder::BuildFromFile(const char* path, NkSVGDOM& dom,
                                      float outW, float outH) noexcept
{
    if(!NkXMLParser::ParseFile(path,dom.xmlDoc)) return false;
    return Build(dom.xmlDoc,dom,outW,outH);
}

} // namespace nkentseu
