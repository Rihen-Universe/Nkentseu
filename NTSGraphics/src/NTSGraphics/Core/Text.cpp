//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-27 at 09:07:41 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "Text.h"

namespace nkentseu {
    
    /*TextShape::TextShape(const Memory::Shared<Font>& font, const std::string& string, uint32 characterSize) : m_Font(font), m_String(string), m_CharacterSize(characterSize)
    {
    }

    const TextGeometry& TextShape::GetGeometry()
    {
        return m_Vertices;
    }

    const TextGeometry& TextShape::GetOutlineGeometry()
    {
        return m_OutlineVertices;
    }

    void TextShape::SetString(const std::string& string)
    {
        if (m_String != string)
        {
            m_String = string;
            m_GeometryNeedUpdate = true;
        }
    }

    void TextShape::SetFont(const Memory::Shared<Font>& font)
    {
        if (m_Font != font)
        {
            m_Font = font;
            m_GeometryNeedUpdate = true;
        }
    }

    void TextShape::SetCharacterSize(uint32 size)
    {
        if (m_CharacterSize != size)
        {
            m_CharacterSize = size;
            m_GeometryNeedUpdate = true;
        }
    }

    void TextShape::SetLineSpacing(float32 spacingFactor)
    {
        if (m_LineSpacingFactor != spacingFactor)
        {
            m_LineSpacingFactor = spacingFactor;
            m_GeometryNeedUpdate = true;
        }
    }

    void TextShape::SetLetterSpacing(float32 spacingFactor)
    {
        if (m_LetterSpacingFactor != spacingFactor)
        {
            m_LetterSpacingFactor = spacingFactor;
            m_GeometryNeedUpdate = true;
        }
    }

    void TextShape::SetStyle(uint32 style)
    {
        if (m_Style != style)
        {
            m_Style = style;
            m_GeometryNeedUpdate = true;
        }
    }

    void TextShape::SetOutlineThickness(float32 thickness)
    {
        if (thickness != m_OutlineThickness)
        {
            m_OutlineThickness = thickness;
            m_GeometryNeedUpdate = true;
        }
    }

    const std::string& TextShape::GetString() const
    {
        return m_String;
    }

    const Memory::Shared<Font>& TextShape::GetFont() const
    {
        return m_Font;
    }

    uint32 TextShape::GetCharacterSize() const
    {
        return m_CharacterSize;
    }

    float32 TextShape::GetLetterSpacing() const
    {
        return m_LetterSpacingFactor;
    }

    float32 TextShape::GetLineSpacing() const
    {
        return m_LineSpacingFactor;
    }

    uint32 TextShape::GetStyle() const
    {
        return m_Style;
    }

    float32 TextShape::GetOutlineThickness() const
    {
        return m_OutlineThickness;
    }

    maths::Vector2f TextShape::FindCharacterPos(usize index) const
    {
        if (index > m_String.size())
            index = m_String.size();

        const bool      isBold = m_Style & TextStyle::Enum::Bold;
        float32         whitespaceWidth;// = m_Font->GetGlyph(U' ', m_CharacterSize, isBold).advance;
        const float32   letterSpacing = (whitespaceWidth / 3.f) * (m_LetterSpacingFactor - 1.f);
        whitespaceWidth += letterSpacing;
        const float32   lineSpacing = m_Font->GetLineSpacing(m_CharacterSize) * m_LineSpacingFactor;

        maths::Vector2f      position;
        uint32 prevChar = 0;
        for (usize i = 0; i < index; ++i)
        {
            const uint32 curChar = m_String[i];

            position.x += m_Font->GetKerning(prevChar, curChar, m_CharacterSize, isBold);
            prevChar = curChar;

            switch (curChar)
            {
            case U' ':
                position.x += whitespaceWidth;
                continue;
            case U'\t':
                position.x += whitespaceWidth * 4;
                continue;
            case U'\n':
                position.y += lineSpacing;
                position.x = 0;
                continue;
            }

            //position.x += m_Font->GetGlyph(curChar, m_CharacterSize, isBold).advance + letterSpacing;
        }

        //position = getTransform().transformPoint(position);

        return position;
    }

    maths::FloatRect TextShape::GetLocalBounds() const
    {
        //EnsureGeometryUpdate();

        return m_Bounds;
    }

    maths::FloatRect TextShape::GetGlobalBounds() const
    {
        //return getTransform().transformRect(getLocalBounds());
        return maths::FloatRect();
    }

    void TextShape::AddLine(const maths::Vector2f& p1, const maths::Vector2f& p2, float32 lineWidth, maths::Vector2f uv0, maths::Vector2f uv1)
    {
        maths::Vector2f normal = (p1 - p2).Normal().Normalized() * lineWidth * 0.5f;

        maths::Vector2f p1_1 = p1 + normal;
        maths::Vector2f p1_2 = p1 - normal;

        maths::Vector2f p2_1 = p2 + normal;
        maths::Vector2f p2_2 = p2 - normal;

        m_Vertices.AddVertice(maths::Vector3f(p1_1), uv0);
        m_Vertices.AddVertice(maths::Vector3f(p1_2), uv0);
        m_Vertices.AddVertice(maths::Vector3f(p2_2), uv1);
        m_Vertices.AddVertice(maths::Vector3f(p2_1), uv1);

        m_Vertices.AddIndices({0, 1, 2, 2, 3, 0});
    }

    void TextShape::EnsureGeometryUpdate()
    {
        if (!m_GeometryNeedUpdate && m_Font->GetTexture(m_CharacterSize)->ID() == m_FontTextureId)
            return;

        m_FontTextureId = m_Font->GetTexture(m_CharacterSize)->ID();

        m_GeometryNeedUpdate = false;

        m_Vertices.Clear();

        if (m_String.empty())
            return;

        const bool  isBold = m_Style & TextStyle::Enum::Bold;
        const bool  isUnderlined = m_Style & TextStyle::Enum::Underlined;
        const bool  isStrikeThrough = m_Style & TextStyle::Enum::StrikeThrough;
        const float32 italicShear = (m_Style & TextStyle::Enum::Italic) ? 12 * maths::DEG2RAD : 0.f;
        const float32 underlineOffset = m_Font->GetUnderlinePosition(m_CharacterSize);
        const float32 underlineThickness = m_Font->GetUnderlineThickness(m_CharacterSize);

        const float32 strikeThroughOffset = m_Font->GetGlyph(U'x', m_CharacterSize, isBold).bounds.GetCenter().y;

        float32       whitespaceWidth;// = m_Font->GetGlyph(U' ', m_CharacterSize, isBold).advance;
        const float32 letterSpacing = (whitespaceWidth / 3.f) * (m_LetterSpacingFactor - 1.f);
        whitespaceWidth += letterSpacing;
        const float32 lineSpacing = m_Font->GetLineSpacing(m_CharacterSize) * m_LineSpacingFactor;
        float32       x = 0.f;
        auto        y = static_cast<float32>(m_CharacterSize);

        // Create one quad for each character
        auto          minX = static_cast<float32>(m_CharacterSize);
        auto          minY = static_cast<float32>(m_CharacterSize);
        float32         maxX = 0.f;
        float32         maxY = 0.f;
        std::uint32_t prevChar = 0;
        for (std::size_t i = 0; i < m_String.size(); ++i)
        {
            const std::uint32_t curChar = m_String[i];

            if (curChar == U'\r')
                continue;

            x += m_Font->GetKerning(prevChar, curChar, m_CharacterSize, isBold);

            if (isUnderlined && (curChar == U'\n' && prevChar != U'\n'))
            {
                AddLine({ x, y }, {x + underlineOffset , y}, underlineThickness);

                if (m_OutlineThickness != 0)
                    AddLine({ x, y }, {x + underlineOffset }, m_OutlineThickness);
            }

            if (isStrikeThrough && (curChar == U'\n' && prevChar != U'\n'))
            {
                AddLine({ x, y }, { x + strikeThroughOffset , y }, underlineThickness);

                if (m_OutlineThickness != 0)
                    AddLine({ x, y }, { x + strikeThroughOffset }, m_OutlineThickness);
            }

            prevChar = curChar;

            if ((curChar == U' ') || (curChar == U'\n') || (curChar == U'\t'))
            {
                minX = std::min(minX, x);
                minY = std::min(minY, y);

                switch (curChar)
                {
                case U' ':
                    x += whitespaceWidth;
                    break;
                case U'\t':
                    x += whitespaceWidth * 4;
                    break;
                case U'\n':
                    y += lineSpacing;
                    x = 0;
                    break;
                }

                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);

                continue;
            }

            if (m_OutlineThickness != 0)
            {
                const Glyph& glyph = m_Font->GetGlyph(curChar, m_CharacterSize, isBold, m_OutlineThickness);

                AddGlyphQuad(maths::Vector2f(x, y), {}, glyph, italicShear);
            }

            const Glyph& glyph = m_Font->GetGlyph(curChar, m_CharacterSize, isBold);

            AddGlyphQuad(maths::Vector2f(x, y), {}, glyph, italicShear);

            const float32 left = glyph.bounds.left;
            const float32 top = glyph.bounds.top;
            const float32 right = glyph.bounds.left + glyph.bounds.width;
            const float32 bottom = glyph.bounds.top + glyph.bounds.height;

            minX = std::min(minX, x + left - italicShear * bottom);
            maxX = std::max(maxX, x + right - italicShear * top);
            minY = std::min(minY, y + top);
            maxY = std::max(maxY, y + bottom);

            //x += glyph.advance + letterSpacing;
        }

        if (m_OutlineThickness != 0)
        {
            const float32 outline = std::abs(std::ceil(m_OutlineThickness));
            minX -= outline;
            maxX += outline;
            minY -= outline;
            maxY += outline;
        }

        // If we're using the underlined style, add the last line
        if (isUnderlined && (x > 0))
        {
            AddLine(m_vertices, x, y, m_fillColor, underlineOffset, underlineThickness);

            if (m_OutlineThickness != 0)
                AddLine(m_OutlineVertices, x, y, m_outlineColor, underlineOffset, underlineThickness, m_outlineThickness);
        }

        // If we're using the strike through style, add the last line across all characters
        if (isStrikeThrough && (x > 0))
        {
            AddLine(m_vertices, x, y, m_fillColor, strikeThroughOffset, underlineThickness);

            if (m_outlineThickness != 0)
                AddLine(m_outlineVertices, x, y, m_outlineColor, strikeThroughOffset, underlineThickness, m_outlineThickness);
        }* /

        m_Bounds.left = minX;
        m_Bounds.top = minY;
        m_Bounds.width = maxX - minX;
        m_Bounds.height = maxY - minY;
    }

    void TextShape::AddGlyphQuad(maths::Vector2f position, const maths::Color& color, const Glyph& glyph, float32 italicShear)
    {
        const float32 padding = 1.0;

        const float32 left = glyph.bounds.left - padding;
        const float32 top = glyph.bounds.top - padding;
        const float32 right = glyph.bounds.left + glyph.bounds.width + padding;
        const float32 bottom = glyph.bounds.top + glyph.bounds.height + padding;

        const float32 u1 = static_cast<float32>(glyph.textureRect.left) - padding;
        const float32 v1 = static_cast<float32>(glyph.textureRect.top) - padding;
        const float32 u2 = static_cast<float32>(glyph.textureRect.left + glyph.textureRect.width) + padding;
        const float32 v2 = static_cast<float32>(glyph.textureRect.top + glyph.textureRect.height) + padding;

        m_Vertices.AddVertice(maths::Vector3f(position.x + left - italicShear * top, position.y + top, 0), maths::Vector2f(u1, v1));
        m_Vertices.AddVertice(maths::Vector3f(position.x + right - italicShear * top, position.y + top, 0), maths::Vector2f(u2, v1));
        m_Vertices.AddVertice(maths::Vector3f(position.x + left - italicShear * bottom, position.y + bottom), maths::Vector2f(u1, v2));
        m_Vertices.AddVertice(maths::Vector3f(position.x + right - italicShear * bottom, position.y + bottom), maths::Vector2f(u2, v2));

        m_Vertices.AddIndices({ 0, 1, 2, 2, 3, 0 });
    }*/

}  //  nkentseu