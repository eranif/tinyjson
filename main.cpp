#include "tinyjson.hpp"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <streambuf>
#include <string>
#include <vector>

void get_file_contents(const char* filename, std::string* content)
{
    // Open the file in read mode.
    FILE* file = fopen(filename, "rb");
    // Check if there was an error.
    if (file == NULL) {
        std::cerr << "Error: Can't open file: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    // Get the file length
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    content->resize(length);
    // Set the contents of the string.
    size_t bytes = fread((void*)content->data(), sizeof(char), length, file);
    (void)bytes;
    // Close the file.
    fclose(file);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " </path/to/lexers.json>" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string lexers_json_file = argv[1];

    tinyjson::element root;
    std::cout << "reading file..." << std::endl;
    std::cout << "parsing..." << std::endl;
    if (!tinyjson::element::parse_file(lexers_json_file.c_str(), &root)) {
        return 1;
    }

    std::cout << "success" << std::endl;
    // std::stringstream ss;
    std::ofstream ss(L"lexer.output.json", std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
    if (!ss.is_open()) {
        std::cerr << "failed to open output file" << std::endl;
        exit(1);
    }
    if (root.is_array()) {
        std::cout << "read " << root.size() << " lexers" << std::endl;
    }

    std::cout << "calling to_string" << std::endl;
    tinyjson::to_string(root, ss);
    std::cout << "success" << std::endl;
    ss.close();

    // std::cout << ss.str() << std::endl;

    // build JSON
    std::cout << "Building lexer JSON" << std::endl;
    tinyjson::element writer;
    tinyjson::element::create_object(&writer);

    auto& lexers_array = writer.add_array("lexers");

    // add a lexer
    lexers_array.add_array_object().add_property("name", "\"test_lexer\"\r\n").add_property("score", 100);
    lexers_array.add_array_object().add_property("name", "second_lexer\r\n").add_property("score", 50);
    lexers_array[1].add_property("value", "a good value");

    std::string val = lexers_array[1]["value"].to_str<std::string>();
    std::cout << val << std::endl;

    // loop and print each lexer
    size_t index = 0;
    for (const auto& lexer : lexers_array) {
        std::stringstream s;
        tinyjson::to_string(lexer, s, false);
        std::cout << "printing lexer " << index << ":" << std::endl;
        std::cout << s.str() << std::endl;
        ++index;
    }
    return 0;
}
