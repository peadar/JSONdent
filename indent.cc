#include <json.h>
#include <fstream>
#include <cstring>
#include <iostream>
#include <unistd.h>

using namespace JSON;
using namespace std;


static int indentLevel = 2;

const char *pad(size_t indent) {
    static size_t maxindent = 8192;
    static const char *spaces = strdup(string(maxindent, ' ').c_str());
    indent = min(indentLevel * indent, maxindent);
    return spaces + maxindent - indent;
}

template <typename numtype>
static void pretty(istream &i, ostream &o, size_t indent);

template <typename numtype> void
prettyArray(istream &i, ostream &o, size_t indent)
{
    o << "[";
    size_t eleCount = 0;
    parseArray(i, [=, &eleCount, &o] (istream &i) -> void {
        o << (eleCount++ ? "," : "") << "\n" << pad(indent + 1);
        pretty<numtype>(i, o, indent+1);
    });
    if (eleCount)
        o << "\n" << pad(indent);
    o << "]";
}

template <typename numtype> static void
prettyObject(istream &i, ostream &o, size_t indent)
{
    o << "{";
    int eleCount = 0;
    parseObject(i, [=, &eleCount, &o] (istream &i, string idx) -> void {
        if (eleCount++ != 0)
            o << ",";
        o << "\n" << pad(indent + 1) << "\"" << Escape(idx) << "\": ";
        pretty<numtype>(i, o, indent + 1);
    });
    if (eleCount)
        o << "\n" << pad(indent);
    o << "}";
}

static void
prettyString(istream &i, ostream &o, size_t indent)
{
    o << "\"" << Escape(parseString(i)) << "\"";
}

static void
prettyNull(istream &i, ostream &o, size_t indent)
{
    parseNull(i);
    o << "null";
}

template <typename numtype> static void
prettyNumber(istream &i, ostream &o, size_t indent)
{
    o << parseNumber<numtype>(i);
}

static void
prettyBoolean(istream &i, ostream &o, size_t indent)
{
    o << (parseBoolean(i) ? "true" : "false");
}

template <typename numtype> static void
pretty(istream &i, ostream &o, size_t indent)
{
    switch (peekType(i)) {
        case Array: prettyArray<numtype>(i, o, indent); return;
        case Object: prettyObject<numtype>(i, o, indent); return;
        case String: prettyString(i, o, indent); return;
        case Number: prettyNumber<numtype>(i, o, indent); return;
        case Boolean: prettyBoolean(i, o, indent); return;
        case Null: prettyNull(i, o, indent); return;
        case Eof: return;
    }
}

static int
usage() {
    clog << "usage: jdent [ -f ] [ files ... ]" << endl;
    return -1;
}

static bool doFloat;

static bool
indent(istream &in, ostream &out)
{
    static unsigned char bom[] = { 0xef, 0xbb, 0xbf };

    // Deal with UTF-8 BOM mark. (Lordy, why would you do that?)
    if (in.peek() == bom[0]) {
        char s[sizeof bom + 1];
        in.get(s, sizeof s);
        if (memcmp(s, bom, sizeof bom) != 0)
            throw InvalidJSON("invalid BOM/JSON");
    }
    try {
        if (doFloat)
            pretty<double> (in, out, 0);
        else
            pretty<long> (in, out, 0);
        cout << endl;
        return true;
    }
    catch (const InvalidJSON &je) {
        cerr << "invalid JSON: " << je.what() << endl;
        return false;
    }
}

int
main(int argc, char *argv[])
{
    cin.tie(0);
    int c;
    while ((c = getopt(argc, argv, "fi:")) != -1) {
        switch (c) {
            case 'f': doFloat = true; break;
            case 'i': indentLevel = strtoul(optarg, 0, 0); break;
            default: return usage();
        }
    }
    bool good = true;
    for (int i = optind; i < argc; ++i) {
        if (strcmp(argv[i], "-") != 0) {
            ifstream inFile;
            inFile.open(argv[i]);
            if (inFile.good())
                good = good && indent(inFile, cout);
            else
                clog << "failed to open " << argv[i]
                        << ": " << strerror(errno) << endl;
        } else {
            good = good && indent(cin, cout);
        }
    }
    if (optind == argc)
        good = indent(cin, cout);
    return good ? 0 : 1;
}
