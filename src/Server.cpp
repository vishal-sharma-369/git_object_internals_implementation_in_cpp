#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <zlib.h>
#include <sstream>
#include <openssl/sha.h>
#include <vector>
#include <algorithm>

class Error
{
    std::string message;
    bool flag;
    public:
    Error(){};
    void setMessage(std::string msg)
    {
        this->message = msg;
    }
    std::string getMessage()
    {
        return this->message;
    }
    void setBool(bool flag)
    {
        this->flag = flag;
    }
    bool isError()
    {
        return flag;
    }
};

// Handling files
Error decompressFile(const std::string& file_path);
std::string sha_file(std::string basicString);
int compressFile(const std::string data, uLong *bound, unsigned char *dest) ;
Error hash_object(std::string file);

// Handling directories
Error decompressTree(const std::string& tree_path);

// ------------------------------------------------------------------------------------------------
// Main program execution 
// ------------------------------------------------------------------------------------------------
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

            std::cout << "Initialized git directory\n";
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }
    } 
    else if(command == "cat-file")
    {
        if (argc < 4 || std::string(argv[2]) != "-p")
        {
            std::cerr<<"Missing parameter: -p <hash>\n";
            return EXIT_FAILURE;
        }
        const auto blob_hash = std::string_view(argv[3], 40);
        const auto blob_dir = blob_hash.substr(0, 2);
        const auto blob_name = blob_hash.substr(2);

        const auto blob_path = std::filesystem::path(".git")/"objects"/blob_dir/blob_name;
        Error err = decompressFile(blob_path);
        if(err.isError())
        {
            std::cout<<err.getMessage()<<std::endl;
            return EXIT_FAILURE;
        }
    }
    else if(command == "hash-object")
    {
        if (argc < 4 || std::string(argv[2]) != "-w")
        {
            std::cerr<<"Missing parameter: -w <file>\n";
            return EXIT_FAILURE;
        }
        const auto file_name = std::string_view(argv[3]);
        const auto file_path = std::filesystem::current_path()/file_name;

        Error err = hash_object(file_path);
        if(err.isError())
        {
            std::cout<<err.getMessage()<<std::endl;
            return EXIT_FAILURE;
        }
    }
    else if(command == "ls-tree")
    {
        std::string flag = "";
        std::string tree_sha = "";
        //if no argument is given
        if(argc<=2){
            std::cerr<<"Invalid Arguments"<< "\n";
            return EXIT_FAILURE;
        }
        //if the flag is not included
        if(argc==3){
            tree_sha = argv[2];
        }else{
            tree_sha = argv[3];
            flag = argv[2];
        }

        //if the flag is wrong
        if(flag.size()!=0 && flag != "--name-only"){
            std::cerr<<"Incorrect flag :"<<flag<<"\nexpected --name-only"<<"\n";
            return EXIT_FAILURE;
        }

        const auto dir_name = tree_sha.substr(0, 2);
        const auto file_name = tree_sha.substr(2);

        std::string path = "./.git/objects/" + dir_name + "/" + file_name;

        Error err = decompressTree(path);
        if(err.isError())
        {
            std::cout<<err.getMessage()<<std::endl;
            return EXIT_FAILURE;
        }
    }
    else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


// ---------------------------------------------------------------------------------------------------
// Defining the core functionalities of mygit system
// ---------------------------------------------------------------------------------------------------

Error decompressFile(const std::string &file_path)
{
    Error error;
    auto in = std::ifstream(file_path);
    if(!in.is_open())
    {
        error.setMessage("Failed to open " + file_path + " file.\n");
        error.setBool(true);
        return error;
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
            error.setMessage("Failed to uncompress Zlib. (code: " + std::to_string(res) + ")\n");
            error.setBool(true);
            return error;
        }
        else
        {
            buf.resize(len);
            break;
        }
    }

    std::cout<<std::string_view(buf).substr(buf.find('\0')+1);
    error.setBool(false);   // Indicate that no error has occurred
    return error;
}

Error hash_object(std::string file_path)
{
    Error error;
    std::ifstream in(file_path);
    
    if(!in.is_open())
    {
        error.setMessage("Failed to open "+file_path+" file.\n");
        error.setBool(true);
        return error;
    }

    std::stringstream data;

    data << in.rdbuf();

    // Create blob content string
    std::string content = "blob " + std::to_string(data.str().length()) + '\0' + data.str();

    // Calculate SHA1 hash
    std::string buffer = sha_file(content);

    // Compress blob content
    uLong bound = compressBound(content.size());
    unsigned char compressedData[bound];
    int res = compressFile(content, &bound, compressedData);

    if(res != Z_OK)
    {
        error.setMessage("Failed to compress Zlib. (code: " + std::to_string(res) + ")\n");
        error.setBool(true);
        return error;
    }

    // Write compressed data to .git/objects 
    std::string dir = ".git/objects/" + buffer.substr(0,2);
    std::filesystem::create_directory(dir);
    std::string objectPath = dir + "/" + buffer.substr(2);
    std::ofstream objectFile(objectPath, std::ios::binary);
    objectFile.write((char *)compressedData, bound);
    objectFile.close();
    return error;
}

int compressFile(const std::string data, uLong *bound, unsigned char *dest)
{
    return compress(dest, bound, (const Bytef*) data.c_str(), data.size());
}

std::string sha_file(std::string data)
{
    unsigned char hash[20];
    SHA1((unsigned char*) data.c_str(), data.size(), hash);
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for(const auto& byte : hash)
    {
        ss << std::setw(2) << static_cast<int>(byte);
    }

    std::cout<<ss.str()<<std::endl;
    return ss.str();
}

// Handling the directories
Error decompressTree(const std::string& tree_path)
{
    Error error;
    std::ifstream file = std::ifstream(tree_path);

    if(!file.is_open())
    {
        error.setMessage("Failed to open file: " + tree_path + "\n");
        error.setBool(true);
        return error;
    }

    std::string tree_data = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

    std::string buf;
    buf.resize(tree_data.size());

    while(true)
    {
        uLong buf_size = buf.size();
        int res = uncompress(reinterpret_cast<Bytef*>(buf.data()), &buf_size, reinterpret_cast<const Bytef*> (tree_data.data()), tree_data.size());

        if(res == Z_BUF_ERROR){
            buf.resize(buf.size() * 2);
        }
        else if(res != Z_OK){
            error.setMessage("Failed to decompress file, code:"+std::to_string(res)+"\n");
            error.setBool(true);
            return error;
        }
        else{
            buf.resize(buf_size);
            break;
        }
    }

    std::string trimmed_data = buf.substr(buf.find('\0')+1);
    std::string line;
    std::vector<std::string> names;

    do
    {
        line = trimmed_data.substr(0, trimmed_data.find('\0'));
        if(line.substr(0, 5) == "40000")
        {
            names.push_back(line.substr(6));
        }
        else 
        {
            names.push_back(line.substr(7));
        }
        trimmed_data = trimmed_data.substr(trimmed_data.find('\0') + 21);
    } while (trimmed_data.size() > 1);

    sort(names.begin(), names.end());

    for(int i = 0; i < names.size(); i++)
    {
        std::cout<<names[i]<<"\n";
    }

    error.setBool(false);
    return error;
}