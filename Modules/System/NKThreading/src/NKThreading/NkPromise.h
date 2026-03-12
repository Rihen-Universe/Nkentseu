#pragma once

#ifndef NKENTSEU_THREADING_NKPROMISE_H_INCLUDED
#define NKENTSEU_THREADING_NKPROMISE_H_INCLUDED

#include "NKThreading/NkFuture.h"

namespace nkentseu {
namespace entseu {

// Legacy compatibility aliases.
template<typename T>
using NkPromise = ::nkentseu::threading::NkPromise<T>;

template<typename T>
using NkFuture = ::nkentseu::threading::NkFuture<T>;

} // namespace entseu
} // namespace nkentseu

#endif // NKENTSEU_THREADING_NKPROMISE_H_INCLUDED
