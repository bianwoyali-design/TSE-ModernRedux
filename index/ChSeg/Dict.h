#ifndef _DICT_H_040401_
#define _DICT_H_040401_

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <cstdlib>

using namespace std;

class CDict
{
public:
    CDict();
    ~CDict();

    bool GetFreq(string&) const { return false; };
    bool IsWord(string&) const;
    void AddFreq(string&) {};

private:
    unordered_map<string, int> mapDict;
    void OpenDict();
};

#endif /* _DICT_H_040401_ */
