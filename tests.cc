#include "yaml_parse.hpp"
#include <filesystem>
#include <fstream>


using namespace syaml;

bool test_python_files(const std::string &dir) {

	std::cout << "--------------------------------------------------------------------------------------------\n";
	std::cout << "------------------ Running tests from python generated files -------------------------------\n";
	std::cout << "--------------------------------------------------------------------------------------------\n";

	bool success = true;

	for (auto f : std::filesystem::directory_iterator(dir)) {
		if (f.path().extension() == ".yaml") {
			std::ifstream ifs(f.path());
			std::stringstream ss;
			ss << ifs.rdbuf();
			std::string src = ss.str();

			Document doc(src);

			try {
				TokenizedDoc tdoc = lex(&doc);

				Parser p;
				try {
					p.parse(&tdoc);
				} catch(std::runtime_error& e) {
					std::cout << " - " KRED "Failed" KNRM " test on file '" KCYN << f.path() << KNRM "'. Failed " KBLU "parsing" KNRM " with error:\n\t" << e.what() << "\n";
					success = false;
				}

			} catch(std::runtime_error& e) {
				std::cout << " - " KRED "Failed" KNRM " test on file '" KCYN << f.path() << KNRM "'. Failed " KMAG "lexing" KNRM " with error:\n\t" << e.what() << "\n";
				success = false;
			}

			// std::cout << "\n - Serialized parsed doc:\n" << serialize(p.root) << "\n";

		}
	}

	return success;
}

bool test_simple() {

	std::cout << "--------------------------------------------------------------------------------------------\n";
	std::cout << "--------------------------- Running simple test  -------------------------------------------\n";
	std::cout << "--------------------------------------------------------------------------------------------\n";

	std::string src = "empty:\na:\n   b: 1.1e2\nc: 2\nx:\n y:\n  z:[ 4\n w: 5\nasd: [1,2]\nf:\n   - 1\n    - 2       - 3\n\n       - \"bye\"\n\n #comment:1";
	// std::string src = "a:\nb: 1.1e2\n";
	// std::string src = "a:\nb: 1\nc: 1.1e2\n";
	std::cout << " Doc:\n" << src << "\n\n";

	bool success = true;

	try {
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
	} catch(...) {
		success = false;
	}

	return success;
}

int main() {

	bool success = true;
	success &= test_simple();
	success &= test_python_files(".");

	std::cout << "\n\n";
	if (success)
		std::cout << KGRN " - All tests succeeded!" << KNRM "\n";
	else
		std::cout << KRED " - At least one test failed!" << KRED "\n";
	std::cout << "\n";

	return success == true ? 0 : 1;
}
