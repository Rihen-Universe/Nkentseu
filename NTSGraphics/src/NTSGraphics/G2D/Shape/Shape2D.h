//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-19 at 08:13:44 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __SHAPE2_D_H__
#define __SHAPE2_D_H__

#pragma once

#include <NTSCore/System.h>
#include <NTSMaths/Matrix/Matrix4.h>

#include "NTSGraphics/Core/GraphicsEnum.h"
#include <NTSGraphics/Core/Texture.h>

#include "NTSGraphics/G2D/Geometry2D.h"
#include "NTSGraphics/G2D/RenderCommand.h"

namespace nkentseu {
    
    class NKENTSEU_API Shape2D {
        public:
            virtual maths::Vector2f LeftTopCorner() = 0;
            virtual maths::Vector2f BoundSize() = 0;
            virtual const RenderPrimitive GetPrimitive() = 0;
            virtual Memory::Shared<Texture2D> GetTexture() = 0;
            virtual ClipRegion GetClipRegion() = 0;

            void AddVertice(Vertex2D vertice);
            void AddVertices(const std::vector<Vertex2D>& vertices);
            Vertex2D GetVertice(uint32 index);
            const std::vector<Vertex2D>& GetVertices();
            void AddIndice(uint32 indice);
            void AddIndices(const std::vector<uint32>& indices);
            uint32 GetIndice(uint32 index);
            const std::vector<uint32>& GetIndices();
        protected:
            std::vector<Vertex2D> vertices; // Vertex de la géométrie 2D
            std::vector<uint32> indices; // Indices de la géométrie 2D
    };

}  //  nkentseu

#endif  // __SHAPE2_D_H__!