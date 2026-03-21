//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-19 at 08:13:44 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "Shape2D.h"

namespace nkentseu {

    void Shape2D::AddVertice(Vertex2D vertice)
    {
        vertices.push_back(vertice);
    }

    void Shape2D::AddVertices(const std::vector<Vertex2D>& vertices)
    {
        this->vertices.insert(this->vertices.end(), vertices.begin(), vertices.end());
    }

    Vertex2D Shape2D::GetVertice(uint32 index)
    {
        return vertices[index];
    }

    const std::vector<Vertex2D>& Shape2D::GetVertices()
    {
        return vertices;
    }

    void Shape2D::AddIndice(uint32 indice)
    {
        indices.push_back(indice);
    }

    void Shape2D::AddIndices(const std::vector<uint32>& indices)
    {
        this->indices.insert(this->indices.end(), indices.begin(), indices.end());
    }

    uint32 Shape2D::GetIndice(uint32 index)
    {
        return indices[index];
    }

    const std::vector<uint32>& Shape2D::GetIndices()
    {
        return indices;
    }

}  //  nkentseu