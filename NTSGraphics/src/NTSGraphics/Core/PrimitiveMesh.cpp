//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-06-24 at 06:17:43 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "PrimitiveMesh.h"

namespace nkentseu {
	void PrimitiveMesh::Set(std::vector<VertexPrimitive> vertices, std::vector<uint32> indices) {
		vertices = std::move(vertices);
		indices = std::move(indices);
	}

	std::vector<VertexPrimitive> PrimitiveMesh::GetVertices() {
		return vertices;
	}

	std::vector<uint32> PrimitiveMesh::GetIndices() {
		return indices;
	}

	const std::vector<VertexPrimitive> PrimitiveMesh::GetVertices() const {
		return vertices;
	}

	const std::vector<uint32> PrimitiveMesh::GetIndices() const {
		return indices;
	}

	Cube::Cube(float32 width, float32 height, float32 depth) {
		SetSize(maths::Vector3f(width, height, depth));
	}

	Cube::Cube(const maths::Vector3f& size) {
		SetSize(size);
	}

	void Cube::SetSize(const maths::Vector3f& size) {
		m_Size = size;

		// Vertices
		float32 halfWidth = m_Size.x / 2.0f;
		float32 halfHeight = m_Size.y / 2.0f;
		float32 halfDepth = m_Size.z / 2.0f;

		std::vector<VertexPrimitive> vertices = {
			// Front
			{ { -halfWidth, -halfHeight, halfDepth }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } },
			{ { halfWidth, -halfHeight, halfDepth }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f } },
			{ { halfWidth, halfHeight, halfDepth }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },
			{ { -halfWidth, halfHeight, halfDepth }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },

			// Back
			{ { halfWidth, -halfHeight, -halfDepth }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 0.0f, 0.0f } },
			{ { -halfWidth, -halfHeight, -halfDepth }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 1.0f, 0.0f } },
			{ { -halfWidth, halfHeight, -halfDepth }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 1.0f, 1.0f } },
			{ { halfWidth, halfHeight, -halfDepth }, { 0.0f, 0.0f, -1.0f, 1.0f }, { 0.0f, 1.0f } },

			// Top
			{ { -halfWidth, halfHeight, halfDepth }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
			{ { halfWidth, halfHeight, halfDepth }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			{ { halfWidth, halfHeight, -halfDepth }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
			{ { -halfWidth, halfHeight, -halfDepth }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },

			// Bottom
			{ { -halfWidth, -halfHeight, -halfDepth }, { 0.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
			{ { halfWidth, -halfHeight, -halfDepth }, { 0.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			{ { halfWidth, -halfHeight, halfDepth }, { 0.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
			{ { -halfWidth, -halfHeight, halfDepth }, { 0.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },

			// Left
			{ { -halfWidth, -halfHeight, -halfDepth }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
			{ { -halfWidth, -halfHeight, halfDepth }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			{ { -halfWidth, halfHeight, halfDepth }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
			{ { -halfWidth, halfHeight, -halfDepth }, { -1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },

			// Right
			{ { halfWidth, -halfHeight, halfDepth }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
			{ { halfWidth, -halfHeight, -halfDepth }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			{ { halfWidth, halfHeight, -halfDepth }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
			{ { halfWidth, halfHeight, halfDepth }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
		};

		std::vector<uint32> indices = {
			0, 1, 2, 2, 3, 0,       // Front
			4, 5, 6, 6, 7, 4,       // Back
			8, 9, 10, 10, 11, 8,    // Top
			12, 13, 14, 14, 15, 12, // Bottom
			16, 17, 18, 18, 19, 16, // Left
			20, 21, 22, 22, 23, 20  // Right
		};


		Set(vertices, indices);
	}

	void Cube::SetSize(float32 width, float32 height, float32 depth) { SetSize(maths::Vector3f(width, height, depth)); }

	maths::Vector3f Cube::GetSize() {
		return m_Size;
	}

	//---------------
	Plane::Plane(float32 width, float32 height) {
		SetSize(width, height);
	}

	Plane::Plane(const maths::Vector2f& size) {
		SetSize(size);
	}

	void Plane::SetSize(const maths::Vector2f& size) { SetSize(size); }

	void Plane::SetSize(float32 width, float32 height) {
		m_Size = maths::Vector2f(width, height);

		// Define vertices for the plane
		float32 halfWidth = width * 0.5f;
		float32 halfHeight = height * 0.5f;
		std::vector<VertexPrimitive> vertices = {
			{ { -halfWidth, 0.0f, halfHeight }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
			{ { halfWidth, 0.0f, halfHeight }, { 0.0f, 1.0f, 0.0f , 1.0f}, { 1.0f, 1.0f } },
			{ { halfWidth, 0.0f, -halfHeight }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			{ { -halfWidth, 0.0f, -halfHeight }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } }
		};

		// Define indices for the plane
		std::vector<uint32> indices = {
			0, 1, 2, 2, 3, 0
		};

		Set(vertices, indices);
	}

	maths::Vector2f Plane::GetSize() const {
		return m_Size;
	}

	//----------------
	Sphere::Sphere(float32 radius, int32 slices, int32 stacks) {
		CreateSphere(radius, slices, stacks);
	}

	void Sphere::CreateSphere(float32 radius, int32 slices, int32 stacks) {
		m_Radius = radius;
		m_Slices = slices;
		m_Stacks = stacks;

		std::vector<VertexPrimitive> vertices;
		std::vector<uint32> indices;

		float32 x, y, z, xy;                              // VertexPrimitive position
		float32 nx, ny, nz, lengthInv = 1.0f / radius;    // VertexPrimitive normal
		float32 s, t;                                     // VertexPrimitive texCoord

		float32 sectorStep = 2 * maths::Pi / m_Slices;
		float32 stackStep = maths::Pi / m_Stacks;
		float32 sectorAngle, stackAngle;

		for (int32 i = 0; i <= m_Stacks; ++i) {
			stackAngle = maths::Pi / 2 - i * stackStep;        // starting from Pi/2 to -Pi/2
			xy = radius * cosf(stackAngle);             // r * cos(u)
			z = radius * sinf(stackAngle);              // r * sin(u)

			// add (sectorCount+1) vertices per stack
			// first and last vertices have same position and normal, but different tex coords
			for (int32 j = 0; j <= m_Slices; ++j) {
				VertexPrimitive VertexPrimitive;

				sectorAngle = j * sectorStep;           // starting from 0 to 2Pi

				// VertexPrimitive position (x, y, z)
				x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
				y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
				VertexPrimitive.position = maths::Vector3f(x, y, z);

				// normalized VertexPrimitive normal (nx, ny, nz)
				nx = x * lengthInv;
				ny = y * lengthInv;
				nz = z * lengthInv;
				VertexPrimitive.normal = maths::Vector3f(nx, ny, nz).Normalized();

				// VertexPrimitive tex coord (s, t) range between [0, 1]
				s = (float32)j / m_Slices;
				t = (float32)i / m_Stacks;
				VertexPrimitive.texUV = maths::Vector2f(s, t);
			}
		}

		// generate CCW index list of sphere triangles
		// k1--k1+1
		// |  / |
		// | /  |
		// k2--k2+1
		std::vector<uint32> lineIndices;
		int32 k1, k2;
		for (int32 i = 0; i < m_Stacks; ++i) {
			k1 = i * (m_Slices + 1);     // beginning of current stack
			k2 = k1 + m_Slices + 1;      // beginning of next stack

			for (int32 j = 0; j < m_Slices; ++j, ++k1, ++k2) {
				// 2 triangles per sector excluding first and last stacks
				// k1 => k2 => k1+1
				if (i != 0) {
					indices.push_back(k1);
					indices.push_back(k2);
					indices.push_back(k1 + 1);
				}

				// k1+1 => k2 => k2+1
				if (i != (m_Stacks - 1)) {
					indices.push_back(k1 + 1);
					indices.push_back(k2);
					indices.push_back(k2 + 1);
				}

				// store indices for lines
				// vertical lines for all stacks, k1 => k2
				lineIndices.push_back(k1);
				lineIndices.push_back(k2);
				if (i != 0)  // horizontal lines except 1st stack, k1 => k+1
				{
					lineIndices.push_back(k1);
					lineIndices.push_back(k1 + 1);
				}
			}
		}

		Set(vertices, indices);
	}

	void Sphere::SetSlices(int32 slices) {
		m_Slices = slices;
		CreateSphere(m_Radius, m_Slices, m_Stacks);
	}

	int32 Sphere::GetSlices() const {
		return m_Slices;
	}

	void Sphere::SetStacks(int32 stacks) {
		m_Stacks = stacks;
		CreateSphere(m_Radius, m_Slices, m_Stacks);
	}

	int32 Sphere::GetStacks() const {
		return m_Stacks;
	}

	void Sphere::SetRadius(float32 radius) {
		m_Radius = radius;
		CreateSphere(m_Radius, m_Slices, m_Stacks);
	}

	float32 Sphere::GetRadius() const {
		return m_Radius;
	}

	//----------------

	// Constructeur
	Capsule::Capsule(float32 radius, float32 height, int32 segments)
		: m_Radius(radius), m_Height(height), m_Segments(segments) {
		CreateCapsule(m_Radius, m_Height, m_Segments);
	}

	// Getters
	float32 Capsule::GetRadius() const {
		return m_Radius;
	}

	float32 Capsule::GetHeight() const {
		return m_Height;
	}

	int32 Capsule::GetSegments() const {
		return m_Segments;
	}

	// Setters
	void Capsule::SetRadius(float32 radius) {
		m_Radius = radius;
		CreateCapsule(m_Radius, m_Height, m_Segments);
	}

	void Capsule::SetHeight(float32 height) {
		m_Height = height;
		CreateCapsule(m_Radius, m_Height, m_Segments);
	}

	void Capsule::SetSegments(int32 segments) {
		m_Segments = segments;
		CreateCapsule(m_Radius, m_Height, m_Segments);
	}

	void Capsule::CreateCapsule(float32 radius, float32 height, int32 segments) {
		// Clear previous data
		vertices.clear();
		indices.clear();

		// Calculate half height
		float32 halfHeight = height * 0.5f;

		// Create the top hemisphere
		for (int32 i = 0; i <= segments; ++i) {
			float32 phi = maths::Pi * (float32(i) / segments);
			for (int32 j = 0; j <= segments; ++j) {
				float32 theta = 2.0f * maths::Pi* (float32(j) / segments);
				maths::Vector3f VertexPrimitive;
				VertexPrimitive.x = radius * std::sinf(phi) * std::cosf(theta);
				VertexPrimitive.y = radius * std::cosf(phi) + halfHeight;
				VertexPrimitive.z = radius * std::sinf(phi) * std::sinf(theta);

				maths::Vector4f normal = (VertexPrimitive - maths::Vector3f(0.0f, halfHeight, 0.0f)).Normalized();
				maths::Vector2f texUV(theta / (2.0f * maths::Pi), phi / maths::Pi);

				vertices.push_back({ VertexPrimitive, normal, texUV });
			}
		}

		// Create the bottom hemisphere
		for (int32 i = 0; i <= segments; ++i) {
			float32 phi = maths::Pi * (float32(i) / segments);
			for (int32 j = 0; j <= segments; ++j) {
				float32 theta = 2.0f * maths::Pi* (float32(j) / segments);
				maths::Vector3f VertexPrimitive;
				VertexPrimitive.x = radius * std::sinf(phi) * std::cosf(theta);
				VertexPrimitive.y = -radius * std::cosf(phi) - halfHeight;
				VertexPrimitive.z = radius * std::sinf(phi) * std::sinf(theta);

				maths::Vector4f normal = (VertexPrimitive - maths::Vector3f(0.0f, -halfHeight, 0.0f)).Normalized();
				maths::Vector4f color(0.0f, 1.0f, 0.0f, 1.0f); // Green color
				maths::Vector2f texUV(theta / (2.0f * maths::Pi), phi / maths::Pi);

				vertices.push_back({ VertexPrimitive, normal, texUV });
			}
		}

		// Create the indices for the top hemisphere
		for (int32 i = 0; i < segments; ++i) {
			for (int32 j = 0; j < segments; ++j) {
				int32 first = i * (segments + 1) + j;
				int32 second = first + segments + 1;

				indices.push_back(first);
				indices.push_back(second);
				indices.push_back(first + 1);

				indices.push_back(second);
				indices.push_back(second + 1);
				indices.push_back(first + 1);
			}
		}

		// Create the indices for the bottom hemisphere
		int32 baseIndex = (segments + 1) * (segments + 1);
		for (int32 i = 0; i < segments; ++i) {
			for (int32 j = 0; j < segments; ++j) {
				int32 first = baseIndex + i * (segments + 1) + j;
				int32 second = first + segments + 1;

				indices.push_back(first);
				indices.push_back(first + 1);
				indices.push_back(second);

				indices.push_back(second);
				indices.push_back(first + 1);
				indices.push_back(second + 1);
			}
		}
	}
}  //  nkentseu