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

enum Command
{
	CREATE,
	UPDATE,
	CHECK,
	EXTRACT,
	ERROR
};

struct FileMetadata
{
	std::string path;
	std::time_t lastModified;
};

struct UniqueFileRecord
{
	std::string hash;
	size_t size;
	std::vector<char> data;
	std::vector<FileMetadata> filesMetadata;
};

struct CommandParameter
{
	Command action;
	std::string archive;
	std::vector<std::string> dirPaths;
	bool hashOnlyFlag = false;
};


struct Helpper
{
	static CommandParameter ComputeAcction(int argc, char* argv[]) {
		CommandParameter result;
		if (argc < 3) 
		{
			std::cerr << "Use: backup.exe create|extract|check|update  hash-only? <name> <directory>+" << std::endl;
			result.action = Command::ERROR;
			return result;
		}
				
		std::string command = argv[1];
		std::transform(command.begin(), command.end(), command.begin(), [](unsigned char c) 
		{
			return std::tolower(c); 
		});
		
		if (command != "create" && command != "update" && command != "check" && command != "extract") {
			std::cerr << "Use: backup.exe create|extract|check|update hash-only? <name> <directory>+" << std::endl;
			result.action = Command::ERROR;
			return result;
		}

		if (command == "update")
		{
			result.action = Command::UPDATE;
		}
		else if (command == "create")
		{
			result.action = Command::CREATE;
		}
		else if (command == "check")
		{
			result.action = Command::CHECK;
		}
		else if (command == "extract")
		{
			result.action = Command::EXTRACT;
		}

		int index = 2; 
		if (std::string(argv[2]) == "hash-only") 
		{
			result.hashOnlyFlag = true;
			index++;
		}
				
		if (index >= argc) 
		{
			std::cerr << "Error: Missing <name> parameter." << std::endl;
			result.action = Command::ERROR;
			return result;
		}
		result.archive = argv[index++];

		if (index >= argc) 
		{
			std::cerr << "Error: Missing <directory> parameters." << std::endl;
			result.action = Command::ERROR;
			return result;
		}

		for (int i = index; i < argc; ++i) 
		{
			result.dirPaths.push_back(argv[i]);
		}

		return result;
	}

	static PathTypeState ensureDirectoryExists(const fs::path& inputPath) {
		if (!fs::exists(inputPath))
		{
			fs::path dirPath = inputPath;

			if (fs::is_regular_file(inputPath) || inputPath.has_extension())
			{
				dirPath = inputPath.parent_path();
			}
			try 
			{
				if (!fs::exists(dirPath))
				{
					if (fs::create_directories(dirPath))
					{
						std::cout << "Created directories for path: " << dirPath << std::endl;
					}
					else
					{
						std::cerr << "Failed to create directories for path: " << dirPath << std::endl;
						return PathTypeState::Error;
					}
				}
			}
			catch (const fs::filesystem_error& e)
			{
				std::cerr << "Error: " << e.what() << std::endl;
				return PathTypeState::Error;;
			}
		}
			
		if (fs::is_directory(inputPath))
		{
			return PathTypeState::Directory;
		}

		return PathTypeState::File;
	}

	// Сериализация на UniqueFileRecord
	static void serializeFileRecord(const UniqueFileRecord& record, std::ofstream& outStream) {
		
		// Сериализация на броя идентични файлове
		size_t identicalFileCount = record.filesMetadata.size();
		outStream.write(reinterpret_cast<const char*>(&identicalFileCount), sizeof(identicalFileCount));
		
		// Сериализация на filesMetadata
		for (FileMetadata file : record.filesMetadata)
		{
			size_t pathLength = file.path.size();
			// сериализация на path
			outStream.write(reinterpret_cast<const char*>(&pathLength), sizeof(pathLength));
			outStream.write(file.path.data(), pathLength);

			// Сериализация на `lastModified`
			outStream.write(reinterpret_cast<const char*>(&file.lastModified), sizeof(file.lastModified));

		}

		// Сериализация на hash
		size_t hashLength = record.hash.size();
		outStream.write(reinterpret_cast<const char*>(&hashLength), sizeof(hashLength));
		outStream.write(record.hash.data(), hashLength);

		// Сериализация на size
		outStream.write(reinterpret_cast<const char*>(&record.size), sizeof(record.size));
				
		// Сериализация на data
		size_t dataLength = record.data.size();
		outStream.write(reinterpret_cast<const char*>(&dataLength), sizeof(dataLength));
		outStream.write(record.data.data(), dataLength);
		
	}

	// Десериализация на FileRecord
	static UniqueFileRecord deserializeFileRecord(std::ifstream& inStream) {
		UniqueFileRecord record;

		// Десериализация на броя идентични файлове
		size_t identicalFileCount;
		inStream.read(reinterpret_cast<char*>(&identicalFileCount), sizeof(identicalFileCount));

		// Десериализация на filesMetadata
		for (int i = 0; i < identicalFileCount; i++)
		{
			// Десериализация на path
			size_t pathLength;
			inStream.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));
			std::string path(pathLength, '\0');
			inStream.read(path.data(), pathLength);

			// Десериализация на lastModified
			std::time_t lastModified;
			inStream.read(reinterpret_cast<char*>(&lastModified), sizeof(lastModified));

			record.filesMetadata.push_back({ path, lastModified });
		}

		// Десериализация на hash
		size_t hashLength;
		inStream.read(reinterpret_cast<char*>(&hashLength), sizeof(hashLength));
		record.hash.resize(hashLength);
		inStream.read(record.hash.data(), hashLength);

		// Десериализация на size
		inStream.read(reinterpret_cast<char*>(&record.size), sizeof(record.size));
		
		// Десериализация на data
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

		std::ostringstream oss;
		oss << std::hex << std::setfill('0') << std::setw(16) << hash;
		return oss.str();
	}
};

class Repository
{
private:
	std::unordered_map<std::string, UniqueFileRecord> fileTable;

	std::vector<UniqueFileRecord> fileRecords;

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
			fileTable[fileHash] = UniqueFileRecord
			{
				fileHash,
				buffer.size(),
				buffer
			};
		}

		auto fileMetadata = std::find_if(fileTable[fileHash].filesMetadata.begin(), fileTable[fileHash].filesMetadata.end(), [&](const FileMetadata& file) 
		{
			return file.path == filePath;
		});

		if (fileMetadata == fileTable[fileHash].filesMetadata.end()) 
		{
			size_t count = fileTable[fileHash].filesMetadata.size();
			if (count == 0)
			{
				std::cout << "Added unique file to archive: Hash: " << fileHash << " File: " << filePath << std::endl;
			}
			else
			{
				std::cout << "Added non unique file to archive: "<< count + 1 << " copies toral Hash: " << fileHash <<  " File : " << filePath << std::endl;
			}

		}
		else 
		{
			std::cout << "The same file with same path already in achive Hash: " << fileHash << " File: " << filePath << std::endl;
		}

		fileTable[fileHash].filesMetadata.push_back({ filePath.string() , std::filesystem::last_write_time(filePath).time_since_epoch().count() });
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
			for (FileMetadata file : fileRecord.filesMetadata)
			{
				fs::path fullPath = dirPath / file.path;
				if (!Helpper::ensureDirectoryExists(fullPath))
				{
					continue;
				}
				std::ofstream outStream(fullPath, std::ios::binary);
				if (!outStream.is_open()) {
					std::cerr << "Неуспешно отваряне на файла: " << fullPath << std::endl;
					continue;
				}

				outStream.write(fileRecord.data.data(), fileRecord.data.size());
				outStream.close();
				std::cout << "Restore to file sysytem: " << fullPath << std::endl;
			}
		}
	}

	void serializeToArchive(const std::string& fileName) {
		std::ofstream outStream(fileName, std::ios::binary);
		if (!outStream.is_open()) {
			std::cerr << "Неуспешно записване на архива" << std::endl;
			return;
		}

		// first save the size of file table
		size_t filesCount = static_cast<size_t>(fileTable.size());
		outStream.write(reinterpret_cast<const char*>(&filesCount), sizeof(filesCount));

		for (const auto& [hash, fileRecord] : fileTable)
		{
			Helpper::serializeFileRecord(fileRecord, outStream);
		}

		outStream.close();
	}

	void deserializeFromArchive(const std::string& fileName)
	{
		fileTable.clear();

		std::ifstream inStream(fileName, std::ios::binary);
		if (!inStream.is_open()) {
			std::cerr << "Неуспешно отваряне на архива" << std::endl;
			return;
		}

		// first read the size of file table
		size_t filesCount;
		inStream.read(reinterpret_cast<char*>(&filesCount), sizeof(filesCount));

		for (int i = 0; i < filesCount; i++)
		{
			UniqueFileRecord fileRecord = Helpper::deserializeFileRecord(inStream);
			fileTable[fileRecord.hash] = fileRecord;
		}

		inStream.close();

	}
};

int main(int argc, char* argv[]) {
	
	CommandParameter command = Helpper::ComputeAcction(argc, argv);

	if (command.action == Command::ERROR)
	{
		return 1;
	}

	if (command.action == Command::UPDATE)
	{
		Repository repo;
		repo.deserializeFromArchive(command.archive);
		for (std::string dirPath : command.dirPaths)
		{
			repo.importAllFromFileSystem(dirPath);
		}
		repo.serializeToArchive(command.archive);
	}
	else if (command.action == Command::CREATE)
	{
		Repository repo;
		for (std::string dirPath : command.dirPaths)
		{
			repo.importAllFromFileSystem(dirPath);
		}
		repo.serializeToArchive(command.archive);
	}
	else if (command.action == Command::CHECK)
	{
		Repository repo;
		repo.deserializeFromArchive(command.archive);
		repo.importAllFromFileSystem(command.dirPaths[0]);
		
	}
	else if (command.action == Command::EXTRACT)
	{
		Repository repo;
		repo.deserializeFromArchive(command.archive);
		repo.restoreFileToFileSystem(command.dirPaths[0]);
		
	}

	return 0;
}
