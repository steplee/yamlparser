#include "yaml_parse.hpp"
#include <filesystem>
#include <fstream>
#include <climits>

using namespace syaml;

namespace syaml {

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

}

/*
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
*/

template <class T> std::string vec_to_string(const std::vector<T>& v) {
	std::stringstream ss;
	auto it = v.begin();
	if (it != v.end()) ss << *it++;
	while (it != v.end()) ss << ", " << *it++;
	return ss.str();
}
bool test_simple() {

	std::cout << "--------------------------------------------------------------------------------------------\n";
	std::cout << "--------------------------- Running simple test  -------------------------------------------\n";
	std::cout << "--------------------------------------------------------------------------------------------\n";

	// std::string src = "empty:\na:\n   b: 1.1e2\nc: 2\nx:\n y:\n  z: 4\n w: 5\nasd: [1,2]\nf:\n   - 1\n   -         \n     - 3\n\n       - \"bye\"\n\n #comment:1";
	std::string src = "empty:\na:\n   b: 1.1e2\nc: 2\nx:\n y:\n  z: 4\n w: 5\nasd: [1,2]\nf:\n - 1\n -\n  - 2\n  - 3\nmyThing:\n x: 1\n y: 2\n";
	// std::string src = "a:\nb: 1.1e2\n";
	// std::string src = "a:\nb: 1\nc: 1.1e2\n";
	std::cout << " Doc:\n" << src << "\n\n";

	bool success = true;

	auto check = [&success](std::string msg, bool cond) {
		if (!cond) {
			std::cout << " - FAILED CHECK: " << msg << "\n";
			success = false;
		}
	};


	try {
		Document doc(src);
		TokenizedDoc tdoc = lex(&doc);
		printf(" - Parsed tdoc:\n");
		for (auto& tok : tdoc.tokens) {
			tok.print(std::cout, *tdoc.doc);
			std::cout << " ";
		}
		std::cout << "\n";

		Parser p;
		auto root = std::unique_ptr<RootNode>(p.parse(&tdoc));
		std::cout << "\n - Serialized parsed doc:\n" << serialize(root.get()) << "\n";

		auto c = root->get("c");
		assert(c);
		std::cout << " c = " << c->as<int>() << "\n";

		auto asd = root->get("asd");
		assert(asd);
		auto asd_vector = asd->as<std::vector<std::string>>();
		std::cout << " asd = " << vec_to_string<std::string>(asd_vector) << "\n";

		auto f = root->get("f");
		// auto f0 = f->get(0);
		auto f1 = f->get(1);
		assert(f1);
		// std::cout << " f1 = " << serialize(f1) << "\n";
		auto f1v = f1->as<std::vector<std::string>>();
		std::cout << " f1 = " << vec_to_string<std::string>(f1v) << "\n";

		auto myThing = root->get("myThing")->as<MyType>();
		std::cout << " myThing = { .x=" << myThing.x << ", .y=" << myThing.y << " }" << "\n";
		check("myThing.x", myThing.x == 1);
		check("myThing.y", myThing.y == 2);

		MyType fake {5,6};
		auto myThing2 = root->get("myNonExistentThing")->as<MyType>(fake);
		std::cout << " myNonExistentThing = { .x=" << myThing2.x << ", .y=" << myThing2.y << " }" << "\n";
		check("myThing2.x", myThing2.x == fake.x);
		check("myThing2.y", myThing2.y == fake.y);

	} catch(std::runtime_error& e) {
		std::cout << " - ERROR " << e.what() << "\n";
		success = false;
	} catch(...) {
		success = false;
	}

	return success;
}

int main() {

	// void* a = malloc(5); // Test that address sanitizer is working.
	// int a=INT_MAX; a++; // Test that ubsan is working.

	bool success = true;
	success &= test_simple();
	// success &= test_python_files(".");

	std::cout << "\n\n";
	if (success)
		std::cout << KGRN " - All tests succeeded!" << KNRM "\n";
	else
		std::cout << KRED " - At least one test failed!" << KRED "\n";
	std::cout << "\n";

	return success == true ? 0 : 1;
}