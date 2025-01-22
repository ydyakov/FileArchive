#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>

namespace fs = std::filesystem;

// Структура за представяне на файл
struct FileRecord {
    std::string path;        // Относителен път
    std::string hash;        // Хеш на съдържанието
    size_t size;             // Размер на файла
    std::time_t lastModified; // Дата на последна промяна
    std::vector<char> data;
};

struct Helpper
{
    // Сериализация на FileRecord
    static void serializeFileRecord(const FileRecord& record, std::ofstream& outStream) {
        // Сериализация на `path`
        size_t pathLength = record.path.size();
        outStream.write(reinterpret_cast<const char*>(&pathLength), sizeof(pathLength));
        outStream.write(record.path.data(), pathLength);

        // Сериализация на `hash`
        size_t hashLength = record.hash.size();
        outStream.write(reinterpret_cast<const char*>(&hashLength), sizeof(hashLength));
        outStream.write(record.hash.data(), hashLength);

        // Сериализация на `size`
        outStream.write(reinterpret_cast<const char*>(&record.size), sizeof(record.size));

        // Сериализация на `lastModified`
        outStream.write(reinterpret_cast<const char*>(&record.lastModified), sizeof(record.lastModified));

        // Сериализация на `data`
        size_t dataLength = record.data.size();
        outStream.write(reinterpret_cast<const char*>(&dataLength), sizeof(dataLength));
        outStream.write(record.data.data(), dataLength);
    }

    // Десериализация на FileRecord
    static FileRecord deserializeFileRecord(std::ifstream& inStream) {
        FileRecord record;

        // Десериализация на `path`
        size_t pathLength;
        inStream.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));
        record.path.resize(pathLength);
        inStream.read(record.path.data(), pathLength);

        // Десериализация на `hash`
        size_t hashLength;
        inStream.read(reinterpret_cast<char*>(&hashLength), sizeof(hashLength));
        record.hash.resize(hashLength);
        inStream.read(record.hash.data(), hashLength);

        // Десериализация на `size`
        inStream.read(reinterpret_cast<char*>(&record.size), sizeof(record.size));

        // Десериализация на `lastModified`
        inStream.read(reinterpret_cast<char*>(&record.lastModified), sizeof(record.lastModified));

        // Десериализация на `data`
        size_t dataLength;
        inStream.read(reinterpret_cast<char*>(&dataLength), sizeof(dataLength));
        record.data.resize(dataLength);
        inStream.read(record.data.data(), dataLength);

        return record;
    }

    static std::string hashBinaryContent(const std::vector<char>& data) {
        const uint64_t FNV_prime = 1099511628211u;
        const uint64_t FNV_offset_basis = 14695981039346656037u;

        uint64_t hash = FNV_offset_basis;

        for (char byte : data) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(byte));
            hash *= FNV_prime;
        }

        // Превръщане на хеша в низ (hex)
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(16) << hash;
        return oss.str();
    }
};

class Repository {
private:
    std::unordered_map<std::string, FileRecord> archiveData;

    std::vector<FileRecord> fileRecords;
    
    void addFileToarchiveData(const fs::path& filePath) 
    {
        std::ifstream inputFile(filePath, std::ios::binary);
        if (!inputFile) {
            std::cerr << "Неуспешно отваряне на файла: " << filePath << std::endl;
            return;
        }

        inputFile.seekg(0, std::ios::end);
        std::streamsize fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        std::vector<char> buffer(fileSize);

        if (!inputFile.read(buffer.data(), fileSize)) {
            std::cerr << "Грешка при четене на файла." << std::endl;
            return;
        }

        inputFile.close(); 
                
        std::string fileHash = Helpper::hashBinaryContent(buffer);

        // Проверка дали файлът вече съществува
        if (archiveData.find(fileHash) == archiveData.end())
        {           
            archiveData[fileHash] = FileRecord{
                filePath.string(),
                fileHash,
                buffer.size(),
                std::filesystem::last_write_time(filePath).time_since_epoch().count(),
                buffer
            };
        }
        
    }

public:
    
    void init(const fs::path& path) {
        std::vector<fs::path> files;

        try {
            // Проверка дали директорията съществува
            if (!fs::exists(path) || !fs::is_directory(path)) {
                std::cerr << "Пътят не е валидна директория: " << path << std::endl;
                return;
            }

            // Обхождане на директорията и поддиректориите
            for (const fs::directory_entry entry : fs::recursive_directory_iterator(path)) 
            {
                if (fs::is_regular_file(entry.path())) {
                    files.push_back(entry.path()); // Добавяне на файла към вектора
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Грешка при обработка на директорията: " << e.what() << std::endl;
        }

        for (const fs::path filePath : files)
        {
            addFileToarchiveData(filePath);
        }
    }

    void serialize(const std::string& fileName) {
        std::ofstream outFile(fileName, std::ios::binary);
        if (!outFile.is_open()) {
            std::cerr << "Неуспешно записване на хранилището\n";
            return;
        }

        // ****

        outFile.close();
    }

    // Десериализация на хранилището
    void deserialize(const std::string& fileName) 
    {
        
    }
};

int main() {
    Repository repo;

    repo.init("json");
    
    FileRecord recordToSave = {
        "example/path/file.txt",
        "abcdef1234567890",
        1024,
        std::time(nullptr),
        {'H', 'e', 'l', 'l', 'o'}
    };

    // Сериализация в бинарен файл
    std::ofstream outFile("archive/record.bin", std::ios::binary);
    if (!outFile) {
        std::cerr << "Неуспешно отваряне на файла за запис." << std::endl;
        return 1;
    }
    Helpper::serializeFileRecord(recordToSave, outFile);
    outFile.close();

    // Десериализация от бинарен файл
    std::ifstream inFile("archive/record.bin", std::ios::binary);
    if (!inFile) {
        std::cerr << "Неуспешно отваряне на файла за четене." << std::endl;
        return 1;
    }
    FileRecord recordLoaded = Helpper::deserializeFileRecord(inFile);
    inFile.close();

    // Показване на десериализирания запис
    std::cout << "Path: " << recordLoaded.path << std::endl;
    std::cout << "Hash: " << recordLoaded.hash << std::endl;
    std::cout << "Size: " << recordLoaded.size << std::endl;
    std::cout << "Last Modified: " << recordLoaded.lastModified;
    std::cout << "Data: ";
    for (char c : recordLoaded.data) {
        std::cout << c;
    }
    std::cout << std::endl;

    

    return 0;
}
