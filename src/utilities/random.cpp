#include "utilities/random.h"

#include <cassert>
#include <cmath>
#include <random>
#include <stdexcept>

namespace
{
std::minstd_rand &rnd()
{
    static std::minstd_rand gen{std::random_device{}()};
    return gen;
}

std::mt19937 &rndMT()
{
    static std::mt19937 mt = [] {
        std::random_device rd;
        std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
        return std::mt19937(seq);
    }();

    return mt;
}
} // namespace

std::size_t random::index(std::size_t size)
{
    assert(size > 0);
    return std::uniform_int_distribution<size_t>(std::size_t{0}, std::size_t{size - 1})(rnd());
}

float random::floatMS(float min, float max)
{
    if (min == max)
        return min;

    if (max < min)
        throw std::runtime_error("Random min greater than random max");

    // uniform_real_distribution requires max - min to be representable
    if (!std::isfinite(max - min))
        throw std::runtime_error("Random range is not finite");

    return std::uniform_real_distribution<float>(min, max)(rnd());
}

float random::floatMT(float min, float max)
{
    if (min == max)
        return min;

    if (max < min)
        throw std::runtime_error("Random min greater than random max");

    if (!std::isfinite(max - min))
        throw std::runtime_error("Random range is not finite");

    std::uniform_real_distribution<float> rnd{min, max};
    return rnd(rndMT());
}