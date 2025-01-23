#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>

namespace fs = std::filesystem;

enum PathTypeState 
{
	Directory, 
	File,      
	Error      
};

struct FileRecord
{
	std::string path;
	std::string hash;
	size_t size;
	std::time_t lastModified;
	std::vector<char> data;
};

struct Helpper
{
	static PathTypeState ensureDirectoryExists(const fs::path& inputPath) {
		if (!fs::exists(inputPath))
		{
			fs::path dirPath = inputPath;

			if (fs::is_regular_file(inputPath) || inputPath.has_extension())
			{
				dirPath = inputPath.parent_path();
			}
			try {
				if (!fs::exists(dirPath))
				{
					if (fs::create_directories(dirPath))
					{
						std::cout << "Created directories for path: " << dirPath << '\n';
					}
					else
					{
						std::cerr << "Failed to create directories for path: " << dirPath << '\n';
						return PathTypeState::Error;
					}
				}
			}
			catch (const fs::filesystem_error& e)
			{
				std::cerr << "Error: " << e.what() << '\n';
				return PathTypeState::Error;;
			}
		}
	
		
		if (fs::is_directory(inputPath))
		{
			return PathTypeState::Directory;
		}

		return PathTypeState::File;
	}

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

class Repository
{
private:
	std::unordered_map<std::string, FileRecord> fileTable;

	std::vector<FileRecord> fileRecords;

	void importFileFromFileSystem(const fs::path& filePath)
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

		if (fileTable.find(fileHash) == fileTable.end())
		{
			fileTable[fileHash] = FileRecord
			{
				filePath.string(),
				fileHash,
				buffer.size(),
				std::filesystem::last_write_time(filePath).time_since_epoch().count(),
				buffer
			};
		}
	}

public:

	void importAllFromFileSystem(const fs::path& path)
	{
		std::vector<fs::path> files;

		try
		{
			if (!fs::exists(path) || !fs::is_directory(path))
			{
				std::cerr << "Пътят не е валидна директория: " << path << std::endl;
				return;
			}

			for (const fs::directory_entry entry : fs::recursive_directory_iterator(path))
			{
				if (fs::is_regular_file(entry.path())) {
					files.push_back(entry.path());
				}
			}
		}
		catch (const std::exception& e) {
			std::cerr << "Грешка при обработка на директорията: " << e.what() << std::endl;
		}

		for (const fs::path filePath : files)
		{
			importFileFromFileSystem(filePath);
		}
	}

	void restoreFileToFileSystem(std::string dirPathString)
	{
		fs::path dirPath = dirPathString;

		if (Helpper::ensureDirectoryExists(dirPath) != PathTypeState::Directory)
		{
			std::cerr << "Пътят не е валидна директория: " << dirPath << std::endl;
			return;
		}

		for (auto [hash, fileRecord] : fileTable)
		{
			fs::path fullPath = dirPath / fileRecord.path;
			if (!Helpper::ensureDirectoryExists(fullPath))
			{
				continue;
			}
			std::ofstream outStream(fullPath, std::ios::binary);
			if (!outStream.is_open()) {
				std::cerr << "Неуспешно отваряне на файла: " << fullPath << std::endl;
			}

			// first save the size of file table
			outStream.write(fileRecord.data.data(), fileRecord.data.size());

			outStream.close();
		}
	}

	void serializeToArchive(const std::string& fileName) {
		std::ofstream outStream(fileName, std::ios::binary);
		if (!outStream.is_open()) {
			std::cerr << "Неуспешно записване на архива\n";
			return;
		}

		// first save the size of file table
		size_t filesCount = static_cast<size_t>(fileTable.size());
		outStream.write(reinterpret_cast<const char*>(&filesCount), sizeof(filesCount));

		for (const auto& [hash, fileRecord] : fileTable)
		{
			Helpper::serializeFileRecord(fileRecord, outStream);
		}

		for (const auto& [hash, fileRecord] : fileTable)
		{
			outStream.write(fileRecord.data.data(), fileRecord.data.size());
		}

		outStream.close();
	}

	void deserializeFromArchive(const std::string& fileName)
	{
		fileTable.clear();

		std::ifstream inStream(fileName, std::ios::binary);
		if (!inStream.is_open()) {
			std::cerr << "Неуспешно отваряне на архива\n";
			return;
		}

		// first read the size of file table
		size_t filesCount;
		inStream.read(reinterpret_cast<char*>(&filesCount), sizeof(filesCount));

		for (int index = 0; index < filesCount; index++)
		{
			FileRecord fileRecord = Helpper::deserializeFileRecord(inStream);
			fileTable[fileRecord.hash] = fileRecord;
		}

		for (auto [hash, fileRecord] : fileTable)
		{
			inStream.read(fileRecord.data.data(), fileRecord.size);
		}

		inStream.close();

	}
};

int main() {
	Repository repo;

	repo.importAllFromFileSystem("json");
	repo.serializeToArchive("archive/record.bin");
	repo.deserializeFromArchive("archive/record.bin");
	repo.restoreFileToFileSystem("archive");

	return 0;
}
