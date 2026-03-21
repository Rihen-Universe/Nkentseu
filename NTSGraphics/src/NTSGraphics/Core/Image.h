//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 4/19/2024 at 12:16:32 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_IMAGE_H__
#define __NKENTSEU_IMAGE_H__

#pragma once

#include "NTSCore/System.h"
#include "NTSCore/Stream/InputStream.h"
#include <NTSMaths/Vector/Vector2.h>
#include "NTSMaths/Color.h"
#include <NTSMaths/Shapes/Rectangle.h>

#include "GraphicsEnum.h"

#include <atomic>
#include <filesystem>

namespace nkentseu {
	class NKENTSEU_API Image {
	public:
		Image();
		Image(const Image& image);
		~Image();
		Image& operator=(const Image& image);

		bool LoadFromFile(const std::filesystem::path& path, bool flipVertical = true);

		bool Create(const maths::Vector2u& size, uint32 channels = 4);
		bool Create(uint32 width, uint32 height, uint32 channels = 4);
		bool Create(const maths::Vector2u& size, const maths::Color& color);
		bool Create(const maths::Vector2u& size, const uint8* pixels, uint32 channels = 4);
		bool Create(const maths::Vector2u& size, const float32* pixels, uint32 channels = 4);
		void CreateMaskFromColor(const maths::Color& color, uint8 alpha = 0);

		Image Clone() const;

		maths::Vector2u GetSize() const;
		uint32 GetChannels() const;
		uint8* GetPixels(); 
		const uint8* GetPixels() const;
		std::vector<maths::Color> GetColors() const;

		bool Save(const std::string& path, bool flipVertical = false);
		bool Save(std::vector<uint8>& output, std::string_view format) const;

		uint32 GetPixel(const maths::Vector2i& position) const;
		uint32 GetPixel(int32 x, int32 y) const;

		uint8 GetPixelRed(const maths::Vector2i& position) const;
		uint8 GetPixelRed(int32 x, int32 y) const;

		uint8 GetPixelGreen(const maths::Vector2i& position) const;
		uint8 GetPixelGreen(int32 x, int32 y) const;

		uint8 GetPixelBlue(const maths::Vector2i& position) const;
		uint8 GetPixelBlue(int32 x, int32 y) const;

		uint8 GetPixelAlpha(const maths::Vector2i& position) const;
		uint8 GetPixelAlpha(int32 x, int32 y) const;

		maths::Color GetColor(const maths::Vector2i& position) const;
		maths::Color GetColor(int32 x, int32 y) const;

		bool SetPixel(const maths::Vector2i& position, uint8 r, uint8 g, uint8 b, uint8 a);
		bool SetPixel(const maths::Vector2i& position, uint8 r, uint8 g, uint8 b);
		bool SetPixel(int32 x, int32 y, uint8 r, uint8 g, uint8 b, uint8 a);
		bool SetPixel(int32 x, int32 y, uint8 r, uint8 g, uint8 b);

		bool SetColor(const maths::Vector2i& position, const maths::Color& color);
		bool SetColor(int32 x, int32 y, const maths::Color& color);

		bool Update(const uint8* data, const maths::Vector2i& offset, const maths::Vector2u& size);

		// Modification de la teinte d'une image par une autre
		void ModifyHue(const Image& hueImage);

		// R�cup�rer une partie de l'image
		Image GetSubImage(const maths::Vector2i& position, const maths::Vector2u& size) const;

		// Convertir une image en niveaux de gris
		void ConvertToGrayscale();

		// Convertir une image en noir et blanc
		void ConvertToBlackAndWhite(uint8 threshold = 128);

		// Generer une image aleatoire
		void GenerateRandomImage();
		void GenerateRandomImageWithAlpha();

		// Generer une heightmap aleatoire
		void GenerateRandomHeightmap();

		// Assombrir une image
		Image Darken(float32 amount = 0.5f) const;

		// eclaircir une image
		Image Lighten(float32 amount = 0.5f) const;

		void AddColor(const maths::Color& color, float32 amount);
		void AttenuateColor(const maths::Color& color, float32 amount);

		void ReplaceColor(const maths::Color& colorToReplace, const maths::Color& newColor);

		void FindBoundingBox(maths::Vector2i point, maths::Color color, maths::Vector2i& bbox_position, maths::Vector2u& bbox_size);
		std::vector<maths::Rectangle> FindAllBoundingBox(maths::Color color);

		void FindBoundingBoxOptimize(maths::Vector2i point, maths::Color color, maths::Vector2i& bbox_position, maths::Vector2u& bbox_size);
		std::vector<maths::Rectangle> FindAllBoundingBoxOptimize(maths::Color color);

		Image FindSubImage(maths::Vector2i point, maths::Color color);
		std::vector<Image> FindAllSubImage(maths::Color color);

		std::vector<Image> FindAllSubImageOptimize(maths::Color color);
		Image FindSubImageOptimize(maths::Vector2i point, maths::Color color);

		Image DoOperation(Image& other, Operation operation);
		Image DoOperation(Image& other, maths::Vector2i otherPosition, Operation operation);
	private:
		maths::Vector2u m_Size;
		uint32 m_Channels = 4;
		std::vector<uint8> m_Data;
		//std::vector<uint8> m_Data;
	};
} // namespace nkentseu

#endif    // __NKENTSEU_IMAGE_H__