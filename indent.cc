#include <json.h>
#include <cstring>
#include <iostream>

using namespace JSON;
using namespace std;

const char *pad(size_t indent) {
    static size_t maxindent = 8192;
    static const char *spaces = strdup(std::string(maxindent, ' ').c_str());
    indent = std::min(4 * indent, maxindent);
    return spaces + maxindent - indent;
}

template <typename numtype> static void pretty(std::istream &i, std::ostream &o, size_t indent);

template <typename numtype> void
prettyArray(std::istream &i, std::ostream &o, size_t indent)
{
    o << "[";
    size_t eleCount = 0;
    parseArray(i, [=, &eleCount, &o] (std::istream &i) -> void {
        o << (eleCount++ ? ", " : "") << "\n" << pad(indent + 1);
        pretty<numtype>(i, o, indent+1);
    });
    if (eleCount)
        o << "\n" << pad(indent);
    o << "]";
}

template <typename numtype> static void
prettyObject(std::istream &i, std::ostream &o, size_t indent)
{
    o << "{";
    int eleCount = 0;
    parseObject(i, [=, &eleCount, &o] (std::istream &i, std::string idx) -> void {
        if (eleCount++ != 0)
            o << ", ";
        o << "\n" << pad(indent + 1) << "\"" << escape(idx) << "\": ";
        pretty<numtype>(i, o, indent + 1);
    });
    if (eleCount)
        o << "\n" << pad(indent);
    o << "}";
}

static void
prettyString(std::istream &i, std::ostream &o, size_t indent)
{
    o << "\"" << escape(parseString(i)) << "\"";
}

template <typename numtype> static void
prettyNumber(std::istream &i, std::ostream &o, size_t indent)
{
    o << parseNumber<numtype>(i);
}

static void
prettyBoolean(std::istream &i, std::ostream &o, size_t indent)
{
    o << (parseBoolean(i) ? "true" : "false");
}

template <typename numtype> static void
pretty(std::istream &i, std::ostream &o, size_t indent)
{
    switch (peekType(i)) {
        case Array: prettyArray<numtype>(i, o, indent); return;
        case Object: prettyObject<numtype>(i, o, indent); return;
        case String: prettyString(i, o, indent); return;
        case Number: prettyNumber<numtype>(i, o, indent); return;
        case Boolean: prettyBoolean(i, o, indent); return;
        case Eof: return;
    }
}

int
main(int argc, char *argv[])
{
    std::cin.tie(0);
    try {
        pretty<long> (std::cin, std::cout, 0);
        std::cout << std::endl;
    }
    catch (const InvalidJSON &je) {
        std::cerr << "invalid JSON: " << je.what() << std::endl;
    }
    return 0;
}
