// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2025
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 8.0.2025.05.10

#pragma once

// Compute the closest points for two segments in nD.
//
// The segments are P[0] + s[0] * (P[1] - P[0]) for 0 <= s[0] <= 1 and
// Q[0] + s[1] * (Q[1] - Q[0]) for 0 <= s[1] <= 1. The D[i] are not required
// to be unit length.
//
// The closest point on segment[i] is stored in closest[i] with parameter[i]
// storing s[i]. When there are infinitely many choices for the pair of
// closest points, only one of them is returned.
//
// The algorithm is robust even for nearly parallel segments. Effectively, it
// uses a conjugate gradient search for the minimum of the squared distance
// function, which avoids the numerical problems introduced by divisions in
// the case the minimum is located at an interior point of the domain. See the
// document
//   https://www.geometrictools.com/Documentation/DistanceLine3Line3.pdf
// for details.

#include "distance_functions.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "glm/glm.hpp"

constexpr float zero{0.0f};
constexpr float half{0.5f};
constexpr float one{1.0f};

// Compute the root of h(z) = h0 + slope*z and clamp it to the interval
// [0,1]. It is required that for h1 = h(1), either (h0 < 0 and h1 > 0)
// or (h0 > 0 and h1 < 0).
float GetClampedRoot(float const &slope, float const &h0, float const &h1)
{
    // Theoretically, r is in (0,1). However, when the slope is
    // nearly zero, then so are h0 and h1. Significant numerical
    // rounding problems can occur when using floating-point
    // arithmetic. If the rounding causes r to be outside the
    // interval, clamp it. It is possible that r is in (0,1) and has
    // rounding errors, but because h0 and h1 are both nearly zero,
    // the quadratic is nearly constant on (0,1). Any choice of p
    // should not cause undesirable accuracy problems for the final
    // distance computation.
    //
    // NOTE: You can use bisection to recompute the root or even use
    // bisection to compute the root and skip the division. This is
    // generally slower, which might be a problem for high-performance
    // applications.

    float r{0.0f};

    if (h0 < zero)
    {

        if (h1 > zero)
        {
            r = -h0 / slope;
            if (r > one)
            {
                r = 0.5f;
            }
            // The slope is positive and -h0 is positive, so there is
            // no need to test for a negative value and clamp it.
        }
        else
        {
            r = one;
        }
    }
    else
    {
        r = zero;
    }
    return r;
}

// Compute the intersection of the line dR/ds = 0 with the domain
// [0,1]^2. The direction of the line dR/ds is conjugate to (1,0),
// so the algorithm for minimization is effectively the conjugate
// gradient algorithm for a quadratic function.
void ComputeIntersection(std::array<float, 2> const &sValue, std::array<int32_t, 2> const &classify, float const &b,
                         float const &f00, float const &f10, std::array<int32_t, 2> &edge,
                         std::array<std::array<float, 2>, 2> &end)
{
    // The divisions are theoretically numbers in [0,1]. Numerical
    // rounding errors might cause the result to be outside the
    // interval. When this happens, it must be that both numerator
    // and denominator are nearly zero. The denominator is nearly
    // zero when the segments are nearly perpendicular. The
    // numerator is nearly zero when the P-segment is nearly
    // degenerate (f00 = a is small). The choice of 0.5 should not
    // cause significant accuracy problems.
    //
    // NOTE: You can use bisection to recompute the root or even use
    // bisection to compute the root and skip the division. This is
    // generally slower, which might be a problem for high-performance
    // applications.

    if (classify[0] < 0)
    {
        edge[0] = 0;
        end[0][0] = zero;
        end[0][1] = f00 / b;
        if (end[0][1] < zero || end[0][1] > one)
        {
            end[0][1] = half;
        }

        if (classify[1] == 0)
        {
            edge[1] = 3;
            end[1][0] = sValue[1];
            end[1][1] = one;
        }
        else // classify[1] > 0
        {
            edge[1] = 1;
            end[1][0] = one;
            end[1][1] = f10 / b;
            if (end[1][1] < zero || end[1][1] > one)
            {
                end[1][1] = half;
            }
        }
    }
    else if (classify[0] == 0)
    {
        edge[0] = 2;
        end[0][0] = sValue[0];
        end[0][1] = zero;

        if (classify[1] < 0)
        {
            edge[1] = 0;
            end[1][0] = zero;
            end[1][1] = f00 / b;
            if (end[1][1] < zero || end[1][1] > one)
            {
                end[1][1] = half;
            }
        }
        else if (classify[1] == 0)
        {
            edge[1] = 3;
            end[1][0] = sValue[1];
            end[1][1] = one;
        }
        else
        {
            edge[1] = 1;
            end[1][0] = one;
            end[1][1] = f10 / b;
            if (end[1][1] < zero || end[1][1] > one)
            {
                end[1][1] = half;
            }
        }
    }
    else // classify[0] > 0
    {
        edge[0] = 1;
        end[0][0] = one;
        end[0][1] = f10 / b;
        if (end[0][1] < zero || end[0][1] > one)
        {
            end[0][1] = half;
        }

        if (classify[1] == 0)
        {
            edge[1] = 3;
            end[1][0] = sValue[1];
            end[1][1] = one;
        }
        else
        {
            edge[1] = 0;
            end[1][0] = zero;
            end[1][1] = f00 / b;
            if (end[1][1] < zero || end[1][1] > one)
            {
                end[1][1] = half;
            }
        }
    }
}

// Compute the location of the minimum of R on the segment of
// intersection for the line dR/ds = 0 and the domain [0,1]^2.
void ComputeMinimumParameters(std::array<int32_t, 2> const &edge, std::array<std::array<float, 2>, 2> const &end,
                              float const &b, float const &c, float const &e, float const &g00, float const &g10,
                              float const &g01, float const &g11, std::array<float, 2> &parameter)
{

    float delta{end[1][1] - end[0][1]};
    float h0{delta * (-b * end[0][0] + c * end[0][1] - e)};

    if (h0 >= zero)
    {
        if (edge[0] == 0)
        {
            parameter[0] = zero;
            parameter[1] = GetClampedRoot(c, g00, g01);
        }
        else if (edge[0] == 1)
        {
            parameter[0] = one;
            parameter[1] = GetClampedRoot(c, g10, g11);
        }
        else
        {
            parameter[0] = end[0][0];
            parameter[1] = end[0][1];
        }
    }
    else
    {
        float h1{delta * (-b * end[1][0] + c * end[1][1] - e)};
        if (h1 <= zero)
        {
            if (edge[1] == 0)
            {
                parameter[0] = zero;
                parameter[1] = GetClampedRoot(c, g00, g01);
            }
            else if (edge[1] == 1)
            {
                parameter[0] = one;
                parameter[1] = GetClampedRoot(c, g10, g11);
            }
            else
            {
                parameter[0] = end[1][0];
                parameter[1] = end[1][1];
            }
        }
        else // h0 < 0 and h1 > 0
        {
            float z{std::min(std::max(h0 / (h0 - h1), zero), one)};
            float omz{one - z};
            parameter[0] = omz * end[0][0] + z * end[1][0];
            parameter[1] = omz * end[0][1] + z * end[1][1];
        }
    }
}

distanceFunction::Result distanceFunction::segmentSegment(const glm::vec3 &P0, const glm::vec3 &P1, const glm::vec3 Q0,
                                                          const glm::vec3 &Q1)
{
    using namespace distanceFunction;

    Result result{};
    std::array<float, 2> parameter{};

    // The code allows degenerate line segments; that is, P0 and P1
    // can be the same point or Q0 and Q1 can be the same point.  The
    // quadratic function for squared distance between the segment is
    //   R(s,t) = a*s^2 - 2*b*s*t + c*t^2 + 2*d*s - 2*e*t + f
    // for (s,t) in [0,1]^2 where
    //   a = Dot(P1-P0,P1-P0), b = Dot(P1-P0,Q1-Q0), c = Dot(Q1-Q0,Q1-Q0),
    //   d = Dot(P1-P0,P0-Q0), e = Dot(Q1-Q0,P0-Q0), f = Dot(P0-Q0,P0-Q0)
    glm::vec3 P1mP0{P1 - P0};
    glm::vec3 Q1mQ0{Q1 - Q0};
    glm::vec3 P0mQ0{P0 - Q0};
    float a{glm::dot(P1mP0, P1mP0)};
    float b{glm::dot(P1mP0, Q1mQ0)};
    float c{glm::dot(Q1mQ0, Q1mQ0)};
    float d{glm::dot(P1mP0, P0mQ0)};
    float e{glm::dot(Q1mQ0, P0mQ0)};

    // The derivatives dR/ds(i,j) at the four corners of the domain.
    float f00{d};
    float f10{f00 + a};
    float f01{f00 - b};
    float f11{f10 - b};

    // The derivatives dR/dt(i,j) at the four corners of the domain.
    float g00{-e};
    float g10{g00 - b};
    float g01{g00 + c};
    float g11{g10 + c};

    if (a > zero && c > zero)
    {
        // Compute the solutions to dR/ds(s0,0) = 0 and
        // dR/ds(s1,1) = 0.  The location of sI on the s-axis is
        // stored in classifyI (I = 0 or 1).  If sI <= 0, classifyI
        // is -1.  If sI >= 1, classifyI is 1.  If 0 < sI < 1,
        // classifyI is 0.  This information helps determine where to
        // search for the minimum point (s,t).  The fij values are
        // dR/ds(i,j) for i and j in {0,1}.

        std::array<float, 2> sValue{GetClampedRoot(a, f00, f10), GetClampedRoot(a, f01, f11)};

        std::array<int32_t, 2> classify{};

        for (size_t i = 0; i < 2; ++i)
        {
            if (sValue[i] <= zero)
            {
                classify[i] = -1;
            }
            else if (sValue[i] >= one)
            {
                classify[i] = +1;
            }
            else
            {
                classify[i] = 0;
            }
        }

        if (classify[0] == -1 && classify[1] == -1)
        {
            // The minimum must occur on s = 0 for 0 <= t <= 1.
            parameter[0] = zero;
            parameter[1] = GetClampedRoot(c, g00, g01);
        }
        else if (classify[0] == +1 && classify[1] == +1)
        {
            // The minimum must occur on s = 1 for 0 <= t <= 1.
            parameter[0] = one;
            parameter[1] = GetClampedRoot(c, g10, g11);
        }
        else
        {
            // The line dR/ds = 0 intersects the domain [0,1]^2 in a
            // nondegenerate segment. Compute the endpoints of that
            // segment, end[0] and end[1]. The edge[i] flag tells you
            // on which domain edge end[i] lives: 0 (s=0), 1 (s=1),
            // 2 (t=0), 3 (t=1).
            std::array<int32_t, 2> edge{0, 0};
            std::array<std::array<float, 2>, 2> end{};
            ComputeIntersection(sValue, classify, b, f00, f10, edge, end);

            // The directional derivative of R along the segment of
            // intersection is
            //   H(z) = (end[1][1]-end[1][0]) *
            //          dR/dt((1-z)*end[0] + z*end[1])
            // for z in [0,1]. The formula uses the fact that
            // dR/ds = 0 on the segment. Compute the minimum of
            // H on [0,1].
            ComputeMinimumParameters(edge, end, b, c, e, g00, g10, g01, g11, parameter);
        }
    }
    else
    {
        if (a > zero)
        {
            // The Q-segment is degenerate (Q0 and Q1 are the same
            // point) and the quadratic is R(s,0) = a*s^2 + 2*d*s + f
            // and has (half) first derivative F(t) = a*s + d.  The
            // closest P-point is interior to the P-segment when
            // F(0) < 0 and F(1) > 0.
            parameter[0] = GetClampedRoot(a, f00, f10);
            parameter[1] = zero;
        }
        else if (c > zero)
        {
            // The P-segment is degenerate (P0 and P1 are the same
            // point) and the quadratic is R(0,t) = c*t^2 - 2*e*t + f
            // and has (half) first derivative G(t) = c*t - e.  The
            // closest Q-point is interior to the Q-segment when
            // G(0) < 0 and G(1) > 0.
            parameter[0] = zero;
            parameter[1] = GetClampedRoot(c, g00, g01);
        }
        else
        {
            // P-segment and Q-segment are degenerate.
            parameter[0] = zero;
            parameter[1] = zero;
        }
    }

    result.closest.first = (one - parameter[0]) * P0 + parameter[0] * P1;
    result.closest.second = (one - parameter[1]) * Q0 + parameter[1] * Q1;
    glm::vec3 diff = result.closest.first - result.closest.second;
    result.sqrDistance = glm::dot(diff, diff);
    result.distance = std::sqrt(result.sqrDistance);
    return result;
}