#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdint>

// 1. Force the compiler to pack the struct exactly byte-for-byte
#pragma pack(push, 1)
struct ZipLocalFileHeader {
    uint32_t signature = 0x04034b50;      // "PK\3\4"
    uint16_t versionNeeded = 20;          // Version 2.0
    uint16_t generalPurposeFlag = 0;      
    uint16_t compressionMethod = 0;       // 0 = Stored (No compression for this test)
    uint16_t lastModFileTime = 0;         
    uint16_t lastModFileDate = 0;         
    uint32_t crc32 = 0;                   // Checksum (0 for now)
    uint32_t compressedSize = 0;          
    uint32_t uncompressedSize = 0;        
    uint16_t fileNameLength = 0;          
    uint16_t extraFieldLength = 0;        
};
#pragma pack(pop)

int main() {
    // 2. Open a new file in binary write mode
    FILE* outFile = fopen("my_first_archive.zip", "wb");
    if (!outFile) {
        std::cerr << "Error: Could not create file." << std::endl;
        return 1;
    }

    // 3. Prepare our data
    ZipLocalFileHeader header;
    const char* filename = "test.txt";
    const char* dummyData = "Hello! This is my first raw ZIP file built from scratch in C++.";

    // 4. Calculate the sizes dynamically
    header.fileNameLength = strlen(filename);
    header.compressedSize = strlen(dummyData);
    header.uncompressedSize = strlen(dummyData); // Same as compressed because Method = 0

    // 5. Write everything to the file in the exact order the ZIP spec requires
    fwrite(&header, sizeof(ZipLocalFileHeader), 1, outFile);
    fwrite(filename, 1, header.fileNameLength, outFile);
    fwrite(dummyData, 1, header.compressedSize, outFile);

    fclose(outFile);
    std::cout << "Success! 'my_first_archive.zip' has been generated." << std::endl;

    return 0;
}