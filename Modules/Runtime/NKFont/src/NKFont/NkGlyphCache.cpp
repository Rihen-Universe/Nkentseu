/**
 * @File    NkGlyphCache.cpp
 * @Brief   Cache LRU + Atlas packer GPU-ready.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "pch.h"
#include "NKFont/NkGlyphCache.h"
#include <cstring>

namespace nkentseu {

// ─────────────────────────────────────────────────────────────────────────────
//  NkGlyphCache
// ─────────────────────────────────────────────────────────────────────────────

void NkGlyphCache::Init(NkMemArena& arena, uint32 capacity) noexcept {
    mCapacity    = capacity;
    mSize        = 0;
    mLRUHead     = nullptr;
    mLRUTail     = nullptr;
    mHits = mMisses = 0;

    // Bucket count = prochaine puissance de 2 >= capacity * 2
    mBucketCount = 1;
    while (mBucketCount < capacity * 2) mBucketCount <<= 1;

    mBuckets = arena.Alloc<NkCachedGlyph*>(mBucketCount);

    // Pré-alloue les entrées et construit la free list
    NkCachedGlyph* entries = arena.Alloc<NkCachedGlyph>(capacity);
    if (!entries || !mBuckets) return;

    mFreeList = entries;
    for (uint32 i = 0; i < capacity - 1; ++i) {
        entries[i].next = &entries[i+1];
        entries[i].prev = nullptr;
    }
    entries[capacity-1].next = nullptr;
}

void NkGlyphCache::LRU_Remove(NkCachedGlyph* e) noexcept {
    if (e->prev) e->prev->next = e->next; else mLRUHead = e->next;
    if (e->next) e->next->prev = e->prev; else mLRUTail = e->prev;
    e->prev = e->next = nullptr;
}

void NkGlyphCache::LRU_PushFront(NkCachedGlyph* e) noexcept {
    e->prev = nullptr;
    e->next = mLRUHead;
    if (mLRUHead) mLRUHead->prev = e;
    mLRUHead = e;
    if (!mLRUTail) mLRUTail = e;
}

NkCachedGlyph* NkGlyphCache::LRU_Evict() noexcept {
    if (!mLRUTail) return nullptr;
    NkCachedGlyph* victim = mLRUTail;
    LRU_Remove(victim);
    // Retire du hash table
    if (mBuckets && mBucketCount > 0) {
        const uint32 bucket = HashKey(victim->glyphId, victim->ppem, victim->flags)
                            & (mBucketCount - 1);
        NkCachedGlyph* prev = nullptr;
        NkCachedGlyph* cur  = mBuckets[bucket];
        while (cur) {
            if (cur == victim) {
                if (prev) prev->next = cur->next;
                else mBuckets[bucket] = cur->next;
                break;
            }
            prev = cur; cur = cur->next;
        }
    }
    --mSize;
    return victim;
}

NkCachedGlyph* NkGlyphCache::Find(
    uint16 glyphId, uint16 ppem, uint8 flags) const noexcept
{
    if (!mBuckets || mBucketCount == 0) return nullptr;
    const uint32 bucket = HashKey(glyphId, ppem, flags) & (mBucketCount - 1);
    NkCachedGlyph* e = mBuckets[bucket];
    while (e) {
        if (e->glyphId == glyphId && e->ppem == ppem && e->flags == flags) {
            ++const_cast<NkGlyphCache*>(this)->mHits;
            return e;
        }
        e = e->next;
    }
    ++const_cast<NkGlyphCache*>(this)->mMisses;
    return nullptr;
}

void NkGlyphCache::Touch(NkCachedGlyph* entry) noexcept {
    if (!entry || entry == mLRUHead) return;
    LRU_Remove(entry);
    LRU_PushFront(entry);
}

NkCachedGlyph* NkGlyphCache::Insert(
    uint16 glyphId, uint16 ppem, uint8 flags,
    const NkGlyphMetrics& metrics,
    const NkBitmap& bitmap,
    NkMemArena& arena) noexcept
{
    if (!mBuckets) return nullptr;

    // Réutilise une entrée existante si même clé
    NkCachedGlyph* entry = Find(glyphId, ppem, flags);
    if (entry) { Touch(entry); return entry; }

    // Obtient un slot libre
    if (mFreeList) {
        entry = mFreeList;
        mFreeList = mFreeList->next;
    } else if (mSize >= mCapacity) {
        entry = LRU_Evict();
        if (!entry) return nullptr;
    } else { return nullptr; }

    // Copie le bitmap dans l'arena permanente
    NkBitmap bm = NkBitmap::Alloc(arena, bitmap.width, bitmap.height);
    if (bm.IsValid() && bitmap.IsValid())
        bm.Blit(bitmap, 0, 0);

    entry->glyphId  = glyphId;
    entry->ppem     = ppem;
    entry->flags    = flags;
    entry->metrics  = metrics;
    entry->bitmap   = bm;
    entry->inAtlas  = false;
    entry->prev = entry->next = nullptr;

    // Insère dans le hash table
    const uint32 bucket = HashKey(glyphId, ppem, flags) & (mBucketCount - 1);
    entry->next = mBuckets[bucket];
    mBuckets[bucket] = entry;

    LRU_PushFront(entry);
    ++mSize;
    return entry;
}

void NkGlyphCache::Clear() noexcept {
    if (mBuckets) ::memset(mBuckets, 0, sizeof(NkCachedGlyph*) * mBucketCount);
    mLRUHead = mLRUTail = nullptr;
    mSize = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkAtlasPacker
// ─────────────────────────────────────────────────────────────────────────────

void NkAtlasPacker::Init(const NkBitmap& atlas, int32 padding) noexcept {
    mAtlas      = atlas;
    mPadding    = padding;
    mNumShelves = 0;
    mFull       = false;
}

NkAtlasPacker::Shelf* NkAtlasPacker::FindShelf(int32 h) noexcept {
    // Cherche une étagère existante qui peut accueillir un glyphe de hauteur h
    for (int32 i = 0; i < mNumShelves; ++i) {
        Shelf& s = mShelves[i];
        if (s.height >= h && s.height <= h + h/2) { // tolérance 50%
            if (s.xCursor + mPadding <= mAtlas.width)
                return &s;
        }
    }
    return nullptr;
}

bool NkAtlasPacker::AddShelf(int32 h, int32& shelfY) noexcept {
    if (mNumShelves >= MAX_SHELVES) return false;
    // Calcule le Y de la nouvelle étagère
    int32 y = mPadding;
    for (int32 i = 0; i < mNumShelves; ++i)
        y = mShelves[i].y + mShelves[i].height + mPadding;
    if (y + h + mPadding > mAtlas.height) { mFull = true; return false; }
    Shelf& s   = mShelves[mNumShelves++];
    s.y        = y;
    s.height   = h;
    s.xCursor  = mPadding;
    shelfY     = y;
    return true;
}

bool NkAtlasPacker::Pack(const NkBitmap& glyph, int32& outX, int32& outY) noexcept {
    if (mFull || !mAtlas.IsValid()) return false;
    const int32 w = glyph.width  + mPadding;
    const int32 h = glyph.height + mPadding;

    Shelf* shelf = FindShelf(h);
    if (!shelf) {
        int32 newY = 0;
        if (!AddShelf(h, newY)) return false;
        shelf = &mShelves[mNumShelves - 1];
    }

    if (shelf->xCursor + w > mAtlas.width) {
        // Étagère pleine — ouvre une nouvelle
        int32 newY = 0;
        if (!AddShelf(h, newY)) return false;
        shelf = &mShelves[mNumShelves - 1];
    }

    outX = shelf->xCursor;
    outY = shelf->y;
    // Copie le bitmap du glyphe dans l'atlas
    mAtlas.Blit(glyph, outX, outY);
    shelf->xCursor += w;
    return true;
}

void NkAtlasPacker::Reset() noexcept {
    mNumShelves = 0;
    mFull       = false;
    mAtlas.Clear();
}

float NkAtlasPacker::Usage() const noexcept {
    if (!mAtlas.IsValid() || mNumShelves == 0) return 0.f;
    int32 usedArea = 0;
    for (int32 i = 0; i < mNumShelves; ++i)
        usedArea += mShelves[i].xCursor * mShelves[i].height;
    return static_cast<float>(usedArea) / static_cast<float>(mAtlas.width * mAtlas.height);
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkTextureAtlas
// ─────────────────────────────────────────────────────────────────────────────

bool NkTextureAtlas::Init(
    NkMemArena& arena, int32 width, int32 height,
    uint32 cacheCapacity) noexcept
{
    bitmap = NkBitmap::Alloc(arena, width, height);
    if (!bitmap.IsValid()) return false;
    packer.Init(bitmap);
    cache.Init(arena, cacheCapacity);
    dirty = false;
    page  = 0;
    return true;
}

bool NkTextureAtlas::AddGlyph(
    uint16 glyphId, uint16 ppem,
    const NkGlyphMetrics& metrics,
    const NkBitmap& glyphBitmap,
    NkMemArena& arena) noexcept
{
    // Insère dans le cache (copie le bitmap)
    NkCachedGlyph* entry = cache.Insert(glyphId, ppem, 0, metrics, glyphBitmap, arena);
    if (!entry) return false;

    if (!entry->inAtlas && glyphBitmap.IsValid()
        && glyphBitmap.width > 0 && glyphBitmap.height > 0) {
        int32 ax = 0, ay = 0;
        if (packer.Pack(glyphBitmap, ax, ay)) {
            entry->uv.atlasX  = ax;
            entry->uv.atlasY  = ay;
            entry->uv.atlasW  = glyphBitmap.width;
            entry->uv.atlasH  = glyphBitmap.height;
            entry->uv.atlasPage = page;
            entry->uv.u0 = static_cast<float>(ax) / bitmap.width;
            entry->uv.v0 = static_cast<float>(ay) / bitmap.height;
            entry->uv.u1 = static_cast<float>(ax + glyphBitmap.width)  / bitmap.width;
            entry->uv.v1 = static_cast<float>(ay + glyphBitmap.height) / bitmap.height;
            entry->inAtlas = true;
            dirty = true;
        }
    }
    return true;
}

const NkCachedGlyph* NkTextureAtlas::FindGlyph(
    uint16 glyphId, uint16 ppem) const noexcept
{
    NkCachedGlyph* g = cache.Find(glyphId, ppem);
    if (g) const_cast<NkGlyphCache&>(cache).Touch(g);
    return g;
}

} // namespace nkentseu
