#include <iostream>
#include <vector>
#include <fstream>

#include "init.h"

int main(int argc, char* argv[]) {
    std::string bufferName { argv[1] };
    std::fstream file;

    std::fstream::openmode mode = std::ios::in | std::ios::out;
    if (std::fstream(bufferName).good() == 0)
        mode |= std::ios::trunc;

    file.open(bufferName, mode);

    insat2(file, (std::string) "hello world", 1);
    delat(file, 1);

    file.write(buffer.data(), buffer.size());
    file.close();

    return 0;
}

/**
 *  insat2 inserts a string into any arbitrary position within a file
 *  This function is preferred over the older insat for purposes related to efficiency
 *  @param &file     File the substring is being inserted into
 *  @param insertion The string being inserted into the file
 *  @param position  The position from the beginning of the file where the string should be inserted
 */

void insat2(std::fstream &file, std::string insertion,
        std::streampos position) {
    std::streampos currpos = file.tellg();
    std::vector<char> buffer{ std::istreambuf_iterator<char>(file), 
        std::istreambuf_iterator<char>() };

    file.seekp(position);

    file.write(insertion.c_str(), insertion.size());
    file.write(buffer.data(), buffer.size());

    file.seekg(currpos);
}

/**
 *  insat inserts a string into any arbitrary position within a file (in an inefficient and verbose manner)
 *  @param &file     File the substring is being inserted into
 *  @param value     A typedef struct which contains a char* and the length of the char*
 *  @param position  The position from the beginning of the file where the char* should be inserted
 */

void insat(std::fstream &file, subbuf value,
        const int position) {
    file.seekg(0, std::ios::end);
    std::streampos eof = file.tellg();

    file.seekp(position, std::ios::beg);
    std::streampos pos = file.tellg();
    std::filebuf* buffer = file.rdbuf();

    std::vector<char> tail(eof-pos);
    file.read(tail.data(), eof-pos);
    file.clear();

    buffer->pubseekpos(pos);
    buffer->pubsync();

    file.write(value.value, value.size);
    file.write(tail.data(), tail.size());
}

/**
 *  delat deletes a character at any arbitrary position
 *  @param &file     File the character is being deleted from
 *  @param position  The position from the beginning of the file which should be deleted
 */

void delat(std::fstream &file, std::streampos position) {
    file.seekp(position);
    file.write("", 1);
}
