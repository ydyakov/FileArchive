#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>






std::string hashBinaryContent(const uint8_t* data, size_t size) {
    const uint64_t FNV_prime = 1099511628211u;
    const uint64_t FNV_offset_basis = 14695981039346656037u;

    uint64_t hash = FNV_offset_basis;

    for (size_t i = 0; i < size; ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= FNV_prime;
    }

    // Превръщане на хеша в низ (hex)
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return oss.str();
}


// Структура за представяне на файл
struct FileRecord {
    std::string path;        // Относителен път
    std::string hash;        // Хеш на съдържанието
    size_t size;             // Размер на файла
    std::time_t lastModified; // Дата на последна промяна
    
};

// Хранилище
class Repository {
private:
    // Хеш таблица: хеш -> съдържание
    std::unordered_map<std::string, uint8_t*> contentTable;

    // Метаданни за файловете
    std::vector<FileRecord> fileRecords;

    // Хеширане на съдържание
    std::string hashContent(const uint8_t* data, size_t size) {
        std::string result = hashBinaryContent(data, size);
        return result;
    }

public:
    // Добавяне на файл в хранилището
    void addFile(const std::filesystem::path& filePath) {
        // Четене на съдържанието на файла
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Неуспешно отваряне на файл: " << filePath << "\n";
            return;
        }

        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open the file: " << filePath << "\n";
            return;
        }

        // Определяне на размера на файла
        file.seekg(0, std::ios::end); // Премести указателя в края на файла
        size_t size = file.tellg();  // Определи текущата позиция (размера на файла)
        file.seekg(0, std::ios::beg); // Върни указателя в началото на файла

        // Създай буфер за съдържанието на файла
        std::vector<uint8_t> buffer(size);

        // Прочети съдържанието на файла
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            std::cerr << "Failed to read the file content.\n";
            return;
        }

        file.close();
                
        uint8_t* data = buffer.data(); 

        // Хеширане на съдържанието
        std::string fileHash = hashContent(data,  size);

        // Проверка дали файлът вече съществува
        if (contentTable.find(fileHash) == contentTable.end())
        {
           
            contentTable[fileHash] = data;
        }

        // Добавяне на запис за файла
        fileRecords.push_back({
            filePath.string(),
            fileHash,
            size,
            std::filesystem::last_write_time(filePath).time_since_epoch().count()
            });
    }

    // Сериализация на хранилището
    void serialize(const std::string& fileName) {
        std::ofstream outFile(fileName, std::ios::binary);
        if (!outFile.is_open()) {
            std::cerr << "Неуспешно записване на хранилището\n";
            return;
        }

        // Записване на хеш таблицата
        size_t tableSize = contentTable.size();
        outFile.write(reinterpret_cast<const char*>(&tableSize), sizeof(tableSize));
        for (const auto& [hash, content] : contentTable) {
            size_t hashSize = hash.size();
            size_t contentSize = content.size();

            outFile.write(reinterpret_cast<const char*>(&hashSize), sizeof(hashSize));
            outFile.write(hash.data(), hashSize);
            outFile.write(reinterpret_cast<const char*>(&contentSize), sizeof(contentSize));
            outFile.write(content.data(), contentSize);
        }

        // Записване на файловите записи
        size_t recordCount = fileRecords.size();
        outFile.write(reinterpret_cast<const char*>(&recordCount), sizeof(recordCount));
        for (const auto& record : fileRecords) {
            size_t pathSize = record.path.size();
            size_t hashSize = record.hash.size();

            outFile.write(reinterpret_cast<const char*>(&pathSize), sizeof(pathSize));
            outFile.write(record.path.data(), pathSize);
            outFile.write(reinterpret_cast<const char*>(&hashSize), sizeof(hashSize));
            outFile.write(record.hash.data(), hashSize);
            outFile.write(reinterpret_cast<const char*>(&record.size), sizeof(record.size));
            outFile.write(reinterpret_cast<const char*>(&record.lastModified), sizeof(record.lastModified));
        }

        outFile.close();
    }

    // Десериализация на хранилището
    void deserialize(const std::string& fileName) {
        std::ifstream inFile(fileName, std::ios::binary);
        if (!inFile.is_open()) {
            std::cerr << "Неуспешно четене на хранилището\n";
            return;
        }

        // Четене на хеш таблицата
        size_t tableSize;
        inFile.read(reinterpret_cast<char*>(&tableSize), sizeof(tableSize));
        contentTable.clear();
        for (size_t i = 0; i < tableSize; ++i) {
            size_t hashSize, contentSize;
            inFile.read(reinterpret_cast<char*>(&hashSize), sizeof(hashSize));

            std::string hash(hashSize, '\0');
            inFile.read(hash.data(), hashSize);

            inFile.read(reinterpret_cast<char*>(&contentSize), sizeof(contentSize));
            std::string content(contentSize, '\0');
            inFile.read(content.data(), contentSize);

            contentTable[hash] = content;
        }

        // Четене на файловите записи
        size_t recordCount;
        inFile.read(reinterpret_cast<char*>(&recordCount), sizeof(recordCount));
        fileRecords.clear();
        for (size_t i = 0; i < recordCount; ++i) {
            size_t pathSize, hashSize;

            inFile.read(reinterpret_cast<char*>(&pathSize), sizeof(pathSize));
            std::string path(pathSize, '\0');
            inFile.read(path.data(), pathSize);

            inFile.read(reinterpret_cast<char*>(&hashSize), sizeof(hashSize));
            std::string hash(hashSize, '\0');
            inFile.read(hash.data(), hashSize);

            size_t size;
            std::time_t lastModified;
            inFile.read(reinterpret_cast<char*>(&size), sizeof(size));
            inFile.read(reinterpret_cast<char*>(&lastModified), sizeof(lastModified));

            fileRecords.push_back({ path, hash, size, lastModified });
        }

        inFile.close();
    }
};

int main() {
    Repository repo;

    // Добавяне на файлове
    repo.addFile("example1.txt");
    repo.addFile("example2.txt");

    // Сериализация
    repo.serialize("repository.dat");

    // Десериализация
    Repository newRepo;
    newRepo.deserialize("repository.dat");

    return 0;
}
