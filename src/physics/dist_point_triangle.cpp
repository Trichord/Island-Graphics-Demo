// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2025
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 8.0.2025.05.10

#include "distance_functions.h"

#include <algorithm>

#include "glm/glm.hpp"

// This query is exact when using arbitrary-precision arithmetic. It
// can be used also for floating-point arithmetic, but rounding errors
// can sometimes lead to inaccurate result.
distanceFunction::Result distanceFunction::pointTriangle(const glm::vec3 &point, const glm::vec3 &A, const glm::vec3 &B,
                                                         const glm::vec3 &C)
{
    using namespace distanceFunction;
    constexpr float zero{0.0f};
    constexpr float one{1.0f};
    constexpr float two{2.0f};

    // The sqrDistance is set in each block of the
    // nested if-then-else statements. The remaining members are all
    // set at the end of the function.
    Result result{};

    glm::vec3 diff{A - point};
    glm::vec3 edge0{B - A};
    glm::vec3 edge1{C - A};
    float a00{glm::dot(edge0, edge0)};
    float a01{glm::dot(edge0, edge1)};
    float a11{glm::dot(edge1, edge1)};
    float b0{glm::dot(diff, edge0)};
    float b1{glm::dot(diff, edge1)};
    float det{std::max(a00 * a11 - a01 * a01, zero)};
    float s{a01 * b1 - a11 * b0};
    float t{a01 * b0 - a00 * b1};

    if (s + t <= det)
    {
        if (s < zero)
        {
            if (t < zero) // region 4
            {
                if (b0 < zero)
                {
                    t = zero;
                    if (-b0 >= a00)
                    {
                        s = one;
                    }
                    else
                    {
                        s = -b0 / a00;
                    }
                }
                else
                {
                    s = zero;
                    if (b1 >= zero)
                    {
                        t = zero;
                    }
                    else if (-b1 >= a11)
                    {
                        t = one;
                    }
                    else
                    {
                        t = -b1 / a11;
                    }
                }
            }
            else // region 3
            {
                s = zero;
                if (b1 >= zero)
                {
                    t = zero;
                }
                else if (-b1 >= a11)
                {
                    t = one;
                }
                else
                {
                    t = -b1 / a11;
                }
            }
        }
        else if (t < zero) // region 5
        {
            t = zero;
            if (b0 >= zero)
            {
                s = zero;
            }
            else if (-b0 >= a00)
            {
                s = one;
            }
            else
            {
                s = -b0 / a00;
            }
        }
        else // region 0
        {
            // minimum at interior point
            s /= det;
            t /= det;
        }
    }
    else
    {
        float tmp0{0.0f};
        float tmp1{0.0f};
        float numer{0.0f};
        float denom{0.0f};

        if (s < zero) // region 2
        {
            tmp0 = a01 + b0;
            tmp1 = a11 + b1;
            if (tmp1 > tmp0)
            {
                numer = tmp1 - tmp0;
                denom = a00 - two * a01 + a11;
                if (numer >= denom)
                {
                    s = one;
                    t = zero;
                }
                else
                {
                    s = numer / denom;
                    t = one - s;
                }
            }
            else
            {
                s = zero;
                if (tmp1 <= zero)
                {
                    t = one;
                }
                else if (b1 >= zero)
                {
                    t = zero;
                }
                else
                {
                    t = -b1 / a11;
                }
            }
        }
        else if (t < zero) // region 6
        {
            tmp0 = a01 + b1;
            tmp1 = a00 + b0;
            if (tmp1 > tmp0)
            {
                numer = tmp1 - tmp0;
                denom = a00 - two * a01 + a11;
                if (numer >= denom)
                {
                    t = one;
                    s = zero;
                }
                else
                {
                    t = numer / denom;
                    s = one - t;
                }
            }
            else
            {
                t = zero;
                if (tmp1 <= zero)
                {
                    s = one;
                }
                else if (b0 >= zero)
                {
                    s = zero;
                }
                else
                {
                    s = -b0 / a00;
                }
            }
        }
        else // region 1
        {
            numer = a11 + b1 - a01 - b0;
            if (numer <= zero)
            {
                s = zero;
                t = one;
            }
            else
            {
                denom = a00 - two * a01 + a11;
                if (numer >= denom)
                {
                    s = one;
                    t = zero;
                }
                else
                {
                    s = numer / denom;
                    t = one - s;
                }
            }
        }
    }

    result.closest.first = point;
    result.closest.second = A + s * edge0 + t * edge1;
    diff = result.closest.first - result.closest.second;
    result.sqrDistance = glm::dot(diff, diff);
    return result;
}