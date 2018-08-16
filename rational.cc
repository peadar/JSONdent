#include "rational.h"
#include "json.h"
#include <iostream>
#include <cassert>
#include <string.h>

using Rat = Rational<int>;


Rat s2r(char *p)
{
    char *q = strchr(p, '/');
    assert(q);
    *q++ = 0;

    return Rat(atoi(p), atoi(q));
}


main(int argc, char *argv[])
{
    char *left = argv[1];
    char *right = argv[2];
    Rat l = s2r(left);
    Rat r = s2r(right);

    std::cout << JSON::make_value(l) << "+" << JSON::make_value(r) << "=" << JSON::make_value(l + r) << std::endl;
    std::cout << JSON::make_value(l) << "-" << JSON::make_value(r) << "=" << JSON::make_value(l - r) << std::endl;
    std::cout << JSON::make_value(l) << "*" << JSON::make_value(r) << "=" << JSON::make_value(l * r) << std::endl;
    std::cout << JSON::make_value(l) << "/" << JSON::make_value(r) << "=" << JSON::make_value(l / r) << std::endl;
}
