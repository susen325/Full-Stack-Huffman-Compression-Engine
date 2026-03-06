#ifndef HUFFMAN_HPP
#define HUFFMAN_HPP
#include "zip_structs.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <string>
#include <filesystem> 

using namespace std;
namespace fs = std::filesystem;

// --- CONSTANTS ---
const string MAGIC_CHECK = "LOCK"; // The hidden validation string

struct Node {
    unsigned char ch;
    long long freq;
    Node *left, *right;
    Node(unsigned char c, long long f) : ch(c), freq(f), left(nullptr), right(nullptr) {}
    Node(long long f, Node* l, Node* r) : ch('\0'), freq(f), left(l), right(r) {}
};

struct Compare {
    bool operator()(Node* a, Node* b) { return a->freq > b->freq; }
};

class BitWriter {
    unsigned char buffer;
    int bitCount;
    ofstream& out;
    string password; 
    int pwdIndex;

public:
    BitWriter(ofstream& outFile, string pwd = "") 
        : buffer(0), bitCount(0), out(outFile), password(pwd), pwdIndex(0) {}

    void writeBit(int bit) {
        buffer = (buffer << 1) | (bit & 1);
        bitCount++;
        if (bitCount == 8) { 
            if (!password.empty()) {
                buffer ^= password[pwdIndex % password.length()];
                pwdIndex++;
            }
            out.put(buffer); 
            buffer = 0; 
            bitCount = 0; 
        }
    }
    
    void flush() { 
        if (bitCount > 0) { 
            buffer <<= (8 - bitCount); 
            if (!password.empty()) {
                buffer ^= password[pwdIndex % password.length()];
            }
            out.put(buffer); 
        } 
    }
};

class BitReader {
    unsigned char buffer;
    int bitCount;
    ifstream& in;
    string password;
    int pwdIndex;

public:
    BitReader(ifstream& inFile, string pwd = "") 
        : buffer(0), bitCount(0), in(inFile), password(pwd), pwdIndex(0) {}

    int readBit() {
        if (bitCount == 0) { 
            if (!in.read(reinterpret_cast<char*>(&buffer), 1)) return -1; 
            if (!password.empty()) {
                buffer ^= password[pwdIndex % password.length()];
                pwdIndex++;
            }
            bitCount = 8; 
        }
        int bit = (buffer >> (bitCount - 1)) & 1;
        bitCount--;
        return bit;
    }
};
class Huffman {
public:
   // --- 1. COMPRESS FOLDER (DUAL-FORMAT) ---
    bool compressFolder(string folderPath, string outputArchiveName, string password = "") {
        if (!fs::exists(folderPath)) {
            cerr << "Error: Folder not found: " << folderPath << endl;
            return false;
        }

        ofstream archive(outputArchiveName, ios::binary);
        if (!archive.is_open()) return false;

        // 1. EXTENSION SNIFFING
        bool isZip = (outputArchiveName.length() >= 4 && 
                      outputArchiveName.substr(outputArchiveName.length() - 4) == ".zip");

        // Collect all files in the folder
        vector<fs::path> fileList;
        for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
            if (entry.is_regular_file()) fileList.push_back(entry.path());
        }
        int fileCount = fileList.size();
        cout << "Archiving " << fileCount << " files..." << endl;

        // ==========================================
        // PATH A: ORIGINAL .HUF BEHAVIOR
        // ==========================================
        if (!isZip) {
            archive.write(reinterpret_cast<const char*>(&fileCount), sizeof(int));
            for (const auto& filePath : fileList) {
                string relPath = fs::relative(filePath, folderPath).string();
                int pathLen = relPath.length();
                archive.write(reinterpret_cast<const char*>(&pathLen), sizeof(int));
                archive.write(relPath.c_str(), pathLen);

                cout << "Compressing: " << relPath << endl;
                if (!compressFileToStream(filePath.string(), archive, password)) return false;
            }
            return true;
        }

        // ==========================================
        // PATH B: STANDARD MULTI-FILE .ZIP BEHAVIOR
        // ==========================================
        
        // This struct remembers the stats for the Central Directory
        struct ZipFileInfo {
            string relativePath;
            uint32_t headerOffset;
            uint32_t payloadSize;
        };
        vector<ZipFileInfo> zipDirectory;

        // Step B1: Write Local Headers and Payloads
        for (const auto& filePath : fileList) {
            // ZIP standard uses forward slashes for paths, even on Windows
            string relPath = fs::relative(filePath, folderPath).generic_string();
            cout << "Zipping: " << relPath << endl;

            ZipLocalFileHeader localHeader;
            localHeader.fileNameLength = relPath.length();
            localHeader.compressionMethod = 0; // Stored payload

            uint32_t currentHeaderOffset = archive.tellp();
            archive.write(reinterpret_cast<const char*>(&localHeader), sizeof(ZipLocalFileHeader));
            archive.write(relPath.c_str(), relPath.length());

            uint32_t dataStartOffset = archive.tellp();
            
            // Inject the Huffman payload
            if (!compressFileToStream(filePath.string(), archive, password)) return false;

            uint32_t dataEndOffset = archive.tellp();
            uint32_t currentPayloadSize = dataEndOffset - dataStartOffset;

            // Fix the Local Header sizes
            archive.seekp(currentHeaderOffset);
            localHeader.compressedSize = currentPayloadSize;
            localHeader.uncompressedSize = currentPayloadSize;
            archive.write(reinterpret_cast<const char*>(&localHeader), sizeof(ZipLocalFileHeader));
            archive.seekp(dataEndOffset); // Jump back to the end

            // Save stats for the Central Directory
            zipDirectory.push_back({relPath, currentHeaderOffset, currentPayloadSize});
        }

        // Step B2: Write the Central Directory
        uint32_t centralDirOffset = archive.tellp();
        for (const auto& fileInfo : zipDirectory) {
            ZipCentralDirectoryFileHeader centralHeader;
            centralHeader.fileNameLength = fileInfo.relativePath.length();
            centralHeader.compressedSize = fileInfo.payloadSize;
            centralHeader.uncompressedSize = fileInfo.payloadSize;
            centralHeader.compressionMethod = 0;
            centralHeader.relativeOffsetOflocalHeader = fileInfo.headerOffset;

            archive.write(reinterpret_cast<const char*>(&centralHeader), sizeof(ZipCentralDirectoryFileHeader));
            archive.write(fileInfo.relativePath.c_str(), fileInfo.relativePath.length());
        }

        // Step B3: Write End of Central Directory (Footer)
        uint32_t eocdOffset = archive.tellp();
        ZipEndOfCentralDirectoryRecord eocd;
        eocd.numEntriesThisDisk = fileCount;
        eocd.numEntriesTotal = fileCount;
        eocd.centralDirectorySize = eocdOffset - centralDirOffset;
        eocd.centralDirectoryOffset = centralDirOffset;

        archive.write(reinterpret_cast<const char*>(&eocd), sizeof(ZipEndOfCentralDirectoryRecord));

        return true;
    }
// --- 2. DECOMPRESS FOLDER (DUAL-FORMAT) ---
    bool decompressFolder(string inputArchiveName, string outputRootFolder, string password = "") {
        if (!fs::exists(inputArchiveName)) {
            cerr << "Error: Archive file not found: " << inputArchiveName << endl;
            return false;
        }
        
        ifstream archive(inputArchiveName, ios::binary);
        if (!archive.is_open()) return false;

        // 1. EXTENSION SNIFFING
        bool isZip = (inputArchiveName.length() >= 4 && 
                      inputArchiveName.substr(inputArchiveName.length() - 4) == ".zip");

        // ==========================================
        // PATH A: ORIGINAL .HUF BEHAVIOR
        // ==========================================
        if (!isZip) {
            int fileCount;
            archive.read(reinterpret_cast<char*>(&fileCount), sizeof(int));
            cout << "Extracting " << fileCount << " files..." << endl;

            for (int i = 0; i < fileCount; i++) {
                int pathLen;
                archive.read(reinterpret_cast<char*>(&pathLen), sizeof(int));
                string relPath(pathLen, ' ');
                archive.read(&relPath[0], pathLen);

                fs::path fullOutputPath = fs::path(outputRootFolder) / relPath;
                fs::create_directories(fullOutputPath.parent_path());

                cout << "Extracting: " << relPath << endl;
                if (!decompressFileFromStream(archive, fullOutputPath.string(), password)) return false;
            }
            return true;
        }

        // ==========================================
        // PATH B: STANDARD MULTI-FILE .ZIP BEHAVIOR
        // ==========================================
        cout << "Extracting ZIP archive..." << endl;
        
        while (archive.peek() != EOF) {
            uint32_t signature;
            archive.read(reinterpret_cast<char*>(&signature), sizeof(uint32_t));

            // 1. Did we hit the Central Directory? (End of file data)
            if (signature == 0x02014b50) {
                break; // We successfully extracted all files!
            }
            
            // 2. Is it a valid Local File Header?
            if (signature != 0x04034b50) {
                cerr << "Error: Corrupted ZIP file or unsupported format." << endl;
                return false;
            }

            // 3. Read the rest of the 30-byte Local File Header (minus the 4-byte signature we just read)
            ZipLocalFileHeader header;
            header.signature = signature;
            archive.read(reinterpret_cast<char*>(&header.versionNeeded), sizeof(ZipLocalFileHeader) - sizeof(uint32_t));

            // 4. Read the filename
            string relPath(header.fileNameLength, ' ');
            archive.read(&relPath[0], header.fileNameLength);

            // Skip any extra fields (ZIP spec allows them, usually 0)
            if (header.extraFieldLength > 0) {
                archive.seekg(header.extraFieldLength, ios::cur);
            }

            // 5. Create output directories safely
            fs::path fullOutputPath = fs::path(outputRootFolder) / relPath;
            
            // (ZIPs sometimes contain empty directory entries. We skip decompressing those.)
            if (relPath.back() == '/' || relPath.back() == '\\') {
                fs::create_directories(fullOutputPath);
                continue; 
            }

            fs::create_directories(fullOutputPath.parent_path());

            cout << "Extracting: " << relPath << endl;
            
            // 6. Decode the Huffman payload exactly where it sits
            if (!decompressFileFromStream(archive, fullOutputPath.string(), password)) {
                cerr << "❌ Failed to extract: " << relPath << endl;
                return false;
            }
        }

        return true;
    }

    // --- 3. COMPRESS FILE (DUAL-FORMAT) ---
    bool compress(string inputFileName, string outputFileName, string password = "") {
        ofstream outFile(outputFileName, ios::binary);
        if (!outFile.is_open()) return false;

        // EXTENSION SNIFFING
        bool isZip = (outputFileName.length() >= 4 && 
                      outputFileName.substr(outputFileName.length() - 4) == ".zip");

        // ORIGINAL BEHAVIOR (Raw .huf file)
        if (!isZip) {
            return compressFileToStream(inputFileName, outFile, password);
        }

        // NEW BEHAVIOR (Standard .zip Envelope)
        ZipLocalFileHeader localHeader;
        string filenameOnly = fs::path(inputFileName).filename().string();
        
        localHeader.fileNameLength = filenameOnly.length();
        localHeader.compressionMethod = 0; // Container Mode

        uint32_t localHeaderOffset = outFile.tellp(); 
        outFile.write(reinterpret_cast<const char*>(&localHeader), sizeof(ZipLocalFileHeader));
        outFile.write(filenameOnly.c_str(), filenameOnly.length());

        uint32_t dataStartOffset = outFile.tellp();
        
        // Inject your original payload
        if (!compressFileToStream(inputFileName, outFile, password)) return false;

        uint32_t dataEndOffset = outFile.tellp();
        uint32_t payloadSize = dataEndOffset - dataStartOffset;

        // Fix the header sizes
        outFile.seekp(localHeaderOffset);
        localHeader.compressedSize = payloadSize;
        localHeader.uncompressedSize = payloadSize; 
        outFile.write(reinterpret_cast<const char*>(&localHeader), sizeof(ZipLocalFileHeader));
        outFile.seekp(dataEndOffset); 

        // Write Central Directory
        ZipCentralDirectoryFileHeader centralHeader;
        centralHeader.fileNameLength = filenameOnly.length();
        centralHeader.compressedSize = payloadSize;
        centralHeader.uncompressedSize = payloadSize;
        centralHeader.compressionMethod = 0;
        centralHeader.relativeOffsetOflocalHeader = localHeaderOffset;

        uint32_t centralDirOffset = outFile.tellp();
        outFile.write(reinterpret_cast<const char*>(&centralHeader), sizeof(ZipCentralDirectoryFileHeader));
        outFile.write(filenameOnly.c_str(), filenameOnly.length());

        // Write End of Central Directory
        uint32_t eocdOffset = outFile.tellp();
        ZipEndOfCentralDirectoryRecord eocd;
        eocd.centralDirectorySize = eocdOffset - centralDirOffset;
        eocd.centralDirectoryOffset = centralDirOffset;

        outFile.write(reinterpret_cast<const char*>(&eocd), sizeof(ZipEndOfCentralDirectoryRecord));

        return true;
    }

    // --- 4. DECOMPRESS FILE (DUAL-FORMAT) ---
    bool decompress(string inputFileName, string outputFileName, string password = "") {
        if (!fs::exists(inputFileName)) {
             cerr << "Error: Input file not found: " << inputFileName << endl;
             return false;
        }
        ifstream inFile(inputFileName, ios::binary);

        // EXTENSION SNIFFING
        bool isZip = (inputFileName.length() >= 4 && 
                      inputFileName.substr(inputFileName.length() - 4) == ".zip");
        
        // SKIP ZIP HEADERS IF NEEDED
        if (isZip) {
            ZipLocalFileHeader header;
            inFile.read(reinterpret_cast<char*>(&header), sizeof(ZipLocalFileHeader));
            inFile.seekg(header.fileNameLength, ios::cur);
        }

        // RUN ORIGINAL DECOMPRESSION
        return decompressFileFromStream(inFile, outputFileName, password);
    }

private:
    bool compressFileToStream(string inputFileName, ofstream& outStream, string password) {
        ifstream inputFile(inputFileName, ios::binary);
        if (!inputFile.is_open()) {
            cerr << "Error: Could not open file: " << inputFileName << endl;
            return false;
        }

        // 1. Write Password Check (Magic String)
        string check = MAGIC_CHECK;
        if (!password.empty()) {
            // Encrypt the check string
            for (int i = 0; i < check.length(); i++) {
                check[i] ^= password[i % password.length()];
            }
        }
        // Write the (possibly encrypted) check string
        outStream.write(check.c_str(), check.length());

        // 2. Standard Compression Logic
        vector<long long> frequencies = getFrequencies(inputFileName);
        inputFile.clear(); inputFile.seekg(0, ios::beg);

        long long totalChars = 0;
        for(auto f : frequencies) totalChars += f;

        Node* root = buildTree(frequencies);
        if (!root) return false;

        unordered_map<unsigned char, string> huffmanCode;
        buildMap(root, "", huffmanCode);

        outStream.write(reinterpret_cast<const char*>(&totalChars), sizeof(long long));
        for(int i=0; i<256; i++) {
            outStream.write(reinterpret_cast<const char*>(&frequencies[i]), sizeof(long long));
        }

        BitWriter bw(outStream, password);
        unsigned char byte;
        while(inputFile.read(reinterpret_cast<char*>(&byte), 1)) {
            for(char bit : huffmanCode[byte]) {
                bw.writeBit(bit == '1' ? 1 : 0);
            }
        }
        bw.flush();
        deleteTree(root);
        return true;
    }

    bool decompressFileFromStream(ifstream& inStream, string outputFileName, string password) {
        // 1. Read Password Check
        char checkBuffer[5] = {0}; // MAGIC_CHECK is 4 chars
        inStream.read(checkBuffer, 4);
        string check(checkBuffer);

        if (!password.empty()) {
            // Decrypt the check string
            for (int i = 0; i < check.length(); i++) {
                check[i] ^= password[i % password.length()];
            }
        }

        // 2. Validate Password
        if (check != MAGIC_CHECK) {
            cerr << "❌ Error: WRONG PASSWORD! Decryption aborted." << endl;
            return false;
        }

        // 3. Standard Decompression Logic
        long long totalChars;
        if(!inStream.read(reinterpret_cast<char*>(&totalChars), sizeof(long long))) return false;
        
        vector<long long> freq(256);
        for (int i = 0; i < 256; i++) inStream.read(reinterpret_cast<char*>(&freq[i]), sizeof(long long));

        Node* root = buildTree(freq);
        if (!root) return false;

        ofstream outFile(outputFileName, ios::binary);
        BitReader br(inStream, password);
        Node* curr = root;
        long long decoded = 0;
        
        while (decoded < totalChars) {
            int bit = br.readBit();
            if (bit == -1) break;
            curr = (bit == 0) ? curr->left : curr->right;
            if (!curr->left && !curr->right) {
                outFile.put(curr->ch);
                curr = root;
                decoded++;
            }
        }
        deleteTree(root);
        return true;
    }

    void deleteTree(Node* root) { if(!root) return; deleteTree(root->left); deleteTree(root->right); delete root; }
    vector<long long> getFrequencies(string filename) {
        vector<long long> freq(256, 0); ifstream file(filename, ios::binary); unsigned char byte;
        while (file.read(reinterpret_cast<char*>(&byte), 1)) freq[byte]++; return freq;
    }
    Node* buildTree(const vector<long long>& freq) {
        priority_queue<Node*, vector<Node*>, Compare> pq;
        for (int i = 0; i < 256; i++) if (freq[i] > 0) pq.push(new Node(i, freq[i]));
        if (pq.empty()) return nullptr;
        while (pq.size() > 1) {
            Node *l = pq.top(); pq.pop(); Node *r = pq.top(); pq.pop(); pq.push(new Node(l->freq + r->freq, l, r));
        }
        return pq.top();
    }
    void buildMap(Node* root, string code, unordered_map<unsigned char, string>& huffmanCode) {
        if (!root) return; if (!root->left && !root->right) huffmanCode[root->ch] = code;
        buildMap(root->left, code + "0", huffmanCode); buildMap(root->right, code + "1", huffmanCode);
    }
};

#endif