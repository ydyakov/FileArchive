#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <openssl/sha.h>

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
    std::unordered_map<std::string, std::string> contentTable;

    // Метаданни за файловете
    std::vector<FileRecord> fileRecords;

    // Хеширане на съдържание
    std::string hashContent(const std::string& content) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(content.c_str()), content.size(), hash);

        std::stringstream ss;
        for (unsigned char byte : hash) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
        }
        return ss.str();
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

        std::ostringstream contentStream;
        contentStream << file.rdbuf();
        std::string content = contentStream.str();
        file.close();

        // Хеширане на съдържанието
        std::string fileHash = hashContent(content);

        // Проверка дали файлът вече съществува
        if (contentTable.find(fileHash) == contentTable.end()) {
            // Добавяне на уникалното съдържание в хеш таблицата
            contentTable[fileHash] = content;
        }

        // Добавяне на запис за файла
        fileRecords.push_back({
            filePath.string(),
            fileHash,
            content.size(),
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
