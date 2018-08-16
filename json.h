// A pretty distilled JSON parser.
#ifndef PME_JSON_H
#define PME_JSON_H

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <istream>
#include <map>
#include <sstream>
#include <typeinfo>
#include <type_traits>

namespace JSON {

class InvalidJSON : public std::exception {
    std::string err;
public:
    const char *what() const throw() { return err.c_str(); }
    InvalidJSON(const std::string &err_) throw() : err(err_) {}
    ~InvalidJSON() throw() {};
};

enum Type { Array, Boolean, Null, Number, Dict, String, Eof, JSONTypeCount };

static inline int
skipSpace(std::istream &l)
{
    while (!l.eof() && isspace(l.peek()))
        l.ignore();
    return l.eof() ? -1 : l.peek();
}

static inline char
expectAfterSpace(std::istream &l, char expected)
{
    char c = skipSpace(l);
    if (c != expected)
        throw InvalidJSON(std::string("expected '") + expected + "', got '" + c + "'");
    l.ignore();
    return c;
}

static inline void
skipText(std::istream &l, const char *text)
{
    for (size_t i = 0; text[i]; ++i) {
        char c;
        l.get(c);
        if (c != text[i])
            throw InvalidJSON(std::string("expected '") + text +  "'");
    }
}

static inline Type
peekType(std::istream &l)
{
    char c = skipSpace(l);
    switch (c) {
        case '{': return Dict;
        case '[': return Array;
        case '"': return String;
        case '-': return Number;
        case 't' : case 'f': return Boolean;
        case 'n' : return Null;
        case -1: return Eof;
        default: {
            if (c >= '0' && c <= '9')
                return Number;
            throw InvalidJSON(std::string("unexpected token '") + char(c) + "' at start of JSON object");
        }
    }
}

template <typename Context> void parseObject(std::istream &l, Context &&ctx);
template <typename Context> void parseArray(std::istream &l, Context &&ctx);

template <typename I> I
parseInt(std::istream &l)
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

template <typename FloatType> static inline FloatType
parseFloat(std::istream &l)
{
    FloatType rv = parseInt<FloatType>(l);
    if (l.peek() == '.') {
        l.ignore();
        FloatType scale = rv < 0 ? -1 : 1;
        char c;
        while (isdigit(c = l.peek())) {
            l.ignore();
            scale /= 10;
            rv = rv + scale * (c - '0');
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
        auto exponent = sign * parseInt<int>(l);
        rv *= std::pow(10.0, exponent);
    }
    return rv;
}
template <typename Integer> inline Integer parseNumber(std::istream &i) { return parseInt<long double>(i); }
template <> inline double parseNumber<double> (std::istream &i) { return parseFloat<double>(i); }
template <> inline float parseNumber<float> (std::istream &i) { return parseFloat<float>(i); }
template <> inline long double parseNumber<long double> (std::istream &i) { return parseFloat<long double>(i); }

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
};

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

static std::string
parseString(std::istream &l)
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

static inline bool
parseBoolean(std::istream &l)
{
    char c = skipSpace(l);
    switch (c) {
        case 't': skipText(l, "true"); return true;
        case 'f': skipText(l, "false"); return false;
        default: throw InvalidJSON("expected 'true' or 'false'");
    }
}

static inline void
parseNull(std::istream &l)
{
    skipSpace(l);
    skipText(l, "null");
}

static inline void // Parse any value but discard the result.
parseValue(std::istream &l)
{
    switch (peekType(l)) {
        case Array: parseArray(l, parseValue); break;
        case Boolean: parseBoolean(l); break;
        case Null: parseNull(l); break;
        case Number: parseNumber<float>(l); break;
        case Dict: parseObject(l, [](std::istream &l, std::string) -> void { parseValue(l); }); break;
        case String: parseString(l); break;
        default: throw InvalidJSON("unknown type for JSON construct");
    }
}

template <typename Context> void
parseObject(std::istream &l, Context &&ctx)
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

template <typename Context> void
parseArray(std::istream &l, Context &&ctx)
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

struct Escape {
    std::string value;
    Escape(std::string value_) : value(value_) { }
};

inline std::ostream & operator << (std::ostream &o, const Escape &escape)
{
    auto flags(o.flags());
    for (auto i = escape.value.begin(); i != escape.value.end();) {
        int c;
        switch (c = (unsigned char)*i++) {
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (unsigned(c) < 32) {
                    o << "\\u" << std::hex << unsigned(c);
                } else if (c & 0x80) {
                    // multibyte UTF-8: build up the unicode codepoint.
                    unsigned long v = c;
                    int count = 0;
                    for (int mask = 0x80; mask & v; mask >>= 1) {
                        if (mask == 0)
                            throw InvalidJSON("malformed UTF-8 string");
                        count++;
                        v &= ~mask;
                    }
                    while (--count) {
                        c = (unsigned char)*i++;
                        if ((c & 0xc0) != 0x80)
                            throw InvalidJSON("illegal character in multibyte sequence");
                        v = (v << 6) | (c & 0x3f);
                    }
                    o << "\\u" << std::hex << std::setfill('0') << std::setw(4) << v;
                } else {
                    o << (char)c;
                }
                break;
        }
    }
    o.flags(flags);
    return o;
}

template <typename Parsee> void parse(std::istream &is, Parsee &);
template <> void parse<int>(std::istream &is, int &parsee) { parsee = parseInt<int>(is); }
template <> void parse<long>(std::istream &is, long &parsee) { parsee = parseInt<long>(is); }
template <> void parse<float>(std::istream &is, float &parsee) { parsee = parseFloat<float>(is); }
template <> void parse<double>(std::istream &is, double &parsee) { parsee = parseFloat<double>(is); }
template <> void parse<std::string>(std::istream &is, std::string &parsee) { parsee = parseString(is); }
template <> void parse<bool>(std::istream &is, bool &parsee) { parsee = parseBoolean(is); }


}


static inline std::ostream &
operator<<(std::ostream &os, const JSON::Type &t)
{
    switch (t) {
        case JSON::Array: os << "Array"; break;
        case JSON::Number: os << "Number"; break;
        case JSON::Dict: os << "Object"; break;
        case JSON::String: os << "String"; break;
        default: throw JSON::InvalidJSON("not a JSON type");
    }
    return os;
}

namespace JSON {
/*
 * General purpose way of printing out JSON objects.
 * Given an std::ostream &s, we can do:
 * s << make_value(anytype)
 * And have it print out.
 * For your own structures, you define:
 * std::ostream &operator << (std::ostream &, const JSON<mytype, context> &)
 * This function can use "Object" below to describe its fields, eg:
 *    std::ostream &operator << (std::ostream &os, const JSON<MyType> &json) {
 *       MyType &myObject = json.object;
 *       Object o(os);
 *       o.field("foo", myObject.foo);
 *       return os;
 *    }
 * Calls to field return the Object, so you can chain-call them, and it also
 * converts to an ostream, so you can do:
 *     return Object(o).field("foo", myObject.foo).field("bar", myObject.bar());
 *
 * There are wrappers for arrays, and C++ containers to do the right thing.
 */

/*
 * A wrapper for objects so we can serialize them as JSON.
 * You can hold some context information in the printer to make life easier.
 */
template <typename T, typename C = char> class Value {
public:
   const T& object;
   const C context;
   Value(const T &object_, const C context_ = C()) : object(object_), context(context_) {}
   const T *operator -> () const { return &object; }
   Value() = delete;
};

/*
 * Easy way to create a JSON object, with a given context
 */
template <typename T, typename C = char>
Value<T, C>
make_value(const T &object, const C context = C()) {
   return Value<T, C>(object, context);
}

/*
 * A field in a JSON object - arbitrary key and value.
 */
template <typename K, typename V>
struct Field {
   const K &k;
   const V &v;
   Field(const K &k_, const V &v_) : k(k_), v(v_) {}
   Field() = delete;
   Field(const Field<K, V> &) = delete;
};

/*
 * A printer for JSON integral types - just serialize directly from C type.
 */
template <typename T, typename C>
typename std::enable_if<std::is_integral<T>::value, std::ostream>::type &
operator << (std::ostream &os, const Value<T, C>&json) { return os << json.object; }

/*
 * A printer for JSON boolean types: print "true" or "false"
 */
template <typename C>
std::ostream &
operator << (std::ostream &os, const Value<bool, C> &json)
   { return os << (json.object ? "true" : "false"); }

/*
 * printers for arrays. char[N] is special, we treat that as a string.
 */
template <typename C, size_t N>
std::ostream &
operator << (std::ostream &os, const Value<char[N], C> &json)
    { return os << Value<const char *, C>(&json.object[0], json.context); }

template <typename T, size_t N, typename C>
std::ostream &
operator << (std::ostream &os, const Value<T[N], C> &json)
{
    os << "[";
    for (size_t i = 0; i < N; ++i) {
        os << (i ? ",\n" : "") << json.object[i];
    }
    return os << "]";
}

/*
 * Print a field of an object
 */
template <typename K, typename V, typename C>
std::ostream &
operator << (std::ostream &os, const Value<Field<K,V>, C> &o)
{
   return os << make_value(o.object.k) << ":" << make_value(o.object.v, o.context);
}

/*
 * is_associative_container: returns true_type for containers with "mapped_type"
 */
constexpr std::false_type is_associative_container(...) {
    return std::false_type{};
}

template <typename C, typename = typename C::mapped_type>
constexpr std::true_type is_associative_container(const C *) {
    return std::true_type{};
}

/*
 * Print a non-associative container
 */
template <typename Container, typename Context>
void print_container(std::ostream &os, const Container &container, Context ctx, std::false_type)
{
   os << "[ ";
   const char *sep = "";
   for (const auto &field : container) {
      os << sep << make_value(field, ctx);
      sep = ",\n";
   }
   os << " ]";
}

/*
 * Print an associative container
 */
template <typename Container,
         typename Context,
         typename = std::true_type,
         typename K = typename Container::key_type,
         typename V = typename Container::mapped_type
         >
void
print_container(std::ostream &os, const Container &container, Context ctx, std::true_type)
{
   os << "{";
   const char *sep = "";
   for (const auto &field : container) {
      Field<K,V> jfield(field.first, field.second);
      os << sep << make_value(jfield, ctx);
      sep = ", ";
   }
   os << "}";
}

/*
 * Print any type of container
 */
template <class Container, typename Context, typename = typename Container::value_type>
std::ostream &
operator << (std::ostream &os, const Value<Container, Context> &container) {
    print_container(os, container.object, container.context, is_associative_container(&container.object));
    return os;
}

/*
 * Print a JSON string (std::string, char *, etc)
 */
template <typename C>
std::ostream &
operator << (std::ostream &os, const Value<std::string, C> &json) {
   return os << "\"" << json.object << "\"";
}

template <typename C>
std::ostream &
operator << (std::ostream &os, const Value<const char *, C> &json) {
   return os << "\"" << json.object << "\"";
}

/*
 * A mapping type that converts the entries in a container to a different type
 * as you iterate over the original container.
 */
template <class NK, class V, class Container> class Mapper {
    const Container &container;
public:
    typedef typename Container::mapped_type mapped_type;
    typedef typename std::pair<NK, const mapped_type &> value_type;
    typedef NK key_type;
    struct iterator {
        typename Container::const_iterator realIterator;
        value_type operator *() {
            const auto &result = *realIterator;
            return std::make_pair(NK(result.first), std::cref(result.second));
        }
        bool operator == (const iterator &lhs) {
            return realIterator == lhs.realIterator;
        }
        bool operator != (const iterator &lhs) {
            return realIterator != lhs.realIterator;
        }
        void operator ++() {
            ++realIterator;
        }
        iterator(typename Container::const_iterator it_) : realIterator(it_) {}
    };
    typedef iterator const_iterator;

    iterator begin() const { return iterator(container.begin()); }
    iterator end() const { return iterator(container.end()); }
    Mapper(const Container &container_): container(container_) {}
};

/* Helper for rendering compound types. */
class Object {
   std::ostream &os;
   const char *sep;
   public:
      Object(std::ostream &os_) : os(os_), sep("") {
         os << "{ ";
      }
      ~Object() {
         os << " }";
      }
      template <typename K, typename V, typename C = char> Object &field(const K &k, const V&v, const C &c = C()) {
         Field<K,V> field(k, v);
         os << sep << make_value(field, c);
         sep = ", ";
         return *this;
      }
      operator std::ostream &() { return os; }
};

/*
 * Fallback printer for pairs.
 */
template <typename F, typename S, typename C>
std::ostream &
operator << (std::ostream &os, const Value<std::pair<F, S>, C> &json) {
   return Object(os)
       .field("first", json.object.first)
       .field("second", json.object.second);
}
}
#endif
