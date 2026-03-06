#ifndef ZIP_STRUCTS_H
#define ZIP_STRUCTS_H

#include <cstdint>

// Force byte-for-byte packing (critical for binary files)
#pragma pack(push, 1)

// 1. Local File Header (Goes right before your Huffman bitstream)
struct ZipLocalFileHeader {
    uint32_t signature = 0x04034b50;      // "PK\3\4"
    uint16_t versionNeeded = 20;
    uint16_t generalPurposeFlag = 0;
    uint16_t compressionMethod = 8;       // Set to 8 (Deflate/Custom Huffman) for your engine
    uint16_t lastModFileTime = 0;
    uint16_t lastModFileDate = 0;
    uint32_t crc32 = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t fileNameLength = 0;
    uint16_t extraFieldLength = 0;
};

// 2. Central Directory Header (Goes at the end, acts as the file index)
struct ZipCentralDirectoryFileHeader {
    uint32_t signature = 0x02014b50;      // "PK\1\2"
    uint16_t versionMadeBy = 20;
    uint16_t versionNeeded = 20;
    uint16_t generalPurposeFlag = 0;
    uint16_t compressionMethod = 8;       // Must match Local Header (8)
    uint16_t lastModFileTime = 0;
    uint16_t lastModFileDate = 0;
    uint32_t crc32 = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t fileNameLength = 0;
    uint16_t extraFieldLength = 0;
    uint16_t fileCommentLength = 0;
    uint16_t diskNumberStart = 0;
    uint16_t internalFileAttributes = 0;
    uint32_t externalFileAttributes = 0;
    uint32_t relativeOffsetOflocalHeader = 0; // Byte offset where Local Header starts (Usually 0)
};

// 3. End of Central Directory Record (The absolute end of the file)
struct ZipEndOfCentralDirectoryRecord {
    uint32_t signature = 0x06054b50;      // "PK\5\6"
    uint16_t diskNumber = 0;
    uint16_t centralDirectoryDiskNumber = 0;
    uint16_t numEntriesThisDisk = 1;      // 1 file for now
    uint16_t numEntriesTotal = 1;         // 1 file for now
    uint32_t centralDirectorySize = 0;    // Size of Central Dir struct + filename length
    uint32_t centralDirectoryOffset = 0;  // Byte offset where Central Directory starts
    uint16_t zipCommentLength = 0;
};

#pragma pack(pop)

#endif // ZIP_STRUCTS_H