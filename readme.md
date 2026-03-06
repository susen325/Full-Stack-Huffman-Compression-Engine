================================================================================
HUFFMAN UNIVERSAL COMPRESSOR
================================================================================

[DESCRIPTION]
A high-performance C++ compression tool capable of archiving files, folders,
and binary streams. It supports universal file types (Text, PDF, Video, Images)
and includes optional password protection.

[COMPILATION]
This project requires a C++17 compatible compiler (GCC 14.2.0+ recommended).
Run the following command in your terminal to build the tool:

    g++ -std=c++17 main.cpp -o huffman

================================================================================
COMMAND GUIDE
================================================================================

1. SINGLE FILE OPERATIONS
   Best for compressing individual documents, videos, or images.

   [Compress a File]
   Usage: ./huffman -c <input_file> <output_file> [password]
   Example: ./huffman -c "video.mp4" "video.huf"

   [Decompress a File]
   Usage: ./huffman -d <archive_file> <output_file> [password]
   Example: ./huffman -d "video.huf" "restored.mp4"

---

2. FOLDER OPERATIONS (ARCHIVING)
   Compresses an entire directory into a single archive file.

   [Compress a Folder]
   Usage: ./huffman -cf <folder_path> <archive_file> [password]
   Example: ./huffman -cf "MyProject" "backup.huf"

   [Extract an Archive]
   Usage: ./huffman -df <archive_file> <output_folder> [password]
   Example: ./huffman -df "backup.huf" "RestoredProject"

================================================================================
PASSWORD PROTECTION
================================================================================

You can secure any file or folder by adding a password at the end of the command.
The file cannot be decompressed without the exact same password.

[Locking]
./huffman -c "secret.txt" "locked.huf" "mypassword123"

[Unlocking]
./huffman -d "locked.huf" "secret.txt" "mypassword123"

(Note: If you use a password to compress, you MUST use it to decompress!)

================================================================================
IMPORTANT NOTES
================================================================================

1. FILE PATHS WITH SPACES:
   Always use quotes if your filename has spaces.
   Correct: ./huffman -c "My Notes.txt" "notes.huf"
   Incorrect: ./huffman -c My Notes.txt notes.huf

2. COMPRESSION PERFORMANCE:
   - Text / Source Code / BMP: Expect 40-60% size reduction.
   - MP4 / JPG / ZIP / PDF: Expect 0% reduction (these are already compressed).
     The tool will still archive and encrypt them safely.

3. MATCHING COMMANDS:
   - Use '-d' only for files compressed with '-c'.
   - Use '-df' only for folders compressed with '-cf'.
