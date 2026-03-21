//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/5/2024 at 10:02:13 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_GRAPHICSPROPERTIES_H__
#define __NKENTSEU_GRAPHICSPROPERTIES_H__

#pragma once

#include "NTSCore/System.h"
#include <NTSMaths/Vector/Vector2.h>

#include "GraphicsEnum.h"

namespace nkentseu {
    
    struct NKENTSEU_API GraphicsInfos {
        std::string vendor = "";
        std::string renderer = "";
        std::string version = "";
        std::string langageVersion = "";
        std::string extension = "";
        std::string profilMask = "";

        std::string ToString() const;

        friend std::string ToString(const GraphicsInfos& gi) {
            return gi.ToString();
        }

        // GraphicsInfos
        GraphicsInfos& operator=(const GraphicsInfos& other) = default;
        GraphicsInfos(const GraphicsInfos& other) = default;
        GraphicsInfos() = default;
    };

    struct NKENTSEU_API GraphicsPixelFormat {
        uint32 redColorBits = 8;
        uint32 greenColorBits = 8;
        uint32 blueColorBits = 8;
        uint32 alphaColorBits = 8;
        uint32 bitsPerPixel = 4;
        uint32 depthBits = 24; // 24
        uint32 stencilBits = 8;
        uint32 antialiasingLevel = 8;
        uint32 accumBits = 0;
        uint32 auxBuffers = 0;
        bool   pbuffer;

        //uint32 attributeFlags = (uint32)GraphicsAttribute::AttributeDefault;
        uint32 attributeFlags = (GraphicsAttribute)GraphicsAttribute::Enum::AttributeCore | (GraphicsAttribute)GraphicsAttribute::Enum::AttributeDebug;
        bool   sRgb = false;

        GraphicsFlag flags = (GraphicsFlag)GraphicsFlag::Enum::DrawToWindow | (GraphicsFlag)GraphicsFlag::Enum::DoubleBuffer | (GraphicsFlag)GraphicsFlag::Enum::SupportOpenGL;
        GraphicsMainLayer mainLayer = GraphicsMainLayer::Enum::MainPlane;
        GraphicsPixelType pixelType = GraphicsPixelType::Enum::RgbaArb;

        // GraphicsPixelFormat
        GraphicsPixelFormat& operator=(const GraphicsPixelFormat& other) = default;
        GraphicsPixelFormat(const GraphicsPixelFormat& other) = default;
        GraphicsPixelFormat() = default;
    };

    struct NKENTSEU_API ContextProperties {
        maths::Vector2u offScreenSize;
        GraphicsPixelFormat pixelFormat;
        maths::Vector2i version = InitVersion(GraphicsApiType::Enum::VulkanApi);
        GraphicsApiType graphicsApi = GraphicsApiType::Enum::VulkanApi;

        ContextProperties(const GraphicsApiType& api);
        ContextProperties(const GraphicsApiType& api, const maths::Vector2i& version);

        // ContextProperties
        ContextProperties& operator=(const ContextProperties& other);
        //ContextProperties(const ContextProperties& other) = default;

        ContextProperties();

        ContextProperties(maths::Vector2u size, GraphicsPixelFormat format);

        ContextProperties(GraphicsPixelFormat format);

        ContextProperties(const ContextProperties& properties);

        static maths::Vector2i InitVersion(const GraphicsApiType& api);
    };
} // namespace nkentseu

#endif    // __NKENTSEU_GRAPHICSPROPERTIES_H__