# Yaml Parser
A simple yaml parser that is not yet fully functional. I did not read the spec when writing this, so it's not really intended to be full-fledged. Rather, it just needs to meet my use case of parsing pretty simple configuration files.

The implementation is header only. Only [yaml_parse.hpp](./yaml_parse.hpp) is needed. Everything else in the repo is for testing.

## :warning:
There's some yaml test files randomly created by a python script. Currently the lexer/parser fail on these. There's two issues:
   1) Parsing multiple layers of lists on one line, for example: ` - - 1`
   2) Lexing/parsing maps using the `{ ... }` syntax.

I'm sure there's other features in the spec that are not handled too, but these are the top priority!

## Usage
#### Building
Like the STB libraries, you should include the `yaml_parser.hpp` header in **one** translation unit with `#define SYAML_IMPL` coming before the `#include`. This will include the function definitions. All other uses of the library must not have that macro defined.
Splitting this way allows as much source code as possible to not be parsed by any dependent source files.
Because this parser supports decoding directly to user defined types via templates, some functions must be included inline.
####
After parsing, use the `get()` methods to navigate the document tree, using either a string argument for `DictNode`s or a integer argument for `ListNode`s. Then when at a target node, call `as<T>()` with the desired type (e.g. int, string, etc.)
`as()` can also turn `DictNode`s into `unordered_map<string, V>`s, and `ListNode`s into `vector<V>`s.
`as()` can also turn nodes into user defined types using a conversion struct. See below.

`get()` will never return a null pointer. Instead if you access out of bounds, or a non-existent key, you'll get a cached `EmptyNode*`. You can easily test if the get failed by using `node->isEmpty()`.

## User Defined decoding
```cpp
// Example of user defined conversion.
//
struct MyType {
	int x;
	int y;
};

namespace syaml {
	// Can use one of 'FromKey' / 'FromList' as base class.
	//
	template <> struct Decode<MyType> : FromKey {
		static MyType decode(const DictNode* d) {
			MyType out;
			auto x = d->get_("x");
			auto y = d->get_("y");
			out.x = x->as_<int>(100); // Get the value in the 'x' node, or the default of 100
			out.y = y->as_<int>(101); // Get the value in the 'x' node, or the default of 101
			return out;
		}
	};
}
```
