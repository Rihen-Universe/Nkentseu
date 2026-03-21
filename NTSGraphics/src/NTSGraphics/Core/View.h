//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-06-14 at 08:48:57 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __VIEW_H__
#define __VIEW_H__

#pragma once

#include <NTSCore/System.h>
#include <NTSMaths/Matrix/Matrix4.h>
#include <NTSMaths/Shapes/Rectangle.h>
#include <NTSMaths/Quaternion/Quaternion.h>
#include <NTSMaths/Angle.h>

namespace nkentseu {

	enum class NKENTSEU_API ProjectionInfo {
		Orthographic,
		Perspective
	};
    
    class NKENTSEU_API View {
    public:
        View();
        explicit View(const maths::FloatRect& rectangle);
        View(const maths::Vector2f& center, const maths::Vector2f& size);

        void SetCenter(const maths::Vector2f& center);
        void SetSize(const maths::Vector2f& size);
        void SetRotation(const maths::Angle& angle);
        void SetViewport(const maths::FloatRect& viewport);
        void SetScissor(const maths::FloatRect& scissor);
        void Reset(const maths::FloatRect& rectangle);
        const maths::Vector2f& GetCenter() const;
        const maths::Vector2f& GetSize() const;
        maths::Angle GetRotation() const;
        const maths::FloatRect& GetViewport() const;
        const maths::FloatRect& GetScissor() const;
        void Move(const maths::Vector2f& offset);
        void Rotate(const maths::Angle& angle);
        void Zoom(float32 factor);
        const maths::matrix4f GetTransform() const;
        const maths::matrix4f GetInverseTransform() const;

    private:

        maths::Vector2f  m_Center;                     //!< Center of the view, in scene coordinates
        maths::Vector2f  m_Size;                       //!< Size of the view, in scene coordinates
        maths::Angle     m_Rotation;                   //!< Angle of rotation of the view rectangle
        maths::FloatRect m_Viewport{ {0, 0}, {1, 1} };   //!< Viewport rectangle, expressed as a factor of the render-target's size
        maths::FloatRect m_Scissor{ {0, 0}, {1, 1} };    //!< Scissor rectangle, expressed as a factor of the render-target's size
        mutable maths::matrix4f m_Transform;          //!< Precomputed projection transform corresponding to the view
        mutable maths::matrix4f m_InverseTransform;   //!< Precomputed inverse projection transform corresponding to the view
        mutable bool      m_TransformUpdated{}; //!< Internal state telling if the transform needs to be updated
        mutable bool      m_InvTransformUpdated{}; //!< Internal state telling if the inverse transform needs to be updated
    };

}  //  nkentseu

#endif  // __VIEW_H__!