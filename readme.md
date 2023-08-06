# Yaml Parser
A simple yaml parser that is not yet fully functional. I did not read the spec when writing this, so it's not really intended to be full-fledged. Rather, it just needs to meet my use case of parsing pretty simple configuration files.

The implementation is header only. Only [yaml_parse.hpp](./yaml_parse.hpp) is needed. Everything else in the repo is for testing.

There's some yaml test files randomly created by a python script. Currently the lexer/parser fail on these. There's two issues:
   1) Parsing multiple layers of lists on one line, for example: ` - - 1`
   2) Lexing/parsing maps using the `{ ... }` syntax.

I'm sure there's other features in the spec that are not handled too, but these are the top priority!

## User Defined decoding
```cpp

// Example of user defined conversion.
//
struct MyType {
	int x;
	int y;
};

template <> struct YamlDecode<MyType> {
	// Must set this:
	static constexpr bool value = true;

	// Exactly one of these must be true:
	static constexpr bool use_dict = true;
	static constexpr bool use_list = true;
	static constexpr bool use_scalar = true;

	// We specified `use_dict`, so `decode` should take a `DictNode`
	static MyType decode(const DictNode* d) {
		MyType out;
		auto x = d->get_("x");
		auto y = d->get_("y");
		tpAssert(x);
		tpAssert(y);
		out.x = x->as_<int>({});
		out.y = y->as_<int>({});
		return out;
	}
};

```
