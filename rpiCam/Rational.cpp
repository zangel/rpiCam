#include "Rational.hpp"

namespace rpiCam
{
    Rational::Rational()
     : numerator(0)
     , denominator(0)
    {

    }

    Rational::Rational(Rational const &that)
     : numerator(that.numerator)
     , denominator(that.denominator)
    {

    }

    Rational::Rational(std::int32_t num, std::int32_t den)
     : numerator(num)
     , denominator(den)
    {
    }
}
