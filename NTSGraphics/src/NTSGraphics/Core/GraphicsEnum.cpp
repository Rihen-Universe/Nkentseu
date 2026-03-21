//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-13 at 09:24:44 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "GraphicsEnum.h"

namespace nkentseu {

	uint32 ShaderInternalType::ComponentCount(ShaderInternalType type) {
		if (type == Enum::Boolean) return 1;
		if (type == Enum::Float) return 1;
		if (type == Enum::Float2) return 2;
		if (type == Enum::Float3) return 3;
		if (type == Enum::Float4) return 4;
		if (type == Enum::Int) return 1;
		if (type == Enum::Int2) return 2;
		if (type == Enum::Int3) return 3;
		if (type == Enum::Int4) return 4;
		/*if (type == Enum::UInt8) return sizeof(uint8);
		if (type == Enum::UInt16) return sizeof(uint16);
		if (type == Enum::UInt32) return sizeof(uint32);
		if (type == Enum::UInt64) return sizeof(uint64);*/
		if (type == Enum::Byte) return 1;
		if (type == Enum::Byte2) return 2;
		if (type == Enum::Byte3) return 3;
		if (type == Enum::Byte4) return 4;
		if (type == Enum::Mat2) return 4;
		if (type == Enum::Mat3) return 9;
		if (type == Enum::Mat4) return 16;
		return 0;
	}

	uint32 ShaderInternalType::ComponentElementSize(ShaderInternalType type) {
		if (type == Enum::Boolean) return sizeof(bool);
		if (type == Enum::Float) return sizeof(float32);
		if (type == Enum::Float2) return sizeof(float32);
		if (type == Enum::Float3) return sizeof(float32);
		if (type == Enum::Float4) return sizeof(float32);
		if (type == Enum::Int) return sizeof(int32);
		if (type == Enum::Int2) return sizeof(int32);
		if (type == Enum::Int3) return sizeof(int32);
		if (type == Enum::Int4) return sizeof(int32);
		/*if (type == Enum::UInt8) return sizeof(uint8);
		if (type == Enum::UInt16) return sizeof(uint16);
		if (type == Enum::UInt32) return sizeof(uint32);
		if (type == Enum::UInt64) return sizeof(uint64);*/
		if (type == Enum::Byte) return sizeof(int8);
		if (type == Enum::Byte2) return sizeof(int8);
		if (type == Enum::Byte3) return sizeof(int8);
		if (type == Enum::Byte4) return sizeof(int8);
		if (type == Enum::Mat2) return sizeof(float32);
		if (type == Enum::Mat3) return sizeof(float32);
		if (type == Enum::Mat4) return sizeof(float32);
		return 0;
	}

	uint32 ShaderInternalType::ComponentSize(ShaderInternalType type) {
		return ComponentCount(type) * ComponentElementSize(type);
	}

	usize IndexBufferType::SizeOf(IndexBufferType indexType) {
		if (indexType == Enum::UInt8) return sizeof(uint8);
		if (indexType == Enum::UInt16) return sizeof(uint16);
		if (indexType == Enum::UInt32) return sizeof(uint32);
		if (indexType == Enum::UInt64) return sizeof(uint64);
		return 0;
	}

	bool TextureFormat::IsColor(TextureFormat format)
	{
		return format == TextureFormat::Enum::RGBA8 ||
			format == TextureFormat::Enum::RGB8 ||
			format == TextureFormat::Enum::SRGB8_ALPHA8 ||
			format == TextureFormat::Enum::RED_INTEGER;
	}

	bool TextureFormat::IsDepth(TextureFormat format)
	{
		return format == TextureFormat::Enum::DEPTH_COMPONENT16 ||
			format == TextureFormat::Enum::DEPTH_COMPONENT24 ||
			format == TextureFormat::Enum::DEPTH_COMPONENT32F;
	}

	bool TextureFormat::IsStencil(TextureFormat format)
	{
		return format == TextureFormat::Enum::STENCIL_INDEX8 ||
			format == TextureFormat::Enum::DEPTH24_STENCIL8 ||
			format == TextureFormat::Enum::DEPTH32F_STENCIL8;
	}

	Operation Operation::FromString(const std::string& operation)
	{
#define STR_OPERATION(op_i) if (std::string(#op_i) == operation) return Operation::Enum::op_i

		STR_OPERATION(Add);
		STR_OPERATION(Multiply);
		STR_OPERATION(Substract);
		STR_OPERATION(Divide);
		STR_OPERATION(Modulo);
		STR_OPERATION(And);
		STR_OPERATION(Or);
		STR_OPERATION(Xor);

		return Operation::Enum::NotDefine;
	}

}  //  nkentseu