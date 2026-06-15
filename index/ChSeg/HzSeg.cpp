// HzSeg handling — UTF-8 version

#include "HzSeg.h"
#include "Dict.h"

const unsigned int MAX_WORD_LENGTH = 12; // max 4 UTF-8 CJK chars (3 bytes each)
const string SEPARATOR("/  ");           // delimiter between words

CHzSeg::CHzSeg()
{
}

CHzSeg::~CHzSeg()
{
}

// Helper: return byte length of UTF-8 character at given position
static int Utf8CharLen(unsigned char byte)
{
    if (byte < 0x80)       return 1;  // ASCII
    if (byte < 0xC0)       return 1;  // continuation byte (should not be at start)
    if (byte < 0xE0)       return 2;
    if (byte < 0xF0)       return 3;
    if (byte < 0xF8)       return 4;
    return 1;
}

// Helper: is this byte the start of a UTF-8 multi-byte character?
static bool IsUtf8Start(unsigned char byte)
{
    return (byte >= 0xC0 || byte < 0x80);
}

// Using Max Matching method to segment a character string (UTF-8).
string CHzSeg::SegmentHzStrMM (CDict &dict, string s1) const
{
    string s2;
    size_t pos = 0;

    while (pos < s1.size()) {
        size_t remaining = s1.size() - pos;
        size_t byte_len = 0;
        int char_count = 0;

        // Build candidate: up to MAX_WORD_LENGTH bytes
        for (size_t i = pos; i < s1.size() && byte_len < MAX_WORD_LENGTH; ) {
            int clen = Utf8CharLen((unsigned char)s1[i]);
            if (clen < 1) clen = 1;
            if (byte_len + clen > MAX_WORD_LENGTH) break;
            byte_len += clen;
            i += clen;
            char_count++;
        }

        if (byte_len == 0) { pos++; continue; }

        string w = s1.substr(pos, byte_len);
        bool isw = dict.IsWord(w);

        // Try shorter substrings by removing one character at a time
        while (byte_len > 0 && !isw) {
            // Remove the last UTF-8 character
            size_t cut = pos + byte_len;
            while (cut > pos) {
                cut--;
                if (IsUtf8Start((unsigned char)s1[cut])) {
                    byte_len = cut - pos;
                    break;
                }
            }
            if (byte_len == 0) break;

            w = s1.substr(pos, byte_len);
            isw = dict.IsWord(w);
        }

        if (byte_len == 0) {
            // Single character fallback
            int clen = Utf8CharLen((unsigned char)s1[pos]);
            if (clen < 1) clen = 1;
            w = s1.substr(pos, clen);
            byte_len = clen;
        }

        s2 += w + SEPARATOR;
        pos += byte_len;
    }

    return s2;
}


// Process a sentence before segmentation (UTF-8 version).
string CHzSeg::SegmentSentenceMM (CDict &dict, string s1) const
{
    string s2;
    unsigned int i, len;

    while (!s1.empty()) {
        unsigned char ch = (unsigned char)s1[0];

        if (ch < 128) {
            // ASCII: collect consecutive ASCII until a non-ASCII or newline
            i = 1;
            len = s1.size();
            while (i < len && ((unsigned char)s1[i] < 128)
                   && (s1[i] != 10) && (s1[i] != 13)) { // LF, CR
                i++;
            }

            if ((ch != 32) && (ch != 10) && (ch != 13)) { // SP, LF, CR
                s2 += s1.substr(0, i) + SEPARATOR;
            } else {
                if (ch == 10 || ch == 13) {
                    s2 += s1.substr(0, i);
                }
            }

            if (i <= s1.size())
                s1 = s1.substr(i);
            else break;

            continue;
        } else {
            // Multi-byte UTF-8: group consecutive multi-byte chars
            i = 0;
            len = s1.length();

            while (i < len && ((unsigned char)s1[i] >= 128)) {
                int clen = Utf8CharLen((unsigned char)s1[i]);
                if (clen < 2) clen = 1; // safety
                i += clen;
            }

            if (i == 0) i = 1; // safety: skip one byte

            // Check for double-byte CJK punctuation in UTF-8 (rare)
            // In UTF-8, CJK punctuation is 3 bytes, so just segment normally
            if (i > 0) {
                if (i <= s1.size())
                    s2 += SegmentHzStrMM(dict, s1.substr(0, i));
                else break;
            }

            if (i <= s1.size())
                s1 = s1.substr(i);
            else break;
        }
    }

    return s2;
}

// translate the encoded URL(%xx) to actual chars
void CHzSeg::Translate(char* SourceStr) const
{
    int i = 0;
    int j = 0;
    char *tempstr, tempchar1, tempchar2;

    tempstr = (char*)malloc(strlen(SourceStr) + 1);
    if (tempstr == NULL) {
        return;
    }

    while (SourceStr[j]) {
        if ((tempstr[i] = SourceStr[j]) == '%') {
            if (SourceStr[j+1] >= 'A')
                tempchar1 = ((SourceStr[j+1] & 0xdf) - 'A') + 10;
            else
                tempchar1 = (SourceStr[j+1] - '0');
            if (SourceStr[j+2] >= 'A')
                tempchar2 = ((SourceStr[j+2] & 0xdf) - 'A') + 10;
            else
                tempchar2 = (SourceStr[j+2] - '0');
            tempstr[i] = tempchar1 * 16 + tempchar2;
            j = j + 2;
        }
        i++;
        j++;
    }
    tempstr[i] = '\0';
    strcpy(SourceStr, tempstr);

    if (tempstr) free(tempstr);
}

/*
 * segment the image URL by '/'
 * omit the domain name
 */
string CHzSeg::SegmentURL(CDict &dict, string url) const
{
    string::size_type idx, nidx;
    char *curl = (char *)url.c_str();
    this->Translate(curl);
    url = curl;
    if ((idx = url.find("http://", 0)) != string::npos
        || (idx = url.find("https://", 0)) != string::npos)
    {
        if ((nidx = url.find("/", idx + 8)) != string::npos) {
            url = url.substr(nidx + 1);  // cut the part of sitename
        }
    }
    idx = 0;
    while ((idx = url.find("/", idx)) != string::npos) {
        url.replace(idx, 1, SEPARATOR);  // replace "/" with SEPARATOR "/  "
        idx += 3;
    }
    if ((idx = url.rfind(".")) != string::npos) {
        url = url.erase(idx);  // erase the file extension
    }

    url += "/  ";

    // segment the string whose length is greater than 8 (4 HZ_chars)
    idx = 0; nidx = 0;
    bool isover = false;
    string stmp;
    while (!isover) {
        if ((nidx = url.find(SEPARATOR, idx)) == string::npos)
            isover = true;
        if (nidx - idx > 0) {
            stmp = url.substr(idx, nidx - idx);
            stmp = SegmentSentenceMM(dict, stmp);
            if (stmp.size() >= 3)
                stmp.erase(stmp.length() - 3);  // erase the tail "/  "
            url = url.replace(idx, nidx - idx, stmp);
            idx += stmp.length() + 3;
        } else if (nidx == string::npos && idx < url.length()) {
            stmp = url.substr(idx);
            stmp = SegmentSentenceMM(dict, stmp);
            stmp.erase(stmp.length() - 3);
            url = url.substr(0, idx) + stmp;
        } else {
            idx = nidx + 3;
        }
    }

    return url;
}
