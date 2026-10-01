// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkOpenglCompat41.cpp — DOUBLURES 4.2-4.5 POUR UN CONTEXTE OPENGL 4.1 CORE
// (voir NkOpenglCompat41.h pour le pourquoi)
//
// REGLE DE CHAQUE DOUBLURE : elle laisse l'etat GL comme elle l'a trouve. Une
// fonction DSA ne touche aucune liaison ; sa doublure lie l'objet pour le
// modifier, puis RELIE ce qui etait lie (NkLie* ci-dessous). Le device, qui
// suppose le DSA, ne voit donc aucune difference.
//
// ETAT QUE LE 4.1 NE SAIT PAS RENDRE, tenu ici :
//   - la CIBLE de chaque texture (glTextureStorage2D ne la recoit pas, et la
//     requete GL_TEXTURE_TARGET est du 4.5) : notee a glCreateTextures ;
//   - le FORMAT des attributs de chaque VAO (vertex_attrib_binding est du 4.3) :
//     note a glVertexArrayAttribFormat, applique en glVertexAttribPointer quand
//     glBindVertexBuffer donne le tampon, le decalage et le pas.
// =============================================================================
#include "NkOpenglCompat41.h"

#if defined(NK_GL_41) || defined(NK_GL_COMPAT41_FORCE)

#include "NKLogger/NkLog.h"
#include <glad/gl.h>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nkentseu {

	namespace {

		// ── Refus nommes : dits UNE fois chacun ─────────────────────────────────
		void NkRefus41(bool &dit, const char *quoi) {
			if (dit)
				return;
			dit = true;
			logger_src.Errorf("[NkRHI_GL][4.1] refus nomme : %s (absent d'OpenGL 4.1 core)\n", quoi);
		}
		bool gDitCalcul = false, gDitImages = false, gDitBaseInstance = false, gDitCopie = false,
			 gDitFormat = false, gDitVao = false;

		// ── Liaisons sauvees / restaurees ───────────────────────────────────────
		GLenum NkRequeteTexture(GLenum cible) {
			switch (cible) {
				case GL_TEXTURE_1D:
					return GL_TEXTURE_BINDING_1D;
				case GL_TEXTURE_3D:
					return GL_TEXTURE_BINDING_3D;
				case GL_TEXTURE_1D_ARRAY:
					return GL_TEXTURE_BINDING_1D_ARRAY;
				case GL_TEXTURE_2D_ARRAY:
					return GL_TEXTURE_BINDING_2D_ARRAY;
				case GL_TEXTURE_CUBE_MAP:
					return GL_TEXTURE_BINDING_CUBE_MAP;
				case GL_TEXTURE_CUBE_MAP_ARRAY:
					return GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
				case GL_TEXTURE_2D_MULTISAMPLE:
					return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
				case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
					return GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
				case GL_TEXTURE_RECTANGLE:
					return GL_TEXTURE_BINDING_RECTANGLE;
				case GL_TEXTURE_BUFFER:
					return GL_TEXTURE_BINDING_BUFFER;
				default:
					return GL_TEXTURE_BINDING_2D;
			}
		}

		std::vector<GLenum> gCibleTexture; // nom de texture -> cible (0 = inconnue)

		GLenum NkCibleDe(GLuint tex) {
			if (tex < gCibleTexture.size() && gCibleTexture[tex] != 0)
				return gCibleTexture[tex];
			return GL_TEXTURE_2D;
		}

		struct NkLieTexture {
				GLenum cible;
				GLint avant = 0;
				explicit NkLieTexture(GLuint tex) : cible(NkCibleDe(tex)) {
					glGetIntegerv(NkRequeteTexture(cible), &avant);
					glBindTexture(cible, tex);
				}
				~NkLieTexture() {
					glBindTexture(cible, (GLuint)avant);
				}
		};

		GLenum NkRequeteTampon(GLenum cible) {
			switch (cible) {
				case GL_ARRAY_BUFFER:
					return GL_ARRAY_BUFFER_BINDING;
				case GL_COPY_READ_BUFFER:
					return GL_COPY_READ_BUFFER; // pname 4.1 (alias _BINDING en 4.2)
				default:
					return GL_COPY_WRITE_BUFFER;
			}
		}

		struct NkLieTampon {
				GLenum cible;
				GLint avant = 0;
				NkLieTampon(GLenum c, GLuint b) : cible(c) {
					glGetIntegerv(NkRequeteTampon(c), &avant);
					glBindBuffer(c, b);
				}
				~NkLieTampon() {
					glBindBuffer(cible, (GLuint)avant);
				}
		};

		struct NkLieFbo {
				GLenum cible;
				GLint avant = 0;
				NkLieFbo(GLenum c, GLuint f) : cible(c) {
					glGetIntegerv(c == GL_READ_FRAMEBUFFER ? GL_READ_FRAMEBUFFER_BINDING : GL_DRAW_FRAMEBUFFER_BINDING,
								  &avant);
					glBindFramebuffer(c, f);
				}
				~NkLieFbo() {
					glBindFramebuffer(cible, (GLuint)avant);
				}
		};

		struct NkLieVao {
				GLint avant = 0;
				explicit NkLieVao(GLuint v) {
					glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &avant);
					glBindVertexArray(v);
				}
				~NkLieVao() {
					glBindVertexArray((GLuint)avant);
				}
		};

		// ── Formats : ce que glTexImage* exige en plus du format interne ────────
		bool NkFormatEtType(GLenum interne, GLenum &format, GLenum &type) {
			switch (interne) {
				case GL_R8:
					format = GL_RED;
					type = GL_UNSIGNED_BYTE;
					return true;
				case GL_R8_SNORM:
					format = GL_RED;
					type = GL_BYTE;
					return true;
				case GL_RG8:
					format = GL_RG;
					type = GL_UNSIGNED_BYTE;
					return true;
				case GL_RG8_SNORM:
					format = GL_RG;
					type = GL_BYTE;
					return true;
				case GL_RGB8:
				case GL_SRGB8:
					format = GL_RGB;
					type = GL_UNSIGNED_BYTE;
					return true;
				case GL_RGBA8:
				case GL_SRGB8_ALPHA8:
					format = GL_RGBA;
					type = GL_UNSIGNED_BYTE;
					return true;
				case GL_RGBA8_SNORM:
					format = GL_RGBA;
					type = GL_BYTE;
					return true;
				case GL_R16:
					format = GL_RED;
					type = GL_UNSIGNED_SHORT;
					return true;
				case GL_RG16:
					format = GL_RG;
					type = GL_UNSIGNED_SHORT;
					return true;
				case GL_RGBA16:
					format = GL_RGBA;
					type = GL_UNSIGNED_SHORT;
					return true;
				case GL_R16F:
					format = GL_RED;
					type = GL_HALF_FLOAT;
					return true;
				case GL_RG16F:
					format = GL_RG;
					type = GL_HALF_FLOAT;
					return true;
				case GL_RGB16F:
					format = GL_RGB;
					type = GL_HALF_FLOAT;
					return true;
				case GL_RGBA16F:
					format = GL_RGBA;
					type = GL_HALF_FLOAT;
					return true;
				case GL_R32F:
					format = GL_RED;
					type = GL_FLOAT;
					return true;
				case GL_RG32F:
					format = GL_RG;
					type = GL_FLOAT;
					return true;
				case GL_RGB32F:
					format = GL_RGB;
					type = GL_FLOAT;
					return true;
				case GL_RGBA32F:
					format = GL_RGBA;
					type = GL_FLOAT;
					return true;
				case GL_R11F_G11F_B10F:
					format = GL_RGB;
					type = GL_UNSIGNED_INT_10F_11F_11F_REV;
					return true;
				case GL_RGB9_E5:
					format = GL_RGB;
					type = GL_UNSIGNED_INT_5_9_9_9_REV;
					return true;
				case GL_RGB10_A2:
					format = GL_RGBA;
					type = GL_UNSIGNED_INT_2_10_10_10_REV;
					return true;
				case GL_RGB10_A2UI:
					format = GL_RGBA_INTEGER;
					type = GL_UNSIGNED_INT_2_10_10_10_REV;
					return true;
				case GL_R8UI:
					format = GL_RED_INTEGER;
					type = GL_UNSIGNED_BYTE;
					return true;
				case GL_R8I:
					format = GL_RED_INTEGER;
					type = GL_BYTE;
					return true;
				case GL_RG8UI:
					format = GL_RG_INTEGER;
					type = GL_UNSIGNED_BYTE;
					return true;
				case GL_RGBA8UI:
					format = GL_RGBA_INTEGER;
					type = GL_UNSIGNED_BYTE;
					return true;
				case GL_R16UI:
					format = GL_RED_INTEGER;
					type = GL_UNSIGNED_SHORT;
					return true;
				case GL_R16I:
					format = GL_RED_INTEGER;
					type = GL_SHORT;
					return true;
				case GL_RG16UI:
					format = GL_RG_INTEGER;
					type = GL_UNSIGNED_SHORT;
					return true;
				case GL_RGBA16UI:
					format = GL_RGBA_INTEGER;
					type = GL_UNSIGNED_SHORT;
					return true;
				case GL_R32UI:
					format = GL_RED_INTEGER;
					type = GL_UNSIGNED_INT;
					return true;
				case GL_R32I:
					format = GL_RED_INTEGER;
					type = GL_INT;
					return true;
				case GL_RG32UI:
					format = GL_RG_INTEGER;
					type = GL_UNSIGNED_INT;
					return true;
				case GL_RG32I:
					format = GL_RG_INTEGER;
					type = GL_INT;
					return true;
				case GL_RGBA32UI:
					format = GL_RGBA_INTEGER;
					type = GL_UNSIGNED_INT;
					return true;
				case GL_RGBA32I:
					format = GL_RGBA_INTEGER;
					type = GL_INT;
					return true;
				case GL_DEPTH_COMPONENT16:
					format = GL_DEPTH_COMPONENT;
					type = GL_UNSIGNED_SHORT;
					return true;
				case GL_DEPTH_COMPONENT24:
				case GL_DEPTH_COMPONENT32:
					format = GL_DEPTH_COMPONENT;
					type = GL_UNSIGNED_INT;
					return true;
				case GL_DEPTH_COMPONENT32F:
					format = GL_DEPTH_COMPONENT;
					type = GL_FLOAT;
					return true;
				case GL_DEPTH24_STENCIL8:
					format = GL_DEPTH_STENCIL;
					type = GL_UNSIGNED_INT_24_8;
					return true;
				case GL_DEPTH32F_STENCIL8:
					format = GL_DEPTH_STENCIL;
					type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
					return true;
				default:
					return false;
			}
		}

		// Octets d'un niveau compresse (blocs 4x4) ; 0 = format non compresse.
		GLsizei NkOctetsCompresses(GLenum interne, GLsizei w, GLsizei h) {
			GLsizei bloc = 0;
			switch (interne) {
				case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
				case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
				case GL_COMPRESSED_RED_RGTC1:
				case GL_COMPRESSED_SIGNED_RED_RGTC1:
					bloc = 8;
					break;
				case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
				case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
				case GL_COMPRESSED_RG_RGTC2:
				case GL_COMPRESSED_SIGNED_RG_RGTC2:
				case GL_COMPRESSED_RGBA_BPTC_UNORM:
				case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
					bloc = 16;
					break;
				default:
					return 0;
			}
			return ((w + 3) / 4) * ((h + 3) / 4) * bloc;
		}

		// Octets d'une image w x h transferee (pas d'une face de cube), selon les
		// parametres de (de)paquetage courants.
		size_t NkOctetsImage(GLsizei w, GLsizei h, GLenum format, GLenum type, bool lecture) {
			size_t comps = 4;
			switch (format) {
				case GL_RED:
				case GL_RED_INTEGER:
				case GL_DEPTH_COMPONENT:
				case GL_STENCIL_INDEX:
					comps = 1;
					break;
				case GL_RG:
				case GL_RG_INTEGER:
					comps = 2;
					break;
				case GL_RGB:
				case GL_BGR:
				case GL_RGB_INTEGER:
					comps = 3;
					break;
				default:
					comps = 4;
					break;
			}
			size_t pixel = 4;
			switch (type) {
				case GL_UNSIGNED_BYTE:
				case GL_BYTE:
					pixel = comps;
					break;
				case GL_UNSIGNED_SHORT:
				case GL_SHORT:
				case GL_HALF_FLOAT:
					pixel = comps * 2;
					break;
				case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
					pixel = 8;
					break;
				case GL_UNSIGNED_INT:
				case GL_INT:
				case GL_FLOAT:
					pixel = comps * 4;
					break;
				default: // types empaquetes : un pixel = 4 octets
					pixel = 4;
					break;
			}
			GLint longueur = 0, alignement = 4, hauteur = 0;
			glGetIntegerv(lecture ? GL_PACK_ROW_LENGTH : GL_UNPACK_ROW_LENGTH, &longueur);
			glGetIntegerv(lecture ? GL_PACK_ALIGNMENT : GL_UNPACK_ALIGNMENT, &alignement);
			glGetIntegerv(lecture ? GL_PACK_IMAGE_HEIGHT : GL_UNPACK_IMAGE_HEIGHT, &hauteur);
			size_t ligne = (size_t)(longueur > 0 ? longueur : w) * pixel;
			if (alignement > 1)
				ligne = (ligne + (size_t)alignement - 1) / (size_t)alignement * (size_t)alignement;
			return ligne * (size_t)(hauteur > 0 ? hauteur : h);
		}

		const void *NkDecale(const void *p, size_t octets) {
			return p ? (const void *)((const char *)p + octets) : p;
		}

		// ── Stockage de texture : glTexStorage* du pilote, sinon niveau par niveau
		PFNGLTEXSTORAGE2DPROC gPiloteTexStorage2D = nullptr;
		PFNGLTEXSTORAGE3DPROC gPiloteTexStorage3D = nullptr;

		GLsizei NkMax1(GLsizei v) {
			return v > 0 ? v : 1;
		}

		// `cible` est LIEE sur l'unite active.
		void NkStockageParNiveaux(GLenum cible, GLsizei niveaux, GLenum interne, GLsizei w, GLsizei h, GLsizei d) {
			GLenum format = GL_RGBA, type = GL_UNSIGNED_BYTE;
			const bool brut = NkFormatEtType(interne, format, type);
			const bool compresse = !brut && NkOctetsCompresses(interne, 4, 4) > 0;
			if (!brut && !compresse) {
				logger_src.Errorf("[NkRHI_GL][4.1] format interne 0x%04X : aucune doublure glTexImage connue, "
								  "essai en RGBA8\n",
								  (unsigned)interne);
				NkRefus41(gDitFormat, "stockage d'un format interne inconnu de la doublure");
			}
			for (GLsizei n = 0; n < niveaux; ++n) {
				const GLsizei lw = NkMax1(w >> n);
				const GLsizei lh = (cible == GL_TEXTURE_1D_ARRAY) ? h : NkMax1(h >> n);
				const GLsizei ld = (cible == GL_TEXTURE_3D) ? NkMax1(d >> n) : d;
				switch (cible) {
					case GL_TEXTURE_1D:
						if (compresse)
							glCompressedTexImage1D(cible, n, interne, lw, 0, NkOctetsCompresses(interne, lw, 1), nullptr);
						else
							glTexImage1D(cible, n, (GLint)interne, lw, 0, format, type, nullptr);
						break;
					case GL_TEXTURE_CUBE_MAP:
						for (GLenum f = 0; f < 6; ++f) {
							if (compresse)
								glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, n, interne, lw, lh, 0,
													   NkOctetsCompresses(interne, lw, lh), nullptr);
							else
								glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, n, (GLint)interne, lw, lh, 0, format,
											 type, nullptr);
						}
						break;
					case GL_TEXTURE_3D:
					case GL_TEXTURE_2D_ARRAY:
					case GL_TEXTURE_CUBE_MAP_ARRAY:
						if (compresse)
							glCompressedTexImage3D(cible, n, interne, lw, lh, ld, 0,
												   NkOctetsCompresses(interne, lw, lh) * ld, nullptr);
						else
							glTexImage3D(cible, n, (GLint)interne, lw, lh, ld, 0, format, type, nullptr);
						break;
					default: // 2D, rectangle, 1D array
						if (compresse)
							glCompressedTexImage2D(cible, n, interne, lw, lh, 0, NkOctetsCompresses(interne, lw, lh),
												   nullptr);
						else
							glTexImage2D(cible, n, (GLint)interne, lw, lh, 0, format, type, nullptr);
						break;
				}
			}
			// Ce que glTexStorage garantit : une texture COMPLETE a `niveaux` niveaux.
			glTexParameteri(cible, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(cible, GL_TEXTURE_MAX_LEVEL, niveaux > 0 ? niveaux - 1 : 0);
		}

		void NkStockage2D(GLenum cible, GLsizei niveaux, GLenum interne, GLsizei w, GLsizei h) {
			if (gPiloteTexStorage2D)
				gPiloteTexStorage2D(cible, niveaux, interne, w, h);
			else
				NkStockageParNiveaux(cible, niveaux, interne, w, h, 1);
		}

		void NkStockage3D(GLenum cible, GLsizei niveaux, GLenum interne, GLsizei w, GLsizei h, GLsizei d) {
			if (gPiloteTexStorage3D)
				gPiloteTexStorage3D(cible, niveaux, interne, w, h, d);
			else
				NkStockageParNiveaux(cible, niveaux, interne, w, h, d);
		}

		// ── Etat des VAO (vertex_attrib_binding emule) ──────────────────────────
		constexpr GLuint kMax41 = 16; // GL_MAX_VERTEX_ATTRIBS de macOS

		struct NkAttrib41 {
				GLint taille = 4;
				GLenum type = GL_FLOAT;
				GLboolean normalise = GL_FALSE;
				bool entier = false;
				bool formatPose = false;
				GLuint decalage = 0;
				GLuint liaison = 0;
		};

		struct NkLiaison41 {
				GLuint tampon = 0;
				GLintptr decalage = 0;
				GLsizei pas = 0;
				GLuint diviseur = 0;
		};

		struct NkVao41 {
				NkAttrib41 attribs[kMax41];
				NkLiaison41 liaisons[kMax41];
		};

		std::vector<NkVao41> gVaos; // nom de VAO -> etat

		NkVao41 *NkEtatVao(GLuint vao) {
			if (vao == 0) {
				NkRefus41(gDitVao, "attributs de sommet hors VAO (VAO 0 inexistant en core)");
				return nullptr;
			}
			if (vao >= gVaos.size())
				gVaos.resize((size_t)vao + 1);
			return &gVaos[vao];
		}

		GLuint NkVaoCourant() {
			GLint v = 0;
			glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &v);
			return (GLuint)v;
		}

		// Pose le pointeur d'UN attribut (son VAO est LIE) si sa liaison a un tampon.
		void NkPoserAttrib(const NkVao41 &e, GLuint a) {
			const NkAttrib41 &at = e.attribs[a];
			if (!at.formatPose || at.liaison >= kMax41)
				return;
			const NkLiaison41 &l = e.liaisons[at.liaison];
			if (l.tampon == 0)
				return;
			NkLieTampon lie(GL_ARRAY_BUFFER, l.tampon);
			const void *ptr = (const void *)(uintptr_t)(l.decalage + (GLintptr)at.decalage);
			if (at.entier)
				glVertexAttribIPointer(a, at.taille, at.type, l.pas, ptr);
			else
				glVertexAttribPointer(a, at.taille, at.type, at.normalise, l.pas, ptr);
			glVertexAttribDivisor(a, l.diviseur);
		}

		void NkPoserLiaison(const NkVao41 &e, GLuint liaison) {
			for (GLuint a = 0; a < kMax41; ++a)
				if (e.attribs[a].formatPose && e.attribs[a].liaison == liaison)
					NkPoserAttrib(e, a);
		}

		void NkFormatAttrib(GLuint vao, GLuint a, GLint taille, GLenum type, GLboolean norm, bool entier,
							GLuint decalage) {
			NkVao41 *e = NkEtatVao(vao);
			if (!e || a >= kMax41)
				return;
			NkAttrib41 &at = e->attribs[a];
			at.taille = taille;
			at.type = type;
			at.normalise = norm;
			at.entier = entier;
			at.decalage = decalage;
			at.formatPose = true;
			NkLieVao lie(vao);
			NkPoserAttrib(*e, a);
		}

		void NkLiaisonAttrib(GLuint vao, GLuint a, GLuint liaison) {
			NkVao41 *e = NkEtatVao(vao);
			if (!e || a >= kMax41 || liaison >= kMax41)
				return;
			e->attribs[a].liaison = liaison;
			NkLieVao lie(vao);
			NkPoserAttrib(*e, a);
		}

		void NkDiviseurLiaison(GLuint vao, GLuint liaison, GLuint diviseur) {
			NkVao41 *e = NkEtatVao(vao);
			if (!e || liaison >= kMax41)
				return;
			e->liaisons[liaison].diviseur = diviseur;
			NkLieVao lie(vao);
			NkPoserLiaison(*e, liaison);
		}

		// ── Copie d'images : deux FBO de service + glBlitFramebuffer ────────────
		GLuint gFboSource = 0, gFboDestination = 0;

		void NkAttacher(GLenum fb, GLenum attache, GLuint tex, GLenum cible, GLint niveau, GLint couche) {
			switch (cible) {
				case GL_TEXTURE_1D_ARRAY:
				case GL_TEXTURE_2D_ARRAY:
				case GL_TEXTURE_3D:
				case GL_TEXTURE_CUBE_MAP_ARRAY:
					glFramebufferTextureLayer(fb, attache, tex, niveau, couche);
					break;
				case GL_TEXTURE_CUBE_MAP:
					glFramebufferTexture2D(fb, attache, GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)couche, tex, niveau);
					break;
				default:
					glFramebufferTexture2D(fb, attache, cible, tex, niveau);
					break;
			}
		}

		// =====================================================================
		// LES DOUBLURES
		// =====================================================================

		// ── Tampons ─────────────────────────────────────────────────────────────
		void GLAD_API_PTR Emu_CreateBuffers(GLsizei n, GLuint *ids) {
			glGenBuffers(n, ids);
			NkLieTampon lie(GL_COPY_WRITE_BUFFER, 0);
			for (GLsizei i = 0; i < n; ++i)
				glBindBuffer(GL_COPY_WRITE_BUFFER, ids[i]); // le nom devient un objet
		}

		void GLAD_API_PTR Emu_NamedBufferData(GLuint b, GLsizeiptr taille, const void *data, GLenum usage) {
			NkLieTampon lie(GL_COPY_WRITE_BUFFER, b);
			glBufferData(GL_COPY_WRITE_BUFFER, taille, data, usage);
		}

		void GLAD_API_PTR Emu_NamedBufferSubData(GLuint b, GLintptr off, GLsizeiptr taille, const void *data) {
			NkLieTampon lie(GL_COPY_WRITE_BUFFER, b);
			glBufferSubData(GL_COPY_WRITE_BUFFER, off, taille, data);
		}

		void *GLAD_API_PTR Emu_MapNamedBufferRange(GLuint b, GLintptr off, GLsizeiptr longueur, GLbitfield acces) {
			NkLieTampon lie(GL_COPY_WRITE_BUFFER, b);
			return glMapBufferRange(GL_COPY_WRITE_BUFFER, off, longueur, acces);
		}

		GLboolean GLAD_API_PTR Emu_UnmapNamedBuffer(GLuint b) {
			NkLieTampon lie(GL_COPY_WRITE_BUFFER, b);
			return glUnmapBuffer(GL_COPY_WRITE_BUFFER);
		}

		void GLAD_API_PTR Emu_GetNamedBufferSubData(GLuint b, GLintptr off, GLsizeiptr taille, void *data) {
			NkLieTampon lie(GL_COPY_READ_BUFFER, b);
			glGetBufferSubData(GL_COPY_READ_BUFFER, off, taille, data);
		}

		void GLAD_API_PTR Emu_CopyNamedBufferSubData(GLuint lu, GLuint ecrit, GLintptr offLu, GLintptr offEcrit,
													 GLsizeiptr taille) {
			NkLieTampon l1(GL_COPY_READ_BUFFER, lu);
			NkLieTampon l2(GL_COPY_WRITE_BUFFER, ecrit);
			glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, offLu, offEcrit, taille);
		}

		// ── Textures ────────────────────────────────────────────────────────────
		void GLAD_API_PTR Emu_CreateTextures(GLenum cible, GLsizei n, GLuint *ids) {
			glGenTextures(n, ids);
			GLint avant = 0;
			glGetIntegerv(NkRequeteTexture(cible), &avant);
			for (GLsizei i = 0; i < n; ++i) {
				if (ids[i] >= gCibleTexture.size())
					gCibleTexture.resize((size_t)ids[i] + 1, 0);
				gCibleTexture[ids[i]] = cible;
				glBindTexture(cible, ids[i]); // le nom devient une texture de cette cible
			}
			glBindTexture(cible, (GLuint)avant);
		}

		void GLAD_API_PTR Emu_TextureStorage1D(GLuint tex, GLsizei niveaux, GLenum interne, GLsizei w) {
			NkLieTexture lie(tex);
			NkStockageParNiveaux(lie.cible, niveaux, interne, w, 1, 1);
		}

		void GLAD_API_PTR Emu_TextureStorage2D(GLuint tex, GLsizei niveaux, GLenum interne, GLsizei w, GLsizei h) {
			NkLieTexture lie(tex);
			NkStockage2D(lie.cible, niveaux, interne, w, h);
		}

		void GLAD_API_PTR Emu_TextureStorage3D(GLuint tex, GLsizei niveaux, GLenum interne, GLsizei w, GLsizei h,
											   GLsizei d) {
			NkLieTexture lie(tex);
			NkStockage3D(lie.cible, niveaux, interne, w, h, d);
		}

		void GLAD_API_PTR Emu_TextureStorage2DMultisample(GLuint tex, GLsizei echantillons, GLenum interne, GLsizei w,
														  GLsizei h, GLboolean fixes) {
			NkLieTexture lie(tex);
			glTexImage2DMultisample(lie.cible, echantillons, interne, w, h, fixes);
		}

		void GLAD_API_PTR Emu_TexStorage2D(GLenum cible, GLsizei niveaux, GLenum interne, GLsizei w, GLsizei h) {
			NkStockageParNiveaux(cible, niveaux, interne, w, h, 1);
		}

		void GLAD_API_PTR Emu_TexStorage3D(GLenum cible, GLsizei niveaux, GLenum interne, GLsizei w, GLsizei h,
										   GLsizei d) {
			NkStockageParNiveaux(cible, niveaux, interne, w, h, d);
		}

		void GLAD_API_PTR Emu_TextureSubImage2D(GLuint tex, GLint niveau, GLint x, GLint y, GLsizei w, GLsizei h,
												GLenum format, GLenum type, const void *pixels) {
			NkLieTexture lie(tex);
			glTexSubImage2D(lie.cible, niveau, x, y, w, h, format, type, pixels);
		}

		void GLAD_API_PTR Emu_TextureSubImage3D(GLuint tex, GLint niveau, GLint x, GLint y, GLint z, GLsizei w,
												GLsizei h, GLsizei d, GLenum format, GLenum type, const void *pixels) {
			NkLieTexture lie(tex);
			if (lie.cible == GL_TEXTURE_CUBE_MAP) {
				// En DSA, la face d'un cube est la coordonnee z ; en 4.1, une cible.
				const size_t face = NkOctetsImage(w, h, format, type, false);
				for (GLsizei i = 0; i < d; ++i)
					glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)(z + i), niveau, x, y, w, h, format, type,
									NkDecale(pixels, face * (size_t)i));
			} else {
				glTexSubImage3D(lie.cible, niveau, x, y, z, w, h, d, format, type, pixels);
			}
		}

		void GLAD_API_PTR Emu_CompressedTextureSubImage2D(GLuint tex, GLint niveau, GLint x, GLint y, GLsizei w,
														  GLsizei h, GLenum format, GLsizei taille, const void *data) {
			NkLieTexture lie(tex);
			glCompressedTexSubImage2D(lie.cible, niveau, x, y, w, h, format, taille, data);
		}

		void GLAD_API_PTR Emu_CompressedTextureSubImage3D(GLuint tex, GLint niveau, GLint x, GLint y, GLint z,
														  GLsizei w, GLsizei h, GLsizei d, GLenum format,
														  GLsizei taille, const void *data) {
			NkLieTexture lie(tex);
			if (lie.cible == GL_TEXTURE_CUBE_MAP) {
				const GLsizei face = d > 0 ? taille / d : taille;
				for (GLsizei i = 0; i < d; ++i)
					glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)(z + i), niveau, x, y, w, h,
											  format, face, NkDecale(data, (size_t)face * (size_t)i));
			} else {
				glCompressedTexSubImage3D(lie.cible, niveau, x, y, z, w, h, d, format, taille, data);
			}
		}

		void GLAD_API_PTR Emu_GenerateTextureMipmap(GLuint tex) {
			NkLieTexture lie(tex);
			glGenerateMipmap(lie.cible);
		}

		void GLAD_API_PTR Emu_BindTextureUnit(GLuint unite, GLuint tex) {
			GLint actif = GL_TEXTURE0;
			glGetIntegerv(GL_ACTIVE_TEXTURE, &actif);
			glActiveTexture(GL_TEXTURE0 + unite);
			if (tex != 0) {
				glBindTexture(NkCibleDe(tex), tex);
			} else {
				// DSA : 0 delie TOUTES les cibles de l'unite.
				static const GLenum kCibles[] = {GL_TEXTURE_1D,		  GL_TEXTURE_2D,		 GL_TEXTURE_3D,
												 GL_TEXTURE_1D_ARRAY, GL_TEXTURE_2D_ARRAY,	 GL_TEXTURE_CUBE_MAP,
												 GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_2D_MULTISAMPLE,
												 GL_TEXTURE_RECTANGLE};
				for (GLenum c : kCibles)
					glBindTexture(c, 0);
			}
			glActiveTexture((GLenum)actif);
		}

		void GLAD_API_PTR Emu_GetTextureImage(GLuint tex, GLint niveau, GLenum format, GLenum type, GLsizei taille,
											  void *pixels) {
			(void)taille;
			NkLieTexture lie(tex);
			if (lie.cible == GL_TEXTURE_CUBE_MAP) {
				GLint w = 0, h = 0;
				glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, niveau, GL_TEXTURE_WIDTH, &w);
				glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, niveau, GL_TEXTURE_HEIGHT, &h);
				const size_t face = NkOctetsImage(w, h, format, type, true);
				for (GLenum f = 0; f < 6; ++f)
					glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, niveau, format, type,
								  pixels ? (void *)((char *)pixels + face * f) : pixels);
			} else {
				glGetTexImage(lie.cible, niveau, format, type, pixels);
			}
		}

		void GLAD_API_PTR Emu_CopyImageSubData(GLuint src, GLenum srcCible, GLint srcNiveau, GLint sx, GLint sy,
											   GLint sz, GLuint dst, GLenum dstCible, GLint dstNiveau, GLint dx,
											   GLint dy, GLint dz, GLsizei w, GLsizei h, GLsizei d) {
			if (srcCible == GL_RENDERBUFFER || dstCible == GL_RENDERBUFFER) {
				NkRefus41(gDitCopie, "glCopyImageSubData depuis/vers un renderbuffer");
				return;
			}
			// La cible notee a la creation fait foi (le device passe GL_TEXTURE_2D).
			if (src < gCibleTexture.size() && gCibleTexture[src])
				srcCible = gCibleTexture[src];
			if (dst < gCibleTexture.size() && gCibleTexture[dst])
				dstCible = gCibleTexture[dst];
			if (gFboSource == 0) {
				glGenFramebuffers(1, &gFboSource);
				glGenFramebuffers(1, &gFboDestination);
			}
			GLint bitsProfondeur = 0;
			{
				NkLieTexture lie(src);
				glGetTexLevelParameteriv(srcCible == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : srcCible,
										 srcNiveau, GL_TEXTURE_DEPTH_SIZE, &bitsProfondeur);
			}
			const bool profondeur = bitsProfondeur > 0;
			const GLenum attache = profondeur ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
			const GLbitfield masque = profondeur ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT;

			NkLieFbo lu(GL_READ_FRAMEBUFFER, gFboSource);
			NkLieFbo ecrit(GL_DRAW_FRAMEBUFFER, gFboDestination);
			const GLboolean ciseaux = glIsEnabled(GL_SCISSOR_TEST);
			if (ciseaux)
				glDisable(GL_SCISSOR_TEST); // le blit est soumis au ciseau, la copie non
			if (!profondeur) {
				glReadBuffer(GL_COLOR_ATTACHMENT0);
				glDrawBuffer(GL_COLOR_ATTACHMENT0);
			}
			for (GLsizei c = 0; c < d; ++c) {
				NkAttacher(GL_READ_FRAMEBUFFER, attache, src, srcCible, srcNiveau, sz + c);
				NkAttacher(GL_DRAW_FRAMEBUFFER, attache, dst, dstCible, dstNiveau, dz + c);
				glBlitFramebuffer(sx, sy, sx + w, sy + h, dx, dy, dx + w, dy + h, masque, GL_NEAREST);
			}
			glFramebufferTexture2D(GL_READ_FRAMEBUFFER, attache, GL_TEXTURE_2D, 0, 0);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attache, GL_TEXTURE_2D, 0, 0);
			if (ciseaux)
				glEnable(GL_SCISSOR_TEST);
		}

		// ── Echantillonneurs, VAO, framebuffers ─────────────────────────────────
		void GLAD_API_PTR Emu_CreateSamplers(GLsizei n, GLuint *ids) {
			glGenSamplers(n, ids); // glSamplerParameter* accepte un nom genere
		}

		void GLAD_API_PTR Emu_CreateVertexArrays(GLsizei n, GLuint *ids) {
			glGenVertexArrays(n, ids);
			NkLieVao lie(0);
			for (GLsizei i = 0; i < n; ++i) {
				glBindVertexArray(ids[i]);
				if (NkVao41 *e = NkEtatVao(ids[i]))
					*e = NkVao41{}; // un nom recycle repart d'un etat vierge
			}
		}

		void GLAD_API_PTR Emu_EnableVertexArrayAttrib(GLuint vao, GLuint a) {
			NkLieVao lie(vao);
			glEnableVertexAttribArray(a);
		}

		void GLAD_API_PTR Emu_VertexArrayAttribFormat(GLuint vao, GLuint a, GLint taille, GLenum type, GLboolean norm,
													  GLuint decalage) {
			NkFormatAttrib(vao, a, taille, type, norm, false, decalage);
		}

		void GLAD_API_PTR Emu_VertexArrayAttribIFormat(GLuint vao, GLuint a, GLint taille, GLenum type,
													   GLuint decalage) {
			NkFormatAttrib(vao, a, taille, type, GL_FALSE, true, decalage);
		}

		void GLAD_API_PTR Emu_VertexArrayAttribBinding(GLuint vao, GLuint a, GLuint liaison) {
			NkLiaisonAttrib(vao, a, liaison);
		}

		void GLAD_API_PTR Emu_VertexArrayBindingDivisor(GLuint vao, GLuint liaison, GLuint diviseur) {
			NkDiviseurLiaison(vao, liaison, diviseur);
		}

		void GLAD_API_PTR Emu_VertexAttribFormat(GLuint a, GLint taille, GLenum type, GLboolean norm, GLuint decalage) {
			NkFormatAttrib(NkVaoCourant(), a, taille, type, norm, false, decalage);
		}

		void GLAD_API_PTR Emu_VertexAttribIFormat(GLuint a, GLint taille, GLenum type, GLuint decalage) {
			NkFormatAttrib(NkVaoCourant(), a, taille, type, GL_FALSE, true, decalage);
		}

		void GLAD_API_PTR Emu_VertexAttribBinding(GLuint a, GLuint liaison) {
			NkLiaisonAttrib(NkVaoCourant(), a, liaison);
		}

		void GLAD_API_PTR Emu_VertexBindingDivisor(GLuint liaison, GLuint diviseur) {
			NkDiviseurLiaison(NkVaoCourant(), liaison, diviseur);
		}

		void GLAD_API_PTR Emu_BindVertexBuffer(GLuint liaison, GLuint tampon, GLintptr decalage, GLsizei pas) {
			NkVao41 *e = NkEtatVao(NkVaoCourant());
			if (!e || liaison >= kMax41)
				return;
			NkLiaison41 &l = e->liaisons[liaison];
			l.tampon = tampon;
			l.decalage = decalage;
			l.pas = pas;
			NkPoserLiaison(*e, liaison);
		}

		void GLAD_API_PTR Emu_CreateFramebuffers(GLsizei n, GLuint *ids) {
			glGenFramebuffers(n, ids);
			NkLieFbo lie(GL_DRAW_FRAMEBUFFER, 0);
			for (GLsizei i = 0; i < n; ++i)
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ids[i]);
		}

		void GLAD_API_PTR Emu_NamedFramebufferTexture(GLuint fb, GLenum attache, GLuint tex, GLint niveau) {
			NkLieFbo lie(GL_DRAW_FRAMEBUFFER, fb);
			glFramebufferTexture(GL_DRAW_FRAMEBUFFER, attache, tex, niveau);
		}

		void GLAD_API_PTR Emu_NamedFramebufferDrawBuffer(GLuint fb, GLenum tampon) {
			NkLieFbo lie(GL_DRAW_FRAMEBUFFER, fb);
			glDrawBuffer(tampon);
		}

		void GLAD_API_PTR Emu_NamedFramebufferDrawBuffers(GLuint fb, GLsizei n, const GLenum *tampons) {
			NkLieFbo lie(GL_DRAW_FRAMEBUFFER, fb);
			glDrawBuffers(n, tampons);
		}

		void GLAD_API_PTR Emu_NamedFramebufferReadBuffer(GLuint fb, GLenum source) {
			NkLieFbo lie(GL_READ_FRAMEBUFFER, fb);
			glReadBuffer(source);
		}

		GLenum GLAD_API_PTR Emu_CheckNamedFramebufferStatus(GLuint fb, GLenum cible) {
			const GLenum c = (cible == GL_READ_FRAMEBUFFER) ? GL_READ_FRAMEBUFFER : GL_DRAW_FRAMEBUFFER;
			NkLieFbo lie(c, fb);
			return glCheckFramebufferStatus(c);
		}

		void GLAD_API_PTR Emu_BlitNamedFramebuffer(GLuint lu, GLuint ecrit, GLint sx0, GLint sy0, GLint sx1, GLint sy1,
												   GLint dx0, GLint dy0, GLint dx1, GLint dy1, GLbitfield masque,
												   GLenum filtre) {
			NkLieFbo l1(GL_READ_FRAMEBUFFER, lu);
			NkLieFbo l2(GL_DRAW_FRAMEBUFFER, ecrit);
			glBlitFramebuffer(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, masque, filtre);
		}

		// ── Tirages ─────────────────────────────────────────────────────────────
		void GLAD_API_PTR Emu_DrawArraysInstancedBaseInstance(GLenum mode, GLint premier, GLsizei nombre,
															  GLsizei instances, GLuint baseInstance) {
			if (baseInstance != 0)
				NkRefus41(gDitBaseInstance, "base instance non nulle (tire depuis l'instance 0)");
			glDrawArraysInstanced(mode, premier, nombre, instances);
		}

		void GLAD_API_PTR Emu_DrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei nombre, GLenum type,
																		  const void *indices, GLsizei instances,
																		  GLint baseSommet, GLuint baseInstance) {
			if (baseInstance != 0)
				NkRefus41(gDitBaseInstance, "base instance non nulle (tire depuis l'instance 0)");
			glDrawElementsInstancedBaseVertex(mode, nombre, type, indices, instances, baseSommet);
		}

		void GLAD_API_PTR Emu_MultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei nombre, GLsizei pas) {
			const size_t p = pas > 0 ? (size_t)pas : 16u; // DrawArraysIndirectCommand
			for (GLsizei i = 0; i < nombre; ++i)
				glDrawArraysIndirect(mode, (const void *)((const char *)indirect + p * (size_t)i));
		}

		void GLAD_API_PTR Emu_MultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei nombre,
														GLsizei pas) {
			const size_t p = pas > 0 ? (size_t)pas : 20u; // DrawElementsIndirectCommand
			for (GLsizei i = 0; i < nombre; ++i)
				glDrawElementsIndirect(mode, type, (const void *)((const char *)indirect + p * (size_t)i));
		}

		// ── Refus : calcul, images ; rien a faire : barrieres, etiquettes ───────
		void GLAD_API_PTR Emu_DispatchCompute(GLuint, GLuint, GLuint) {
			NkRefus41(gDitCalcul, "calcul (compute shaders, GL 4.3) -- glDispatchCompute ignore");
		}

		void GLAD_API_PTR Emu_DispatchComputeIndirect(GLintptr) {
			NkRefus41(gDitCalcul, "calcul (compute shaders, GL 4.3) -- glDispatchCompute ignore");
		}

		void GLAD_API_PTR Emu_BindImageTexture(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum) {
			NkRefus41(gDitImages, "images load/store (GL 4.2) -- glBindImageTexture ignore");
		}

		void GLAD_API_PTR Emu_MemoryBarrier(GLbitfield) {
			// Sans SSBO ni image, il n'y a aucune ecriture incoherente a ordonner.
		}

		void GLAD_API_PTR Emu_ObjectLabel(GLenum, GLuint, GLsizei, const GLchar *) {
		}

		void GLAD_API_PTR Emu_PushDebugGroup(GLenum, GLuint, GLsizei, const GLchar *) {
		}

		void GLAD_API_PTR Emu_PopDebugGroup(void) {
		}

		void GLAD_API_PTR Emu_DebugMessageInsert(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *) {
		}

	} // namespace

	// Toutes les doublures, une ligne chacune (X-macro) : la meme liste sert a
	// les POSER (NkGLCompat41Installer) et, en simulation, a faire le vide que
	// laisse un contexte 4.1 (NkGLCompat41Simuler).
#define NK_GL41_DOUBLURES(X)                                                                                \
	X(CreateBuffers) \
	X(NamedBufferData) \
	X(NamedBufferSubData) \
	X(MapNamedBufferRange) \
	X(UnmapNamedBuffer) \
	X(GetNamedBufferSubData) \
	X(CopyNamedBufferSubData) \
	X(CreateTextures) \
	X(TextureStorage1D) \
	X(TextureStorage2D) \
	X(TextureStorage3D) \
	X(TextureStorage2DMultisample) \
	X(TextureSubImage2D) \
	X(TextureSubImage3D) \
	X(CompressedTextureSubImage2D) \
	X(CompressedTextureSubImage3D) \
	X(GenerateTextureMipmap) \
	X(BindTextureUnit) \
	X(GetTextureImage) \
	X(CreateSamplers) \
	X(CreateVertexArrays) \
	X(EnableVertexArrayAttrib) \
	X(VertexArrayAttribFormat) \
	X(VertexArrayAttribIFormat) \
	X(VertexArrayAttribBinding) \
	X(VertexArrayBindingDivisor) \
	X(CreateFramebuffers) \
	X(NamedFramebufferTexture) \
	X(NamedFramebufferDrawBuffer) \
	X(NamedFramebufferDrawBuffers) \
	X(NamedFramebufferReadBuffer) \
	X(CheckNamedFramebufferStatus) \
	X(BlitNamedFramebuffer) \
	X(TexStorage2D) \
	X(TexStorage3D) \
	X(BindVertexBuffer) \
	X(VertexAttribFormat) \
	X(VertexAttribIFormat) \
	X(VertexAttribBinding) \
	X(VertexBindingDivisor) \
	X(DrawArraysInstancedBaseInstance) \
	X(DrawElementsInstancedBaseVertexBaseInstance) \
	X(MultiDrawArraysIndirect) \
	X(MultiDrawElementsIndirect) \
	X(CopyImageSubData) \
	X(DispatchCompute) \
	X(DispatchComputeIndirect) \
	X(BindImageTexture) \
	X(MemoryBarrier) \
	X(ObjectLabel) \
	X(PushDebugGroup) \
	X(PopDebugGroup) \
	X(DebugMessageInsert)

	int NkGLCompat41Installer() {
		// ARB_texture_storage est souvent exposee par macOS : on la garde.
		gPiloteTexStorage2D = glad_glTexStorage2D;
		gPiloteTexStorage3D = glad_glTexStorage3D;
		gCibleTexture.clear();
		gVaos.clear();
		gFboSource = gFboDestination = 0;

		// Pose la doublure de glXxx SEULEMENT si le pilote ne la fournit pas.
		int poses = 0;
#define NK_GL41_POSER(nom) \
	if (!glad_gl##nom) { \
		glad_gl##nom = &Emu_##nom; \
		++poses; \
	}
		NK_GL41_DOUBLURES(NK_GL41_POSER)
#undef NK_GL41_POSER
		// glDebugMessageCallback/Control restent NULS : InstallGLDebugCallback le
		// teste et se tait. glClipControl reste NUL : le device le teste et remappe
		// la profondeur dans les vertex shaders (NkGL41AdapterGLSL).
		return poses;
	}

#if defined(NK_GL_SIMULER_41)
	void NkGLCompat41Simuler() {
		// Le vide d'un contexte 4.1 core d'Apple : tout ce que ce fichier double,
		// plus glClipControl et la sortie de debogage KHR_debug.
#define NK_GL41_VIDER(nom) glad_gl##nom = nullptr;
		NK_GL41_DOUBLURES(NK_GL41_VIDER)
#undef NK_GL41_VIDER
		glad_glClipControl = nullptr;
		glad_glDebugMessageCallback = nullptr;
		glad_glDebugMessageControl = nullptr;
		logger_src.Errorf("[NkRHI_GL][4.1] SIMULATION du contexte macOS 4.1 core (NK_GL_SIMULER_41) : "
						  "outil de mise au point, jamais une livraison\n");
	}
#endif

	const char *NkGLCompat41Refus() {
		return "calcul (compute), SSBO, images load/store, base instance non nulle, glClipControl (profondeur "
			   "remappee dans les vertex shaders), sortie de debogage KHR_debug";
	}

} // namespace nkentseu

#else

namespace nkentseu {
	int NkGLCompat41Installer() {
		return 0;
	}
	const char *NkGLCompat41Refus() {
		return "";
	}
} // namespace nkentseu

#endif
