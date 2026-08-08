#pragma once
// =============================================================================
// CALE — ce fichier n'existait PAS en ABI 3.0.
//
// POURQUOI IL EST QUAND MEME LA
// -----------------------------
// Le banc de compatibilite compile les sources d'AUJOURD'HUI (les exemples, qui
// journalisent) contre les en-tetes D'HIER. Ce qu'on veut eprouver, c'est la
// DISPOSITION DES STRUCTURES — la vtable, la fabrique — pas la capacite d'un
// vieux compilateur a lire du code neuf.
//
// Sans cette cale, il faudrait figer aussi une copie des exemples, qui
// divergerait aussitot des vrais. On neutralise donc le journal, et rien
// d'autre : le module produit est celui qu'un stagiaire aurait obtenu en 3.0,
// journal en moins.
//
// CE QUE CA NE TESTE PAS, et il faut le savoir : la compatibilite du CODE
// SOURCE. Un stagiaire qui recopierait un exemple d'aujourd'hui dans un atelier
// d'hier aurait, lui, une erreur de compilation — franche et immediate, donc
// sans danger.
// =============================================================================

#define NKC_LOG_TRACE(...) ((void)0)
#define NKC_LOG_DEBUG(...) ((void)0)
#define NKC_LOG_INFO(...)  ((void)0)
#define NKC_LOG_WARN(...)  ((void)0)
#define NKC_LOG_ERROR(...) ((void)0)

#define NKC_MODULE_LOGGING(kind)
