#include "parse.hpp"
#include <filesystem>

using namespace syaml;

void test_python_files(const std::string &dir) {
}

void test_simple() {

		std::string src = "a:\n   b: 1.1e2\nc: 2\nx:\n y:\n  z: 4\n w: 5\nasd: [1,2]\nf:\n   - 1\n    - 2       - 3\n\n       - \"bye\"\n\n #comment:1";
		// std::string src = "a:\nb: 1.1e2\n";
		// std::string src = "a:\nb: 1\nc: 1.1e2\n";
		std::cout << " Doc:\n" << src << "\n\n";

		Document doc(src);
		TokenizedDoc tdoc = lex(&doc);
		printf(" - Parsed tdoc:\n");
		for (auto& tok : tdoc.tokens) {
			tok.print(std::cout, doc);
			std::cout << " ";
		}
		std::cout << "\n";

		Parser p;
		p.parse(&tdoc);

		std::cout << "\n - Serialized parsed doc:\n" << serialize(p.root) << "\n";

		// auto b = p.root->get("a")->get("b")->as<double>();
		// std::cout << " - 'b' = " << b << "\n";
}

int main() {

	test_simple();
	test_python_files(".");

	return 0;
}
