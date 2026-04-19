// NKSerialization/NkAllHeaders.h
// =============================================================================
// Header d'inclusion unique pour l'ensemble du module NKSerialization.
// Inclure ce fichier suffit pour accéder à toute l'API.
// =============================================================================
#pragma once

// ── Archive centrale ──────────────────────────────────────────────────────────
#include "NKSerialization/NkArchive.h"

// ── Interface sérialisable + registres ───────────────────────────────────────
#include "NKSerialization/NkISerializable.h"
#include "NKSerialization/NkSerializableRegistry.h"
#include "NKSerialization/NkSchemaVersioning.h"

// ── Point d'entrée unifié ─────────────────────────────────────────────────────
#include "NKSerialization/NkSerializer.h"

// ── Formats texte ─────────────────────────────────────────────────────────────
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/XML/NkXMLReader.h"
#include "NKSerialization/XML/NkXMLWriter.h"
#include "NKSerialization/YAML/NkYAMLReader.h"
#include "NKSerialization/YAML/NkYAMLWriter.h"

// ── Formats binaires ──────────────────────────────────────────────────────────
#include "NKSerialization/Binary/NkBinaryReader.h"
#include "NKSerialization/Binary/NkBinaryWriter.h"
#include "NKSerialization/Native/NkNativeFormat.h"

// ── Réflexion ─────────────────────────────────────────────────────────────────
#include "NKSerialization/Native/NkReflect.h"

// ── Système d'assets ──────────────────────────────────────────────────────────
#include "NKSerialization/Asset/NkAssetMetadata.h"
#include "NKSerialization/Asset/NkAssetImporter.h"

// ── Benchmark ────────────────────────────────────────────────────────────────
// #include "NKSerialization/NkSerializationBenchmark.h" // Inclure si nécessaire
