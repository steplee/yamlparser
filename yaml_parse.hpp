#pragma once
#include <algorithm>
#include <cassert>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#define simpleAssert(cond) assert((cond));

// #define syamlPrintf(...) printf(__VA_ARGS__);
#define syamlPrintf(...) {};

#ifdef SYAML_IMPL
#include <iostream>
#endif
#include <sstream>
#include <type_traits>

//
// A simple YAML-like recursive descent parser.
// Did not read the spec at all while writing this, so don't expect it to recognize valid YAML
//
//      o You access children nodes with the `get` function, which has two overloads:
//          - Only use the `get(int)` key on list nodes.
//          - Only use the `get(const char*)` key on dict nodes.
//          - Never use `get` on a scalar node.
//      o You get unwrapped data out by using the `as` template function. It has three usages:
//          - For scalar nodes, only use `as<T>()` with T a fundamental type or a string.
//          - For dict nodes, only use `as<T>()` with T an unordered map.
//          - For list nodes, only use `as<T>()` with T a vector.
//
// NOTE: `tryScalar()` accepts 'ident' tokens, which is useful for 'true', but in general,
// prefer using strings with quotes.
//
// NOTE: Lots of inefficiencies such as:
//				copying string keys rather than using `string_view`s into the document
// string. 				extraneous copying of nodes (the UPtrs result in lots of short
// lived objects).
//
// FIXME: This implementation fails the tests from serialized pyyaml outputs because it does not
// support:
//          1) Parsing multiple layers of lists on one line, for example: ' - - 1'
//          2) Lexing/parsing maps using the '{ ... }' syntax.
//
// NOTE: Adding a set() method made the code a little incoherent because originally I thought to tie values
//       to ranges of the document's source string.
//       With set() however, there is no longer any fixed relationship to the document.
//       So I handled that by having every node have a string. It is empty if the node corresponds to one
//       from the document. Otherwise it is the textual representation of the value passed to set()
//

namespace syaml {

    namespace {
		// 1 = no-match, 0 = match.
		// If one string ends when another continues, return no-match.
        inline int my_strcmp(const char* a, const char* b) {
            while (1) {
                if (*a == '\0' and *b == '\0') return 0;
                if (*a == '\0' or *b == '\0') return 1;
                if (*a != *b) return 1;
                a++;
                b++;
            }
			// simpleAssert(false);
			// NOTE: Return *no-match* if we've reached end.
            return 1;
        }

		// Compare the beginning `len` prefix only. If `len` is -1, use the above overload.
		// If one or both strings end before `len`, act like above overload.
        inline int my_strcmp(const char* a, const char* b, int len) {
			if (len == -1) return my_strcmp(a,b);

			int i=0;
            while (i<len) {
                if (*a == '\0' and *b == '\0') return 0;
                if (*a == '\0' or *b == '\0') return 1;
                if (*a != *b) return 1;
                a++;
                b++;
				i++;
            }
			// NOTE: Return match if we've reached the end.
            return 0;
        }
    }


#ifdef SYAML_IMPL

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

    namespace {
        void format_(std::ostream& os) {
        }
        template <class T, class... Ts> void format_(std::ostream& os, T t, Ts... ts) {
            os << t;
            format_(os, ts...);
        }
    }
#define syamlAssert(cond, ...)                                                                             \
    if (!(cond)) {                                                                                         \
        std::stringstream ss;                                                                              \
        format_(ss, ##__VA_ARGS__);                                                                        \
        throw std::runtime_error(ss.str());                                                                \
    }
#define syamlWarn(cond, ...)                                                                               \
    if (!(cond)) {                                                                                         \
        std::stringstream ss;                                                                              \
        format_(ss, ##__VA_ARGS__);                                                                        \
        std::cout << (ss.str()) << "\n";                                                                   \
    }
#endif

    template <class T> struct Decode {
        static constexpr bool use_dict   = false;
        static constexpr bool use_list   = false;
        static constexpr bool use_scalar = false;
        // static constexpr bool value = use_dict | use_list | use_scalar;
        static constexpr bool value = false;
    };

    struct FromKey {
        static constexpr bool value      = true;
        static constexpr bool use_dict   = true;
        static constexpr bool use_list   = false;
        static constexpr bool use_scalar = false;
    };
    struct FromList {
        static constexpr bool value      = true;
        static constexpr bool use_dict   = false;
        static constexpr bool use_list   = true;
        static constexpr bool use_scalar = false;
    };

    template <class V> using Opt = std::optional<V>;

    template <class V> using Map = std::unordered_map<std::string, V>;

    // https://stackoverflow.com/questions/12042824/how-to-write-a-type-trait-is-container-or-is-vector
    template <typename T, typename _ = void> struct is_vector {
        static constexpr bool value = false;
    };
    template <typename T>
    struct is_vector<
        T, typename std::enable_if<std::is_same<
               T, std::vector<typename T::value_type, typename T::allocator_type>>::value>::type> {
        static const bool value = true;
    };

    template <typename T, typename _ = void> struct is_map {
        static constexpr bool value = false;
    };
    template <typename T>
    struct is_map<
        T, typename std::enable_if<std::is_same<
               T, std::unordered_map<typename T::value_type, typename T::mapped_type>>::value>::type> {
        static const bool value = true;
    };

    template <typename T, typename _ = void> struct is_decodable {
        static constexpr bool value = false;
    };
    template <typename T>
    struct is_decodable<T, typename std::enable_if<Decode<T>::value
                                                   // WARNING: This is broken.
                                                   // FIXME: Why?
                                                   // Decode<T>::use_dict | Decode<T>::use_dict |
                                                   // Decode<T>::use_scalar
                                                   >::type> {
        static const bool value = true;
    };

    // Non vector/map/decodable
    template <class T>
    using is_scalar = std::negation<std::disjunction<is_vector<T>, is_map<T>, is_decodable<T>>>;
    // using is_scalar    = std::disjunction<std::is_same<T,std::string>, std::is_fundamental<T>>;

    // ---------------------------------------------------------------------------------------------------
    //
    //   Lexing & Document
    //
    // ---------------------------------------------------------------------------------------------------

    struct SourceRange {
        uint32_t start;
        uint32_t end;
    };

    struct TokenizedDoc;
    struct Document {
        std::string src;

        inline Document(const std::string& s)
            : src(s) {
        }

		const std::string getRangeString(SourceRange rng, bool trimQuotes = false) const;
		const std::stringstream getRangeStream(const SourceRange& rng) const;
        std::string findLineAround(int i, int o) const;
        std::vector<std::pair<uint32_t, std::string>> linesAround(int i, int N = 3) const;




        uint32_t distanceFromStartOfLine(int i) const ;
    };

    struct Tok {
        enum Lexeme {
            eWhitespace,
            eNL,
            eColon,
            eComma,
            eDash,
            eIdent,
            eNumber,
            eString,
            eOpenBrace,
            eCloseBrace,
            eEOF
        } lexeme;
        uint32_t n = 0;
        uint32_t start;
        uint32_t end;

        inline bool operator==(const Lexeme& l) const {
            return lexeme == l;
        }
        inline bool operator!=(const Lexeme& l) const {
            return lexeme != l;
        }

        inline void print(std::ostream& os, const Document& doc) {
            if (lexeme == eNL) {
                os << "\n";
                return;
            }
            os << "(";
            switch (lexeme) {
            case eWhitespace: os << "ws, n=" << n; break;
            case eNL: os << "nl, n=" << n; break;
            case eColon: os << "colon"; break;
            case eComma: os << "comma"; break;
            case eDash: os << "dash"; break;
            case eIdent: os << "ident"; break;
            case eNumber: os << "num"; break;
            case eString: os << "str"; break;
            case eOpenBrace: os << "openBrace"; break;
            case eCloseBrace: os << "closeBrace"; break;
            case eEOF: os << "eof"; break;
            }
            if (lexeme != eNL) os << ", '" << std::string_view { doc.src }.substr(start, end - start);
            os << "')";
        }
    };

    struct Parser;

    using ConstTok = const Tok;
    struct TokenizedDoc {
        Document* doc;
        std::vector<Tok> tokens;
        inline ConstTok& operator[](uint32_t i) const {
            return tokens[i];
        }
        inline uint32_t size() const {
            return tokens.size();
        }

        inline const std::string getTokenString(ConstTok& t) const {
            return doc->getRangeString({ t.start, t.end });
        }
        inline const std::stringstream getTokenStream(ConstTok& t) const {
            return doc->getRangeStream({ t.start, t.end });
        }
        inline const std::string getTokenRangeString(const SourceRange& ts, bool trimQuotes = false) const {
            const auto& l = tokens[ts.start];
            // const auto& r = tokens[ts.end    ];
            const auto& r = tokens[ts.end - 1];
            return doc->getRangeString({ l.start, r.end }, trimQuotes);
        }
        inline std::stringstream getTokenRangeStream(const SourceRange& ts) const {
            const auto& l = tokens[ts.start];
            // const auto& r = tokens[ts.end  ];
            const auto& r = tokens[ts.end - 1];
            return doc->getRangeStream({ l.start, r.end });
        }
    };

    static inline bool is_alpha(char c) {
        return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z');
    }
    static inline bool is_numer(char c) {
        return c >= '0' and c <= '9';
    }

    inline TokenizedDoc lex(Document* doc) {
        TokenizedDoc out;
        out.doc       = doc;

        auto& ts      = out.tokens;
        const auto& s = doc->src;
        uint32_t N    = (uint32_t)s.length();
        simpleAssert(N > 0);
        uint32_t i = 0;
        while (i < N) {
            uint32_t i0 = i;
            uint32_t n  = 0;

            // Comment
            if (s[i] == '#') {
                while (i < N and s[i] != '\n') i++;
                continue;
            }

            // Whitespace
            else if (s[i] == ' ' or s[i] == '\t') {
                while (i < N and (s[i] == ' ' or s[i] == '\t')) {
                    n++;
                    i++;
                }
                ts.push_back(Tok { Tok::eWhitespace, n, i0, i });
            }

            // String
            else if (s[i] == '\"') {
                i++;
                while (i < N and s[i] != '"') {
                    n++;
                    i++;
                }
                simpleAssert(s[i] == '"');
                // ts.push_back(Tok{Tok::eString,n,i0+1,i++});
                ts.push_back(Tok { Tok::eString, n, i0, ++i });
            }

            // Ident
            else if (is_alpha(s[i])) {
                while (i < N and (is_alpha(s[i]) or is_numer(s[i]))) { i++; }
                ts.push_back(Tok { Tok::eIdent, n, i0, i });
            }

            // Single dash
            else if (s[i] == '-'
                     and (i >= s.length() or s[i + 1] == ' ' or s[i + 1] == '\n' or s[i + 1] == '\t')) {
                ts.push_back(Tok { Tok::eDash, n, i0, ++i });
            }

            // Number
            else if (s[i] == '.' or s[i] == '-' or is_numer(s[i])) {
                if (s[i] == '-') i++;
                int n_e = 0;
                int n_d = 0;
                while (i < N) {
                    if (s[i] == 'e') {
                        if (n_e) simpleAssert(false && "multiple 'e' in float.");
                        n_e++;
                        i++;
                        // allow like '1e-2'
                        if (i < N and s[i] == '-') { i++; }
                    } else if (s[i] == '.') {
                        if (n_d) simpleAssert(false && "multiple '.' in float.");
                        n_d++;
                        i++;
                    } else if (is_numer(s[i])) {
                        i++;
                    } else if (s[i] == '#' or s[i] == ',' or s[i] == ']' or s[i] == ' ' or s[i] == '\n'
                               or s[i] == '\t') {
                        break;
                    } else {
                        throw std::runtime_error("error while lexing a number, unexpected char: "
                                                 + std::string { s[i] });
                    }
                }
                ts.push_back(Tok { Tok::eNumber, n, i0, i });
            }

            // etc.
            else if (s[i] == '\n')
                ts.push_back(Tok { Tok::eNL, n, i0, ++i });
            else if (s[i] == ',')
                ts.push_back(Tok { Tok::eComma, n, i0, ++i });
            else if (s[i] == ':')
                ts.push_back(Tok { Tok::eColon, n, i0, ++i });
            else if (s[i] == '[')
                ts.push_back(Tok { Tok::eOpenBrace, n, i0, ++i });
            else if (s[i] == ']')
                ts.push_back(Tok { Tok::eCloseBrace, n, i0, ++i });
            else { simpleAssert(false); }
        }

        ts.push_back(Tok { Tok::eEOF, 0, i, i });

        return out;
    }

    // ---------------------------------------------------------------------------------------------------
    //
    //   AST
    //
    // ---------------------------------------------------------------------------------------------------

    struct ListNode;
    struct DictNode;
    struct ScalarNode;
    struct EmptyNode;
    struct RootNode;
    struct Parser;
    namespace {
        struct Serialization;
    }

    struct Node {

    public:
        // From parsed document
        inline Node(TokenizedDoc* tdoc, SourceRange tokRange)
            : parent(nullptr)
            , tdoc(tdoc)
            , tokRange(tokRange) {
        }

        // From dynamic set() call
        inline Node(const std::string& valueStr)
            : parent(nullptr)
            , tdoc(nullptr)
            , tokRange({})
            , valueStr(valueStr) {
        }

        inline virtual ~Node() {};

        RootNode* getRoot(bool required) const;

        // template <class T> Node* get(const T& k) const;
        Node* get(const char* k, int len) const;
        Node* get(const char* k) const;
        Node* get(uint32_t i) const;
        template <class T> void set(const char* k, const T& v);

        template <class T> T as(Opt<T> def = {}) const;

        friend struct Parser;
        friend struct Serialization;
        friend struct RootNode;   // why is this neeeded.
        friend struct ListNode;   // why is this neeeded.
        friend struct ScalarNode; // why is this neeeded.
        friend struct DictNode;   // why is this neeeded.

        bool isEmpty() const;

        // protected:
    public:
        // template <class T, class TV=typename T::value_type>
        // std::enable_if_t<is_vector<TV>::value, T> as_(Opt<std::vector<T>> def) const;
        template <class T> std::enable_if_t<is_vector<T>::value, T> as_(Opt<T> def) const;
        // template <class T, class TV> std::enable_if_t<is_map   <TV>::value, T>    as_(Opt<Map<T>>
        // def) const;
        template <class T> std::enable_if_t<is_map<T>::value, T> as_(Opt<T> def) const;
        template <class T> std::enable_if_t<is_scalar<T>::value, T> as_(Opt<T> def) const;
        template <class T> std::enable_if_t<is_decodable<T>::value, T> as_(Opt<T> def) const;

        inline virtual Node* get_(const char* k, int len=-1) const {
            simpleAssert(false);
            return nullptr;
		}
        inline virtual Node* get_(uint32_t i) const {
            simpleAssert(false);
            return nullptr;
        }
        // void set_(const char* k, DictNode* v); // NOTE:Takes ownership
        template <class T> void set_(const char* k, const T& v);

        Node* parent       = {};
        TokenizedDoc* tdoc = {};
        SourceRange tokRange;
        std::string valueStr; // if not null: this node is from a set() call
        bool valueStrIsString = false;

        DictNode* asDict();
        ListNode* asList();
        ScalarNode* asScalar();
        EmptyNode* asEmpty();
    };

    struct EmptyNode : public Node {
        using Node::Node;
        inline virtual ~EmptyNode() {
        }

        virtual Node* get_(const char* k, int len=-1) const override;
        virtual Node* get_(uint32_t k) const override;
    };
    struct ListNode : public Node {

        // private:
        std::vector<Node*> children;
        bool fromDash = false;

    public:
        using Node::Node;
        virtual ~ListNode();

        virtual Node* get_(const char* k, int len=-1) const override;
        virtual Node* get_(uint32_t k) const override;

        template <class V> inline std::vector<V> toVector() const {
            std::vector<V> out;
            for (auto& child : children) { out.push_back(child->as_<V>({})); }
            return out;
        }
        inline bool isFromDash() const {
            return fromDash;
        }
    };

    struct DictNode : public Node {

        // private:
        // std::unordered_map<std::string, Node*> children;
        std::vector<std::pair<std::string, Node*>> children;

    public:
        using Node::Node;

        inline DictNode()
            : Node("") {
        }

        virtual ~DictNode();

        virtual Node* get_(const char* k, int len=-1) const override;
        virtual Node* get_(uint32_t k) const override;

        template <class V> inline Map<V> toMap() const {
            Map<V> out;
            for (auto& kv : children) { out[kv.first] = kv.second->as_<V>({}); }
            return out;
        }
    };

    struct RootNode : public DictNode {
        std::mutex mtx;

        // Only allow a move constructor.
        // We don't want to do a deep copy, so we want to take ownership of o's children
        RootNode(DictNode&& o);

        virtual ~RootNode();

        inline EmptyNode* getEmptySentinel() {
            return sentinel;
        }

        inline std::unique_lock<std::mutex> guard() {
            return std::unique_lock<std::mutex>(mtx);
        }

        EmptyNode* sentinel;
    };

    struct ScalarNode : public Node {

    private:
    public:
        using Node::Node;
        virtual ~ScalarNode();

        virtual Node* get_(const char* k, int len=-1) const override;
        virtual Node* get_(uint32_t k) const override;

        template <class V> inline V toScalar() const {

            if constexpr (std::is_same<V, std::string>::value) {
                // return tdoc->getRangeString(range);
                if (valueStr.length()) {
                    if (valueStrIsString)
                        return valueStr.substr(1, valueStr.length() - 2);
                    else
                        return valueStr;
                } else
                    return tdoc->getTokenRangeString(tokRange, true);
            }

            if constexpr (std::is_same<V, bool>::value) {
                std::string s = toScalar<std::string>();
                if (s.length() > 0 and (s[0] == 't' or s[0] == '1' or s[0] == 'T'))
                    return true;
                else if (s.length() > 0 and (s[0] == 'f' or s[0] == '0' or s[0] == 'F'))
                    return false;
                else
                    throw std::runtime_error(std::string { "toScalar<bool>() failed with bad value: " }
                                             + s);
            }

            if constexpr (std::is_fundamental<V>::value) {
                V o;
                std::stringstream ss;
                if (valueStr.length()) {
                    ss = std::stringstream { valueStr };
                } else {
                    ss = tdoc->getTokenRangeStream(tokRange);
                }
                ss >> o;
                assert(ss.eof() && "failed or partial parse");
                // std::cout << " - parse this str :: " << ss.str() << " => " << o << "\n";
                return o;
            }

            throw std::runtime_error("toScalar<V>() called with invalid type V for ScalarNode");
        }
    };

    // ---------------------------------------------------------------------------------------------------
    //
    //   Parsing
    //
    // ---------------------------------------------------------------------------------------------------

    struct Parser {
    public:
        TokenizedDoc* tdoc;

        RootNode* parse(TokenizedDoc* doc);

        ~Parser();

        Tok lex();
        void parseDict(DictNode* parent);

        Node* tryDict();
        Node* tryList();
        Node* tryListFromDash();
        Node* tryScalar();

        uint32_t I = 0;

        // inline bool eof() { return I >= tdoc->size(); }
        ConstTok& peek();
        ConstTok& advance();
        bool eof();
        void skipUntilNonEmptyLine();
    };

    // ---------------------------------------------------------------------------------------------------
    //
    //   Conversions
    //
    // ---------------------------------------------------------------------------------------------------

	/*
    template <class T> inline Node* Node::get(const T& k) const {
        auto root = getRoot(false);
        auto g    = root ? root->guard() : decltype(root->guard()) {};
        return this->get_(k);
    }
	*/
	inline Node* Node::get(const char* k, int len) const {
        auto root = getRoot(false);
        auto g    = root ? root->guard() : decltype(root->guard()) {};
        return this->get_(k, len);
	}
	inline Node* Node::get(const char* k) const {
        return this->get(k, -1);
	}
	inline Node* Node::get(uint32_t i) const {
        auto root = getRoot(false);
        auto g    = root ? root->guard() : decltype(root->guard()) {};
        return this->get_(i);
	}

    template <class T> inline void Node::set(const char* k, const T& v) {
        auto root = getRoot(false);
        auto g    = root ? root->guard() : decltype(root->guard()) {};
        return this->set_(k, v);
    }

    // template <typename std::enable_if_t<is_vector<T>::value, T> >
    template <class T>
    // inline std::enable_if_t<is_vector<TV>::value, T> Node::as_(Opt<std::vector<T>> def) const {
    inline std::enable_if_t<is_vector<T>::value, T> Node::as_(Opt<T> def) const {
        using TV               = typename T::value_type;
        const ListNode* asList = dynamic_cast<const ListNode*>(this);

        simpleAssert(asList != nullptr && "Node.as<vector> called on non-ListNode");
        return asList->toVector<TV>();
    }

    template <class T>
    // inline std::enable_if_t<is_map<TV>::value, T> Node::as_(Opt<Map<T>> def) const {
    inline std::enable_if_t<is_map<T>::value, T> Node::as_(Opt<T> def) const {
        using TV               = typename T::value_type;
        const DictNode* asDict = dynamic_cast<const DictNode*>(this);
        simpleAssert(asDict != nullptr && "Node.as<map> called on non-ListNode");

        return asDict->toMap<TV>();
    }

    // template <typename std::enable_if_t<std::is_integral<T>::value, T> >
    template <class T> inline std::enable_if_t<is_scalar<T>::value, T> Node::as_(Opt<T> def) const {
        const ScalarNode* asScalar = dynamic_cast<const ScalarNode*>(this);
        simpleAssert(asScalar != nullptr
                     && "Node.as<T> called on non-ScalarNode (with T not in {vector,map})");

        return asScalar->toScalar<T>();
    }

    template <class T> inline std::enable_if_t<is_decodable<T>::value, T> Node::as_(Opt<T> def) const {

        // TODO: Would be nicer to allow any combination, based on dynamic_cast(this) and on
        // `use_dict` etc.

        if constexpr (Decode<T>::use_dict) {
            auto asDict = dynamic_cast<const DictNode*>(this);
            simpleAssert(asDict != nullptr && "Node.as<map> called on non-ListNode");
            return Decode<T>::decode(asDict);
        }

        if constexpr (Decode<T>::use_list) {
            auto asList = dynamic_cast<const ListNode*>(this);
            simpleAssert(asList != nullptr && "Node.as<map> called on non-ListNode");
            return Decode<T>::decode(asList);
        }
    }

    template <class T> T Node::as(Opt<T> def) const {
        auto root = getRoot(false);
        auto g    = root ? root->guard() : decltype(root->guard()) {};
        if (dynamic_cast<const EmptyNode*>(this)) {
            if (def)
                return *def;
            else
                throw std::runtime_error("as<>() called on an EmptyNode with no default provided.");
        }
        return as_<T>(def);
    }

    template <class T> void Node::set_(const char* k, const T& v) {
        auto self = dynamic_cast<DictNode*>(this);
        if (not self) { throw std::runtime_error("set_ is only supported on DictNodes for now!"); }

        std::string kk { k };
        auto oldIt = std::find_if(self->children.begin(), self->children.end(),
                                  [k](const auto& kv) { return 0 == my_strcmp(kv.first.c_str(), k); });
        if (oldIt != self->children.end()) delete oldIt->second;
        if (oldIt != self->children.end()) self->children.erase(oldIt);

        if constexpr (std::is_same<T, DictNode*>::value) {
            dynamic_cast<DictNode*>(v)->parent = this;
            self->children.push_back({ kk, v });
            return;
        }

        std::string valueStr;
        bool valueStrIsString = false;
        if constexpr (std::is_same<T, std::string>::value) {
            valueStr         = "\"" + v + "\"";
            valueStrIsString = true;
        } else {
            std::stringstream ss;
            ss << v;
            valueStr = ss.str();
        }
        auto newNode              = new ScalarNode(valueStr);
        newNode->parent           = this;
        newNode->valueStrIsString = valueStrIsString;

        self->children.push_back({ kk, newNode });
    }

#ifdef SYAML_IMPL

    /*
void Node::set_(const char* k, DictNode* v) {
            auto self = dynamic_cast<DictNode*>(this);
            if (not self) {
                    throw std::runtime_error("set_ is only supported on DictNodes for now!");
            }

            std::string kk{k};
            auto oldIt = std::find_if(self->children.begin(), self->children.end(),
                                                    [k](const auto& kv) { return 0 ==
my_strcmp(kv.first.c_str(), k); }); if (oldIt != self->children.end()) delete oldIt->second; if (oldIt !=
self->children.end()) self->children.erase(oldIt);

            dynamic_cast<DictNode*>(v)->parent = this;
            self->children.push_back({kk,v});
    }
    */

    uint32_t Document::distanceFromStartOfLine(int i) const {
        uint32_t d = 0;
        while (i > 0 and src[i] != '\n') {
            i--;
            d++;
        }
        return d;
    }

    const std::stringstream Document::getRangeStream(const SourceRange& rng) const {
        std::string snippet = getRangeString(rng);
        return std::stringstream(std::move(snippet));
    }

    std::string Document::findLineAround(int i, int o) const {
        // if (i < 0 or i >= src.length()) return "~";
        if (i < 0 or i >= (int)src.length()) return "";
        int s = i;
        // int e = i+1;
        int e = i;
        while (s >= 0 and src[s] != '\n') s--;
        while (e < (int)src.length() and src[e] != '\n') e++;
        // if (src[e]=='\n') e--;
        // if (o == 0) return std::string_view{src}.substr(s,e);
        if (o == 0) return src.substr(s + 1, e - s - 1);
        if (o > 0) return findLineAround(e + 1, o - 1);
        if (o < 0) return findLineAround(s - 1, o + 1);
        return "";
    }
    std::vector<std::pair<uint32_t, std::string>> Document::linesAround(int i, int N) const {
        std::vector<std::pair<uint32_t, std::string>> out(N);
        for (int o = 0; o < N; o++) {
            uint32_t nl = 0;
            for (int j = 0; j < i; j++) nl += src[j] == '\n';
            out[o] = { nl + o - 1, findLineAround(i, o - 1) };
        }
        return out;
    }

    const std::string Document::getRangeString(SourceRange rng, bool trimQuotes) const {
        assert(rng.end >= rng.start);
        if (trimQuotes and src[rng.start] == '"') rng.start++;
        if (trimQuotes and src[rng.end - 1] == '"') rng.end--;
        std::string snippet = src.substr(rng.start, rng.end - rng.start);
        return snippet;
    }

    RootNode::RootNode(DictNode&& o)
        : DictNode(o.tdoc, o.tokRange) {
        children = std::move(o.children);
        for (auto kv : children) kv.second->parent = this; // dont forget this.
        parent           = o.parent;
        sentinel         = new EmptyNode(tdoc, { 0, 0 });
        sentinel->parent = this;
    }

    RootNode::~RootNode() {
        delete sentinel;
    }

    Node* ScalarNode::get_(const char* k, int len) const {
        syamlAssert(false, "ScalarNode.get(str) called.");
        return 0;
    }
    Node* ScalarNode::get_(uint32_t k) const {
        syamlAssert(false, "ScalarNode.get(int) called.");
        return 0;
    }

    Node* EmptyNode::get_(const char* k, int len) const {
        syamlAssert(false, "EmptyNode.get(str) called.");
    }
    Node* EmptyNode::get_(uint32_t k) const {
        syamlAssert(false, "EmptyNode.get(int)");
    }

    inline Node* ListNode::get_(const char* k, int len) const {
        syamlAssert(false, "ListNode.get(str) called.");
        return 0;
    }
    inline Node* ListNode::get_(uint32_t k) const {
        if (k >= 0 and k < children.size()) {
            return children[k];
        } else {
            syamlWarn(k >= 0 and k < children.size(), "ListNode.get(int) out-of-bounds (asked ", k,
                      " have ", children.size(), " children)");
            return getRoot(true)->getEmptySentinel();
        }
    }

    inline Node* DictNode::get_(const char* k, int len) const {
        decltype(children.begin()) it;
        if constexpr (is_vector<decltype(children)>::value) {
            it = std::find_if(children.begin(), children.end(),
                              [k,len](const auto& kv) { return 0 == my_strcmp(kv.first.c_str(), k, len); });
        } else {
            assert(false);
        }

        if (it == children.end()) {
            syamlWarn(it != children.end(), "DictNode.get(k) key not found ('", k, "' have ",
                      children.size(), " children)");
            return getRoot(true)->getEmptySentinel();
        }

        return it->second;
	}
    inline Node* DictNode::get_(uint32_t k) const {
        syamlAssert(false, "DictNode.get(int) called.");
        return 0;
    }

    bool Node::isEmpty() const {
        return dynamic_cast<const EmptyNode*>(this) != nullptr;
    }

    struct ParserGuard {
        uint32_t I0;
        bool terminated = false;
        Parser* parser;
        ParserGuard(Parser* parser);
        inline ~ParserGuard() {
            // if (!terminated) throw std::runtime_error("unterminated ParserGuard");
            if (!terminated) assert(false && "unterminated ParserGuard");
        }
        inline void accept() {
            terminated = true;
        }
        inline void reject() {
            terminated = true;
            parser->I  = I0;
        }
        inline SourceRange currentRange() const {
            return SourceRange { I0, parser->I };
        }
    };

    ParserGuard::ParserGuard(Parser* parser)
        : parser(parser) {
        I0 = parser->I;
    }

    ConstTok& Parser::peek() {
        return (*tdoc)[I];
    }
    ConstTok& Parser::advance() {
        return (*tdoc)[I++];
    }
    bool Parser::eof() {
        syamlAssert(I <= tdoc->size());
        return peek() == Tok::eEOF;
    }

    Parser::~Parser() {
    }

    RootNode* Parser::parse(TokenizedDoc* tdoc_) {
        tdoc            = tdoc_;

        auto rootAsDict = (DictNode*)tryDict();
        syamlAssert(rootAsDict != nullptr);

        RootNode* root = new RootNode(std::move(*rootAsDict));
        delete rootAsDict;

        return root;
    }

    namespace {
        inline void print_line_debug(TokenizedDoc* tdoc, uint32_t startPosTok) {
            // auto doc = tdoc->doc;
            uint32_t startPos = tdoc->tokens[startPosTok].start;
            auto doc          = tdoc->doc;
            auto lines        = doc->linesAround(startPos);
            int off           = doc->distanceFromStartOfLine(startPos);
            // std::string tab = "\t";
            std::string tab = "         ";
            for (uint32_t i = 0; i < lines.size(); i++) {
                char lineNo[8];
                sprintf(lineNo, "% 4d", lines[i].first);
                std::cout << tab << KBLU << lineNo << KNRM "| " << KWHT << lines[i].second << KNRM "\n";
                if (i == lines.size() / 2) {
                    std::cout << tab << "      ";
                    for (int i = 0; i < off; i++) std::cout << " ";
                    std::cout << KYEL "^" KNRM "\n";
                }
            }
        }
    }

    Node* Parser::tryScalar() {
        ParserGuard pg(this);

        try {

            Tok cur = peek();
            while (cur == Tok::eWhitespace) cur = advance();

            if (eof()) { throw std::runtime_error("tried scalar, but is eof"); }

            cur = advance();

            // if (cur == Tok::eString or cur == Tok::eNumber) {
            if (cur == Tok::eString or cur == Tok::eNumber or cur == Tok::eIdent) {
                ScalarNode* newNode = new ScalarNode(tdoc, pg.currentRange());
                return pg.accept(), newNode;
            }
        } catch (std::runtime_error& e) {
            std::cout << " - In tryScalar(), starting here:\n";
            print_line_debug(tdoc, pg.I0);
            pg.reject();
            throw e;
        }

        return pg.reject(), nullptr;
    }

    Node* Parser::tryList() {
        ParserGuard pg(this);

        using NodeUPtr = std::unique_ptr<Node>;
        std::vector<NodeUPtr> cs;

        try {

            while (peek() == Tok::eWhitespace) advance();

            Tok open = advance();
            if (open != Tok::eOpenBrace) return pg.reject(), nullptr;

            if (eof()) { throw std::runtime_error("inside list, should've parsed something, got eof"); }

            while (!eof()) {
                while (peek() == Tok::eWhitespace or peek() == Tok::eNL) {
                    while (peek() == Tok::eWhitespace) advance();
                    while (peek() == Tok::eNL) advance();
                }
                Tok cur = peek();
                if (eof()) { throw std::runtime_error("inside list, should've parsed something, got eof"); }

                if (cur == Tok::eCloseBrace) {
                    advance();
                    break;
                }

                Node* next = nullptr;
                if (!next) next = tryList();
                if (!next) next = tryScalar();

                if (!next) {
                    std::stringstream ss;
                    cur = peek();
                    cur.print(ss, *tdoc->doc);
                    syamlPrintf("inside list, should've parsed list or scalar, peek() is %s\n",
                                ss.str().c_str());
                    throw std::runtime_error("inside list, should've parsed list or scalar");
                }

                cs.push_back(NodeUPtr { next });

				while (peek() == Tok::eWhitespace) advance();

                Tok after = peek();
                if (after == Tok::eCloseBrace) {
                    advance();
                    break;
                } else if (after == Tok::eComma) {
                    advance();
                } else {
                    throw std::runtime_error("inside list, should've parsed comma or ending ']'");
                }
            }
        } catch (std::runtime_error& e) {
            std::cout << " - In tryList(), starting here:\n";
            print_line_debug(tdoc, pg.I0);
            pg.reject();
            throw e;
        }

        ListNode* newNode = new ListNode(tdoc, pg.currentRange());

        for (auto& c : cs) newNode->children.push_back(c.release());
        for (auto& c : newNode->children) c->parent = newNode;
        syamlPrintf("return list with nitems=%zu\n", cs.size());

        return pg.accept(), newNode;
    }

    Node* Parser::tryListFromDash() {
        ParserGuard pg(this);

        using NodeUPtr = std::unique_ptr<Node>;
        std::vector<NodeUPtr> cs;

        try {

            uint32_t indent = 0;
            while (peek() == Tok::eNL) {
                indent = 0;
                advance();
                if (peek() == Tok::eWhitespace) {
                    // indent = advance().n;
                    indent = peek().n;
                    syamlPrintf("see indent %d\n", indent);
                }
            }
            if (peek() == Tok::eWhitespace) {
                // indent = advance().n;
                indent = peek().n;
                syamlPrintf("see indent %d\n", indent);
            }

            syamlPrintf("start tryListFromDash at I=%d\n", I);

            while (!eof()) {

                uint32_t thisIndent = 0;
                uint32_t savedI     = I;
                if (peek() == Tok::eWhitespace) { thisIndent = advance().n; }
                while (peek() == Tok::eNL) {
                    thisIndent = 0;
                    advance();
                    if (peek() == Tok::eWhitespace) { thisIndent = advance().n; }
                }
                if (eof()) break;

                syamlPrintf(" - peek() '%s': this indent=%d, expected indent=%d\n",
                            tdoc->getTokenString(peek()).c_str(), thisIndent, indent);
                if (thisIndent < indent) {
                    syamlPrintf(" - exiting tryListFromDash because indent was %d < %d\n", thisIndent,
                                indent);

                    // NOTE: This is really tricky: if we fail on this indent, we must **rewind
                    // back to newline** if (thisIndent) I -= 1;
                    I = savedI;

                    break;
                }

                if (thisIndent > indent) {
                    syamlPrintf("inside list from dash, higher indent...\n");
                    I          = savedI;
                    Node* next = nullptr;
                    if (!next) next = tryListFromDash(); // FIXME: Is this correct?
                    if (!next) next = tryDict();

                    if (!next) {
                        throw std::runtime_error("inside dashList with indent > expected, "
                                                 "should've parsed list or dict");
                    }
                    cs.push_back(NodeUPtr { next });
                    continue;
                }

                Tok open = advance();
                if (open != Tok::eDash) {
                    std::stringstream ss;
                    open.print(ss, *tdoc->doc);
                    syamlPrintf(" - exiting tryListFromDash, expected dash, got %s\n", ss.str().c_str());
                    return pg.reject(), nullptr;
                }

                Tok cur = peek();
                while (peek() == Tok::eWhitespace) cur = advance();
                if (eof()) {
                    throw std::runtime_error("inside dashList, should've parsed something, got eof");
                }

                if (cur == Tok::eCloseBrace) {
                    advance();
                    break;
                }

                Node* next = nullptr;
                if (!next) next = tryList();
                if (!next) next = tryListFromDash();
                if (!next) next = tryScalar();
                // if (!next) next = tryDict(); // WARNING: This is not supported yet.

                if (!next) { throw std::runtime_error("inside dashList, should've parsed list or scalar"); }

                cs.push_back(NodeUPtr { next });

                // throw std::runtime_error("nothing parse in inner dict");
            }
        } catch (std::runtime_error& e) {
            std::cout << " - In tryListFromDash(), starting here:\n";
            print_line_debug(tdoc, pg.I0);
            pg.reject();
            throw e;
        }

        ListNode* newNode = new ListNode(tdoc, pg.currentRange());
        newNode->fromDash = true;

        for (auto& c : cs) newNode->children.push_back(c.release());
        for (auto& c : newNode->children) c->parent = newNode;
        syamlPrintf("return list with nitems=%zu\n", cs.size());

        return pg.accept(), newNode;
    }

    void Parser::skipUntilNonEmptyLine() {
        // Go from current position to end of line. If we see any non whitespace, stop.
        while (peek() == Tok::eWhitespace) advance();
        // Do the skipping process
        int rollback = I;
        while (peek() == Tok::eNL) {
            advance();
            rollback = I;
            while (peek() == Tok::eWhitespace) advance();
        }
        I = rollback;
    }

    Node* Parser::tryDict() {
        ParserGuard pg(this);

        uint32_t indent = 0;
        using NodeUPtr  = std::unique_ptr<Node>;
        std::vector<std::pair<std::string, NodeUPtr>> cs;

        try {

            while (peek() == Tok::eNL) {
                indent = 0;
                advance();
                if (peek() == Tok::eWhitespace) {
                    // indent = advance().n;
                    indent = peek().n;
                    syamlPrintf("see indent %d\n", indent);
                }
            }
            if (peek() == Tok::eWhitespace) {
                // indent = advance().n;
                indent = peek().n;
                syamlPrintf("see indent %d\n", indent);
            }

            syamlPrintf("start tryDict at I=%d\n", I);

            while (!eof()) {

                uint32_t thisIndent = 0;
                uint32_t savedI     = I;
                if (peek() == Tok::eWhitespace) { thisIndent = advance().n; }
                while (peek() == Tok::eNL) {
                    thisIndent = 0;
                    advance();
                    if (peek() == Tok::eWhitespace) { thisIndent = advance().n; }
                }
                syamlPrintf(" - next key '%s': this indent=%d, expected indent=%d\n",
                            tdoc->getTokenString(peek()).c_str(), thisIndent, indent);
                if (thisIndent < indent) {
                    syamlPrintf(" - exiting tryDict because indent was %d < %d\n", thisIndent, indent);

                    // NOTE: This is really tricky: if we fail on this indent, we must **rewind
                    // back to newline** if (thisIndent) I -= 1;
                    I = savedI;

                    break;
                }

                if (eof()) { break; }

                Tok keyTok = advance();
                if (keyTok != Tok::eIdent) {
                    syamlPrintf(" - keyTok @ %d not ident. fail tryDict\n", I - 1);
                    return pg.reject(), nullptr;
                }
                syamlPrintf(" - keyTok @ %d = %s\n", I - 1, tdoc->getTokenString(keyTok).c_str());

                Tok colon = advance();
                if (colon != Tok::eColon) {
                    syamlPrintf(" - missing colon. fail tryDict\n");
                    return pg.reject(), nullptr;
                }

                while (peek() == Tok::eWhitespace) { advance(); }

                if (eof()) { throw std::runtime_error("nope"); }

                Tok cur = peek();

                // We MUST be starting a list
                if (cur == Tok::eOpenBrace) {
                    // advance();
                    Node* innerList = tryList();
                    if (innerList) {
                        while (peek() == Tok::eWhitespace) { advance(); }
                        while (peek() == Tok::eNL) { advance(); }
                        cs.push_back({ tdoc->getTokenString(keyTok), NodeUPtr { innerList } });
                    } else
                        throw std::runtime_error("looked like a list inside a map, but failed "
                                                 "to parse the inner list");
                    continue;
                }

                // We MUST be starting a list, map, or empty item
                if (cur == Tok::eNL) {

                    ParserGuard lookahead_pg(this);
                    cur = peek();
                    while (cur == Tok::eNL) {
                        advance();
                        cur = peek();
                    }

                    // For the inner dict/list, check that the indentation lines up (yes: must
                    // do this here and not the recursive call)
                    uint32_t innerIndent = 0;
                    if (peek() == Tok::eWhitespace) {
                        innerIndent = peek().n;
                        advance();
                    }
                    if (innerIndent <= indent) {
                        syamlPrintf("in tryDict(), innerIndent %d <= indent %d. This must mean "
                                    "that the "
                                    "current item '%s' is "
                                    "empty.\n",
                                    innerIndent, indent, tdoc->getTokenString(keyTok).c_str());
                        cs.push_back({ tdoc->getTokenString(keyTok),
                                       NodeUPtr { new EmptyNode(tdoc, lookahead_pg.currentRange()) } });
                        lookahead_pg.reject();
                        continue;
                    }

                    // NOTE: Always reject `lookahead_pg` because tryDict/tryListFromDash wants
                    // the whitespace to process itself.

                    // We MUST be starting a new list
                    if (peek() == Tok::eDash) {
                        // lookahead_pg.accept();
                        lookahead_pg.reject();

                        while (peek() == Tok::eWhitespace) advance();

                        Node* innerList = tryListFromDash();
                        if (innerList) {
                            cs.push_back({ tdoc->getTokenString(keyTok), NodeUPtr { innerList } });
                        } else
                            throw std::runtime_error("looked like a list (from dash) inside a map, "
                                                     "but failed to parse the inner list");
                        continue;
                    }

                    lookahead_pg.reject();
                    {
                        // We MUST be starting a new map
                        while (peek() == Tok::eNL) advance();

                        Node* innerDict = tryDict();
                        if (innerDict) {
                            cs.push_back({ tdoc->getTokenString(keyTok), NodeUPtr { innerDict } });
                        } else {
                            int rollback = I;
                            while (peek() == Tok::eWhitespace) advance();
                            if (eof()) {
                                // ok.
                                break;
                            } else {
                                I = rollback;
                                throw std::runtime_error("looked like a map inside a map, but failed "
                                                         "to parse the inner one");
                            }
                        }
                        continue;
                    }
                }

                // We MUST be starting a scalar
                Node* innerScalar = tryScalar();
                if (innerScalar) {
                    while (peek() == Tok::eWhitespace) { advance(); }
                    while (peek() == Tok::eNL) { advance(); }
                    cs.push_back({ tdoc->getTokenString(keyTok), NodeUPtr { innerScalar } });
                    continue;
                } else
                    throw std::runtime_error("looked like a scalar inside a map, but failed to "
                                             "parse the inner scalar");

                throw std::runtime_error("nothing parse in inner dict");
            }
        } catch (std::runtime_error& e) {
            std::cout << " - In tryDict(), starting here:\n";
            print_line_debug(tdoc, pg.I0);
            pg.reject();
            throw e;
        }

        if (cs.size()) {
            DictNode* newNode = new DictNode(tdoc, pg.currentRange());
            if constexpr (is_vector<decltype(newNode->children)>::value) {
                for (auto& kv : cs) newNode->children.push_back({ kv.first, kv.second.release() });
            } else {
                // for (auto& kv : cs) newNode->children[kv.first] = kv.second.release();
                syamlAssert(false);
            }
            for (auto& kv : newNode->children) kv.second->parent = newNode;
            return pg.accept(), newNode;
        } else
            return pg.reject(), nullptr;
    }

    ListNode::~ListNode() {
        for (auto c : children) delete c;
        children.clear();
    }
    DictNode::~DictNode() {
        for (auto kv : children) delete kv.second;
        children.clear();
    }
    ScalarNode::~ScalarNode() {
    }

    DictNode* Node::asDict() {
        auto out = dynamic_cast<DictNode*>(this);
        if (!out) throw std::runtime_error("bad cast to DictNode");
        return out;
    }
    ListNode* Node::asList() {
        auto out = dynamic_cast<ListNode*>(this);
        if (!out) throw std::runtime_error("bad cast to ListNode");
        return out;
    }
    ScalarNode* Node::asScalar() {
        auto out = dynamic_cast<ScalarNode*>(this);
        if (!out) throw std::runtime_error("bad cast to ScalarNode");
        return out;
    }

    RootNode* Node::getRoot(bool required) const {
        Node* node = const_cast<Node*>(this);
        while (node->parent) node = node->parent;
        RootNode* root = dynamic_cast<RootNode*>(node);
        if (required) syamlAssert(root != nullptr, "getRoot() failed");
        return root;
    }

    namespace {

        struct Serialization {
        private:
            std::stringstream ss;
            bool lastWasNl   = true;
            bool lastWasDash = false;

        public:
            void serialize_(Node* node, int depth);
            std::string serialize(Node* node);
        };

        void Serialization::serialize_(Node* node, int depth) {

            auto indent = [this](int depth) {
                for (int i = 0; i < depth * 4; i++) ss << " ";
            };
            auto newline = [this]() {
                if (!lastWasNl) {
                    lastWasNl = true;
                    ss << "\n";
                }
            };
            auto dash = [this]() {
                if (!lastWasDash) {
                    lastWasDash = true;
                    ss << "- ";
                }
            };

            auto d = dynamic_cast<DictNode*>(node);
            if (d == nullptr) d = dynamic_cast<RootNode*>(node);
            if (d) {
                newline();
                for (auto kv : d->children) {
                    auto key   = kv.first;
                    auto child = kv.second;
                    indent(depth);
                    ss << key << ":";
                    lastWasDash = lastWasNl = false;
                    ss << " ";
                    serialize_(child, depth + 1);
                    newline();
                }
            } else if (auto l = dynamic_cast<ListNode*>(node)) {

                if (!l->isFromDash()) {
                    // if (0) {
                    ss << "[";
                    lastWasDash = lastWasNl = false;
                    for (uint32_t i = 0; i < l->children.size(); i++) {
                        serialize_(l->children[i], 1 + depth);
                        if (i < l->children.size() - 1) ss << ", ";
                    }
                    ss << "]";
                    lastWasDash = lastWasNl = false;
                } else {
                    newline();
                    for (auto child : l->children) {
                        ListNode* sub = dynamic_cast<ListNode*>(child);
                        if (sub == nullptr or !sub->isFromDash()) {
                            newline();
                            indent(depth);
                            dash();
                        }
                        lastWasNl = false;
                        serialize_(child, depth + 1);
                        lastWasDash = false;
                    }
                }
            } else if (auto s = dynamic_cast<ScalarNode*>(node)) {
                if (s->valueStr.length() != 0) {
                    ss << s->valueStr;
                } else {
                    ss << s->tdoc->getTokenRangeString(s->tokRange);
                }
                lastWasDash = lastWasNl = false;
            } else if (dynamic_cast<EmptyNode*>(node)) {
                ss << " ";
                lastWasDash = lastWasNl = false;
            } else {
                syamlAssert(false);
            }
        }

        std::string Serialization::serialize(Node* root) {
            serialize_(root, 0);
            return ss.str();
        }

    }

    std::string serialize(Node* root) {
        syaml::Serialization s;
        return s.serialize(root);
    }

#else

    std::string serialize(Node* root);

#endif

}
