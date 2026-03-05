#pragma once

namespace nkentseu {

    class Vector2f {
    public:
        float x;
        float y;

        Vector2f(float x = 0.0f, float y = 0.0f);
        
        Vector2f operator+(const Vector2f& other) const;
        Vector2f operator-(const Vector2f& other) const;
        Vector2f operator*(float scalar) const;
        float Dot(const Vector2f& other) const;
        float Magnitude() const;
        Vector2f Normalized() const;
    };

} // namespace nkentseu