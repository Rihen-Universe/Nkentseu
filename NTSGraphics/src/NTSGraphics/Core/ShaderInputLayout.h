//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-06-29 at 10:04:30 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __SHADER_INPUT_LAYOUT_H__
#define __SHADER_INPUT_LAYOUT_H__

#pragma once

#include <NTSCore/System.h>

#include "Context.h"
#include "Shader.h"
#include "GraphicsEnum.h"

namespace nkentseu {
	class Shader;

	struct NKENTSEU_API VertexInputAttribute {
		std::string name;
		ShaderInternalType type;
		uint32 location;
		uint32 offset;
		uint32 size = 0;
		bool normalized = false;

		VertexInputAttribute() = default;
		VertexInputAttribute(std::string n, ShaderInternalType t, uint32 loc, bool norm = false);
	};

	struct NKENTSEU_API VertexInputLayout {
		std::vector<VertexInputAttribute> attributes;
		uint32 stride;
		uint32 componentCount = 0;

		VertexInputLayout() = default;
		VertexInputLayout(std::initializer_list<VertexInputAttribute> attrList);
		VertexInputLayout(std::vector<VertexInputAttribute> attrList);

		void AddAttribute(const VertexInputAttribute& attribute);
		const VertexInputAttribute& GetAttribut(uint32 index) const;
		const VertexInputAttribute& GetAttribut(const std::string& name) const;
		std::vector<VertexInputAttribute>::iterator begin();
		std::vector<VertexInputAttribute>::iterator end();
		std::vector<VertexInputAttribute>::const_iterator begin() const;
		std::vector<VertexInputAttribute>::const_iterator end() const;

	private:
		friend struct ShaderInputLayout;
		void CalculateOffsetsAndStride();
	};

	struct NKENTSEU_API UniformInputAttribute {
		std::string name = "";
		ShaderStage stage = ShaderStage::Enum::Vertex;
		BufferUsageType usage;
		uint32 offset = 0;
		uint32 size = 0;
		usize instance = 1;
		uint32 binding = 0;
		uint32 set;

		//void* data = nullptr;
		//usize dataSize = 0;

		friend std::ostream& operator<<(std::ostream& os, const UniformInputAttribute& e) {
			return os << e.ToString();
		}

		std::string ToString() const;

		friend std::string ToString(const UniformInputAttribute& v) {
			return v.ToString();
		}

		UniformInputAttribute() = default;
		UniformInputAttribute(std::string n, ShaderStage st, BufferUsageType usg, uint32 sz, uint32 bind = 0,
			uint32 set = 0, usize inst = 1);
	};

	struct NKENTSEU_API UniformInputLayout {
		std::vector<UniformInputAttribute> attributes;
		uint32 sizes = 0;
		usize maxBindingPoint = 0;

		UniformInputLayout() = default;
		UniformInputLayout(const UniformInputLayout& layout) = default;
		UniformInputLayout(std::initializer_list<UniformInputAttribute> attrList);
		UniformInputLayout(std::vector<UniformInputAttribute> attrList);

		void AddAttribute(const UniformInputAttribute& attribute);
		const UniformInputAttribute& GetAttribut(uint32 index) const;
		const UniformInputAttribute& GetAttribut(const std::string& name) const;

		std::vector<UniformInputAttribute>::iterator begin();
		std::vector<UniformInputAttribute>::iterator end();
		std::vector<UniformInputAttribute>::const_iterator begin() const;
		std::vector<UniformInputAttribute>::const_iterator end() const;

	private:
		friend struct ShaderInputLayout;
		void CalculateSize();
	};

	struct NKENTSEU_API SamplerInputAttribute {
		std::string name;
		uint32 binding;
		uint32 set;
		ShaderStage stage;
		SamplerType type;

		SamplerInputAttribute(std::string n, uint32 bind, uint32 set, ShaderStage st, SamplerType tp);
	};

	struct NKENTSEU_API SamplerInputLayout {
		std::vector<SamplerInputAttribute> attributes;

		SamplerInputLayout() = default;
		SamplerInputLayout(std::initializer_list<SamplerInputAttribute> attrList);
		SamplerInputLayout(std::vector<SamplerInputAttribute> attrList);

		void AddAttribute(const SamplerInputAttribute& attribute);
		const SamplerInputAttribute& GetAttribut(uint32 index) const;
		const SamplerInputAttribute& GetAttribut(const std::string& name) const;

		std::vector<SamplerInputAttribute>::iterator begin();
		std::vector<SamplerInputAttribute>::iterator end();
		std::vector<SamplerInputAttribute>::const_iterator begin() const;
		std::vector<SamplerInputAttribute>::const_iterator end() const;
	};

	struct NKENTSEU_API PushConstantInputAttribute {
		std::string name;
		ShaderStage stage;
		uint32 offset;
		uint32 size;

		PushConstantInputAttribute(std::string n, ShaderStage st, uint32 sz);
	};

	struct NKENTSEU_API PushConstantInputLayout {
		std::vector<PushConstantInputAttribute> attributes;
		uint32 sizes;

		PushConstantInputLayout() = default;
		PushConstantInputLayout(std::initializer_list<PushConstantInputAttribute> attrList);
		PushConstantInputLayout(std::vector<PushConstantInputAttribute> attrList);

		void AddAttribute(const PushConstantInputAttribute& attribute);
		const PushConstantInputAttribute& GetAttribut(uint32 index) const;
		const PushConstantInputAttribute& GetAttribut(const std::string& name) const;

		std::vector<PushConstantInputAttribute>::iterator begin();
		std::vector<PushConstantInputAttribute>::iterator end();
		std::vector<PushConstantInputAttribute>::const_iterator begin() const;
		std::vector<PushConstantInputAttribute>::const_iterator end() const;

	private:
		friend struct ShaderInputLayout;
		void CalculateSize();
	};

	class NKENTSEU_API ShaderInputLayout {
	public:
		VertexInputLayout vertexInput;
		UniformInputLayout uniformInput;
		SamplerInputLayout samplerInput;
		PushConstantInputLayout pushConstantInput;
		bool activateBlending = true;

		static Memory::Shared<ShaderInputLayout> Create(Memory::Shared<Context> context);

		virtual bool Initialize();
		virtual bool Release();

		virtual bool UpdatePushConstant(const std::string& name, void* data, usize size, Memory::Shared<Shader> shader = nullptr) = 0;

		//virtual bool BindUniformBuffer() = 0;
		//virtual bool BindTexture() = 0;
	};

	struct NKENTSEU_API ShaderFilePathAttribut {
		//std::filesystem::path path;
		std::string path;
		ShaderStage stage;

		ShaderFilePathAttribut() = default;
		ShaderFilePathAttribut(const std::string& path, const ShaderStage& stage);
	};

	class NKENTSEU_API ShaderFilePathLayout {
	public:
		std::vector<ShaderFilePathAttribut> attributs;

		ShaderFilePathLayout() = default;
		ShaderFilePathLayout(std::initializer_list<ShaderFilePathAttribut> attrList);
		bool AddPath(const std::string& path, const ShaderStage& stage); // on ajoute pas deux fichier de même type
		bool AddPath(const ShaderFilePathAttribut& attrib); // on ajoute pas deux fichier de même type

		usize size();
		usize size() const noexcept;

		std::vector<ShaderFilePathAttribut>::iterator begin();
		std::vector<ShaderFilePathAttribut>::iterator end();
		std::vector<ShaderFilePathAttribut>::const_iterator begin() const;
		std::vector<ShaderFilePathAttribut>::const_iterator end() const;
	};

}  //  nkentseu

#endif  // __SHADER_INPUT_LAYOUT_H__!