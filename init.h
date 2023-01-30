#ifndef INIT_H
#define INIT_H

typedef struct {
    const char* value;
    const int size;
} subbuf;

void delat(std::fstream &file, std::streampos position);
void insat2(std::fstream &file, std::vector<char> buffer,
        std::string toInsert, std::streampos position);
void insat(std::fstream &file, subbuf value,
        const int position);

#endif
