// -----------------------------------------------------------------------------
// FICHIER: NKContainers/String/NkStringFormat.h
// DESCRIPTION: Système de formatage de chaînes unifié
//              Supporte deux syntaxes : placeholders {i:props} et style printf %d
//              Permet l'extension via spécialisation de NkToString<T>
// AUTEUR: Rihen
// DATE: 2026-04-26
// VERSION: 2.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_STRING_NKSTRINGFORMAT_H
    #define NK_CONTAINERS_STRING_NKSTRINGFORMAT_H

    // -------------------------------------------------------------------------
    // INCLUSIONS DES DÉPENDANCES DU PROJET
    // -------------------------------------------------------------------------
    #include "NkString.h"
    #include "NkStringView.h"
    #include "NKCore/NkTypes.h"
    #include "NKCore/NkTraits.h"
    #include "NKMemory/NkAllocator.h"

    // -------------------------------------------------------------------------
    // INCLUSIONS STANDARD
    // -------------------------------------------------------------------------
    #include <cctype>
    #include <cstdio>
    #include <cstdarg>
    #include <cmath>

    // -------------------------------------------------------------------------
    // NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu
    {
        // ---------------------------------------------------------------------
        // NAMESPACE DÉDIÉ AU FORMATAGE DE CHAÎNES
        // ---------------------------------------------------------------------
        namespace string
        {
            // =================================================================
            // SECTION 1 : PROPRIÉTÉS DE FORMATAGE (PARTAGÉES)
            // =================================================================

            // -----------------------------------------------------------------
            // Énumération : Alignement du texte dans le champ
            // -----------------------------------------------------------------
            enum class NkFormatAlign : unsigned char
            {
                Unset  = 0,  ///< Aucun alignement (par défaut)
                Left   = 1,  ///< Alignement à gauche (<)
                Right  = 2,  ///< Alignement à droite (>)
                Center = 3,  ///< Centrage (^)
            };

            // -----------------------------------------------------------------
            // Énumération : Base numérique pour l'affichage des entiers
            // -----------------------------------------------------------------
            enum class NkFormatBase : unsigned char
            {
                Dec      = 10,  ///< Décimal (par défaut)
                Hex      = 16,  ///< Hexadécimal minuscules (hex)
                HexUpper = 17,  ///< Hexadécimal majuscules (HEX)
                Oct      = 8,   ///< Octal (oct)
                Bin      = 2,   ///< Binaire (bin)
            };

            // -----------------------------------------------------------------
            // Énumération : Style d'affichage des flottants
            // -----------------------------------------------------------------
            enum class NkFormatFloat : unsigned char
            {
                Default    = 0,  ///< Notation fixe (%f)
                Scientific = 1,  ///< Notation scientifique minuscule (%e)
                SciUpper   = 2,  ///< Notation scientifique majuscule (%E)
                General    = 3,  ///< Notation compacte automatique (%g)
            };

            // -----------------------------------------------------------------
            // Structure : Propriétés de formatage parsées depuis un placeholder
            // -----------------------------------------------------------------
            struct NkFormatProps
            {
                int           width      = 0;   ///< Largeur minimale du champ
                int           precision  = -1;  ///< Précision (-1 = non spécifiée)
                NkFormatAlign align      = NkFormatAlign::Unset;  ///< Alignement
                char          fill       = ' ';  ///< Caractère de remplissage
                NkFormatBase  base       = NkFormatBase::Dec;  ///< Base numérique
                NkFormatFloat floatStyle = NkFormatFloat::Default;  ///< Style flottant
                bool          showSign   = false;  ///< Toujours afficher le signe (+)
                bool          showPrefix = false;  ///< Afficher préfixe 0x/0b/0o (#)
                bool          upperStr   = false;  ///< Convertir chaîne en majuscules
                bool          lowerStr   = false;  ///< Convertir chaîne en minuscules

                // -----------------------------------------------------------------
                // Méthodes utilitaires pour vérifier la présence d'options
                // -----------------------------------------------------------------
                bool HasWidth() const
                {
                    return width > 0;
                }

                bool HasPrecision() const
                {
                    return precision >= 0;
                }
            };

            // =================================================================
            // SECTION 2 : TRAIT DE CONVERSION NkToString (PLACEHOLDER STYLE)
            // =================================================================

            // -----------------------------------------------------------------
            // Trait principal : spécialiser pour vos types personnalisés
            // -----------------------------------------------------------------
            /**
             * @brief Trait de conversion pour le style placeholder {i:props}
             * @tparam T Type à convertir en NkString
             * @note Spécialisez ce trait dans votre code pour supporter vos types
             *       Exemple : template<> struct NkToString<Vec3> { ... };
             */
            template <typename T>
            struct NkToString
            {
                // Pas de méthode Convert() par défaut → erreur de compilation explicite
                // si l'utilisateur tente de formater un type non-spécialisé.
            };

            // -----------------------------------------------------------------
            // Helpers internes pour la conversion des types primitifs
            // -----------------------------------------------------------------
            NkString NkIntToString(long long v, const NkFormatProps& props);
            NkString NkUIntToString(unsigned long long v, const NkFormatProps& props);
            NkString NkFloatToString(double v, const NkFormatProps& props);

            // -----------------------------------------------------------------
            // Spécialisations pour les types primitifs intégrés
            // -----------------------------------------------------------------

            // --- bool ---
            template <>
            struct NkToString<bool>
            {
                static NkString Convert(bool v, const NkFormatProps& props)
                {
                    NkString s(v ? "true" : "false");
                    if (props.upperStr)
                    {
                        s.ToUpper();
                    }
                    return NkApplyFormatProps(s, props);
                }
            };

            // --- char ---
            template <>
            struct NkToString<char>
            {
                static NkString Convert(char v, const NkFormatProps& props)
                {
                    return NkApplyFormatProps(NkString(1u, v), props);
                }
            };

            // --- Entiers signés ---
            template <>
            struct NkToString<signed char>
            {
                static NkString Convert(signed char v, const NkFormatProps& p)
                {
                    return NkIntToString(v, p);
                }
            };

            template <>
            struct NkToString<short>
            {
                static NkString Convert(short v, const NkFormatProps& p)
                {
                    return NkIntToString(v, p);
                }
            };

            template <>
            struct NkToString<int>
            {
                static NkString Convert(int v, const NkFormatProps& p)
                {
                    return NkIntToString(v, p);
                }
            };

            template <>
            struct NkToString<long>
            {
                static NkString Convert(long v, const NkFormatProps& p)
                {
                    return NkIntToString(v, p);
                }
            };

            template <>
            struct NkToString<long long>
            {
                static NkString Convert(long long v, const NkFormatProps& p)
                {
                    return NkIntToString(v, p);
                }
            };

            // --- Entiers non-signés ---
            template <>
            struct NkToString<unsigned char>
            {
                static NkString Convert(unsigned char v, const NkFormatProps& p)
                {
                    return NkUIntToString(v, p);
                }
            };

            template <>
            struct NkToString<unsigned short>
            {
                static NkString Convert(unsigned short v, const NkFormatProps& p)
                {
                    return NkUIntToString(v, p);
                }
            };

            template <>
            struct NkToString<unsigned int>
            {
                static NkString Convert(unsigned int v, const NkFormatProps& p)
                {
                    return NkUIntToString(v, p);
                }
            };

            template <>
            struct NkToString<unsigned long>
            {
                static NkString Convert(unsigned long v, const NkFormatProps& p)
                {
                    return NkUIntToString(v, p);
                }
            };

            template <>
            struct NkToString<unsigned long long>
            {
                static NkString Convert(unsigned long long v, const NkFormatProps& p)
                {
                    return NkUIntToString(v, p);
                }
            };

            // --- Flottants ---
            template <>
            struct NkToString<float>
            {
                static NkString Convert(float v, const NkFormatProps& p)
                {
                    return NkFloatToString(static_cast<double>(v), p);
                }
            };

            template <>
            struct NkToString<double>
            {
                static NkString Convert(double v, const NkFormatProps& p)
                {
                    return NkFloatToString(v, p);
                }
            };

            template <>
            struct NkToString<long double>
            {
                static NkString Convert(long double v, const NkFormatProps& p)
                {
                    return NkFloatToString(static_cast<double>(v), p);
                }
            };

            // --- Chaînes de caractères ---
            template <>
            struct NkToString<const char*>
            {
                static NkString Convert(const char* v, const NkFormatProps& props)
                {
                    NkString s(v ? v : "(null)");
                    if (props.upperStr)
                    {
                        s.ToUpper();
                    }
                    else if (props.lowerStr)
                    {
                        s.ToLower();
                    }
                    if (props.HasPrecision() && static_cast<int>(s.Length()) > props.precision)
                    {
                        s.Resize(static_cast<NkString::SizeType>(props.precision));
                    }
                    return NkApplyFormatProps(s, props);
                }
            };

            template <>
            struct NkToString<char*>
            {
                static NkString Convert(char* v, const NkFormatProps& p)
                {
                    return NkToString<const char*>::Convert(v, p);
                }
            };

            template <>
            struct NkToString<NkString>
            {
                static NkString Convert(const NkString& v, const NkFormatProps& props)
                {
                    NkString s(v);
                    if (props.upperStr)
                    {
                        s.ToUpper();
                    }
                    else if (props.lowerStr)
                    {
                        s.ToLower();
                    }
                    if (props.HasPrecision() && static_cast<int>(s.Length()) > props.precision)
                    {
                        s.Resize(static_cast<NkString::SizeType>(props.precision));
                    }
                    return NkApplyFormatProps(s, props);
                }
            };

            // --- Pointeurs ---
            template <>
            struct NkToString<void*>
            {
                static NkString Convert(void* v, const NkFormatProps&)
                {
                    char buf[32];
                    ::snprintf(buf, sizeof(buf), "%p", v);
                    return NkString(buf);
                }
            };

            template <>
            struct NkToString<const void*>
            {
                static NkString Convert(const void* v, const NkFormatProps&)
                {
                    char buf[32];
                    ::snprintf(buf, sizeof(buf), "%p", v);
                    return NkString(buf);
                }
            };

            template <typename T>
            struct NkToString<T*>
            {
                static NkString Convert(T* v, const NkFormatProps&)
                {
                    char buf[32];
                    ::snprintf(buf, sizeof(buf), "%p", static_cast<const void*>(v));
                    return NkString(buf);
                }
            };

            // =================================================================
            // SECTION 3 : TRAIT DE CONVERSION NkToFormatf (PRINTF STYLE)
            // =================================================================

            // -----------------------------------------------------------------
            // Trait pour le style printf : spécialiser pour types personnalisés
            // -----------------------------------------------------------------
            /**
             * @brief Trait de conversion pour le style printf %d, %s, etc.
             * @tparam T Type à convertir en NkString
             * @param opts Chaîne d'options brute entre % et le spécificateur
             * @note Spécialisez ce trait pour supporter vos types avec printf-style
             *       Exemple : template<> struct NkToFormatf<Vec3> { ... };
             */
            template <typename T>
            struct NkToFormatf
            {
                // Pas de méthode Convert() par défaut → erreur explicite si non-spécialisé
            };

            // -----------------------------------------------------------------
            // Spécialisations pour les types primitifs intégrés (printf-style)
            // -----------------------------------------------------------------

            // --- bool ---
            template <>
            struct NkToFormatf<bool>
            {
                static NkString Convert(bool v, const char*)
                {
                    return NkString(v ? "true" : "false");
                }
            };

            // --- char ---
            template <>
            struct NkToFormatf<char>
            {
                static NkString Convert(char v, const char* opts)
                {
                    char buf[8];
                    char fmt[16];
                    if (opts && *opts)
                    {
                        ::snprintf(fmt, sizeof(fmt), "%%%sc", opts);
                    }
                    else
                    {
                        fmt[0] = '%';
                        fmt[1] = 'c';
                        fmt[2] = '\0';
                    }
                    ::snprintf(buf, sizeof(buf), fmt, v);
                    return NkString(buf);
                }
            };

            // --- Entiers signés ---
            template <>
            struct NkToFormatf<signed char>
            {
                static NkString Convert(signed char v, const char* opts)
                {
                    char buf[32];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%sd", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, static_cast<int>(v));
                    return NkString(buf);
                }
            };

            template <>
            struct NkToFormatf<short>
            {
                static NkString Convert(short v, const char* opts)
                {
                    char buf[32];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%shd", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, v);
                    return NkString(buf);
                }
            };

            template <>
            struct NkToFormatf<int>
            {
                static NkString Convert(int v, const char* opts)
                {
                    char buf[32];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%sd", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, v);
                    return NkString(buf);
                }
            };

            template <>
            struct NkToFormatf<long>
            {
                static NkString Convert(long v, const char* opts)
                {
                    char buf[32];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%sld", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, v);
                    return NkString(buf);
                }
            };

            template <>
            struct NkToFormatf<long long>
            {
                static NkString Convert(long long v, const char* opts)
                {
                    char buf[32];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%slld", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, v);
                    return NkString(buf);
                }
            };

            // --- Entiers non-signés ---
            template <>
            struct NkToFormatf<unsigned char>
            {
                static NkString Convert(unsigned char v, const char* opts)
                {
                    char buf[32];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%su", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, static_cast<unsigned>(v));
                    return NkString(buf);
                }
            };

            template <>
            struct NkToFormatf<unsigned short>
            {
                static NkString Convert(unsigned short v, const char* opts)
                {
                    char buf[32];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%shu", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, v);
                    return NkString(buf);
                }
            };

            template <>
            struct NkToFormatf<unsigned int>
            {
                static NkString Convert(unsigned int v, const char* opts)
                {
                    char buf[32];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%su", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, v);
                    return NkString(buf);
                }
            };

            template <>
            struct NkToFormatf<unsigned long>
            {
                static NkString Convert(unsigned long v, const char* opts)
                {
                    char buf[32];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%slu", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, v);
                    return NkString(buf);
                }
            };

            template <>
            struct NkToFormatf<unsigned long long>
            {
                static NkString Convert(unsigned long long v, const char* opts)
                {
                    char buf[32];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%sllu", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, v);
                    return NkString(buf);
                }
            };

            // --- Flottants ---
            template <>
            struct NkToFormatf<float>
            {
                static NkString Convert(float v, const char* opts)
                {
                    char buf[64];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%sf", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, static_cast<double>(v));
                    return NkString(buf);
                }
            };

            template <>
            struct NkToFormatf<double>
            {
                static NkString Convert(double v, const char* opts)
                {
                    char buf[64];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%sf", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, v);
                    return NkString(buf);
                }
            };

            template <>
            struct NkToFormatf<long double>
            {
                static NkString Convert(long double v, const char* opts)
                {
                    char buf[64];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%sLf", opts && *opts ? opts : "");
                    ::snprintf(buf, sizeof(buf), fmt, v);
                    return NkString(buf);
                }
            };

            // --- Chaînes ---
            template <>
            struct NkToFormatf<const char*>
            {
                static NkString Convert(const char* v, const char* opts)
                {
                    if (!v)
                    {
                        return NkString("(null)");
                    }
                    if (!opts || !*opts)
                    {
                        return NkString(v);
                    }
                    char buf[512];
                    char fmt[16];
                    ::snprintf(fmt, sizeof(fmt), "%%%ss", opts);
                    ::snprintf(buf, sizeof(buf), fmt, v);
                    return NkString(buf);
                }
            };

            template <>
            struct NkToFormatf<char*>
            {
                static NkString Convert(char* v, const char* opts)
                {
                    return NkToFormatf<const char*>::Convert(v, opts);
                }
            };

            template <>
            struct NkToFormatf<NkString>
            {
                static NkString Convert(const NkString& v, const char* opts)
                {
                    return NkToFormatf<const char*>::Convert(v.CStr(), opts);
                }
            };

            // --- Pointeurs ---
            template <>
            struct NkToFormatf<void*>
            {
                static NkString Convert(void* v, const char*)
                {
                    char buf[32];
                    ::snprintf(buf, sizeof(buf), "%p", v);
                    return NkString(buf);
                }
            };

            template <typename T>
            struct NkToFormatf<T*>
            {
                static NkString Convert(T* v, const char*)
                {
                    char buf[32];
                    ::snprintf(buf, sizeof(buf), "%p", static_cast<void*>(v));
                    return NkString(buf);
                }
            };

            // =================================================================
            // SECTION 4 : ARGUMENTS DE FORMATAGE (TYPE-ERASED)
            // =================================================================

            // -----------------------------------------------------------------
            // Structure représentant un argument formatable (type effacé)
            // -----------------------------------------------------------------
            struct NkFormatArg
            {
                const void* data = nullptr;
                NkString (*convert)(const void*, const NkFormatProps&) = nullptr;
            };

            // -----------------------------------------------------------------
            // Factory : crée un NkFormatArg pour un type donné
            // -----------------------------------------------------------------
            template <typename T>
            inline NkFormatArg NkMakeFormatArg(const T& value)
            {
                NkFormatArg arg;
                arg.data = static_cast<const void*>(&value);
                arg.convert = [](const void* d, const NkFormatProps& p) -> NkString
                {
                    return NkToString<T>::Convert(*static_cast<const T*>(d), p);
                };
                return arg;
            }

            // -----------------------------------------------------------------
            // Spécialisation pour const char* : le pointeur EST la donnée
            // -----------------------------------------------------------------
            inline NkFormatArg NkMakeFormatArg(const char* value)
            {
                NkFormatArg arg;
                arg.data = static_cast<const void*>(value);
                arg.convert = [](const void* d, const NkFormatProps& p) -> NkString
                {
                    return NkToString<const char*>::Convert(static_cast<const char*>(d), p);
                };
                return arg;
            }

            // =================================================================
            // SECTION 5 : BUFFER D'ARGUMENTS HYBRIDE (SSO / HEAP)
            // =================================================================

            // -----------------------------------------------------------------
            // Constante : nombre d'arguments stockés sur la pile (SSO)
            // -----------------------------------------------------------------
            static constexpr int NK_FORMAT_SSO_ARGS = 16;

            // -----------------------------------------------------------------
            // Classe : buffer dynamique avec optimisation SSO
            // -----------------------------------------------------------------
            class NkFormatArgBuffer
            {
            public:
                // -----------------------------------------------------------------
                // Constructeur : initialise avec allocateur optionnel
                // -----------------------------------------------------------------
                explicit NkFormatArgBuffer(memory::NkIAllocator* alloc = nullptr) noexcept
                    : mData(mSSO)
                    , mCount(0)
                    , mCapacity(NK_FORMAT_SSO_ARGS)
                    , mAllocator(alloc)
                    , mHeap(nullptr)
                {
                }

                // -----------------------------------------------------------------
                // Destructeur : libère la mémoire heap si nécessaire
                // -----------------------------------------------------------------
                ~NkFormatArgBuffer()
                {
                    FreeHeap();
                }

                // -----------------------------------------------------------------
                // Suppression des copies : sécurité des pointeurs vers temporaires
                // -----------------------------------------------------------------
                NkFormatArgBuffer(const NkFormatArgBuffer&) = delete;
                NkFormatArgBuffer& operator=(const NkFormatArgBuffer&) = delete;

                // -----------------------------------------------------------------
                // Ajoute un argument au buffer (avec croissance automatique)
                // -----------------------------------------------------------------
                void Push(const NkFormatArg& arg)
                {
                    if (mCount == mCapacity)
                    {
                        Grow();
                    }
                    mData[mCount++] = arg;
                }

                // -----------------------------------------------------------------
                // Accesseurs en lecture seule
                // -----------------------------------------------------------------
                const NkFormatArg* Data() const noexcept
                {
                    return mData;
                }

                int Count() const noexcept
                {
                    return mCount;
                }

            private:
                // -----------------------------------------------------------------
                // Buffer stack pour le cas SSO (pas d'allocation dynamique)
                // -----------------------------------------------------------------
                NkFormatArg mSSO[NK_FORMAT_SSO_ARGS];

                // -----------------------------------------------------------------
                // Pointeur vers les données actuelles (SSO ou heap)
                // -----------------------------------------------------------------
                NkFormatArg* mData;

                // -----------------------------------------------------------------
                // Nombre d'arguments actuellement stockés
                // -----------------------------------------------------------------
                int mCount;

                // -----------------------------------------------------------------
                // Capacité actuelle du buffer
                // -----------------------------------------------------------------
                int mCapacity;

                // -----------------------------------------------------------------
                // Allocateur personnalisé (peut être nullptr)
                // -----------------------------------------------------------------
                memory::NkIAllocator* mAllocator;

                // -----------------------------------------------------------------
                // Buffer heap alloué dynamiquement en cas de débordement SSO
                // -----------------------------------------------------------------
                NkFormatArg* mHeap;

                // -----------------------------------------------------------------
                // Libère la mémoire heap et revient au mode SSO
                // -----------------------------------------------------------------
                void FreeHeap()
                {
                    if (mHeap)
                    {
                        if (mAllocator)
                        {
                            mAllocator->Deallocate(mHeap);
                        }
                        else
                        {
                            ::operator delete(static_cast<void*>(mHeap));
                        }
                        mHeap = nullptr;
                        mData = mSSO;
                    }
                }

                // -----------------------------------------------------------------
                // Double la capacité et migre vers le heap si nécessaire
                // -----------------------------------------------------------------
                void Grow()
                {
                    int newCap = mCapacity * 2;
                    NkFormatArg* newBuf;

                    if (mAllocator)
                    {
                        newBuf = static_cast<NkFormatArg*>(
                            mAllocator->Allocate(static_cast<usize>(newCap) * sizeof(NkFormatArg))
                        );
                    }
                    else
                    {
                        newBuf = static_cast<NkFormatArg*>(
                            ::operator new(static_cast<size_t>(newCap) * sizeof(NkFormatArg))
                        );
                    }

                    for (int i = 0; i < mCount; ++i)
                    {
                        newBuf[i] = mData[i];
                    }

                    FreeHeap();
                    mHeap = newBuf;
                    mData = newBuf;
                    mCapacity = newCap;
                }
            };

            // =================================================================
            // SECTION 6 : API PUBLIQUE — PLACEHOLDER STYLE {i:props}
            // =================================================================

            // -----------------------------------------------------------------
            // Parse une chaîne de propriétés → structure NkFormatProps
            // -----------------------------------------------------------------
            NkFormatProps NkParseFormatProps(const char* props, int propsLen);

            // -----------------------------------------------------------------
            // Applique alignement/rembourrage à une chaîne déjà convertie
            // -----------------------------------------------------------------
            NkString NkApplyFormatProps(const NkString& value, const NkFormatProps& props);

            // -----------------------------------------------------------------
            // Moteur interne de formatage (implémenté dans .cpp)
            // -----------------------------------------------------------------
            NkString NkFormatImpl(const char* fmt, const NkFormatArg* args, int argCount);

            // -----------------------------------------------------------------
            // Namespace interne pour l'expansion des packs variadiques
            // -----------------------------------------------------------------
            namespace detail
            {
                // -----------------------------------------------------------------
                // Cas de base : pack vide
                // -----------------------------------------------------------------
                inline void NkFillArgBuffer(NkFormatArgBuffer&)
                {
                }

                // -----------------------------------------------------------------
                // Récursion : ajoute le premier argument puis traite le reste
                // -----------------------------------------------------------------
                template <typename First, typename... Rest>
                void NkFillArgBuffer(NkFormatArgBuffer& buf,
                                     const First& first,
                                     const Rest&... rest)
                {
                    buf.Push(NkMakeFormatArg(first));
                    NkFillArgBuffer(buf, rest...);
                }
            } // namespace detail

            // -----------------------------------------------------------------
            // Surcharge : format sans arguments (cas trivial)
            // -----------------------------------------------------------------
            inline NkString NkFormat(const char* fmt)
            {
                return NkFormatImpl(fmt, nullptr, 0);
            }

            // -----------------------------------------------------------------
            // Fonction principale : formatage avec arguments variadiques
            // -----------------------------------------------------------------
            template <typename... Args>
            NkString NkFormat(const char* fmt, const Args&... args)
            {
                NkFormatArgBuffer buf;
                detail::NkFillArgBuffer(buf, args...);
                return NkFormatImpl(fmt, buf.Data(), buf.Count());
            }

            // -----------------------------------------------------------------
            // Surcharge avec allocateur personnalisé (pour environnements contraints)
            // -----------------------------------------------------------------
            template <typename... Args>
            NkString NkFormatAlloc(memory::NkIAllocator& alloc,
                                   const char* fmt,
                                   const Args&... args)
            {
                NkFormatArgBuffer buf(&alloc);
                detail::NkFillArgBuffer(buf, args...);
                return NkFormatImpl(fmt, buf.Data(), buf.Count());
            }

            // =================================================================
            // SECTION 7 : API PUBLIQUE — PRINTF STYLE %d, %s, etc.
            // =================================================================

            // -----------------------------------------------------------------
            // Formatage style printf avec arguments variadiques C
            // -----------------------------------------------------------------
            NkString NkFormatf(const char* fmt, ...);

            // -----------------------------------------------------------------
            // Version avec va_list pour chaînage depuis d'autres fonctions
            // -----------------------------------------------------------------
            NkString NkVFormatf(const char* fmt, va_list args);

            // -----------------------------------------------------------------
            // Helper : conversion binaire (extension non-standard de printf)
            // -----------------------------------------------------------------
            NkString NkToBinaryString(unsigned long long value, int width = 0, char fill = '0');

            // =================================================================
            // SECTION 8 : EXTENSION NkString::Fmt (MÉTHODE STATIQUE)
            // =================================================================

        } // namespace string

    } // namespace nkentseu

    // ---------------------------------------------------------------------
    // DÉFINITION DU TEMPLATE NkString::Fmt (hors-classe)
    // À inclure après la définition complète de NkString
    // ---------------------------------------------------------------------
    namespace nkentseu
    {
        namespace string
        {
            // -----------------------------------------------------------------
            // Méthode statique : formatage placeholder depuis NkString
            // -----------------------------------------------------------------
            template <typename... Args>
            inline NkString NkString::Fmt(const Char* format, const Args&... args)
            {
                NkFormatArgBuffer buf;
                detail::NkFillArgBuffer(buf, args...);
                return NkFormatImpl(format, buf.Data(), buf.Count());
            }
        } // namespace string
    } // namespace nkentseu

#endif // NK_CONTAINERS_STRING_NKSTRINGFORMAT_H

// =============================================================================
// EXEMPLES D'UTILISATION — DEUX STYLES DE FORMATAGE
// =============================================================================
/*
    // -------------------------------------------------------------------------
    // STYLE 1 : Placeholder {i:props} — moderne et type-safe
    // -------------------------------------------------------------------------
    #include "NKContainers/String/NkStringFormat.h"
    using namespace nkentseu::string;

    // Exemple basique : insertion positionnelle
    NkString msg1 = NkFormat("Hello {0}, you have {1} messages", "Alice", 5);
    // Résultat : "Hello Alice, you have 5 messages"

    // Exemple avec propriétés : alignement, base, précision
    NkString msg2 = NkFormat("{0:w=8 >} | {1:.3} | {2:hex # w=6 0}", 42, 3.14159, 255);
    // Résultat : "      42 | 3.142 | 0x00ff"

    // Exemple avec booléens et chaînes
    NkString msg3 = NkFormat("Active: {0}, Name: {1:upper}", true, "bob");
    // Résultat : "Active: true, Name: BOB"

    // -------------------------------------------------------------------------
    // STYLE 2 : Printf %d, %s — familier et compact
    // -------------------------------------------------------------------------

    // Exemple basique : spécificateurs standards
    NkString msg4 = NkFormatf("Score: %d, Rate: %.2f, User: %s", 100, 3.14159, "Alice");
    // Résultat : "Score: 100, Rate: 3.14, User: Alice"

    // Exemple avec flags et largeur : alignement, padding, préfixe
    NkString msg5 = NkFormatf("%08x | %-10s | %+d", 255, "hello", -42);
    // Résultat : "000000ff | hello      | -42"

    // Exemple avec binaire (extension) : %b non-standard
    NkString msg6 = NkFormatf("Mask: %b", 42u);
    // Résultat : "Mask: 101010"

    // -------------------------------------------------------------------------
    // EXTENSION UTILISATEUR : Spécialisation de NkToString<T>
    // -------------------------------------------------------------------------

    // Définir un type personnalisé
    struct Vec3 { float x, y, z; };

    // Spécialiser NkToString dans le namespace nkentseu
    namespace nkentseu
    {
        template <>
        struct string::NkToString<Vec3>
        {
            static NkString Convert(const Vec3& v, const NkFormatProps& props)
            {
                // Formatage interne avec les propriétés reçues
                NkString s = string::NkFormat("({0:.3}, {1:.3}, {2:.3})", v.x, v.y, v.z);
                // Appliquer alignement/largeur si spécifié par l'appelant
                return string::NkApplyFormatProps(s, props);
            }
        };
    }

    // Utilisation transparente avec NkFormat
    Vec3 pos{1.0f, 2.5f, -3.14159f};
    NkString msg7 = NkFormat("Position: {0:w=30 ^}", pos);
    // Résultat : "Position:       (1.000, 2.500, -3.142)       "

    // -------------------------------------------------------------------------
    // COMPARAISON RAPIDE : QUAND UTILISER QUEL STYLE ?
    // -------------------------------------------------------------------------

    // ✓ Placeholder {i:props} :
    //   - Formatage riche avec propriétés structurées (w=, hex, upper...)
    //   - Type-safe : erreurs détectées à la compilation pour types non-spécialisés
    //   - Réordonnancement facile des arguments sans modifier la chaîne
    //   - Extension via NkToString<T> avec accès aux propriétés structurées

    // ✓ Printf %d, %s :
    //   - Syntaxe compacte et familière pour les développeurs C/C++
    //   - Migration progressive depuis du code legacy utilisant printf
    //   - Support natif des flags (%08x, %-10s, %+d) sans parsing supplémentaire
    //   - Extension via NkToFormatf<T> avec chaîne d'options brute

    // -------------------------------------------------------------------------
    // BONNES PRATIQUES ET PERFORMANCES
    // -------------------------------------------------------------------------

    // 1. Réutiliser un buffer dans les boucles chaudes pour éviter les allocations :
    void LogBatch(const int* values, usize count)
    {
        NkString buffer;
        buffer.Reserve(64);  // Pré-allouer pour éviter les réallocations
        for (usize i = 0; i < count; ++i)
        {
            buffer.Clear();  // Réinitialiser sans libérer la capacité
            buffer.Append("Value[");
            buffer.Append(string::NkFormatValue(i));  // Formatage de l'index
            buffer.Append("] = ");
            buffer.Append(string::NkFormatValue(values[i]));  // Formatage de la valeur
            SendLog(buffer.CStr());  // Aucune allocation après la première itération
        }
    }

    // 2. Utiliser NkFormatAlloc avec un pool allocator en temps réel :
    void GameLoopUpdate(memory::NkIAllocator& frameAlloc)
    {
        // Formatage sans allocation système dans la boucle de jeu
        NkString status = string::NkFormatAlloc(
            frameAlloc,
            "FPS: {0:.1} | Entities: {1} | Memory: {2} KB",
            currentFPS,
            entityCount,
            usedMemoryKB
        );
        RenderUI(status);
    }

    // 3. Valider les entrées utilisateur avant interpolation (sécurité) :
    NkString BuildQuerySafe(NkStringView table, NkStringView column, int id)
    {
        // Rejeter les identifiants contenant des caractères dangereux
        if (!table.IsAlphaNumeric() || !column.IsAlphaNumeric())
        {
            return NkString("[ERROR] Invalid identifier");
        }
        // Interpolation sûre : les valeurs numériques sont formatées, pas concaténées
        return string::NkFormat(
            "SELECT * FROM {0} WHERE {1} = {2}",
            table, column, id
        );
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Apache-2.0 / Propriétaire selon contexte
//
// Généré par Rihen le 2026-04-26
// Dernière modification : 2026-04-26
// ============================================================