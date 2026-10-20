#include "stdafx.h"
#include "ModelAsset.h"
#include <algorithm>
#include <cmath>

namespace Models
{
    Matrix Identity()
    {
        return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    }

    Matrix Multiply(const Matrix& a, const Matrix& b)
    {
        Matrix result{};
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                for (int k = 0; k < 4; ++k)
                {
                    result[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
                }
            }
        }
        return result;
    }

    Vec3 Normalize(Vec3 v)
    {
        float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (length > 1e-8f)
        {
            for (auto& value : v)
            {
                value /= length;
            }
        }
        return v;
    }

    Vec4 NormalizeRotation(Vec4 q)
    {
        float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        if (length < 1e-8f)
        {
            return {0, 0, 0, 1};
        }
        for (auto& value : q)
        {
            value /= length;
        }
        return q;
    }

    Matrix Compose(const Vec3& t, const Vec4& rotation, const Vec3& s)
    {
        Vec4 q = NormalizeRotation(rotation);
        float x = q[0], y = q[1], z = q[2], w = q[3];
        return {(1 - 2 * y * y - 2 * z * z) * s[0],
                (2 * x * y + 2 * z * w) * s[0],
                (2 * x * z - 2 * y * w) * s[0],
                0,
                (2 * x * y - 2 * z * w) * s[1],
                (1 - 2 * x * x - 2 * z * z) * s[1],
                (2 * y * z + 2 * x * w) * s[1],
                0,
                (2 * x * z + 2 * y * w) * s[2],
                (2 * y * z - 2 * x * w) * s[2],
                (1 - 2 * x * x - 2 * y * y) * s[2],
                0,
                t[0],
                t[1],
                t[2],
                1};
    }

    Vec3 TransformPoint(const Matrix& m, const Vec3& p)
    {
        return {m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12],
                m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
                m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]};
    }

    Vec3 TransformNormal(const Matrix& m, const Vec3& n)
    {
        // Cofactor matrix = inverse transpose * determinant. Normalize away its magnitude.
        float a = m[5] * m[10] - m[9] * m[6], b = m[9] * m[2] - m[1] * m[10],
              c = m[1] * m[6] - m[5] * m[2];
        float determinant = m[0] * a + m[4] * b + m[8] * c;
        if (std::abs(determinant) < 1e-12f)
        {
            return {};
        }
        float sign = determinant < 0 ? -1.0f : 1.0f;
        return Normalize(
            {sign * (a * n[0] + b * n[1] + c * n[2]),
             sign * ((m[8] * m[6] - m[4] * m[10]) * n[0] + (m[0] * m[10] - m[8] * m[2]) * n[1] +
                     (m[4] * m[2] - m[0] * m[6]) * n[2]),
             sign * ((m[4] * m[9] - m[8] * m[5]) * n[0] + (m[8] * m[1] - m[0] * m[9]) * n[1] +
                     (m[0] * m[5] - m[4] * m[1]) * n[2])});
    }

    Vec4 Slerp(Vec4 a, Vec4 b, float t)
    {
        a = NormalizeRotation(a);
        b = NormalizeRotation(b);
        float dot = 0;
        for (int i = 0; i < 4; ++i)
        {
            dot += a[i] * b[i];
        }
        if (dot < 0)
        {
            dot = -dot;
            for (auto& value : b)
            {
                value = -value;
            }
        }
        dot = std::clamp(dot, -1.0f, 1.0f);
        float x = 1 - t, y = t;
        if (dot < .9995f)
        {
            float angle = std::acos(dot), denominator = std::sin(angle);
            x = std::sin((1 - t) * angle) / denominator;
            y = std::sin(t * angle) / denominator;
        }
        Vec4 result;
        for (int i = 0; i < 4; ++i)
        {
            result[i] = a[i] * x + b[i] * y;
        }
        return NormalizeRotation(result);
    }
}
