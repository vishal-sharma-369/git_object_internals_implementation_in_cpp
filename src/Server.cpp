#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <zlib.h>
#include <sstream>
#include <vector>
#include <openssl/sha.h>
#include <iomanip>
#include <unordered_map>
#include <algorithm>

/**
 * Reads and decompresses data from a blob object, then processes it to extract the actual content.
 * Note: Blobs only store the contents of a file, not its name or permissions.
 * The formate of a blob object looks like this: blob <size>\0<content> (size is in bytes)
 *
 * This function performs the following steps:
 *
 * 1. **Open the File**:
 *    - Opens the file specified by the `file_path` parameter in binary mode for reading.
 *    - If the file cannot be opened, an error message is printed, and an empty string is returned.
 *
 * 2. **Read Compressed Data**:
 *    - Reads the entire content of the file into a `std::string` named `compressed_data`.
 *    - The file is read using an input stream iterator that iterates through each character in binary mode.
 *
 * 3. **Initialize zlib Stream**:
 *    - Initializes a `z_stream` structure for decompression with default values.
 *    - Sets the input size (`avail_in`) and the pointer to the compressed data (`next_in`).
 *    - Configures `zalloc`, `zfree`, and `opaque` to `Z_NULL` (default memory management).
 *    - Calls `inflateInit` to set up the decompression stream.
 *    - If initialization fails, an error message is printed, and an empty string is returned.
 *
 * 4. **Prepare for Decompression**:
 *    - Allocates a buffer (`std::vector<char>`) to hold decompressed data.
 *    - Uses a loop to call `inflate`, decompressing the data in chunks:
 *      - **Set Output Buffer**: Defines the size and location for decompressed data (`avail_out` and `next_out`).
 *      - **Call `inflate`**: Processes data from the input buffer and writes it to the output buffer.
 *      - **Check for Errors**: Handles any errors during decompression (e.g., stream error, data error, memory error).
 *      - **Append Data**: Adds the decompressed data from the buffer to the `decompressed_data` string.
 *      - Continues processing until all compressed data is decompressed (`Z_STREAM_END`).
 *    - Cleans up the zlib stream using `inflateEnd`.
 *
 * 5. **Process Decompressed Data**:
 *    - Initializes a `std::istringstream` with the decompressed data to facilitate further processing.
 *    - Reads the header part of the data using `std::getline` until a null terminator (`'\0'`).
 *    - Reads the remaining part of the data, which is the actual content, into a `std::string`.
 *
 * 6. **Return Actual Data**:
 *    - Returns the string containing the actual content after extracting the header.
 *
 * The function is designed to handle compressed Git object data and separate out the content from the header.
 */
std::string decompress_git_object_and_remove_header(std::string &file_path) {
    auto in = std::ifstream(file_path);
    if(!in.is_open())
    {
        std::cout<<"Failed to open " + file_path + " file.\n";
        return "";
    }
    const auto blob_data = std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

    auto buf = std::string();
    buf.resize(blob_data.size());
    while(true)
    {
        auto len = buf.size();
        if(auto res = uncompress((uint8_t*)buf.data(), &len, (const uint8_t*)blob_data.data(), blob_data.size()); res == Z_BUF_ERROR)
        {
            buf.resize(buf.size()*2);
        } 
        else if(res != Z_OK)
        {
            std::cout<<"Failed to uncompress (git object)Zlib. (code: " + std::to_string(res) + ")\n";
            return "";
        }
        else
        {
            buf.resize(len);
            break;
        }
    }

    std::string trimmed_data = buf.substr(buf.find('\0')+1);
    return trimmed_data;
}

/**
 * Compresses the contents of a file and formats it as a Git-style blob object.
 * The format of a blob object looks like this: blob <size>\0<content> (size is in bytes)
 *
 * This function performs the following steps:
 *
 * 1. **Open the File**:
 *    - Opens the file specified by the `file_path` parameter in binary mode for reading.
 *    - If the file cannot be opened, an error message is printed, and an empty string is returned.
 *
 * 2. **Read File Data**:
 *    - Reads the entire content of the file into a `std::vector<char>` named `file_data`.
 *    - The file is read using an input stream iterator that iterates through each character in binary mode.
 *    - The `file_data` vector will hold the raw bytes read from the file.
 *
 * 3. **Create Git-style Header**:
 *    - Constructs a header string in the Git object format: "blob <size>\0".
 *    - `file_data.size()` gives the size of the file content, which is converted to a string and appended after "blob ".
 *    - The header is terminated with a null character (`'\0'`).
 *
 * 4. **Combine Header and Data**:
 *    - Combines the Git-style header and the file data into a single `std::string` called `full_data`.
 *    - This string will be the input data for compression.
 *
 * 5. **Initialize zlib Stream**:
 *    - Initializes a `z_stream` structure for compression with default values.
 *    - Sets the input size (`avail_in`) and the pointer to the data to be compressed (`next_in`).
 *    - The `reinterpret_cast<Bytef*>(full_data.data())` converts the `char*` pointer from `std::string::data()` to `Bytef*` needed by zlib.
 *    - Configures `zalloc`, `zfree`, and `opaque` to `Z_NULL` (default memory management).
 *    - Calls `deflateInit` to set up the compression stream with zlib's best compression setting (`Z_BEST_COMPRESSION`).
 *    - If initialization fails, an error message is printed, and an empty string is returned.
 *
 * 6. **Prepare for Compression**:
 *    - Allocates a buffer (`std::vector<char>`) to hold compressed data chunks.
 *    - Uses a loop to call `deflate`, compressing the data in chunks:
 *      - **Set Output Buffer**: Defines the size and location for compressed data (`avail_out` and `next_out`).
 *      - **Call `deflate`**: Processes data from the input buffer and writes compressed data to the output buffer.
 *      - **Check for Errors**: Handles any errors during compression (e.g., stream error).
 *      - **Append Data**: Adds the compressed data from the buffer to the `compressed_data` string.
 *      - Continues processing until all data has been compressed (`strm.avail_out` is not zero).
 *    - Cleans up the zlib stream using `deflateEnd`.
 *
 * 7. **Return Compressed Data**:
 *    - Returns the string containing the compressed data after the compression process is completed.
 *
 * This function is designed to handle file data and compress it in the Git object format.
 */

// This is just utility function below, the description above is for the function underneath this function
void compressFile(const std::string data, uLong *bound, unsigned char *dest) {
    compress(dest, bound, (const Bytef *)data.c_str(), data.size());
}

void compress_blob_data_and_write_to_objects(const std::string& file_path, std::string& buffer) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error: File '" << file_path << "' not found." << std::endl;
        return;
    }

    std::stringstream file_data;
    file_data << file.rdbuf();

    // Combine the header and file data
    std::string full_data = "blob " + std::to_string(file_data.str().length()) + '\0' + file_data.str();

    uLong bound = compressBound(full_data.size());
    unsigned char compressedData[bound];
    compressFile(full_data, &bound, compressedData);

    std::string dir = ".git/objects/" + buffer.substr(0,2);
    std::filesystem::create_directory(dir);
    std::string objectPath = dir + "/" + buffer.substr(2);
    std::ofstream objectFile(objectPath, std::ios::binary);
    // Ensure the file stream is open before writing
    if (!objectFile) {
        std::cerr << "Error: Could not open file for writing: " << objectPath << '\n';
        return;
    }
    objectFile.write((char *)compressedData, bound);
    objectFile.close();
    std::cout<<buffer<<std::endl;
}

/**
 * Compresses a tree format string using zlib with the best compression settings.
 * 
 * This function performs the following steps:
 *
 * 1. **Initialize zlib Stream**:
 *    - Sets up the zlib stream (`z_stream`) with default values for compression.
 *    - Specifies the input data's size and starting point.
 *    - Initializes the compression process using the best compression level.
 *    - If initialization fails, an error is reported, and the function returns an empty string.
 *
 * 2. **Set Up Compression Buffer**:
 *    - Prepares a buffer to store chunks of compressed data.
 *    - Uses a loop to perform compression in steps:
 *      - **Configure Output Buffer**: Specifies the size and location for storing compressed data.
 *      - **Call `deflate`**: Compresses the input data in chunks.
 *      - **Check for Errors**: Handles any compression errors (e.g., stream error).
 *      - **Append Compressed Data**: Adds the compressed data from the buffer to the final string.
 *      - Continues compression until all input data is processed.
 *    - Ends the compression process and cleans up resources.
 *
 * 3. **Return Compressed Data**:
 *    - Returns the final compressed string containing the tree format.
 * 
 * The function is designed to compress a tree format string in preparation for storage or transmission.
 */
void compress_tree_format_and_write_to_objects(const std::string& tree_format, std::string& tree_hash_hex) {
    // Combine the header and file data
    uLong bound = compressBound(tree_format.size());
    unsigned char compressedData[bound];
    compressFile(tree_format, &bound, compressedData);

    std::string dir = ".git/objects/" + tree_hash_hex.substr(0,2);
    std::filesystem::create_directory(dir);
    std::string objectPath = dir + "/" + tree_hash_hex.substr(2);
    std::ofstream objectFile(objectPath, std::ios::binary);
    // Ensure the file stream is open before writing
    if (!objectFile) {
        std::cerr << "Error: Could not open file for writing: " << objectPath << '\n';
        return;
    }
    objectFile.write((char *)compressedData, bound);
    objectFile.close();
    std::cout<<tree_hash_hex<<std::endl;
}

/**
 * Creates a SHA-1 hash of the uncompressed contents of a file, using a Git-style header.
 * The header format is "blob <size>\0" where <size> is the size of the file in bytes.
 *
 * This function performs the following steps:
 *
 * 1. **Open the File**:
 *    - Opens the file specified by the `file_name` parameter in binary mode for reading.
 *    - If the file cannot be opened, an error message is printed, and an empty string is returned.
 *
 * 2. **Read File Contents**:
 *    - Reads the entire content of the file into a `std::vector<char>` named `file_data`.
 *    - The file is read using an input stream iterator that iterates through each character in binary mode.
 *
 * 3. **Create Git-Style Header**:
 *    - Constructs a header string that includes the word "blob", the size of the file in bytes, and a null terminator (`'\0'`).
 *    - This header, combined with the actual file contents, forms the data to be hashed.
 *
 * 4. **Concatenate Header and File Contents**:
 *    - Concatenates the header and the file data into a single string named `store_data`.
 *
 * 5. **Calculate SHA-1 Hash**:
 *    - Uses the `SHA1` function from a cryptographic library to calculate the SHA-1 hash of the `store_data` string.
 *    - The `hash` array, of size `SHA_DIGEST_LENGTH`, holds the resulting 20-byte (160-bit) hash.
 *
 * 6. **Convert Hash to Hexadecimal String**:
 *    - **Initialize Output Stream**:
 *      - An `std::ostringstream` object named `oss` is created to format the hash as a hexadecimal string.
 *    - **Iterate Over Each Byte**:
 *      - The `hash` array contains 20 bytes of data. Each byte is 8 bits, so the total hash size is 160 bits (20 bytes * 8 bits/byte).
 *      - Each element in the `hash` array is an `unsigned char`, which holds one byte of data.
 *    - **Hexadecimal Conversion**:
 *      - **`std::hex`**: This manipulator sets the output stream to hexadecimal mode. It tells the `ostringstream` to interpret and format the integers in hexadecimal (base 16).
 *      - **`std::setw(2)`**: Ensures that each hexadecimal value is two characters wide. This is crucial because each byte (8 bits) is represented by two hexadecimal digits. For instance, a byte with a value of 15 would be displayed as "0f" in hexadecimal.
 *      - **`std::setfill('0')`**: Pads the output with leading zeros if the hexadecimal value is less than two characters. For example, a value of 5 would be padded to "05".
 *      - **`static_cast<int>(hash[i])`**: Converts the `unsigned char` to an `int` for formatting purposes. This conversion is necessary because the stream manipulators (`std::hex`, `std::setw`, and `std::setfill`) work with `int` values. It ensures that the byte is properly interpreted and displayed as a hexadecimal number.
 *      - Each byte in the `hash` array is processed individually, converted to its hexadecimal representation, and appended to the `oss` stream.
 *    - **Resulting Hexadecimal String**:
 *      - After the loop completes, the `oss` object contains the complete hexadecimal string representation of the SHA-1 hash.
 *      - The `oss.str()` method is used to retrieve this string, which represents the entire hash in a human-readable hexadecimal format.
 *
 * The function assumes that the SHA-1 implementation and relevant libraries are properly included and linked in your project.
 * Notes: the SHA hash needs to be computed over the "uncompressed" contents of the file, not the compressed version.
 * The input for the SHA hash is the header (blob <size>\0) + the actual contents of the file, not just the contents of the file.
 */
std::string create_sha_hash(const std::string &file_name, bool return_in_binary, bool is_symlink = false) {
    std::string store_data;
    if (is_symlink) {
        // Handle symlink by reading its target
        std::string target_path = std::filesystem::read_symlink(file_name).string();
        // Create the Git-style header for symlinks: "blob <size>\0".
        std::string header = "blob " + std::to_string(target_path.size()) + '\0';
        store_data = header + target_path;
    } else {
        // Open the file in binary mode for reading.
        std::ifstream file(file_name, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: File '" << file_name << "' not found." << std::endl;
            return ""; // Return an empty string if the file cannot be opened.
        }
        // Read file contents into a vector of characters.
        std::vector<char> file_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close(); // Close the file as it is no longer needed.
        // Create the Git-style header: "blob <size>\0".
        std::string header = "blob " + std::to_string(file_data.size()) + '\0';
        store_data = header + std::string(file_data.begin(), file_data.end());
    }
    // Array to hold the SHA-1 hash (20 bytes, 160 bits).
    unsigned char hash[SHA_DIGEST_LENGTH];
    // Calculate the SHA-1 hash of the concatenated header and file contents.
    SHA1(reinterpret_cast<const unsigned char *>(store_data.c_str()), store_data.size(), hash);

    if (return_in_binary) {
        // Return the binary representation of the SHA-1 hash.
        return std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH);
    } 
    else {
        // Create a string stream to convert the hash to a hexadecimal string.
        std::ostringstream oss;
        for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        // Return the final hexadecimal string representation of the SHA-1 hash.
        return oss.str();
    }
}
/**
 * Creates a SHA-1 hash of a given tree format string and optionally returns it in hexadecimal format.
 *
 * This function performs the following steps:
 *
 * 1. **Compute SHA-1 Hash**:
 *    - Computes the SHA-1 hash of the input `tree_format` string.
 *    - The hash is stored in a 20-byte array (`hash`).
 *
 * 2. **Return in Desired Format**:
 *    - If `return_in_hex` is `true`, converts the hash to a hexadecimal string.
 *    - If `return_in_hex` is `false`, returns the raw binary hash as a string.
 * 
 * The function is designed to create a hash for a tree object in Git and return it in the desired format.
 */
std::string create_tree_hash(const std::string& tree_format, bool return_in_hex) {
    // Step 1: Compute the SHA-1 hash of the tree format string.
    unsigned char hash[SHA_DIGEST_LENGTH]; // Array to hold the 20-byte SHA-1 hash.
    SHA1(reinterpret_cast<const unsigned char*>(tree_format.c_str()), tree_format.size(), hash); // Compute the SHA-1 hash.
    // Step 2: Return the hash in the desired format.
    if (return_in_hex) {
        // If the hexadecimal format is requested, convert the hash to a hex string.
        std::ostringstream hex_stream; // Stream to hold the hex string.
        for (unsigned char c : hash) {
            // Convert each byte of the hash to a 2-digit hex value and append to the stream.
            hex_stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
        return hex_stream.str(); // Return the hexadecimal string.
    } else {
        // Return the raw binary hash as a string.
        return std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH);
    }
}

/**
 * Creates a tree format string representing the contents of a directory for use in a Git tree object.
 *
 * This function performs the following steps:
 *
 * 1. **Initialize Structures**:
 *    - Creates necessary variables and data structures to hold file modes, names, and hashes.
 *
 * 2. **Iterate Through Directory Entries**:
 *    - Loops through all files and subdirectories in the specified `directory_path`.
 *    - For each file, directory, or symlink, calculates the SHA-1 hash, determines the mode, and stores the information.
 *    - Skips the `.git` directory.
 *
 * 3. **Sort Entries**:
 *    - Sorts the collected directory entries alphabetically by their filenames.
 *
 * 4. **Build Tree Format String**:
 *    - Constructs the tree format string by concatenating the mode, name, and hash of each entry.
 *    - Prepends the string with the tree header indicating the type and size of the content.
 *
 * 5. **Return Tree Format String**:
 *    - Returns the final tree format string that can be used to create a Git tree object.
 */
std::string create_tree_format(const std::string& directory_path) {
    // Step 1: Initialize structures.
    std::filesystem::path path(directory_path); // Filesystem path object representing the directory.
    std::string entries_string; // String to accumulate the entries in the tree format.
    std::string mode; // String to hold the file mode (permissions).
    std::string file_hash; // String to hold the SHA-1 hash of the file's content or subtree.
    std::unordered_map<std::string, std::pair<std::string, std::string>> entries; // Map to store filename, mode, and hash.
    // Step 2: Iterate through directory entries.
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.path().filename() == ".git") {
            continue; // Skip the .git directory.
        }
        // Check if the entry is a regular file.
        if (entry.is_regular_file()) {
            std::string full_file_path = entry.path().string(); // Get the full file path.
            file_hash = create_sha_hash(full_file_path, true); // Compute the SHA-1 hash of the file.
            // Determine the file mode based on its permissions.
            std::filesystem::perms permissions = entry.status().permissions();
            if ((permissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) {
                mode = "100755"; // Executable file mode.
            } else {
                mode = "100644"; // Regular file mode.
            }
            entries[entry.path().filename().string()] = {mode, file_hash}; // Store the mode and hash.
        } 
        // Check if the entry is a symbolic link.
        else if (entry.is_symlink()) {
            std::string full_file_path = entry.path().string(); // Get the full path of the symlink.
            file_hash = create_sha_hash(full_file_path, true, true); // Compute the SHA-1 hash of the symlink.
            mode = "120000"; // Symlink mode.
            entries[entry.path().filename().string()] = {mode, file_hash}; // Store the mode and hash.
        } 
        // Check if the entry is a directory.
        else if (entry.is_directory()) {
            mode = "40000"; // Directory mode.
            std::string full_directory_path = entry.path().string(); // Get the full path of the directory.
            std::string tree_format = create_tree_format(full_directory_path); // Recursively create the tree format for the subdirectory.
            file_hash = create_tree_hash(tree_format, false); // Compute the SHA-1 hash of the subdirectory tree format.
            entries[entry.path().filename().string()] = {mode, file_hash}; // Store the mode and hash.
        }
    }
    // Step 3: Sort the entries by filename.
    std::vector<std::string> sorted_keys; // Vector to hold sorted filenames.
    for (const auto& [key, value] : entries) {
        sorted_keys.push_back(key); // Add the filename to the vector.
    }
    std::sort(sorted_keys.begin(), sorted_keys.end()); // Sort the filenames alphabetically.
    // Step 4: Build the tree format string.
    for (int i = 0; i < sorted_keys.size(); i++) {
        // Concatenate the mode, filename, and hash for each entry in the sorted order.
        entries_string += entries[sorted_keys[i]].first + " " + sorted_keys[i] + '\0' + entries[sorted_keys[i]].second;
    }
    // Prepend the tree header to the accumulated entries string.
    std::string tree_format_string = "tree " + std::to_string(entries_string.size()) + '\0' + entries_string;
    // Step 5: Return the final tree format string.
    return tree_format_string;
}

int main(int argc, char *argv[])
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    
    if (argc < 2) {
        std::cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }
    
    std::string command = argv[1];
    
    if (command == "init") { 
        try {
            std::filesystem::create_directory(".git");
            std::filesystem::create_directory(".git/objects");
            std::filesystem::create_directory(".git/refs");
    
            std::ofstream headFile(".git/HEAD");
            if (headFile.is_open()) {
                headFile << "ref: refs/heads/main\n";
                headFile.close();
            } else {
                std::cerr << "Failed to create .git/HEAD file.\n";
                return EXIT_FAILURE;
            }
    
            std::cout << "Initialized mygit repository\n";
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }
    }
    else if (command == "cat-file") {
        if (argc <= 3) {
            std::cerr << "Invalid arguments, required `-p <blob_sha>`\n";
            return EXIT_FAILURE;
        }
        std::string flag = argv[2];
        if (flag != "-p") {
            std::cerr << "Invalid flag for cat-file, expected `-p`\n";
            return EXIT_FAILURE;
        }
        const std::string value = argv[3];
        const std::string dir_name = value.substr(0, 2);
        const std::string blob_sha = value.substr(2);
        std::string path = ".git/objects/" + dir_name + "/" + blob_sha;
        std::string file_content = decompress_git_object_and_remove_header(path);
        std::cout << file_content;
    }
    else if(command == "hash-object") {
        if (argc <= 3) {
            std::cerr << "Invalid arguments, required `-w test.txt`\n";
            return EXIT_FAILURE;
        }
        std::string flag = argv[2];
        if (flag != "-w") {
            std::cerr << "Invalid flag for hash-object, expected `-w`\n";
            return EXIT_FAILURE;
        }
        std::string file_name = argv[3];
        std::string sha_hash = create_sha_hash(file_name, false);
        compress_blob_data_and_write_to_objects(file_name, sha_hash);
    }
    else if (command == "ls-tree") {
        if (argc <= 3) {
            std::cerr << "Invalid arguments, required `--name-only <tree_sha>`\n";
            return EXIT_FAILURE;
        }
        std::string flag = argv[2];
        if (flag != "--name-only") {
            std::cerr << "Invalid flag for ls-tree, expected `--name-only`\n";
            return EXIT_FAILURE;
        }
        std::string tree_hash = argv[3];
        std::string dir = ".git/objects/" + tree_hash.substr(0, 2);
        std::string file = tree_hash.substr(2);
        std::string path = dir + "/" + file;
        //change function name to decompress zlib data
        //after this function, the tree header will be discarded
        std::string decompressed_tree_data = decompress_git_object_and_remove_header(path);

        // Store the tree object names inside this vector
        std::vector<std::string> names;
        // Loop until the decompressed_tree_data string is empty
        while (!decompressed_tree_data.empty()) {
            // The decompressed tree_data string is in this format: 
            // "<mode> <name>\0<20_byte_sha><mode> <name>\0<20_byte_sha>"
            // Initialize variables to hold parts of the current tree object entry
            std::string mode = "";               // Will hold the file mode (e.g., "100644")
            std::string twenty_byte_hash = "";   // Will hold the SHA-1 hash of the object (20 bytes)
            std::string tree_file_name = "";     // Will hold the file name
            // Find the position of the first space in the string, which separates the mode from the file name
            int empty_space_position = decompressed_tree_data.find(' ');
            // Extract the mode from the beginning of the string up to the first space
            mode = decompressed_tree_data.substr(0, empty_space_position);

            // Remove the extracted mode and the space from the original string
            decompressed_tree_data.erase(0, empty_space_position + 1);
            // Find the position of the null terminator ('\0') that separates the file name from the SHA-1 hash
            int null_terminator_position = decompressed_tree_data.find('\0');            
            // Extract the file name from the string up to the null terminator
            tree_file_name = decompressed_tree_data.substr(0, null_terminator_position);           
            // Remove the extracted file name and the null terminator from the original string
            decompressed_tree_data.erase(0, null_terminator_position + 1);
            // Extract the next 20 characters, which represent the SHA-1 hash (20 bytes)
            twenty_byte_hash = decompressed_tree_data.substr(0, 20);         
            // Remove the extracted SHA-1 hash from the original string
            decompressed_tree_data.erase(0, 20);
            // Output the extracted file name to the console
            names.push_back(tree_file_name);
        }

        sort(names.begin(), names.end());

        for(int i = 0; i < names.size(); i++)
        {
            std::cout<<names[i]<<"\n";
        }
    }
    else if(command == "write-tree") {
        // Generate the tree format string and hash from the working directory
        std::string directory_path = "."; // Assuming current working directory
        std::string tree_format = create_tree_format(directory_path);
        std::string tree_hash_hex = create_tree_hash(tree_format, true); // Get hex hash
        compress_tree_format_and_write_to_objects(tree_format, tree_hash_hex);
    }
    else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}