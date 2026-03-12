#pragma once

#ifndef NKENTSEU_THREADING_NKSHAREDMUTEX_H_INCLUDED
#define NKENTSEU_THREADING_NKSHAREDMUTEX_H_INCLUDED

#include "NKThreading/Synchronization/NkReaderWriterLock.h"

namespace nkentseu {
namespace threading {

using NkSharedMutex = NkReaderWriterLock;
using NkSharedLock  = NkReadLock;
using NkUniqueLock  = NkWriteLock;

} // namespace threading
namespace entseu {

// Legacy compatibility alias.
using NkSharedMutex = ::nkentseu::threading::NkSharedMutex;
using NkSharedLock  = ::nkentseu::threading::NkSharedLock;
using NkUniqueLock  = ::nkentseu::threading::NkUniqueLock;

} // namespace entseu
} // namespace nkentseu

#endif // NKENTSEU_THREADING_NKSHAREDMUTEX_H_INCLUDED
