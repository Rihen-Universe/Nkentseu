//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-06-14 at 08:48:57 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "View.h"
#include <NTSLogger/Formatter.h>

namespace nkentseu {
    using namespace maths;
    
    View::View()
    {
        Reset(FloatRect({ 0, 0 }, { 1000, 1000 }));
    }

    View::View(const FloatRect& rectangle)
    {
        Reset(rectangle);
    }

    View::View(const Vector2f& center, const Vector2f& size) : m_Center(center), m_Size(size)
    {
    }

    void View::SetCenter(const Vector2f& center)
    {
        m_Center = center;
        m_TransformUpdated = false;
        m_InvTransformUpdated = false;
    }

    void View::SetSize(const Vector2f& size)
    {
        m_Size = size;

        m_TransformUpdated = false;
        m_InvTransformUpdated = false;
    }

    void View::SetRotation(const Angle& angle)
    {
        m_Rotation = angle;

        m_TransformUpdated = false;
        m_InvTransformUpdated = false;
    }

    void View::SetViewport(const FloatRect& viewport)
    {
        m_Viewport = viewport;
    }

    void View::SetScissor(const FloatRect& scissor)
    {
        m_Scissor = scissor;
    }

    void View::Reset(const FloatRect& rectangle)
    {
        m_Center = Vector2f(rectangle.GetCenterX(), rectangle.GetCenterY());
        m_Size = Vector2f(rectangle.width, rectangle.height);
        m_Rotation = Angle();

        m_TransformUpdated = false;
        m_InvTransformUpdated = false;
    }

    const Vector2f& View::GetCenter() const
    {
        return m_Center;
    }

    const Vector2f& View::GetSize() const
    {
        return m_Size;
    }

    Angle View::GetRotation() const
    {
        return m_Rotation;
    }

    const FloatRect& View::GetViewport() const
    {
        return m_Viewport;
    }

    const FloatRect& View::GetScissor() const
    {
        return m_Scissor;
    }

    void View::Move(const Vector2f& offset)
    {
        SetCenter(m_Center + offset);
    }

    void View::Rotate(const Angle& angle)
    {
        SetRotation(m_Rotation + angle);
    }

    void View::Zoom(float32 factor)
    {
        SetSize(m_Size * factor);
    }

    const matrix4f View::GetTransform() const
    {
        if (!m_TransformUpdated)
        {
            const float32 cosine = maths::Cos(m_Rotation);
            const float32 sine = maths::Sin(m_Rotation);
            const float32 tx = -m_Center.x * cosine - m_Center.y * sine + m_Center.x;
            const float32 ty = m_Center.x * sine - m_Center.y * cosine + m_Center.y;

            const float32 a = 2.f / m_Size.x;
            const float32 b = -2.f / m_Size.y;
            const float32 c = -a * m_Center.x;
            const float32 d = -b * m_Center.y;

            m_Transform = matrix4f(a * cosine, a * sine, a * tx + c, 0.0f,
                                   -b * sine, b * cosine, b * ty + d, 0.f,
                                    0.f, 0.f, 1.f, 0.f,
                                    0.f, 0.f, 0.f, 1.f);

            m_TransformUpdated = true;
        }

        return m_Transform;
    }

    const matrix4f View::GetInverseTransform() const
    {
        if (!m_InvTransformUpdated)
        {
            m_InverseTransform = GetTransform().Inverse();
            m_InvTransformUpdated = true;
        }

        return m_InverseTransform;
    }

}  //  nkentseu