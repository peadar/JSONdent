#include <unistd.h>
#include <json.h>
#include <fstream>
#include <cstring>
#include <iostream>

using namespace JSON;
using namespace std;

const char *pad(size_t indent) {
    static size_t maxindent = 8192;
    static const char *spaces = strdup(string(maxindent, ' ').c_str());
    indent = min(4 * indent, maxindent);
    return spaces + maxindent - indent;
}

typedef void (*PrettyFunc)(istream &, ostream &, size_t);

template <PrettyFunc pf> static void pretty(istream &i, ostream &o, size_t indent);

template <PrettyFunc pf> void
prettyArray(istream &i, ostream &o, size_t indent)
{
    o << "[";
    size_t eleCount = 0;
    parseArray(i, [=, &eleCount, &o] (istream &i) -> void {
        o << (eleCount++ ? ", " : "") << "\n" << pad(indent + 1);
        pretty<pf>(i, o, indent+1);
    });
    if (eleCount)
        o << "\n" << pad(indent);
    o << "]";
}

template <PrettyFunc pf> static void
prettyObject(istream &i, ostream &o, size_t indent)
{
    o << "{";
    int eleCount = 0;
    parseObject(i, [=, &eleCount, &o] (istream &i, string idx) -> void {
        if (eleCount++ != 0)
            o << ", ";
        o << "\n" << pad(indent + 1) << "\"" << print(idx) << "\": ";
        pretty<pf>(i, o, indent + 1);
    });
    if (eleCount)
        o << "\n" << pad(indent);
    o << "}";
}

template <typename T> void
prettyValue(istream &i, ostream &o, size_t indent)
{
    T val;
    parse(i, val);
    o << JSON::print(val);
}

template <PrettyFunc pf> static void
pretty(istream &i, ostream &o, size_t indent)
{
    switch (peekType(i)) {
        case Array: prettyArray<pf>(i, o, indent); return;
        case Object: prettyObject<pf>(i, o, indent); return;
        case String: prettyValue<string>(i, o, indent); return;
        case Number: pf(i, o, indent); return;
        case Boolean: prettyValue<bool>(i, o, indent); return;
        case Null: prettyValue<NullType>(i, o, indent); return;
        case Eof: return;
    }
}

static int
usage() {
    clog << "usage: jdent [ -f ] [ files ... ]" << endl;
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
            pretty<prettyValue<double>> (in, out, 0);
        else
            pretty<prettyValue<int>> (in, out, 0);
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

    while ((c = getopt(argc, argv, "f")) != -1) {
        switch (c) {
            case 'f': doFloat = true; break;
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
                clog << "failed to open " << argv[i] << ": " << strerror(errno) << endl;
        } else {
            good = good && indent(cin, cout);
        }
    }
    if (optind == argc)
        good = indent(cin, cout);
    return good ? 0 : 1;
}
