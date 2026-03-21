//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-13 at 09:24:44 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __GRAPHICS_ENUM_H__
#define __GRAPHICS_ENUM_H__

#pragma once

#include <NTSCore/System.h>

namespace nkentseu {

	ENUMERATION(ShaderStage, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_ADD_CONTENT(Enum::Vertex)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Fragment)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Geometry)
		ENUM_TO_STRING_ADD_CONTENT(Enum::TesControl)
		ENUM_TO_STRING_ADD_CONTENT(Enum::TesEvaluation)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Compute)
		ENUM_TO_STRING_END(Enum::NotDefine),
		,
		Vertex = 1 << 0,  // 0x01
		Fragment = 1 << 1,  // 0x02
		Geometry = 1 << 2,  // 0x04
		TesControl = 1 << 3,  // 0x08
		TesEvaluation = 1 << 4,  // 0x10
		Compute = 1 << 5,  // 0x20
		NotDefine
	);

	ENUMERATION(ShaderInternalType, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::Boolean)
		ENUM_TO_STRING_SET_CONTENT(Enum::Float)
		ENUM_TO_STRING_SET_CONTENT(Enum::Float2)
		ENUM_TO_STRING_SET_CONTENT(Enum::Float3)
		ENUM_TO_STRING_SET_CONTENT(Enum::Float4)
		ENUM_TO_STRING_SET_CONTENT(Enum::Int)
		ENUM_TO_STRING_SET_CONTENT(Enum::Int2)
		ENUM_TO_STRING_SET_CONTENT(Enum::Int3)
		ENUM_TO_STRING_SET_CONTENT(Enum::Int4)
		ENUM_TO_STRING_SET_CONTENT(Enum::Byte)
		ENUM_TO_STRING_SET_CONTENT(Enum::Byte2)
		ENUM_TO_STRING_SET_CONTENT(Enum::Byte3)
		ENUM_TO_STRING_SET_CONTENT(Enum::Byte4)
		ENUM_TO_STRING_SET_CONTENT(Enum::Mat2)
		ENUM_TO_STRING_SET_CONTENT(Enum::Mat3)
		ENUM_TO_STRING_SET_CONTENT(Enum::Mat4)
		ENUM_TO_STRING_END(Enum::NotDefine),
		static uint32 ComponentCount(ShaderInternalType type);
	static uint32 ComponentElementSize(ShaderInternalType type);
	static uint32 ComponentSize(ShaderInternalType type);
	,
		Boolean, Float, Float2, Float3, Float4, Int, Int2, Int3, Int4, Byte, Byte2, Byte3, Byte4, Mat2, Mat3, Mat4, NotDefine
		);

	ENUMERATION(BufferUsageType, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::DynamicDraw)
		ENUM_TO_STRING_SET_CONTENT(Enum::StreamDraw)
		ENUM_TO_STRING_END(Enum::StaticDraw),
		,
		StaticDraw, DynamicDraw, StreamDraw
	);

	ENUMERATION(BufferSpecificationType, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::Index)
		ENUM_TO_STRING_SET_CONTENT(Enum::Uniform)
		ENUM_TO_STRING_SET_CONTENT(Enum::Storage)
		ENUM_TO_STRING_SET_CONTENT(Enum::Texture)
		ENUM_TO_STRING_SET_CONTENT(Enum::Constant)
		ENUM_TO_STRING_END(Enum::Vertex),
		,
		Vertex, Index, Uniform, Storage, Texture, Constant
	);

	ENUMERATION(IndexBufferType, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::UInt8)
		ENUM_TO_STRING_SET_CONTENT(Enum::UInt16)
		ENUM_TO_STRING_SET_CONTENT(Enum::UInt32)
		ENUM_TO_STRING_SET_CONTENT(Enum::UInt64)
		ENUM_TO_STRING_END(Enum::NotDefine),
		static usize SizeOf(IndexBufferType indexType);
	,
		NotDefine, UInt8, UInt16, UInt32, UInt64
		);

	ENUMERATION(SamplerType, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::SamplerImage)
		ENUM_TO_STRING_SET_CONTENT(Enum::StorageImage)
		ENUM_TO_STRING_END(Enum::CombineImage),
		,
		CombineImage, SamplerImage, StorageImage
	);

    ENUMERATION(TextCord, uint32,
        ENUM_TO_STRING_BEGIN
        ENUM_TO_STRING_SET_CONTENT(Enum::Pixels)
        ENUM_TO_STRING_END(Enum::Normalized),
        ,
        Normalized, //!< Texture coordinates in range [0 .. 1]
        Pixels      //!< Texture coordinates in range [0 .. size]
    );

    ENUMERATION(TextureFormat, uint32,
        ENUM_TO_STRING_BEGIN
        ENUM_TO_STRING_SET_CONTENT(Enum::RGBA8)
        ENUM_TO_STRING_SET_CONTENT(Enum::RGB8)
        ENUM_TO_STRING_SET_CONTENT(Enum::SRGB8_ALPHA8)
        ENUM_TO_STRING_SET_CONTENT(Enum::RED_INTEGER)
        ENUM_TO_STRING_SET_CONTENT(Enum::DEPTH_COMPONENT16)
        ENUM_TO_STRING_SET_CONTENT(Enum::DEPTH_COMPONENT24)
        ENUM_TO_STRING_SET_CONTENT(Enum::DEPTH_COMPONENT32F)
        ENUM_TO_STRING_SET_CONTENT(Enum::STENCIL_INDEX8)
        ENUM_TO_STRING_SET_CONTENT(Enum::DEPTH24_STENCIL8)
        ENUM_TO_STRING_SET_CONTENT(Enum::DEPTH32F_STENCIL8)
        ENUM_TO_STRING_END(Enum::None),
        // Verifier si un format est un format de couleur
        static bool IsColor(TextureFormat format);

		// Verifier si un format est un format de profondeur
		static bool IsDepth(TextureFormat format);

		// Verifier si un format est un format de stencil
		static bool IsStencil(TextureFormat format);
		,
        None = 0,

        // Formats de couleur
        RGBA8,                 // Couleur RGBA 8 bits non normalisee
        RGB8,                  // Couleur RGB 8 bits non normalisee
        SRGB8_ALPHA8,          // Couleur sRGB avec canal alpha 8 bits non normalisee
        RED_INTEGER,

        // Formats de profondeur
        DEPTH_COMPONENT16,     // Profondeur 16 bits non normalisee
        DEPTH_COMPONENT24,     // Profondeur 24 bits non normalisee
        DEPTH_COMPONENT32F,    // Profondeur 32 bits en virgule flottante

        // Formats de stencil
        STENCIL_INDEX8,        // Index de stencil 8 bits
        DEPTH24_STENCIL8,      // Profondeur 24 bits avec index de stencil 8 bits
        DEPTH32F_STENCIL8      // Profondeur 32 bits en virgule flottante avec index de stencil 8 bits
    );

	ENUMERATION(RenderPrimitive, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::Points)
		ENUM_TO_STRING_SET_CONTENT(Enum::Lines)
		ENUM_TO_STRING_SET_CONTENT(Enum::LineStrip)
		ENUM_TO_STRING_SET_CONTENT(Enum::Triangles)
		ENUM_TO_STRING_SET_CONTENT(Enum::TriangleStrip)
		ENUM_TO_STRING_SET_CONTENT(Enum::TriangleFan)
		ENUM_TO_STRING_SET_CONTENT(Enum::Paths)
		ENUM_TO_STRING_END(Enum::Points),
		,
		Points,
		Lines,
		LineStrip,
		Triangles,
		TriangleStrip,
		TriangleFan,
		Paths
	);

	ENUMERATION(CullModeType, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::Front)
		ENUM_TO_STRING_SET_CONTENT(Enum::Back)
		ENUM_TO_STRING_SET_CONTENT(Enum::FrontBack)
		ENUM_TO_STRING_END(Enum::NoCull),
		,
		Front, Back, FrontBack, NoCull
	);

	ENUMERATION(FrontFaceType, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::CounterClockwise)
		ENUM_TO_STRING_END(Enum::Clockwise),
		,
		CounterClockwise, Clockwise
	);

	ENUMERATION(PolygonModeType, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::Line)
		ENUM_TO_STRING_SET_CONTENT(Enum::Point)
		ENUM_TO_STRING_SET_CONTENT(Enum::FillRectangle)
		ENUM_TO_STRING_END(Enum::Fill),
		,
		Line, Fill, Point, FillRectangle
	);

	ENUMERATION(Operation, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::Add)
		ENUM_TO_STRING_SET_CONTENT(Enum::Multiply)
		ENUM_TO_STRING_SET_CONTENT(Enum::Substract)
		ENUM_TO_STRING_SET_CONTENT(Enum::Divide)
		ENUM_TO_STRING_SET_CONTENT(Enum::Modulo)
		ENUM_TO_STRING_SET_CONTENT(Enum::And)
		ENUM_TO_STRING_SET_CONTENT(Enum::Or)
		ENUM_TO_STRING_SET_CONTENT(Enum::Xor)
		ENUM_TO_STRING_END(Enum::NotDefine),
		Operation FromString(const std::string& operation);
		,
		NotDefine = 0,
		Add,
		Multiply,
		Substract,
		Divide,
		Modulo,
		And,
		Or,
		Xor
	);

	ENUMERATION(GraphicsBufferCount, ulong,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::Simple)
		ENUM_TO_STRING_SET_CONTENT(Enum::Double)
		ENUM_TO_STRING_SET_CONTENT(Enum::Triple)
		ENUM_TO_STRING_END(Enum::Simple),
	,
		Simple,
		Double,
		Triple
		);

	ENUMERATION(GraphicsAttribute, ulong,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_ADD_CONTENT(Enum::AttributeDefault)
		ENUM_TO_STRING_ADD_CONTENT(Enum::AttributeCore)
		ENUM_TO_STRING_ADD_CONTENT(Enum::AttributeDebug)
		ENUM_TO_STRING_END(Enum::AttributeDefault),
	,
		AttributeDefault = 0,
		AttributeCore = 1 << 1,
		AttributeDebug = 1 << 2
		);

	ENUMERATION(GraphicsFlag, ulong,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_ADD_CONTENT(Enum::DrawToWindow)
		ENUM_TO_STRING_ADD_CONTENT(Enum::DrawToBitmap)
		ENUM_TO_STRING_ADD_CONTENT(Enum::SupportGDI)
		ENUM_TO_STRING_ADD_CONTENT(Enum::SupportOpenGL)
		ENUM_TO_STRING_ADD_CONTENT(Enum::GenericAccelerated)
		ENUM_TO_STRING_ADD_CONTENT(Enum::GenericFormat)
		ENUM_TO_STRING_ADD_CONTENT(Enum::NeedPalette)
		ENUM_TO_STRING_ADD_CONTENT(Enum::NeedSystemPalette)
		ENUM_TO_STRING_ADD_CONTENT(Enum::TripleBuffer)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Stereo)
		ENUM_TO_STRING_ADD_CONTENT(Enum::SwapLayerBuffer)
		ENUM_TO_STRING_ADD_CONTENT(Enum::DepthDontCare)
		ENUM_TO_STRING_ADD_CONTENT(Enum::DoubleBufferDontCare)
		ENUM_TO_STRING_ADD_CONTENT(Enum::StereoDontCare)
		ENUM_TO_STRING_ADD_CONTENT(Enum::SwapCopy)
		ENUM_TO_STRING_ADD_CONTENT(Enum::SupportDirectDraw)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Direct3DAccelerated)
		ENUM_TO_STRING_ADD_CONTENT(Enum::SupportComposition)
		ENUM_TO_STRING_ADD_CONTENT(Enum::TripeBufferDontCare)
		ENUM_TO_STRING_END(Enum::DrawToWindow),
	,
		DrawToWindow = 1 << 1,
		DrawToBitmap = 1 << 2,
		SupportGDI = 1 << 3,
		SupportOpenGL = 1 << 4,
		GenericAccelerated = 1 << 5,
		GenericFormat = 1 << 6,
		NeedPalette = 1 << 7,
		NeedSystemPalette = 1 << 8,
		DoubleBuffer = 1 << 9,
		TripleBuffer = 1 << 10,
		Stereo = 1 << 11,
		SwapLayerBuffer = 1 << 12,
		DepthDontCare = 1 << 13,
		DoubleBufferDontCare = 1 << 14,
		StereoDontCare = 1 << 15,
		SwapExchange = 1 << 16,
		SwapCopy = 1 << 17,
		SupportDirectDraw = 1 << 18,
		Direct3DAccelerated = 1 << 19,
		SupportComposition = 1 << 20,
		TripeBufferDontCare = 1 << 21
		);

	ENUMERATION(GraphicsMainLayer, uint8,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::MainPlane)
		ENUM_TO_STRING_SET_CONTENT(Enum::OverlayPlane)
		ENUM_TO_STRING_SET_CONTENT(Enum::UnderlayPlane)
		ENUM_TO_STRING_END(Enum::MainPlane),
		Operation FromString(const std::string& operation);
	,
		MainPlane,
		OverlayPlane,
		UnderlayPlane
		);

	ENUMERATION(GraphicsApiType, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_SET_CONTENT(Enum::OpenglApi)
		ENUM_TO_STRING_SET_CONTENT(Enum::VulkanApi)
		ENUM_TO_STRING_SET_CONTENT(Enum::DirectX11Api)
		ENUM_TO_STRING_SET_CONTENT(Enum::DirectX12Api)
		ENUM_TO_STRING_SET_CONTENT(Enum::SoftwareApi)
		ENUM_TO_STRING_SET_CONTENT(Enum::MetalApi)
		ENUM_TO_STRING_END(Enum::OpenglApi),
	,
		OpenglApi,
		VulkanApi,
		DirectX11Api,
		DirectX12Api,
		SoftwareApi,
		MetalApi,
		);

	ENUMERATION(GraphicsPixelType, ulong,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_ADD_CONTENT(Enum::RgbaArb)
		ENUM_TO_STRING_ADD_CONTENT(Enum::RgbaExt)
		ENUM_TO_STRING_ADD_CONTENT(Enum::RgbaFloatArb)
		ENUM_TO_STRING_ADD_CONTENT(Enum::RgbaFloatAti)
		ENUM_TO_STRING_ADD_CONTENT(Enum::RgbaUnsignedFloatExt)
		ENUM_TO_STRING_ADD_CONTENT(Enum::ColorIndexArb)
		ENUM_TO_STRING_ADD_CONTENT(Enum::ColorIndexExt)
		ENUM_TO_STRING_END(Enum::RgbaArb),
		Operation FromString(const std::string& operation);
	,
		RgbaArb = 1 << 1,
		RgbaExt = 1 << 2,
		RgbaFloatArb = 1 << 3,
		RgbaFloatAti = 1 << 4,
		RgbaUnsignedFloatExt = 1 << 5,
		ColorIndexArb = 1 << 6,
		ColorIndexExt = 1 << 7
		);

	ENUMERATION(TextStyle, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_ADD_CONTENT(Enum::Regular)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Bold)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Italic)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Underlined)
		ENUM_TO_STRING_ADD_CONTENT(Enum::StrikeThrough)
		ENUM_TO_STRING_END(Enum::Regular),
		,
		Regular = 0,
		Bold = 1 << 0,
		Italic = 1 << 1,
		Underlined = 1 << 2,
		StrikeThrough = 1 << 3
	);

	ENUMERATION(TextAlign, uint32,
		ENUM_TO_STRING_BEGIN
		ENUM_TO_STRING_ADD_CONTENT(Enum::Left)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Center)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Right)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Justify)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Top)
		ENUM_TO_STRING_ADD_CONTENT(Enum::Bottom)
		ENUM_TO_STRING_END(Enum::Left),
		,
		Left,
		Center,
		Right,
		Top,
		Bottom,
		Justify
	);

}  //  nkentseu

#endif  // __GRAPHICS_ENUM_H__!