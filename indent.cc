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

static void pretty(istream &i, ostream &o, size_t indent);

void
prettyArray(istream &i, ostream &o, size_t indent)
{
    o << "[";
    size_t eleCount = 0;
    parseArray(i, [=, &eleCount, &o] (istream &i) -> void {
        o << (eleCount++ ? ", " : "") << "\n" << pad(indent + 1);
        pretty(i, o, indent+1);
    });
    if (eleCount)
        o << "\n" << pad(indent);
    o << "]";
}

static void
prettyObject(istream &i, ostream &o, size_t indent)
{
    o << "{";
    int eleCount = 0;
    parseObject(i, [=, &eleCount, &o] (istream &i, string idx) -> void {
        if (eleCount++ != 0)
            o << ", ";
        o << "\n" << pad(indent + 1) << print(idx) << ": ";
        pretty(i, o, indent + 1);
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

template <> void
prettyValue<std::string>(istream &i, ostream &o, size_t indent)
{
    char buf[16384];
    char *p = buf;
    parseString(i, p);
    *p = 0;
    o << JSON::print(buf);
}


static void
pretty(istream &i, ostream &o, size_t indent)
{
    switch (peekType(i)) {
        case JArray: prettyArray(i, o, indent); return;
        case JObject: prettyObject(i, o, indent); return;
        case JString: prettyValue<string>(i, o, indent); return;
        case JNumber: prettyValue<Number>(i, o, indent); return;
        case JBoolean: prettyValue<bool>(i, o, indent); return;
        case JNull: prettyValue<Null>(i, o, indent); return;
        case JEof: return;
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
        pretty(in, out, 0);
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
