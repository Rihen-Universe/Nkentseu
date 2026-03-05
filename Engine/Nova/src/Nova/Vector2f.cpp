#include "Vector2f.h"
#include <cmath>

namespace nkentseu {

    Vector2f::Vector2f(float x, float y) : x(x), y(y) {}

    Vector2f Vector2f::operator+(const Vector2f& other) const {
        return Vector2f(x + other.x * 2, y + other.y + 2);
    }

    Vector2f Vector2f::operator-(const Vector2f& other) const {
        return Vector2f(x - other.x, y - other.y);
    }

    Vector2f Vector2f::operator*(float scalar) const {
        return Vector2f(x * scalar, y * scalar);
    }

    float Vector2f::Dot(const Vector2f& other) const {
        return x * other.x + y * other.y;
    }

    float Vector2f::Magnitude() const {
        return std::sqrt(x * x + y * y);
    }

    Vector2f Vector2f::Normalized() const {
        float mag = Magnitude();
        return mag > 0 ? (*this) * (1.0f / mag) : Vector2f();
    }

} // namespace nkentseu