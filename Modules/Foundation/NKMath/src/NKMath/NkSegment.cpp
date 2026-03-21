//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 4/12/2024 at 6:22:06 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//


#include "NkSegment.h"

namespace nkentseu {

	namespace math
	{
		NkRangeFloat NkSegment::Project(const NkVector2f& onto) {
			NkVector2f ontoUnit = NkVector2f(onto).Normalized();
			return NkRangeFloat(ontoUnit.Dot(points[0]), ontoUnit.Dot(points[1]));
		}
		float32 NkSegment::Len() {
			return (points[1] - points[0]).Len();
		}
		bool NkSegment::Equivalent(const NkSegment& b) {
			if (*this == b) return true;
			return Len() == NkSegment(b).Len();
		}
	}
}    // namespace nkentseu