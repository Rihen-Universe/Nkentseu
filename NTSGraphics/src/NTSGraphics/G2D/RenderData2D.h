//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-16 at 11:50:30 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __RENDER_DATA2_D_H__
#define __RENDER_DATA2_D_H__

#pragma once

#include <NTSCore/System.h>
#include "Geometry2D.h"
#include "NTSMaths/Color.h"
#include "NTSGraphics/Core/Texture.h"

namespace nkentseu {

    ENUMERATION(RenderMode2D, uint32,
        ENUM_TO_STRING_BEGIN
        ENUM_TO_STRING_ADD_CONTENT(Enum::RenderColor)
        ENUM_TO_STRING_ADD_CONTENT(Enum::RenderTexture)
        ENUM_TO_STRING_END(Enum::RenderColor),
        ,
        RenderColor = 1 << 0,  // 0x01
        RenderTexture = 1 << 1,  // 0x02
        RenderBoth = RenderColor | RenderTexture,  // 0x03
    );

    ENUMERATION(BlendMode2D, uint32,
        ENUM_TO_STRING_BEGIN
        ENUM_TO_STRING_SET_CONTENT(Enum::AlphaBlending)
        ENUM_TO_STRING_SET_CONTENT(Enum::AdditiveBlending)
        ENUM_TO_STRING_SET_CONTENT(Enum::MultiplicativeBlending)
        ENUM_TO_STRING_SET_CONTENT(Enum::SubtractiveBlending)
        ENUM_TO_STRING_SET_CONTENT(Enum::MinBlending)
        ENUM_TO_STRING_SET_CONTENT(Enum::MaxBlending)
        ENUM_TO_STRING_SET_CONTENT(Enum::Zero)
        ENUM_TO_STRING_SET_CONTENT(Enum::One)
        ENUM_TO_STRING_SET_CONTENT(Enum::SrcColor)
        ENUM_TO_STRING_SET_CONTENT(Enum::OneMinusSrcColor)
        ENUM_TO_STRING_SET_CONTENT(Enum::DstColor)
        ENUM_TO_STRING_SET_CONTENT(Enum::OneMinusDstColor)
        ENUM_TO_STRING_SET_CONTENT(Enum::DstAlpha)
        ENUM_TO_STRING_SET_CONTENT(Enum::OneMinusDstAlpha)
        ENUM_TO_STRING_SET_CONTENT(Enum::SrcAlphaSaturate)
        ENUM_TO_STRING_SET_CONTENT(Enum::ConstantColor)
        ENUM_TO_STRING_SET_CONTENT(Enum::OneMinusConstantColor)
        ENUM_TO_STRING_SET_CONTENT(Enum::ConstantAlpha)
        ENUM_TO_STRING_SET_CONTENT(Enum::OneMinusConstantAlpha)
        ENUM_TO_STRING_END(Enum::None),
        ,
        None,
        AlphaBlending,
        AdditiveBlending,
        MultiplicativeBlending,
        SubtractiveBlending,
        MinBlending,
        MaxBlending,
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstColor,
        OneMinusDstColor,
        DstAlpha,
        OneMinusDstAlpha,
        SrcAlphaSaturate,
        ConstantColor,
        OneMinusConstantColor,
        ConstantAlpha,
        OneMinusConstantAlpha
    );
    
    struct NKENTSEU_API RenderData2D {
        virtual Geometry2D GetGeometry(uint32 currentVertexCount, uint32 currentIndexCount) const = 0;
        virtual ~RenderData2D() {}
    };

    enum class BorderType {
        None,
        Outer,
        Inner,
        Both
    };

}  //  nkentseu

#endif  // __RENDER_DATA2_D_H__!