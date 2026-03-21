//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-16 at 03:06:30 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __RENDER_CACHE2_D_H__
#define __RENDER_CACHE2_D_H__

#pragma once

#include <NTSCore/System.h>

#include "NTSGraphics/Core/Context.h"
#include "NTSGraphics/Core/Shader.h"
#include "NTSGraphics/Core/VertexArrayBuffer.h"
#include "NTSGraphics/Core/UniformBuffer.h"
#include "NTSGraphics/Core/Renderer.h"

#include "Shape/Shape2D.h"
#include "RenderData2D.h"
#include "RenderCommand.h"
#include <NTSGraphics/Core/Text.h>

namespace nkentseu {

    #define RENDER_CACH_MAX_QUADS 50000

    #define RENDER_CACH_MAX_VERTICES RENDER_CACH_MAX_QUADS * 4
    #define RENDER_CACH_MAX_INDICES RENDER_CACH_MAX_QUADS * 6

    class NKENTSEU_API RenderCache2D {
    public:
        RenderCache2D(const Memory::Shared<Context>& context);
        ~RenderCache2D();

        bool Destroy();

        bool Initialize();
        void Prepare(Memory::Shared<Renderer> renderer);
        void Present(Memory::Shared<Renderer> renderer);

        // Démarre l'ajout d'une commande de rendu
        void BeginCommand(RenderPrimitive primitive);

        // Ajoute une commande de rendu à la liste
        void AddCommand(const RenderCommandTest& command);

        // Modifie les informations de la commande en cours
        void SetCommandClipRegion(const ClipRegion& clipRegion);
        void SetCommandTextureId(Memory::Shared<Texture2D> texture);

        // Crée une commande de rendu directement
        RenderCommandTest* CreateCommand(RenderPrimitive primitive, const ClipRegion& clipRegion, Memory::Shared<Texture2D> texture);

        // Renvoie la commande en cours
        RenderCommandTest* GetCurrentCommand();

        // Renvoie une commande par son index
        RenderCommandTest* GetCommand(uint32 index);
        RenderCommandTest* GetLastCommand();
        RenderCommandTest* GetBeginCommand();

        // Ajoute un vertex à la commande en cours
        void AddVertex(const maths::Vector2f& position, const maths::Vector4f& color, const maths::Vector2f& uv);
        void AddVertex(const Vertex2D& vertex);

        // Ajoute un indice à la commande en cours
        void AddIndex(uint32 index);

        RenderCommandTest* AddShape(Shape2D* shape);

        // Termine l'ajout d'une commande de rendu
        // Si send est true, la commande actuelle est sauvegardée dans commands
        void EndCommand(bool send = true);

        // Vide la liste des commandes de rendu
        void Clear();

        // Ajoute un rectangle plein
        RenderCommandTest* AddFilledRectangle(const maths::Vector2f& position, const maths::Vector2f& size, const maths::Color& color, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute un rectangle avec contour
        RenderCommandTest* AddOutlineRectangle(const maths::Vector2f& position, const maths::Vector2f& size, const maths::Color& color, float32 lineWidth = 1.0f, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute un rectangle arrondi plein
        RenderCommandTest* AddFilledRoundedRectangle(const maths::Vector2f& position, const maths::Vector2f& size, float32 radius, const maths::Color& color, uint32 segments = 16, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute un rectangle arrondi avec contour
        RenderCommandTest* AddOutlineRoundedRectangle(const maths::Vector2f& position, const maths::Vector2f& size, float32 radius, const maths::Color& color, float32 lineWidth = 1.0f, uint32 segments = 16, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute un rectangle arrondi plein
        RenderCommandTest* AddFilledRoundedRectangle(const maths::Vector2f& position, const maths::Vector2f& size, const maths::Vector4f& radius, const maths::Color& color, uint32 segments = 16, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute un rectangle arrondi avec contour
        RenderCommandTest* AddOutlineRoundedRectangle(const maths::Vector2f& position, const maths::Vector2f& size, const maths::Vector4f& radius, const maths::Color& color, float32 lineWidth = 1.0f, uint32 segments = 16, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute un rectangle arrondi plein
        RenderCommandTest* AddFilledRoundedRectangle(const maths::Vector2f& position, const maths::Vector2f& size, const maths::Vector2f radius[4], const maths::Color& color, uint32 segments = 16, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute un rectangle arrondi avec contour
        RenderCommandTest* AddOutlineRoundedRectangle(const maths::Vector2f& position, const maths::Vector2f& size, const maths::Vector2f radius[4], const maths::Color& color, float32 lineWidth = 1.0f, uint32 segments = 16, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute un cercle plein
        RenderCommandTest* AddFilledCircle(const maths::Vector2f& position, float32 radius, const maths::Color& color, uint32 segments = 32, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute un cercle avec contour
        RenderCommandTest* AddOutlineCircle(const maths::Vector2f& position, float32 radius, const maths::Color& color, float32 lineWidth = 1.0f, uint32 segments = 32, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute une ellipse plein
        RenderCommandTest* AddFilledEllipse(const maths::Vector2f& position, const maths::Vector2f& radius, const maths::Color& color, uint32 segments = 32, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute une ellipse avec contour
        RenderCommandTest* AddOutlineEllipse(const maths::Vector2f& position, const maths::Vector2f& radius, const maths::Color& color, float32 lineWidth = 1.0f, uint32 segments = 32, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute un triangle plein
        RenderCommandTest* AddFilledTriangle(const maths::Vector2f& p1, const maths::Vector2f& p2, const maths::Vector2f& p3, const maths::Color& color, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute un triangle avec contour
        RenderCommandTest* AddOutlineTriangle(const maths::Vector2f& p1, const maths::Vector2f& p2, const maths::Vector2f& p3, const maths::Color& color, float32 lineWidth = 1.0f, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        RenderCommandTest* AddPoint(const maths::Vector2f& p, const maths::Color& color, float32 lineWidth = 1.0f, Memory::Shared<Texture2D> texture = nullptr, maths::Vector2f uv = maths::Vector2f());

        // Ajoute une ligne
        RenderCommandTest* AddLine(const maths::Vector2f& p1, const maths::Vector2f& p2, const maths::Color& color, float32 lineWidth = 1.0f, Memory::Shared<Texture2D> texture = nullptr, maths::Vector2f uv0 = {}, maths::Vector2f uv1 = {});

        // Ajoute un chemin (path)
        RenderCommandTest* AddFillPath(const std::vector<maths::Vector2f>& points, const maths::Color& color, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        RenderCommandTest* AddOutlinePath(const std::vector<maths::Vector2f>& points, const maths::Color& color, bool closed, float32 lineWidth = 1.0f, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute une courbe de Bézier
        RenderCommandTest* AddBezierCurve(const std::vector<maths::Vector2f>& points, int32 degree, bool closed, const std::vector<maths::Vector2f>& controlPoints, float32 lineWidth = 1.0f, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        // Ajoute une courbe
        RenderCommandTest* AddCurve(const std::vector<maths::Vector2f>& points, int32 degree, bool closed, float32 lineWidth = 1.0f, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

        void AddText(Memory::Shared<Font> font, const std::string& texte, const maths::Vector2f& position, uint32 characteSize, bool bold, bool italic, bool underline, bool strikeThrough, const maths::Color& color, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});
        RenderCommandTest* AddText(Memory::Shared<FontNG> font, const std::string& texte, const maths::Vector2f& position, uint32 characteSize, bool bold, bool italic, bool underline, bool strikeThrough, const maths::Color& color, Memory::Shared<Texture2D> texture = nullptr, const std::vector<maths::Vector2f>& uvs = {});

    private:
        std::vector<RenderCommandTest> commands; // Liste des commandes de rendu
        std::vector<Vertex2D> vertices; // Vertex de la géométrie 2D
        std::vector<uint32> indices; // Indices de la géométrie 2D

        RenderCommandTest currentCommand; // Commande en cours
        std::vector<Vertex2D> currentVertices; // Vertex de la géométrie 2D
        std::vector<uint32> currentIndices; // Indices de la géométrie 2D

        bool beginCommand = false;
        uint32 currentVertice = 0; // Index du vertex en cours
        uint32 currentIndice = 0; // Index de l'indice en cours

        bool isPresent = false;

        // renderer
        Memory::Shared<Context> m_Context = nullptr;
        Memory::Shared<Shader> m_Shader = nullptr;
        Memory::Shared<ShaderInputLayout> shaderInputLayout = nullptr;

        std::vector<Texture2DBindingShared> textureBindings;
        std::vector<Texture2DBindingShared> fontTextureBindings;

        Memory::Shared<UniformBuffer> uniformBuffer = nullptr;

        Memory::Shared<VertexBuffer> vertexBuffer = nullptr;
        Memory::Shared<IndexBuffer> indexBuffer = nullptr;
        Memory::Shared<VertexArrayBuffer> vertexArray = nullptr;

        Memory::Shared<Texture2D> defaultTeture = nullptr;
        Memory::Shared<Texture2DBinding> defaultTetureBinding = nullptr;

        private:
            int32 AddInternalArc(const maths::Vector2f& center, const maths::Vector2f& radius, const maths::Vector2f& rad, const maths::Color& color, float32 departAngle, float32 endAngle, float32 segments, int32 decal, const std::vector<maths::Vector2f>& uvs = {});
            int32 AddInternalArc(uint32 depart, const maths::Vector2f& center, const maths::Vector2f& position, const maths::Vector2f& size, const maths::Vector2f& radius, const maths::Vector2f& rad, const maths::Color& color, float32 startAngle, float32 endAngle, uint32 segments, const std::vector<maths::Vector2f>& uvs = {});

        void SetFontTextureId(Memory::Shared<Texture2D> texture);
        Memory::Shared<Texture2DBinding> SelectBinding(Texture2DBindingShared current, std::vector<Texture2DBindingShared> bindings, bool* hasBind, bool useTexture, uint32 textureId);
    };
}  //  nkentseu

#endif  // __RENDER_CACHE2_D_H__!