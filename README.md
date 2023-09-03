# tinyjson
A small and efficient C++ JSON library (for both read/write)

## Building
---

```bash
mkdir build-release
cd $_
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j10
```

## Example usage
---

### Parsing `JSON` file
---

```c++

// Read the json content from the file
std::string file_content = read_file("/path/to/file.json");

// parse it
tinyjson::element root;
if(!tinyjson::parse(file_content.c_str(), &root)) {
    exit(EXIT_FAILURE);
}

// Or we could parse the file directly:
// tinyjson::parse_file("/path/to/file.json", &root);

// accessing elements
// if root was an array, we can access the elements using [] access which is `O(1)`
const auto& elem0 = root[0];

if(elem0.is_object() && elem0.contains("name")) {
    std::string name;
    if(elem0.as_str(&name)) {
        std::cout << "first element name is: " << name << std::endl;
    }
}


// we can shorten the code a bit:
std::string name;
root[0]["name"].as_str(&name);

```

### Building `JSON`

```c++
// lets build the JSON,
// our end goal is to have this:
//
//  [
//      {
//          "name": "tinyjson",
//          "author": "Eran Ifrah"
//      }
//  ]
///
tinyjson::element arr;
tinyjson::element::create_array(&arr);
arr.add_array_object()
    .add_property("name", "tinyjson")
    .add_property("author", "Eran Ifrah");

// format it to string
std::stringstream ss;
tinyjson::to_string(arr, ss);
std::cout << ss.str() << std::endl;
```
