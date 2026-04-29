// =============================================================================
// NKNetwork/Protocol/NkBitStream.h
// =============================================================================
// DESCRIPTION :
//   Sérialisation bit-à-bit haute performance pour les messages réseau.
//
// OBJECTIF PRINCIPAL :
//   Réduire la bande passante réseau en encodant les données au niveau du bit
//   plutôt qu'au niveau de l'octet, permettant des économies significatives :
//   • Booléen : 1 bit au lieu de 8 bits (1 byte)
//   • Entier borné [0..3] : 2 bits au lieu de 32 bits (4 bytes)
//   • Float quantifié : précision configurable, bits adaptatifs
//
// CAS D'USAGE TYPIQUES :
//   • PacketHeader : seqNum(uint16) + ackMask(uint32) + flags(uint8) = 7 bytes
//   • Position 3D : float16 × 3 = 6 bytes au lieu de float32 × 3 = 12 bytes
//   • Delta position : int8 × 3 = 3 bytes pour petits mouvements
//   • Quaternion : compression "smallest 3" = 32 bits au lieu de 128 bits
//
// SÉCURITÉ ET ROBUSTESSE :
//   • Méthodes Read* retournent des valeurs par défaut si buffer épuisé
//   • Tester IsOverflowed() après lecture pour détecter corruption/troncation
//   • WriteBits/ReadBits gèrent l'alignement bit-à-bit automatiquement
//   • Protection contre les dépassements de buffer via flag mOverflow
//
// UTILISATION BASIQUE :
//   // Écriture dans un buffer
//   uint8 buffer[256];
//   NkBitWriter writer(buffer, sizeof(buffer));
//   writer.WriteU32(sequenceNumber);
//   writer.WriteBool(isPlayerAlive);
//   writer.WriteF32Q(playerX, -1000.f, 1000.f, 0.01f);  // Précision 1cm
//   writer.WriteString(playerName);
//   if (writer.IsOverflowed()) { /* Gérer erreur */ }
//
//   // Lecture depuis un buffer
//   NkBitReader reader(receivedBuffer, receivedSize);
//   uint32 seq = reader.ReadU32();
//   bool alive = reader.ReadBool();
//   float x = reader.ReadF32Q(-1000.f, 1000.f, 0.01f);
//   NkString name; reader.ReadString(name);
//   if (reader.IsOverflowed()) { /* Données corrompues */ }
//
// DÉPENDANCES :
//   • NkNetDefines.h    : Types fondamentaux (uint8, float32, etc.) et macros
//   • NKContainers/String/NkString.h : Gestion des chaînes pour sérialisation
//   • NKMath/NkMath.h   : Fonctions mathématiques (NkAbs, NkSqrt, etc.)
//
// RÈGLES D'UTILISATION :
//   • Allouer un buffer suffisamment grand avant création du writer/reader
//   • Vérifier IsOverflowed() après chaque séquence d'écriture/lecture
//   • Utiliser WriteF32Q/ReadF32Q pour les floats avec plage connue (économie)
//   • Aligner sur byte avant WriteBytes/ReadBytes pour performances optimales
//   • Les méthodes sont noexcept — gérer les erreurs via IsOverflowed()
//
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================

#pragma once

#ifndef NKENTSEU_NETWORK_NKBITSTREAM_H
#define NKENTSEU_NETWORK_NKBITSTREAM_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules requis dans l'ordre de dépendance hiérarchique.

    #include "NKNetwork/NkNetworkApi.h"
    #include "NKNetwork/Core/NkNetDefines.h"
    #include "NKContainers/String/NkString.h"
    #include "NKMath/NKMath.h"
    #include "NKCore/Assert/NkAssert.h"

    // En-têtes standards pour opérations mémoire
    #include <cstring>
    #include <cstdint>

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL ET DÉCLARATIONS
    // -------------------------------------------------------------------------
    // Toutes les déclarations du module résident dans nkentseu::net.

    namespace nkentseu {

        namespace net {

            // Import des types mathématiques pour commodité
            using namespace math;

            // =================================================================
            // CLASSE : NkBitWriter — Écriture bit-à-bit dans un buffer
            // =================================================================
            /**
             * @class NkBitWriter
             * @brief Encodeur bit-à-bit haute performance pour sérialisation réseau.
             *
             * Cette classe permet d'écrire des données dans un buffer mémoire
             * avec un granularité au niveau du bit, maximisant la compression :
             *   • Écriture de types primitifs (bool, uint8-64, int8-32, float32)
             *   • Quantification de floats avec précision configurable (WriteF32Q)
             *   • Encodage optimal d'entiers bornés (WriteInt)
             *   • Compression de vecteurs et quaternions (WriteVec3fQ, WriteQuatf)
             *   • Sérialisation de chaînes et buffers bruts
             *
             * MODÈLE DE FONCTIONNEMENT :
             *   • Buffer externe fourni à la construction (pas d'allocation interne)
             *   • Curseur de bit (mBitPos) gère l'écriture bit-à-bit
             *   • Flag d'overflow (mOverflow) signale les dépassements de capacité
             *   • Alignement automatique sur byte pour WriteBytes/ReadBytes
             *
             * @note Toutes les méthodes sont noexcept — vérifier IsOverflowed() pour erreurs
             * @note Thread-safe en écriture exclusive (un writer par buffer/thread)
             *
             * @example
             * @code
             * uint8 buffer[512];
             * NkBitWriter writer(buffer, sizeof(buffer));
             *
             * // Écriture de données de jeu
             * writer.WriteU32(playerId);
             * writer.WriteBool(isJumping);
             * writer.WriteF32Q(position.x, -500.f, 500.f, 0.01f);  // 1cm précision
             * writer.WriteF32Q(position.y, -500.f, 500.f, 0.01f);
             * writer.WriteF32Q(position.z, -500.f, 500.f, 0.01f);
             * writer.WriteString(playerName);
             *
             * // Vérification et envoi
             * if (!writer.IsOverflowed()) {
             *     socket.SendTo(buffer, writer.BytesWritten(), remoteAddr);
             * } else {
             *     NK_NET_LOG_ERROR("Buffer trop petit pour le paquet");
             * }
             * @endcode
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkBitWriter
            {
            public:

                // -------------------------------------------------------------
                // Constructeur
                // -------------------------------------------------------------

                /**
                 * @brief Initialise un writer sur un buffer externe.
                 * @param buf Pointeur vers le buffer de destination (doit rester valide).
                 * @param capacity Taille du buffer en bytes.
                 * @note Le buffer n'est pas possédé — durée de vie gérée par l'appelant.
                 * @note Tous les compteurs sont initialisés à zéro.
                 */
                NkBitWriter(uint8* buf, uint32 capacity) noexcept;

                // -------------------------------------------------------------
                // Écriture de types primitifs — encodage standard
                // -------------------------------------------------------------

                /**
                 * @brief Écrit un booléen sur 1 bit.
                 * @param v Valeur à écrire (true = 1, false = 0).
                 * @note Économie : 1 bit au lieu de 8 bits pour un bool standard.
                 */
                void WriteBool(bool v) noexcept;

                /**
                 * @brief Écrit un uint8 sur 8 bits.
                 * @param v Valeur à écrire (0-255).
                 */
                void WriteU8(uint8 v) noexcept;

                /**
                 * @brief Écrit un uint16 sur 16 bits.
                 * @param v Valeur à écrire (0-65535).
                 */
                void WriteU16(uint16 v) noexcept;

                /**
                 * @brief Écrit un uint32 sur 32 bits.
                 * @param v Valeur à écrire (0-4294967295).
                 */
                void WriteU32(uint32 v) noexcept;

                /**
                 * @brief Écrit un uint64 sur 64 bits (en deux fois 32 bits).
                 * @param v Valeur à écrire (0-2^64-1).
                 * @note Écriture little-endian : low 32 bits d'abord, puis high 32 bits.
                 */
                void WriteU64(uint64 v) noexcept;

                /**
                 * @brief Écrit un int8 sur 8 bits (cast vers uint8).
                 * @param v Valeur à écrire (-128 à 127).
                 */
                void WriteI8(int8 v) noexcept;

                /**
                 * @brief Écrit un int16 sur 16 bits (cast vers uint16).
                 * @param v Valeur à écrire (-32768 à 32767).
                 */
                void WriteI16(int16 v) noexcept;

                /**
                 * @brief Écrit un int32 sur 32 bits (cast vers uint32).
                 * @param v Valeur à écrire (-2147483648 à 2147483647).
                 */
                void WriteI32(int32 v) noexcept;

                /**
                 * @brief Écrit un float32 en copiant sa représentation binaire IEEE 754.
                 * @param v Valeur float à écrire.
                 * @note Utilise std::memcpy pour éviter les violations de strict aliasing.
                 * @note 32 bits exacts — pas de compression.
                 */
                void WriteF32(float32 v) noexcept;

                // -------------------------------------------------------------
                // Écriture de types quantifiés — économie de bande passante
                // -------------------------------------------------------------

                /**
                 * @brief Écrit un float32 quantifié sur N bits avec précision configurable.
                 * @param v Valeur float à écrire.
                 * @param minV Valeur minimale de la plage de quantification.
                 * @param maxV Valeur maximale de la plage de quantification.
                 * @param prec Précision souhaitée (ex: 0.01f = 1cm pour des positions).
                 * @note Algorithme : normalisation → quantification → encodage sur bits optimaux.
                 * @note Économie typique : 10-16 bits au lieu de 32 bits pour un float.
                 * @example
                 * @code
                 * // Position avec précision 1cm sur plage [-1000m, +1000m]
                 * writer.WriteF32Q(position.x, -1000.f, 1000.f, 0.01f);
                 * // Résultat : ~16 bits au lieu de 32 bits
                 * @endcode
                 */
                void WriteF32Q(float32 v, float32 minV, float32 maxV, float32 prec) noexcept;

                /**
                 * @brief Écrit un entier borné [minV..maxV] sur le nombre minimal de bits.
                 * @param v Valeur entière à écrire.
                 * @param minV Borne inférieure inclusive.
                 * @param maxV Borne supérieure inclusive.
                 * @note Assertion en debug si v hors plage [minV, maxV].
                 * @note Économie : log2(maxV-minV+1) bits au lieu de 32 bits.
                 * @example
                 * @code
                 * // État de jeu sur 4 valeurs possibles : 2 bits au lieu de 32
                 * writer.WriteInt(gameState, 0, 3);  // gameState ∈ [0,3]
                 * @endcode
                 */
                void WriteInt(int32 v, int32 minV, int32 maxV) noexcept;

                // -------------------------------------------------------------
                // Écriture de types composites — vecteurs et quaternions
                // -------------------------------------------------------------

                /**
                 * @brief Écrit un vecteur 3D en écrivant chaque composante float32.
                 * @param v Vecteur à écrire (x, y, z).
                 * @note 96 bits totaux (32 bits × 3 composantes).
                 * @see WriteVec3fQ Pour version quantifiée avec économie de bits.
                 */
                void WriteVec3f(const NkVec3f& v) noexcept;

                /**
                 * @brief Écrit un vecteur 3D quantifié avec précision configurable.
                 * @param v Vecteur à écrire (x, y, z).
                 * @param minV Valeur minimale pour chaque composante.
                 * @param maxV Valeur maximale pour chaque composante.
                 * @param prec Précision souhaitée pour chaque composante.
                 * @note Délègue à WriteF32Q pour chaque axe — économie ~50% vs WriteVec3f.
                 */
                void WriteVec3fQ(const NkVec3f& v, float32 minV, float32 maxV, float32 prec) noexcept;

                /**
                 * @brief Écrit un quaternion avec compression "smallest 3 components".
                 * @param q Quaternion à écrire (x, y, z, w).
                 * @note Algorithme :
                 *   1. Trouver la composante de plus grande valeur absolue
                 *   2. Encoder son index sur 2 bits
                 *   3. Encoder les 3 autres composantes quantifiées sur ~10 bits chacune
                 *   4. Reconstruire la composante manquante via norme unitaire à la lecture
                 * @note Résultat : 32 bits totaux au lieu de 128 bits (float32 × 4).
                 * @note Précision : ~0.0001 sur composantes [-0.7072, +0.7072].
                 * @warning Le quaternion doit être normalisé avant appel.
                 */
                void WriteQuatf(const NkQuatf& q) noexcept;

                /**
                 * @brief Écrit une chaîne C-style avec préfixe de longueur.
                 * @param s Pointeur vers la chaîne à écrire (peut être nullptr).
                 * @param maxLen Longueur maximale autorisée (défaut: 256).
                 * @note Format : uint16 length + length × uint8 characters.
                 * @note Si s == nullptr : écrit length = 0.
                 * @note Tronque silencieusement si len > maxLen.
                 */
                void WriteString(const char* s, uint32 maxLen = 256) noexcept;

                /**
                 * @brief Écrit un buffer brut d'octets avec alignement sur byte.
                 * @param data Pointeur vers les données à écrire.
                 * @param size Nombre d'octets à écrire.
                 * @note Appel automatique à AlignToByte() avant écriture.
                 * @note Définit mOverflow si dépassement de capacité.
                 */
                void WriteBytes(const uint8* data, uint32 size) noexcept;

                // -------------------------------------------------------------
                // Écriture de bits bruts — méthode de bas niveau
                // -------------------------------------------------------------

                /**
                 * @brief Écrit numBits bits de la valeur v dans le buffer.
                 * @param v Valeur contenant les bits à écrire (LSB en premier).
                 * @param numBits Nombre de bits à écrire (1-32).
                 * @note Écriture bit-à-bit : bit de poids fort de v en premier.
                 * @note Définit mOverflow si dépassement de capacité.
                 * @note Méthode de base utilisée par toutes les autres Write*.
                 */
                void WriteBits(uint32 v, uint32 numBits) noexcept;

                /**
                 * @brief Aligne le curseur de bit sur la prochaine frontière de byte.
                 * @note Ajoute des bits de bourrage (padding) si nécessaire.
                 * @note Requis avant WriteBytes/ReadBytes pour accès mémoire direct.
                 */
                void AlignToByte() noexcept;

                // -------------------------------------------------------------
                // Accesseurs — état et métriques du writer
                // -------------------------------------------------------------

                /**
                 * @brief Retourne le nombre total de bits écrits.
                 * @return Compteur de bits (0 à capacity×8).
                 */
                [[nodiscard]] uint32 BitsWritten() const noexcept;

                /**
                 * @brief Retourne le nombre total d'octets écrits (arrondi au supérieur).
                 * @return (BitsWritten() + 7) / 8.
                 * @note Utiliser pour connaître la taille réelle à envoyer sur le réseau.
                 */
                [[nodiscard]] uint32 BytesWritten() const noexcept;

                /**
                 * @brief Teste si un dépassement de buffer a été détecté.
                 * @return true si une écriture a échoué par manque de place.
                 * @note Une fois true, reste true — toutes les écritures suivantes sont ignorées.
                 */
                [[nodiscard]] bool IsOverflowed() const noexcept;

            private:

                // -------------------------------------------------------------
                // Utilitaire privé — calcul du nombre de bits requis
                // -------------------------------------------------------------

                /**
                 * @brief Calcule le nombre minimal de bits pour représenter maxVal.
                 * @param maxVal Valeur maximale à encoder.
                 * @return Nombre de bits : ceil(log2(maxVal + 1)), minimum 1.
                 * @note Utilisé par WriteInt et WriteF32Q pour encodage optimal.
                 */
                [[nodiscard]] static uint32 BitsRequired(uint32 maxVal) noexcept;

                // -------------------------------------------------------------
                // Membres privés — état interne du writer
                // -------------------------------------------------------------

                /// Pointeur vers le buffer de destination (non possédé).
                uint8* mBuf = nullptr;

                /// Capacité du buffer en bytes.
                uint32 mCap = 0;

                /// Position courante en bits dans le buffer (0 à mCap×8).
                uint32 mBitPos = 0;

                /// Flag d'erreur : true si dépassement de capacité détecté.
                bool mOverflow = false;
            };

            // =================================================================
            // CLASSE : NkBitReader — Lecture bit-à-bit depuis un buffer
            // =================================================================
            /**
             * @class NkBitReader
             * @brief Décodeur bit-à-bit haute performance pour désérialisation réseau.
             *
             * Cette classe permet de lire des données depuis un buffer mémoire
             * avec granularité bit-à-bit, symétrique à NkBitWriter :
             *   • Lecture de types primitifs (bool, uint8-64, int8-32, float32)
             *   • Déquantification de floats avec plage/précision connue (ReadF32Q)
             *   • Décodage optimal d'entiers bornés (ReadInt)
             *   • Décompression de vecteurs et quaternions (ReadVec3fQ, ReadQuatf)
             *   • Désérialisation de chaînes et buffers bruts
             *
             * MODÈLE DE FONCTIONNEMENT :
             *   • Buffer en lecture seule fourni à la construction
             *   • Curseur de bit (mBitPos) gère la lecture bit-à-bit
             *   • Flag d'overflow (mOverflow) signale buffer épuisé ou corrompu
             *   • Valeurs par défaut retournées en cas d'erreur (0, false, etc.)
             *
             * @note Toutes les méthodes sont noexcept — vérifier IsOverflowed() pour erreurs
             * @note Thread-safe en lecture exclusive (un reader par buffer/thread)
             * @warning Toujours lire dans le même ordre que l'écriture pour cohérence
             *
             * @example
             * @code
             * NkBitReader reader(receivedBuffer, receivedSize);
             *
             * // Lecture dans le même ordre que l'écriture
             * uint32 playerId = reader.ReadU32();
             * bool isJumping = reader.ReadBool();
             * float x = reader.ReadF32Q(-500.f, 500.f, 0.01f);
             * float y = reader.ReadF32Q(-500.f, 500.f, 0.01f);
             * float z = reader.ReadF32Q(-500.f, 500.f, 0.01f);
             * NkString playerName; reader.ReadString(playerName);
             *
             * // Vérification d'intégrité
             * if (reader.IsOverflowed()) {
             *     NK_NET_LOG_ERROR("Paquet corrompu ou tronqué");
             *     return;  // Ignorer le paquet
             * }
             *
             * // Traitement des données valides
             * ProcessPlayerUpdate(playerId, isJumping, {x,y,z}, playerName);
             * @endcode
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkBitReader
            {
            public:

                // -------------------------------------------------------------
                // Constructeur
                // -------------------------------------------------------------

                /**
                 * @brief Initialise un reader sur un buffer en lecture seule.
                 * @param buf Pointeur vers le buffer source (doit rester valide).
                 * @param size Taille du buffer en bytes.
                 * @note Le buffer n'est pas possédé — durée de vie gérée par l'appelant.
                 * @note mSize est converti en bits pour cohérence avec mBitPos.
                 */
                NkBitReader(const uint8* buf, uint32 size) noexcept;

                // -------------------------------------------------------------
                // Lecture de types primitifs — décodage standard
                // -------------------------------------------------------------

                /**
                 * @brief Lit un booléen sur 1 bit.
                 * @return true si bit = 1, false si bit = 0.
                 * @note Retourne false par défaut si buffer épuisé ou overflow.
                 */
                [[nodiscard]] bool ReadBool() noexcept;

                /**
                 * @brief Lit un uint8 sur 8 bits.
                 * @return Valeur lue (0-255), ou 0 si erreur.
                 */
                [[nodiscard]] uint8 ReadU8() noexcept;

                /**
                 * @brief Lit un uint16 sur 16 bits.
                 * @return Valeur lue (0-65535), ou 0 si erreur.
                 */
                [[nodiscard]] uint16 ReadU16() noexcept;

                /**
                 * @brief Lit un uint32 sur 32 bits.
                 * @return Valeur lue (0-4294967295), ou 0 si erreur.
                 */
                [[nodiscard]] uint32 ReadU32() noexcept;

                /**
                 * @brief Lit un uint64 sur 64 bits (en deux fois 32 bits).
                 * @return Valeur lue, ou 0 si erreur.
                 * @note Lecture little-endian : low 32 bits d'abord, puis high 32 bits.
                 */
                [[nodiscard]] uint64 ReadU64() noexcept;

                /**
                 * @brief Lit un int8 sur 8 bits (cast depuis uint8).
                 * @return Valeur lue (-128 à 127), ou 0 si erreur.
                 */
                [[nodiscard]] int8 ReadI8() noexcept;

                /**
                 * @brief Lit un int16 sur 16 bits (cast depuis uint16).
                 * @return Valeur lue (-32768 à 32767), ou 0 si erreur.
                 */
                [[nodiscard]] int16 ReadI16() noexcept;

                /**
                 * @brief Lit un int32 sur 32 bits (cast depuis uint32).
                 * @return Valeur lue (-2147483648 à 2147483647), ou 0 si erreur.
                 */
                [[nodiscard]] int32 ReadI32() noexcept;

                /**
                 * @brief Lit un float32 en copiant sa représentation binaire IEEE 754.
                 * @return Valeur float lue, ou 0.0f si erreur.
                 * @note Utilise std::memcpy pour éviter les violations de strict aliasing.
                 */
                [[nodiscard]] float32 ReadF32() noexcept;

                // -------------------------------------------------------------
                // Lecture de types quantifiés — déquantification
                // -------------------------------------------------------------

                /**
                 * @brief Lit un float32 quantifié et le déquantifie avec précision connue.
                 * @param minV Valeur minimale de la plage de quantification.
                 * @param maxV Valeur maximale de la plage de quantification.
                 * @param prec Précision utilisée à l'écriture (doit correspondre).
                 * @return Valeur float déquantifiée dans [minV, maxV], ou minV si erreur.
                 * @note Doit utiliser les mêmes paramètres que WriteF32Q à l'écriture.
                 * @note Symétrique de WriteF32Q — essentiel pour cohérence client/serveur.
                 */
                [[nodiscard]] float32 ReadF32Q(float32 minV, float32 maxV, float32 prec) noexcept;

                /**
                 * @brief Lit un entier borné [minV..maxV] encodé sur bits optimaux.
                 * @param minV Borne inférieure inclusive (doit correspondre à l'écriture).
                 * @param maxV Borne supérieure inclusive (doit correspondre à l'écriture).
                 * @return Valeur entière dans [minV, maxV], ou minV si erreur.
                 * @note Symétrique de WriteInt — ordre et paramètres doivent matcher.
                 */
                [[nodiscard]] int32 ReadInt(int32 minV, int32 maxV) noexcept;

                // -------------------------------------------------------------
                // Lecture de types composites — vecteurs et quaternions
                // -------------------------------------------------------------

                /**
                 * @brief Lit un vecteur 3D en lisant chaque composante float32.
                 * @return Vecteur {x, y, z} lu, ou {0,0,0} si erreur.
                 * @note 96 bits totaux lus (32 bits × 3 composantes).
                 * @see ReadVec3fQ Pour version quantifiée avec économie de bits.
                 */
                [[nodiscard]] NkVec3f ReadVec3f() noexcept;

                /**
                 * @brief Lit un vecteur 3D quantifié avec précision connue.
                 * @param minV Valeur minimale pour chaque composante.
                 * @param maxV Valeur maximale pour chaque composante.
                 * @param prec Précision utilisée à l'écriture (doit correspondre).
                 * @return Vecteur déquantifié {x, y, z}, ou {minV,minV,minV} si erreur.
                 * @note Délègue à ReadF32Q pour chaque axe — symétrique de WriteVec3fQ.
                 */
                [[nodiscard]] NkVec3f ReadVec3fQ(float32 minV, float32 maxV, float32 prec) noexcept;

                /**
                 * @brief Lit un quaternion compressé "smallest 3 components".
                 * @return Quaternion reconstruit {x, y, z, w}, ou identité si erreur.
                 * @note Algorithme symétrique de WriteQuatf :
                 *   1. Lire l'index de la composante manquante (2 bits)
                 *   2. Lire les 3 composantes quantifiées (~10 bits chacune)
                 *   3. Reconstruire la 4ème composante via norme unitaire : w = ±sqrt(1-x²-y²-z²)
                 *   4. Appliquer le signe basé sur la composante de plus grande valeur
                 * @note Précision : ~0.0001 sur composantes reconstruites.
                 * @warning Le quaternion résultant peut nécessiter renormalisation.
                 */
                [[nodiscard]] NkQuatf ReadQuatf() noexcept;

                /**
                 * @brief Lit une chaîne C-style avec préfixe de longueur.
                 * @param out Référence vers NkString à remplir avec la valeur lue.
                 * @param maxLen Longueur maximale autorisée (défaut: 256).
                 * @note Format attendu : uint16 length + length × uint8 characters.
                 * @note Définit mOverflow si len > maxLen ou dépassement de buffer.
                 * @note La chaîne résultante est toujours terminée par '\0'.
                 */
                void ReadString(NkString& out, uint32 maxLen = 256) noexcept;

                /**
                 * @brief Lit un buffer brut d'octets avec alignement sur byte.
                 * @param dst Pointeur vers le buffer de destination.
                 * @param size Nombre d'octets à lire.
                 * @note Appel automatique à AlignToByte() avant lecture.
                 * @note Définit mOverflow si dépassement de buffer source.
                 * @note dst doit avoir au moins size bytes disponibles.
                 */
                void ReadBytes(uint8* dst, uint32 size) noexcept;

                // -------------------------------------------------------------
                // Lecture de bits bruts — méthode de bas niveau
                // -------------------------------------------------------------

                /**
                 * @brief Lit numBits bits depuis le buffer et retourne la valeur.
                 * @param numBits Nombre de bits à lire (1-32).
                 * @return Valeur lue avec bits de poids fort en premier, ou 0 si erreur.
                 * @note Méthode de base utilisée par toutes les autres Read*.
                 * @note Définit mOverflow si buffer épuisé ou numBits invalide.
                 */
                [[nodiscard]] uint32 ReadBits(uint32 numBits) noexcept;

                /**
                 * @brief Aligne le curseur de bit sur la prochaine frontière de byte.
                 * @note Saute les bits de bourrage (padding) si nécessaire.
                 * @note Requis avant WriteBytes/ReadBytes pour accès mémoire direct.
                 */
                void AlignToByte() noexcept;

                // -------------------------------------------------------------
                // Accesseurs — état et métriques du reader
                // -------------------------------------------------------------

                /**
                 * @brief Retourne le nombre total de bits lus.
                 * @return Compteur de bits (0 à mSize).
                 */
                [[nodiscard]] uint32 BitsRead() const noexcept;

                /**
                 * @brief Retourne le nombre total d'octets lus (arrondi au supérieur).
                 * @return (BitsRead() + 7) / 8.
                 * @note Utiliser pour debugging ou calcul de progression.
                 */
                [[nodiscard]] uint32 BytesRead() const noexcept;

                /**
                 * @brief Retourne le nombre de bits restants à lire dans le buffer.
                 * @return mSize - mBitPos, ou 0 si déjà à la fin.
                 * @note Utile pour validation : un paquet bien formé doit avoir BitsLeft() == 0 à la fin.
                 */
                [[nodiscard]] uint32 BitsLeft() const noexcept;

                /**
                 * @brief Teste si un dépassement ou erreur de lecture a été détecté.
                 * @return true si buffer épuisé prématurément ou lecture invalide.
                 * @note Une fois true, reste true — toutes les lectures suivantes retournent des valeurs par défaut.
                 */
                [[nodiscard]] bool IsOverflowed() const noexcept;

                /**
                 * @brief Teste si le buffer est entièrement consommé.
                 * @return true si BitsLeft() == 0.
                 * @note Utile pour validation de fin de paquet dans les protocoles variable-length.
                 */
                [[nodiscard]] bool IsEmpty() const noexcept;

            private:

                // -------------------------------------------------------------
                // Utilitaire privé — calcul du nombre de bits requis
                // -------------------------------------------------------------

                /**
                 * @brief Calcule le nombre minimal de bits pour représenter maxVal.
                 * @param maxVal Valeur maximale à encoder.
                 * @return Nombre de bits : ceil(log2(maxVal + 1)), minimum 1.
                 * @note Identique à la version de NkBitWriter pour cohérence.
                 */
                [[nodiscard]] static uint32 BitsRequired(uint32 maxVal) noexcept;

                // -------------------------------------------------------------
                // Membres privés — état interne du reader
                // -------------------------------------------------------------

                /// Pointeur vers le buffer source en lecture seule (non possédé).
                const uint8* mBuf = nullptr;

                /// Taille totale du buffer en bits (size × 8).
                uint32 mSize = 0;

                /// Position courante en bits dans le buffer (0 à mSize).
                uint32 mBitPos = 0;

                /// Flag d'erreur : true si buffer épuisé ou lecture invalide.
                bool mOverflow = false;
            };

        } // namespace net

    } // namespace nkentseu

#endif // NKENTSEU_NETWORK_NKBITSTREAM_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKBITSTREAM.H
// =============================================================================
// Ce fichier fournit la sérialisation bit-à-bit pour optimisation réseau.
// Les exemples suivants illustrent les cas d'usage courants.

// -----------------------------------------------------------------------------
// Exemple 1 : Sérialisation d'un paquet de mouvement de joueur
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Protocol/NkBitStream.h"
    #include "NKMath/NkVec3.h"

    struct PlayerMoveUpdate
    {
        uint32 playerId;
        bool isJumping;
        bool isCrouching;
        nkentseu::math::NkVec3f position;
        nkentseu::math::NkVec3f velocity;
        float32 yaw;  // Rotation horizontale [0, 360[
    };

    void SerializePlayerMove(const PlayerMoveUpdate& move, uint8* outBuf, uint32 bufSize)
    {
        using namespace nkentseu::net;

        NkBitWriter writer(outBuf, bufSize);

        // Identifiant et flags : 32 + 1 + 1 = 34 bits
        writer.WriteU32(move.playerId);
        writer.WriteBool(move.isJumping);
        writer.WriteBool(move.isCrouching);

        // Position quantifiée : précision 1cm sur [-500m, +500m] = ~16 bits/composante
        writer.WriteVec3fQ(move.position, -500.f, 500.f, 0.01f);

        // Velocity quantifiée : précision 0.1 m/s sur [-50, +50] = ~10 bits/composante
        writer.WriteVec3fQ(move.velocity, -50.f, 50.f, 0.1f);

        // Yaw quantifié : précision 0.1° sur [0, 360[ = 12 bits
        writer.WriteF32Q(move.yaw, 0.f, 360.f, 0.1f);

        // Vérification et utilisation
        if (!writer.IsOverflowed())
        {
            // writer.BytesWritten() donne la taille exacte à envoyer
            SendNetworkPacket(outBuf, writer.BytesWritten());
        }
        else
        {
            NK_NET_LOG_ERROR("Buffer trop petit pour PlayerMoveUpdate");
        }
    }

    PlayerMoveUpdate DeserializePlayerMove(const uint8* inBuf, uint32 inSize)
    {
        using namespace nkentseu::net;

        NkBitReader reader(inBuf, inSize);
        PlayerMoveUpdate move = {};

        move.playerId = reader.ReadU32();
        move.isJumping = reader.ReadBool();
        move.isCrouching = reader.ReadBool();
        move.position = reader.ReadVec3fQ(-500.f, 500.f, 0.01f);
        move.velocity = reader.ReadVec3fQ(-50.f, 50.f, 0.1f);
        move.yaw = reader.ReadF32Q(0.f, 360.f, 0.1f);

        // Validation d'intégrité
        if (reader.IsOverflowed())
        {
            NK_NET_LOG_WARN("Paquet PlayerMoveUpdate corrompu ou tronqué");
            return {};  // Retourner état par défaut sécurisé
        }

        // Optionnel : vérifier que tout le buffer a été consommé
        if (reader.BitsLeft() > 7)  // Tolérance de 1 byte de padding
        {
            NK_NET_LOG_DEBUG("Bits non consommés en fin de paquet : {}", reader.BitsLeft());
        }

        return move;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Compression de quaternion pour rotation d'entité
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Protocol/NkBitStream.h"
    #include "NKMath/NkQuat.h"

    void SerializeRotation(const nkentseu::math::NkQuatf& rotation, uint8* outBuf)
    {
        using namespace nkentseu::net;

        NkBitWriter writer(outBuf, 64);  // 64 bytes = 512 bits, largement suffisant

        // Quaternion compressé : 32 bits au lieu de 128 bits
        writer.WriteQuatf(rotation);

        // Taille réelle : 4 bytes exacts
        NK_NET_LOG_DEBUG("Quaternion sérialisé en {} bytes", writer.BytesWritten());
    }

    nkentseu::math::NkQuatf DeserializeRotation(const uint8* inBuf)
    {
        using namespace nkentseu::net;

        NkBitReader reader(inBuf, 4);  // 4 bytes = 32 bits pour quaternion compressé
        return reader.ReadQuatf();
    }

    // Note : Le quaternion reconstruit peut nécessiter une renormalisation
    // pour compenser les erreurs d'arrondi de quantification :
    // quat = NkNormalize(quat);
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Encodage adaptatif d'entiers bornés
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Protocol/NkBitStream.h"

    void SerializeGameState(uint8* outBuf)
    {
        using namespace nkentseu::net;

        NkBitWriter writer(outBuf, 128);

        // État du jeu sur 4 valeurs : 2 bits au lieu de 32
        enum class GameState : uint8 { Lobby, Playing, Paused, Ended };
        writer.WriteInt(static_cast<int32>(currentState), 0, 3);

        // Nombre de joueurs [0..64] : 6 bits au lieu de 32
        writer.WriteInt(playerCount, 0, 64);

        // Score [0..999999] : 20 bits au lieu de 32
        writer.WriteInt(currentScore, 0, 999999);

        // Timer de round [0..300] secondes : 9 bits au lieu de 32
        writer.WriteInt(roundTimer, 0, 300);

        // Total : 2+6+20+9 = 37 bits ≈ 5 bytes au lieu de 16 bytes
    }

    GameState DeserializeGameState(const uint8* inBuf, int32& outPlayerCount,
                                    int32& outScore, int32& outTimer)
    {
        using namespace nkentseu::net;

        NkBitReader reader(inBuf, 8);  // 8 bytes = 64 bits, suffisant pour 37 bits

        int32 stateVal = reader.ReadInt(0, 3);
        outPlayerCount = reader.ReadInt(0, 64);
        outScore = reader.ReadInt(0, 999999);
        outTimer = reader.ReadInt(0, 300);

        if (reader.IsOverflowed())
        {
            return GameState::Lobby;  // Valeur par défaut sécurisée
        }

        return static_cast<GameState>(stateVal);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Sérialisation de chaîne avec validation
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Protocol/NkBitStream.h"

    bool SerializeChatMessage(const char* playerName, const char* message,
                               uint8* outBuf, uint32 bufSize)
    {
        using namespace nkentseu::net;

        NkBitWriter writer(outBuf, bufSize);

        // Nom du joueur : max 32 caractères
        writer.WriteString(playerName, 32);

        // Message : max 256 caractères
        writer.WriteString(message, 256);

        if (writer.IsOverflowed())
        {
            NK_NET_LOG_WARN("Chat message trop long pour le buffer");
            return false;
        }

        return true;
    }

    bool DeserializeChatMessage(const uint8* inBuf, uint32 inSize,
                                 NkString& outPlayer, NkString& outMessage)
    {
        using namespace nkentseu::net;

        NkBitReader reader(inBuf, inSize);

        reader.ReadString(outPlayer, 32);
        reader.ReadString(outMessage, 256);

        if (reader.IsOverflowed())
        {
            NK_NET_LOG_WARN("Chat message corrompu ou tronqué");
            return false;
        }

        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Protocole variable-length avec validation de fin de paquet
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Protocol/NkBitStream.h"

    struct VariablePacket
    {
        uint8 messageType;
        uint32 payloadSize;
        uint8 payload[1024];  // Variable
    };

    bool SerializeVariablePacket(const VariablePacket& pkt, uint8* outBuf, uint32 bufSize)
    {
        using namespace nkentseu::net;

        NkBitWriter writer(outBuf, bufSize);

        // En-tête fixe
        writer.WriteU8(pkt.messageType);
        writer.WriteU32(pkt.payloadSize);

        // Validation de la taille avant écriture variable
        if (pkt.payloadSize > 1024)
        {
            NK_NET_LOG_ERROR("Payload size invalide : {}", pkt.payloadSize);
            return false;
        }

        // Alignement sur byte avant données brutes
        writer.AlignToByte();

        // Payload variable
        writer.WriteBytes(pkt.payload, pkt.payloadSize);

        return !writer.IsOverflowed();
    }

    bool DeserializeVariablePacket(const uint8* inBuf, uint32 inSize,
                                    VariablePacket& outPkt)
    {
        using namespace nkentseu::net;

        NkBitReader reader(inBuf, inSize);

        outPkt.messageType = reader.ReadU8();
        outPkt.payloadSize = reader.ReadU32();

        // Validation de la taille lue
        if (outPkt.payloadSize > 1024 || reader.IsOverflowed())
        {
            NK_NET_LOG_WARN("Payload size invalide ou buffer corrompu");
            return false;
        }

        // Alignement sur byte avant lecture des données brutes
        reader.AlignToByte();

        // Lecture du payload
        reader.ReadBytes(outPkt.payload, outPkt.payloadSize);

        if (reader.IsOverflowed())
        {
            NK_NET_LOG_WARN("Lecture du payload échouée : buffer trop court");
            return false;
        }

        // Optionnel : vérifier qu'il ne reste pas de données non consommées
        // (utile pour détecter les paquets mal formés ou les attaques)
        if (reader.BitsLeft() > 7)  // Tolérance 1 byte de padding
        {
            NK_NET_LOG_DEBUG("Données non consommées en fin de paquet : {} bits",
                reader.BitsLeft());
            // Selon la politique de sécurité : ignorer ou rejeter le paquet
        }

        return true;
    }
*/

// -----------------------------------------------------------------------------
// Bonnes pratiques et notes d'utilisation
// -----------------------------------------------------------------------------
/*
    Ordre de sérialisation :
    -----------------------
    - Toujours lire dans le MÊME ordre que l'écriture
    - Documenter l'ordre des champs dans un fichier de protocole dédié
    - Utiliser des structs de sérialisation pour garantir la cohérence

    Quantification des floats :
    --------------------------
    - Choisir minV/maxV/prec en fonction des besoins réels du jeu
    - Exemple : position [-1000, +1000] avec préc 0.01 → 16 bits vs 32 bits
    - Tester la précision visuelle pour éviter les artefacts de quantification

    Gestion des erreurs :
    --------------------
    - Toujours vérifier IsOverflowed() après une séquence de lecture
    - Retourner des valeurs par défaut sécurisées en cas d'erreur
    - Logger les paquets corrompus pour debugging et détection d'attaques

    Performance :
    ------------
    - Les méthodes sont inline-friendly — le compilateur optimise les appels
    - WriteBits/ReadBits bouclent bit-à-bit — éviter pour gros buffers bruts
    - Utiliser WriteBytes/ReadBytes avec AlignToByte() pour données binaires

    Sécurité :
    ---------
    - Valider les valeurs lues (bornes, plages attendues) avant utilisation
    - Limiter maxLen pour WriteString/ReadString pour éviter les allocations excessives
    - Rejeter les paquets avec BitsLeft() significatif en fin de lecture (anti-tampering)

    Debugging :
    ----------
    - Utiliser BitsWritten()/BytesWritten() pour profiler la taille des paquets
    - Comparer la taille théorique vs réelle pour détecter les inefficacités
    - Activer le logging des IsOverflowed() en développement pour identifier les buffers trop petits

    Compatibilité client/serveur :
    -----------------------------
    - Les paramètres de quantification (minV/maxV/prec) doivent être IDENTIQUES
    - Utiliser des constantes partagées (dans un header commun) pour éviter les divergences
    - Versionner le format de sérialisation si évolution nécessaire

    Extensions futures :
    -------------------
    - Support des tableaux dynamiques avec préfixe de longueur
    - Compression delta pour valeurs consécutives (positions, timestamps)
    - Chiffrement intégré des buffers via WriteBytes/ReadBytes chiffrés
    - Checksum/CRC pour détection de corruption en plus de IsOverflowed()
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================