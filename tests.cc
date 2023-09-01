
// #define SYAML_IMPL

#include "yaml_parse.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <climits>

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"


using namespace syaml;

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
			auto x = d->get_("xxxx",1); // note: this matches the key 'x\0' because we pass len=1
			auto y = d->get_("y");
			out.x = x->as_<int>(100); // Get the value in the 'x' node, or the default of 100
			out.y = y->as_<int>(101); // Get the value in the 'x' node, or the default of 101
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
	std::string src = "list1: [1,2  \t  ]\ntrue: true\nfalse: 0\nempty:\na:\n   q: \"str\"\n  b: 1.1e2\nc: 2\nx:\n y:\n  z: 4\n w: 5\nasd: [1,2]\nf:\n - 1\n -\n  - 2\n  - 3\nmyThing:\n x: 1\n y: 2\n #comment";
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

		auto q = root->get("a")->get("q")->as<std::string>();
		std::cout << " - q = " << q << "\n";

		// Test set()
		root->set<int>("newValue", 1);
		std::cout << " - newValue = " << root->get("newValue")->as<int>() << "\n";
		root->get("a")->set<int>("newValue", 1);
		root->get("a")->set<std::string>("newValue2", "str");
		std::cout << " - a/newValue = " << root->get("a")->get("newValue")->as<int>() << "\n";
		std::cout << " - a/newValue2 = " << root->get("a")->get("newValue2")->as<std::string>() << "\n";
		check("newValue2 string", root->get("a")->get("newValue2")->as<std::string>() == std::string{"str"});

		check("true works", root->get("true")->as<bool>() == true);
		check("false works", root->get("false")->as<bool>() == false);

		auto newSubNode = new DictNode();
		newSubNode->set<int>("newKey", 2);
		root->set<DictNode*>("newSubNode", newSubNode);

		std::cout << "\n - Serialized parsed doc:\n" << serialize(root.get()) << "\n";


	} catch(std::runtime_error& e) {
		std::cout << " - ERROR " << e.what() << "\n";
		success = false;
	} catch(...) {
		success = false;
	}

	return success;
}

bool test_complex() {
	std::cout << "--------------------------------------------------------------------------------------------\n";
	std::cout << "--------------------------- Running complex test  -------------------------------------------\n";
	std::cout << "--------------------------------------------------------------------------------------------\n";

	// TODO: Parse like this:
	//
	// std::string src = "list1: [1,2,  \t {a:3} ]\n";
	std::string src = "list1: [1,2,  \t  ]\n";
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

		auto c = root->get("c");
		assert(c);

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
	success &= test_complex();
	// success &= test_python_files(".");

	std::cout << "\n\n";
	if (success)
		std::cout << KGRN " - All tests succeeded!" << KNRM "\n";
	else
		std::cout << KRED " - At least one test failed!" << KRED "\n";
	std::cout << "\n";

	return success == true ? 0 : 1;
}
