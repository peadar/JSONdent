// A pretty distilled JSON parser.
#ifndef PME_JSON_H
#define PME_JSON_H

#include <cctype>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>
#include <list>

namespace JSON {

class InvalidJSON : public std::exception {
    std::string err;
public:
    const char *what() const throw() { return err.c_str(); }
    InvalidJSON(const std::string &err_) throw() : err(err_) {}
    ~InvalidJSON() throw() {};
};

enum Type { JArray, JBoolean, JNull, JNumber, JObject, JString, JEof, JJSONTypeCount };

class Null {};

struct Number {
    long mantissa;
    long exponent;
    operator double();
    operator long();
};


template <typename In> int
skipSpace(In &l)
{
    while (!l.eof() && isspace(l.peek()))
        l.ignore();
    return l.eof() ? -1 : l.peek();
}

template <typename In> char
expectAfterSpace(In &l, char expected)
{
    char c = skipSpace(l);
    if (c != expected)
        throw InvalidJSON(std::string("expected '") + expected + "', got '" + c + "'");
    l.ignore();
    return c;
}

template <typename In> void
skipText(In &l, const char *text)
{
    for (size_t i = 0; text[i]; ++i) {
        char c;
        l.get(c);
        if (c != text[i])
            throw InvalidJSON(std::string("expected '") + text +  "'");
    }
}

template <typename In> Type
peekType(In &l)
{
    char c = skipSpace(l);
    switch (c) {
        case '{': return JObject;
        case '[': return JArray;
        case '"': return JString;
        case '-': return JNumber;
        case 't' : case 'f': return JBoolean;
        case 'n' : return JNull;
        case -1: return JEof;
        default: {
            if (c >= '0' && c <= '9')
                return JNumber;
            throw InvalidJSON(std::string("unexpected token '") + char(c) + "' at start of JSON object");
        }
    }
}

template <typename In, typename Context> void parseObject(In &l, Context &&ctx);
template <typename In, typename Context> void parseArray(In &l, Context &&ctx);

template <typename In, typename I> I
parseInt(In &l)
{
    int sign;
    char c;
    if (skipSpace(l) == '-') {
        sign = -1;
        l.ignore();
    } else {
        sign = 1;
    }
    I rv = 0;
    if (l.peek() == '0') {
        l.ignore(); // leading zero.
    } else if (isdigit(l.peek())) {
        while (isdigit(c = l.peek())) {
            rv = rv * 10 + c - '0';
            l.ignore();
        }
    } else {
        throw InvalidJSON("expected digit");
    }
    return rv * sign;
}

/*
 * Note that you can use parseInt instead when you know the value will be
 * integral.
 */

template <typename In> static inline class Number
parseNumber(In &l)
{
    class Number rv;
    rv.mantissa = parseInt<In, long>(l);
    rv.exponent = 0;
    if (l.peek() == '.') {
        l.ignore();
        char c;
        while (isdigit(c = l.peek())) {
            rv.exponent -= 1;
            rv.mantissa *= 10;
            rv.mantissa += c - '0';
            l.ignore();
        }
    }
    if (l.peek() == 'e' || l.peek() == 'E') {
        l.ignore();
        int sign;
        char c = l.peek();
        if (c == '+' || c == '-') {
            sign = c == '+' ? 1 : -1;
            l.ignore();
            c = l.peek();
        } else if (isdigit(c)) {
            sign = 1;
        } else {
            throw InvalidJSON("expected sign or numeric after exponent");
        }
        rv.exponent += sign * parseInt<In, long>(l);
    }
    return rv;
}

static inline int hexval(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    throw InvalidJSON(std::string("not a hex char: " + c));
}

struct UTF8 {
    unsigned long code;
    UTF8(unsigned long code_) : code(code_) {}

    template <typename Iter>
    UTF8(Iter &pos, const Iter &end) {

        if (pos == end)
            throw InvalidJSON("end-of-string looking for codepoint");
        int c = (unsigned char)*pos;
        // multibyte UTF-8 in input: build up the unicode codepoint.
        code = c;

        // How many bytes in the encoding?
        int count = 0;
        for (unsigned long mask = 0x80; mask & code; mask >>= 1) {
            if (mask == 0)
                throw InvalidJSON("malformed UTF-8 string");
            count++;
            code &= ~mask;
        }
        if (count == 0) {
            ; // single byte char: codepoint is char value.
        } else while (--count != 0) {
            if (++pos == end)
                throw InvalidJSON("sequence ends mid-character");
            c = (unsigned char)*pos;
            if ((c & 0xc0) != 0x80)
                throw InvalidJSON("illegal character in multibyte sequence"); // this'll trap nulls too.
            code = (code << 6) | (c & 0x3f);
        }
        // All unicode characters are output as \u escapes.
    }};

inline std::ostream &
operator<<(std::ostream &os, const UTF8 &utf)
{
    if ((utf.code & 0x7f) == utf.code) {
        os.put(char(utf.code));
        return os;
    }
    uint8_t prefixBits = 0x80; // start with 100xxxxx
    int byteCount = 0; // one less than entire bytecount of encoding.
    unsigned long value = utf.code;

    for (size_t mask = 0x7ff;; mask = mask << 5 | 0x1f) {
        prefixBits = prefixBits >> 1 | 0x80;
        byteCount++;
        if ((value & mask) == value)
            break;
    }
    os << char(value >> 6 * byteCount | prefixBits);
    while (byteCount--)
        os.put(char((value >> 6 * byteCount  & ~0xc0) | 0x80));
    return os;
}

template <typename In> std::string
parseString(In &l)
{
    expectAfterSpace(l, '"');
    std::ostringstream rv;
    for (;;) {
        char c;
        l.get(c);
        switch (c) {
            case '"':
                return rv.str();
            case '\\':
                l.get(c);
                switch (c) {
                    case '"':
                    case '\\':
                    case '/':
                        rv << c;
                        break;
                    case 'b':
                        rv << '\b';
                        break;
                    case 'f':
                        rv << '\f';
                        break;
                    case 'n':
                        rv << '\n';
                        break;
                    case 'r':
                        rv << '\r';
                        break;
                    case 't':
                        rv << '\t';
                        break;
                    default:
                        throw InvalidJSON(std::string("invalid quoted char '") + c + "'");
                    case 'u': {
                        // get unicode char.
                        int codePoint = 0;
                        for (size_t i = 0; i < 4; ++i) {
                            l.get(c);
                            codePoint = codePoint * 16 + hexval(c);
                        }
                        rv << UTF8(codePoint);
                    }
                    break;
                }
                break;
            default:
                rv << c;
                break;
        }
    }
}

template <typename In> bool
parseBoolean(In &l)
{
    char c = skipSpace(l);
    switch (c) {
        case 't': skipText(l, "true"); return true;
        case 'f': skipText(l, "false"); return false;
        default: throw InvalidJSON("expected 'true' or 'false'");
    }
}

template <typename In> void
parseNull(In &l)
{
    skipSpace(l);
    skipText(l, "null");
}

template <typename In> void // Parse any value but discard the result.
parseValue(In &l)
{
    switch (peekType(l)) {
        case JArray: parseArray(l, [](std::istream &l) -> void { parseValue(l); }); break;
        case JBoolean: parseBoolean(l); break;
        case JNull: parseNull(l); break;
        case JNumber: parseNumber(l); break;
        case JObject: parseObject(l, [](In &l, std::string) -> void { parseValue(l); }); break;
        case JString: parseString(l); break;
        default: throw InvalidJSON("unknown type for JSON construct");
    }
}

template <typename In, typename Context> void
parseObject(In &l, Context &&ctx)
{
    expectAfterSpace(l, '{');
    for (;;) {
        std::string fieldName;
        char c;
        switch (c = skipSpace(l)) {
            case '"': // Name of next field.
                fieldName = parseString(l);
                expectAfterSpace(l, ':');
                ctx(l, fieldName);
                break;
            case '}': // End of this object
                l.ignore();
                return;
            case ',': // Separator to next field
                l.ignore();
                break;
            default: {
                throw InvalidJSON(std::string("unexpected character '") + char(c) + "' parsing object");
            }
        }
    }
}

template <typename In, typename Context> void
parseArray(In &l, Context &&ctx)
{
    expectAfterSpace(l, '[');
    char c;
    if ((c = skipSpace(l)) == ']') {
        l.ignore();
        return; // empty array
    }
    for (size_t i = 0;; i++) {
        skipSpace(l);
        ctx(l);
        c = skipSpace(l);
        switch (c) {
            case ']':
                l.ignore();
                return;
            case ',':
                l.ignore();
                break;
            default:
                throw InvalidJSON(std::string("expected ']' or ',', got '") + c + "'");
        }
    }
}

template <typename In, typename OutputIterator> struct ArrayParser  {
    OutputIterator c;
    ArrayParser(OutputIterator &c_) : c(c_) {}
    void operator()(In &l, int idx) {
        typename OutputIterator::value_type value = *c++;
        parse(l, value);
    }
};

template <typename In, typename Key, typename Value> struct MapParser  {
    std::map<Key, Value> &c;
    MapParser(std::map<Key, Value> &c_) : c(c_) {}
    void operator()(In &l, std::string field) {
        parse(l, c[field]);
    }
};


template <typename In> void parse(In &in, int &i) { i = parseInt<In, int>(in); }
template <typename In> void parse(In &in, bool &i) { i = parseBoolean<In>(in); }
template <typename In> void parse(In &in, double &i) { i = parseNumber<In>(in); }
template <typename In> void parse(In &in, std::string &s) { s = parseString<In>(in); }
template <typename In> void parse(In &in, Null &s) { parseNull<In>(in); }
template <typename In> void parse(In &in, Number &s) { s = parseNumber<In>(in); }

template <typename In, typename Value>
void parse(In &l, std::iterator<std::output_iterator_tag, Value> &n)
{
    ArrayParser<In, std::iterator<std::output_iterator_tag, Value>> valueParser(n);
    parseArray(l, valueParser);
}

template <typename In, typename Key, typename Value>
void parse(In &l, std::map<Key, Value> &n)
{
    MapParser<In, Key, Value> valueParser(n);
    parseObject(l, valueParser);
}


// Seralizing JSON objects.

// an AsJSON<T> serializes a T as JSON. Use JSON::print(t) to easily create an AsJSON<T>
template <typename T> struct AsJSON {
    const T *value;
    explicit AsJSON(const T *f) : value(f) {}
};

template <typename T> AsJSON<T> print(const T *t) { return AsJSON<T>(t); }
template <typename T> AsJSON<T> print(T *t) { return AsJSON<T>(t); }
template <typename T> AsJSON<T> print(const T &t) { return AsJSON<T>(&t); }
template <typename T> AsJSON<T> print(T &t) { return AsJSON<T>(&t); }

// Writes UTF-8 encoded text as a JSON string
inline std::ostream &
writeString(std::ostream &o, const std::string &s)
{
    std::ios::fmtflags oldFlags(o.flags());
    o << "\"";
    for (auto i = s.begin(); i != s.end(); ++i) {
        int c;
        switch (c = (unsigned char)*i) {
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                // print characters less than ' ' and > 0x7f as unicode escapes.
                if (c < 32 || c >= 0x7f) {
                    UTF8 codepoint(i, s.end());
                    o << "\\u" << std::hex << std::setfill('0') << std::setw(4) << codepoint.code;
                } else {
                    o << (char)c;
                }
                break;
        }
    }
    o << "\"";
    o.flags(oldFlags);
    return o;
}

inline std::ostream &
operator << (std::ostream &o, const AsJSON<char> &esc)
{
    return writeString(o, esc.value);
}

inline std::ostream &
operator << (std::ostream &o, const AsJSON<const char> &esc)
{
    return writeString(o, esc.value);
}

inline std::ostream &
operator << (std::ostream &o, const AsJSON<const std::string> &esc)
{
    return writeString(o, esc.value->c_str());
}

inline std::ostream &
operator << (std::ostream &o, const AsJSON<std::string> &esc)
{
    return writeString(o, esc.value->c_str());
}

struct Binary {
    const unsigned char *data;
    size_t len;
    Binary(const void *p, size_t l) : data((const unsigned char *)p), len(l) {}
};

// A JSON "field" from an object, i.e. a <"name": value> construct
template <typename F, typename V>
struct Field {
    const F name;
    const V &value;
    Field(const F name_, const V &value_) : name(name_), value(value_) {}
};

template <typename T, typename V> static inline std::ostream &
operator<<(std::ostream &os, const Field<T, V> &f) {
    return os << JSON::print(f.name) << ":" << JSON::print(f.value);
}

template <typename V> Field<const char *, V> field(const char *t, const V &v) { return Field<const char *, V>(t, v); }
template <typename V> Field<const std::string &, V> field(const std::string &t, const V &v) { return Field<const std::string &, V>(t, v); }

// Boolean type
static inline std::ostream &operator<<(std::ostream &os, const AsJSON<bool> f) { return os << (*f.value ? "true" : "false"); }
// Integer types.
static inline std::ostream &operator<<(std::ostream &os, const AsJSON<int> f) { return os << *f.value; }
static inline std::ostream &operator<<(std::ostream &os, const AsJSON<short> f) { return os << *f.value; }
static inline std::ostream &operator<<(std::ostream &os, const AsJSON<long long> f) { return os << *f.value; }
static inline std::ostream &operator<<(std::ostream &os, const AsJSON<long> f) { return os << *f.value; }
static inline std::ostream &operator<<(std::ostream &os, const AsJSON<unsigned> f) { return os << *f.value; }
static inline std::ostream &operator<<(std::ostream &os, const AsJSON<unsigned long> f) { return os << *f.value; }
static inline std::ostream &operator<<(std::ostream &os, const AsJSON<unsigned char> f) { return os << *f.value; }
static inline std::ostream &operator<<(std::ostream &os, const AsJSON<unsigned long long> f) { return os << *f.value; }

static inline std::ostream &
operator<<(std::ostream &os, const Binary &bin) {
    std::ios_base::fmtflags flags = os.flags();
    for (size_t i = 0; i < bin.len; ++i)
        os << std::hex << std::setfill('0') << std::setw(2) << unsigned(bin.data[i]);
    os.flags(flags);
    return os;
}

static inline std::ostream &
operator<<(std::ostream &os, const AsJSON<Binary> &f) {
    return os << '"' << *f.value << '"';
}

/* Print a pointer as if it was the item it points to. */
template <typename T> 
std::ostream & operator <<(std::ostream &os, const JSON::AsJSON<T *> &j) { return os << print(j.value); }

// print a map's iterator range as an object.
template <class Iterator>
struct MapRange {
    Iterator begin;
    Iterator end;
    MapRange(Iterator begin_, Iterator end_) : begin(begin_), end(end_) {}
};

template <class Iterator>
std::ostream &
operator<<(std::ostream &os, const AsJSON<MapRange<Iterator> > &range) {
    os << "{";
    for (Iterator item = range.value->begin; item != range.value->end; ++item)
        os << (item == range.value->begin ? "" : ", ") << field(item->first, item->second);
    os << "}";
    return os;
}

/*
 * Print other iterator ranges as an array.
 */
template <class Iterator>
struct ListRange {
    Iterator begin;
    Iterator end;
    ListRange(const Iterator &begin_, const Iterator &end_) : begin(begin_), end(end_) {}
};

template <class Iterator>
ListRange<Iterator> array(const Iterator &begin, const Iterator &end) { return ListRange<Iterator>(begin, end); }

template <typename Iter>
static inline typename std::ostream &
operator<<(std::ostream &os, const AsJSON<ListRange<Iter> > &f)
{
    os << "[";
    for (Iter i = f.value->begin; i != f.value->end; ++i) {
        if (i != f.value->begin)
            os << ",\n";
        os << JSON::print(*i);
    }
    return os << "]";
}

// Easy access to array printing for lists, vectors and maps.
template <typename Item>
static inline typename std::ostream &
operator<<(std::ostream &os, const AsJSON<std::list<Item> > &f)
{
    return os << JSON::print(array(f.value->begin(), f.value->end()));
}

template <typename Item>
static inline typename std::ostream &
operator<<(std::ostream &os, const AsJSON<std::vector<Item> > &f)
{
    return os << JSON::print(array(f.value->begin(), f.value->end()));
}

static inline std::ostream &
operator << (std::ostream &os, const AsJSON<Number> &number) {
    static unsigned long pow10[] = {
        1UL,
        10UL,
        100UL,
        1000UL,
        10000UL,
        100000UL,
        1000000UL,
        10000000UL,
        100000000UL,
        1000000000UL,
        10000000000UL,
        100000000000UL,
        1000000000000UL,
        10000000000000UL,
        100000000000000UL,
        1000000000000000UL,
        10000000000000000UL,
        100000000000000000UL,
        1000000000000000000UL
    };
    os << number.value->mantissa;
    if (number.value->exponent) {
        os << "e" << number.value->exponent;
    }
    return os;
}

std::ostream &
operator << (std::ostream &os, const AsJSON<Null> &null) {
    os << "null";
}


template <class Value, class Key> std::ostream &
operator<<(std::ostream &os, const AsJSON<std::map<Key, Value> > &map)
{ return os << JSON::print(MapRange<typename std::map<Key, Value>::const_iterator>(map.value->begin(), map.value->end())); }

} // End of Namespace JSON


#endif
