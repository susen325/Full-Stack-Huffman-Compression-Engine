#include "huffman.hpp"
#include <iostream>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {
    // Basic Usage Check - Updated to show dual-format support!
    if (argc < 4) {
        cout << "Usage:\n";
        cout << "  Compress File:    ./huffman -c <input_file> <output.huf|.zip> [password]\n";
        cout << "  Decompress File:  ./huffman -d <input_archive> <output_file> [password]\n";
        cout << "  Compress Folder:  ./huffman -cf <input_folder> <output_archive> [password]\n";
        cout << "  Extract Folder:   ./huffman -df <input_archive> <output_folder> [password]\n";
        return 1;
    }

    string mode = argv[1];
    string input = argv[2];
    string output = argv[3];
    
    // Check for optional password
    string password = "";
    if (argc >= 5) {
        password = argv[4];
        cout << "🔒 Password Protection Enabled!" << endl;
    }

    Huffman huff;

    // --- Routing Logic ---
    if (mode == "-c") {
        cout << "Compressing file..." << endl;
        if (huff.compress(input, output, password)) {
            cout << "File archived successfully!" << endl;
        }
    } 
    else if (mode == "-d") {
        cout << "Decompressing file..." << endl;
        if (huff.decompress(input, output, password)) {
            cout << "File extracted successfully!" << endl;
        }
    }
    else if (mode == "-cf") {
        cout << "Compressing folder..." << endl;
        if (huff.compressFolder(input, output, password)) {
            cout << "Folder archived successfully!" << endl;
        }
    }
    else if (mode == "-df") {
        cout << "Extracting archive..." << endl;
        if (huff.decompressFolder(input, output, password)) {
            cout << "Folder extracted successfully!" << endl;
        }
    }
    else {
        cout << "❌ Invalid mode! Please use -c, -d, -cf, or -df." << endl;
        return 1;
    }

    return 0;
}