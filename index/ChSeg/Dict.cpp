// Dict handling — UTF-8 version

#include "Dict.h"

CDict::CDict()
{
    OpenDict();
}

CDict::~CDict()
{
    mapDict.clear();
}

void CDict::OpenDict()
{
    // Try UTF-8 dictionary first, then fall back to original
    const char* files[] = { "words_utf8.dict", "words.dict", nullptr };

    for (int i = 0; files[i]; i++) {
        ifstream ifs(files[i]);
        if (!ifs) continue;

        cout << "Loading dictionary: " << files[i] << endl;

        string line;
        while (getline(ifs, line)) {
            // Format: id word freq
            // Skip empty lines and the first field (id)
            size_t sp1 = line.find(' ');
            if (sp1 == string::npos) continue;
            size_t sp2 = line.find(' ', sp1 + 1);
            string word;
            if (sp2 != string::npos)
                word = line.substr(sp1 + 1, sp2 - sp1 - 1);
            else
                word = line.substr(sp1 + 1);

            if (!word.empty())
                mapDict.insert(make_pair(word, 0));
        }

        cout << "Loaded " << mapDict.size() << " words from " << files[i] << endl;
        return;
    }

    cerr << "Cannot open dictionary file!" << endl;
    exit(1);
}

bool CDict::IsWord(string& str) const
{
    return mapDict.find(str) != mapDict.end();
}
