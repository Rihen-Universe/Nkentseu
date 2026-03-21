//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-06-24 at 06:17:43 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __PRIMITIVE_MESH_H__
#define __PRIMITIVE_MESH_H__

#pragma once

#include <NTSCore/System.h>

#include <NTSMaths/Vector/Vector3.h>
#include <NTSMaths/Vector/Vector4.h>

namespace nkentseu {
	struct NKENTSEU_API VertexPrimitive {
		maths::Vector3f position;
		maths::Vector4f normal;
		maths::Vector2f texUV;
	};

	class NKENTSEU_API PrimitiveMesh {
	public:
		virtual void Set(std::vector<VertexPrimitive> vertices, std::vector<uint32> indices);
		std::vector<VertexPrimitive> GetVertices();
		std::vector<uint32> GetIndices();

		const std::vector<VertexPrimitive> GetVertices() const;
		const std::vector<uint32> GetIndices() const;

		std::vector<VertexPrimitive> vertices;
		std::vector<uint32> indices;
	};

	class NKENTSEU_API Cube : public PrimitiveMesh {
	public:
		Cube(float32 width = 1, float32 height = 1, float32 depth = 1);
		Cube(const maths::Vector3f& size);

		void SetSize(const maths::Vector3f& size);
		void SetSize(float32 width, float32 height, float32 depth);
		maths::Vector3f GetSize();

	protected:
		maths::Vector3f m_Size;
	};

	class NKENTSEU_API Plane : public PrimitiveMesh {
	public:
		Plane(float32 width = 1, float32 height = 1);
		Plane(const maths::Vector2f& size);

		void SetSize(const maths::Vector2f& size);
		void SetSize(float32 width, float32 height);
		maths::Vector2f GetSize() const;

	private:
		maths::Vector2f m_Size;
	};

	class NKENTSEU_API Sphere : public PrimitiveMesh {
	public:
		Sphere(float32 radius = 1, int32 slices = 8, int32 stacks = 8);

		void SetSlices(int32 slices);
		int32 GetSlices() const;
		void SetStacks(int32 stacks);
		int32 GetStacks() const;
		void SetRadius(float32 radius);
		float32 GetRadius() const;

	private:
		void CreateSphere(float32 radius, int32 slices, int32 stacks);

	private:
		float32 m_Radius;
		int32 m_Slices;
		int32 m_Stacks;
	};

	class NKENTSEU_API Tube : public PrimitiveMesh {
	public:
		Tube(float32 radius, float32 height, int32 segments) {
			SetRadius(radius);
			SetHeight(height);
			SetSegments(segments);
		}

		void SetRadius(float32 radius) {
			m_Radius = radius;
			CreateTube(m_Radius, m_Height, m_Segments);
		}

		float32 GetRadius() const {
			return m_Radius;
		}

		void SetHeight(float32 height) {
			m_Height = height;
			CreateTube(m_Radius, m_Height, m_Segments);
		}

		float32 GetHeight() const {
			return m_Height;
		}

		void SetSegments(int32 segments) {
			m_Segments = segments;
			CreateTube(m_Radius, m_Height, m_Segments);
		}

		int32 GetSegments() const {
			return m_Segments;
		}

	private:
		void CreateTube(float32 radius, float32 height, int32 segments) {
			vertices.clear();
			indices.clear();

			const int32 rings = 1;  // We're creating a tube, so it only has one ring of vertices
			const float32 ringAngle = maths::Pi2 / static_cast<float32>(segments);

			for (int32 i = 0; i <= segments; ++i) {
				float32 theta = i * ringAngle;
				float32 x = radius * cos(theta);
				float32 z = radius * sin(theta);

				vertices.push_back({ {x, -height / 2, z}, {0.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} });
				vertices.push_back({ {x, height / 2, z}, {0.0f, 1.0f, 0.0f, 1.0f},  {0.0f, 1.0f} });

				if (i > 0) {
					int32 currentIndex = i * 2;
					int32 prevIndex = currentIndex - 2;

					indices.push_back(prevIndex);
					indices.push_back(currentIndex - 1);
					indices.push_back(currentIndex);

					indices.push_back(prevIndex);
					indices.push_back(currentIndex);
					indices.push_back(prevIndex + 1);
				}
			}
		}

	private:
		float32 m_Radius;
		float32 m_Height;
		int32 m_Segments;
	};

	class NKENTSEU_API Capsule : public PrimitiveMesh {
	public:
		Capsule(float32 radius, float32 height, int32 segments);

		// Getters
		float32 GetRadius() const;
		float32 GetHeight() const;
		int32 GetSegments() const;

		// Setters
		void SetRadius(float32 radius);
		void SetHeight(float32 height);
		void SetSegments(int32 segments);

	private:
		void CreateCapsule(float32 radius, float32 height, int32 segments);

	private:
		float32 m_Radius;
		float32 m_Height;
		int32 m_Segments;
	};
}  //  nkentseu

#endif  // __PRIMITIVE_MESH_H__!