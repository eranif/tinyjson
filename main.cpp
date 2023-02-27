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
    if(file == NULL) {
        std::cerr << "Error: Can't open file: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    // Get the file length
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    content->resize(length);
    // Set the contents of the string.
    size_t bytes = fread(content->data(), sizeof(char), length, file);
    (void)bytes;
    // Close the file.
    fclose(file);
}

int main(int argc, char** argv)
{
    tinyjson::Element root;
    std::cout << "reading file..." << std::endl;
    std::string content;
    get_file_contents(R"(/home/eran/devl/tinyjson/build-debug/lexers.json)", &content);
    std::cout << "success" << std::endl;

    std::cout << "json file size: " << content.length() / 1024 / 1024 << "MiB" << std::endl;
    std::cout << "parsing..." << std::endl;
    if(!tinyjson::Element::parse(content.c_str(), &root)) {
        return 1;
    }
    std::cout << "success" << std::endl;
    std::stringstream ss;
    if(root.is_array()) {
        std::cout << "read " << root.size() << " lexers" << std::endl;
        //        for(size_t i = 0; i < root.size(); ++i) {
        //            auto lexer = root[i];
        //            std::string_view lexer_name;
        //            {
        //                auto prop = lexer["Name"];
        //                if(prop.as_str(&lexer_name)) {
        //                    //std::cout << lexer_name << std::endl;
        //                }
        //            }
        //            {
        //                auto prop = lexer["Name"];
        //                if(prop.as_str(&lexer_name)) {
        //                    //std::cout << lexer_name << std::endl;
        //                }
        //            }
        //
        //            {
        //                auto prop = lexer["Name"];
        //                if(prop.as_str(&lexer_name)) {
        //                    //std::cout << lexer_name << std::endl;
        //                }
        //            }
        //        }
    }

    std::cout << "calling to_string" << std::endl;
    tinyjson::to_string(root, ss);
    std::cout << "success" << std::endl;

    // std::cout << ss.str() << std::endl;

    // build JSON
    std::cout << "Building lexer JSON" << std::endl;
    tinyjson::Element writer;
    tinyjson::Element::create_object(&writer);

    auto& lexers_array = writer.add_array("lexers");

    // add a lexer
    lexers_array.add_array_object().add_property("name", "\"test_lexer\"\r\n").add_property("score", 100);
    lexers_array.add_array_object().add_property("name", "second_lexer\r\n").add_property("score", 50);
    lexers_array[1].add_property("value", "a good value");

    // loop and print each lexer
    size_t index = 0;
    for(const auto& lexer : lexers_array) {
        std::stringstream s;
        tinyjson::to_string(lexer, s);
        std::cout << "printing lexer " << index << ":" << std::endl;
        std::cout << s.str() << std::endl;
        ++index;
    }
    return 0;
}
