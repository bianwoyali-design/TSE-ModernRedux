/**
 * SearchEngine.hpp — TSE query engine
 *
 * Loads inverted index, document index, and dictionary.
 * Handles query segmentation, inverted list lookup, merging, and ranking.
 */
#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <chrono>

namespace tse {

// ============================================================
// Document metadata
// ============================================================
struct DocInfo {
    int doc_id;
    long offset;
    std::string checksum;
};

// ============================================================
// Search result
// ============================================================
struct SearchResult {
    std::string url;
    std::string title;
    std::string snippet;
    long size;
};

// ============================================================
// UTF-8 helper
// ============================================================
inline int Utf8CharLen(unsigned char byte) {
    if (byte < 0x80) return 1;
    if (byte < 0xC0) return 1; // continuation byte
    if (byte < 0xE0) return 2;
    if (byte < 0xF0) return 3;
    if (byte < 0xF8) return 4;
    return 1;
}

// ============================================================
// Simple dictionary (loads words.dict)
// ============================================================
class Dictionary {
public:
    bool Load(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs) return false;
        std::string line;
        while (std::getline(ifs, line)) {
            // Format: id word freq
            size_t sp1 = line.find(' ');
            if (sp1 == std::string::npos) continue;
            size_t sp2 = line.find(' ', sp1 + 1);
            std::string word;
            if (sp2 != std::string::npos)
                word = line.substr(sp1 + 1, sp2 - sp1 - 1);
            else
                word = line.substr(sp1 + 1);
            if (!word.empty()) words_.insert(word);
        }
        return true;
    }

    bool Contains(const std::string& word) const {
        return words_.count(word) > 0;
    }

    size_t Size() const { return words_.size(); }

private:
    std::unordered_set<std::string> words_;
};

// ============================================================
// Chinese word segmentation (forward maximum matching)
// ============================================================
class Segmenter {
public:
    explicit Segmenter(const Dictionary& dict) : dict_(dict) {}

    std::vector<std::string> Segment(const std::string& text) {
        std::vector<std::string> result;
        size_t pos = 0;

        while (pos < text.size()) {
            // Skip whitespace
            if (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n') {
                pos++;
                continue;
            }

            unsigned char c = static_cast<unsigned char>(text[pos]);
            std::string token;

            if (c < 0x80) {
                // ASCII: collect consecutive ASCII chars
                size_t end = pos;
                while (end < text.size() &&
                       static_cast<unsigned char>(text[end]) < 0x80 &&
                       text[end] != ' ' && text[end] != '\t') {
                    end++;
                }
                token = text.substr(pos, end - pos);
                pos = end;
                // Convert to lowercase
                for (auto& ch : token) ch = static_cast<char>(std::tolower((unsigned char)ch));
                result.push_back(token);
            } else {
                // Multi-byte (CJK): try maximum matching
                size_t max_len = std::min(size_t(24), text.size() - pos); // max 8 CJK chars (3 bytes each)
                size_t best_len = 1;
                std::string best_word;

                // Try progressively shorter substrings
                for (size_t len = max_len; len >= 3; len -= 3) {
                    if (pos + len > text.size()) continue;
                    std::string candidate = text.substr(pos, len);
                    if (dict_.Contains(candidate)) {
                        best_len = len;
                        best_word = candidate;
                        break;
                    }
                }

                if (best_len > 1) {
                    result.push_back(best_word);
                    pos += best_len;
                } else {
                    // Single character
                    int clen = Utf8CharLen(c);
                    if (clen < 1) clen = 1;
                    result.push_back(text.substr(pos, clen));
                    pos += clen;
                }
            }
        }

        return result;
    }

private:
    const Dictionary& dict_;
};

// ============================================================
// Search Engine
// ============================================================
class SearchEngine {
public:
    SearchEngine() = default;

    /**
     * Load indices and dictionary from the data directory.
     * Expected files:
     *   data_dir/sun.iidx        — inverted index (term \t docId1 docId2 ...)
     *   data_dir/Doc.idx         — document index (DocId \t offset \t checksum)
     *   data_dir/Tianwang.raw.XX — raw page data (version/url/date/ip/length/raw)
     *   data_dir/words_utf8.dict — Chinese word dictionary (UTF-8)
     */
    bool Load(const std::string& data_dir) {
        data_dir_ = data_dir;

        if (!LoadDocIndex(data_dir + "/Doc.idx")) {
            std::cerr << "Warning: Could not load Doc.idx\n";
        }
        if (!LoadInvertedIndex(data_dir + "/sun.iidx")) {
            std::cerr << "Warning: Could not load sun.iidx\n";
        }
        if (!dict_.Load(data_dir + "/words_utf8.dict")) {
            // Try GB2312 version
            if (!dict_.Load(data_dir + "/words.dict")) {
                std::cerr << "Warning: Could not load dictionary\n";
            }
        }

        segmenter_ = std::make_unique<Segmenter>(dict_);

        // Auto-discover raw data files
        FindRawFiles();

        std::cout << "Loaded " << doc_infos_.size() << " documents, "
                  << inverted_index_.size() << " terms, "
                  << dict_.Size() << " dictionary words\n";
        return true;
    }

    struct QueryResult {
        std::string query;
        std::vector<std::string> terms;
        std::vector<SearchResult> results;
        int total = 0;
        double time_ms = 0;
    };

    QueryResult Search(const std::string& query, int page = 1, int per_page = 20) {
        QueryResult qr;
        qr.query = query;

        auto t1 = std::chrono::steady_clock::now();

        // Segment query
        if (segmenter_) {
            qr.terms = segmenter_->Segment(query);
        } else {
            qr.terms.push_back(query);
        }

        // Clean terms: filter out very short ones
        std::vector<std::string> clean_terms;
        for (const auto& t : qr.terms) {
            if (t.size() >= 2 || (t.size() == 1 && static_cast<unsigned char>(t[0]) < 0x80)) {
                clean_terms.push_back(t);
            }
        }
        qr.terms = clean_terms;

        if (qr.terms.empty()) {
            qr.total = 0;
            auto t2 = std::chrono::steady_clock::now();
            qr.time_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
            return qr;
        }

        // Get posting lists for all query terms
        std::vector<std::vector<int>> posting_lists;
        for (const auto& term : qr.terms) {
            auto it = inverted_index_.find(term);
            if (it != inverted_index_.end()) {
                posting_lists.push_back(it->second);
            } else {
                posting_lists.push_back({}); // empty list = no matches
            }
        }

        // Merge: intersect all posting lists (AND semantics)
        std::vector<int> merged = posting_lists.empty() ? std::vector<int>{} : posting_lists[0];
        for (size_t i = 1; i < posting_lists.size(); i++) {
            merged = Intersect(merged, posting_lists[i]);
        }

        // Compute term frequency scores within merged docs
        std::map<int, double> scores;
        for (int doc_id : merged) {
            double score = 0;
            for (const auto& pl : posting_lists) {
                if (std::binary_search(pl.begin(), pl.end(), doc_id)) {
                    score += 1.0; // Simple TF: count matching terms
                }
            }
            scores[doc_id] = score;
        }

        // Sort by score descending
        std::vector<std::pair<int, double>> ranked(scores.begin(), scores.end());
        std::sort(ranked.begin(), ranked.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        qr.total = static_cast<int>(ranked.size());

        // Paginate
        int start = (page - 1) * per_page;
        int end = std::min(start + per_page, static_cast<int>(ranked.size()));

        for (int i = start; i < end; i++) {
            int doc_id = ranked[i].first;
            qr.results.push_back(GetResult(doc_id));
        }

        auto t2 = std::chrono::steady_clock::now();
        qr.time_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

        return qr;
    }

    /**
     * Get page snapshot with keyword highlighting
     */
    std::string GetSnapshot(const std::string& url, const std::string& query) {
        // Read raw page content from raw files
        for (const auto& raw_file : raw_files_) {
            std::ifstream ifs(raw_file, std::ios::binary);
            if (!ifs) continue;

            std::string content((std::istreambuf_iterator<char>(ifs)),
                                std::istreambuf_iterator<char>());

            // Find the URL in raw content
            std::string url_prefix = "url: " + url;
            size_t pos = content.find(url_prefix);
            if (pos == std::string::npos) continue;

            // Skip past the raw record header: version, url, date, ip, length lines,
            // then HTTP response header, then blank line(s), then body
            size_t body_start = content.find("\n\n", pos);
            if (body_start == std::string::npos) continue;
            body_start += 2;
            size_t body = content.find("\n\n", body_start);
            if (body != std::string::npos) body += 2;
            else body = body_start;

            // Extract raw HTML and convert to clean text
            std::string html = content.substr(body);
            std::string clean = HtmlToPlainText(html);

            // Segment query and highlight terms
            std::vector<std::string> terms;
            if (segmenter_)
                terms = segmenter_->Segment(query);
            else
                terms.push_back(query);

            // Build highlighted HTML
            std::string result;
            result.reserve(clean.size() * 2);
            // Escape HTML entities in clean text
            for (size_t i = 0; i < clean.size(); ) {
                // Check if any query term starts here (case-insensitive)
                bool matched = false;
                for (const auto& term : terms) {
                    if (term.size() < 2) continue;
                    if (clean.size() - i < term.size()) continue;
                    bool same = true;
                    for (size_t j = 0; j < term.size(); j++) {
                        if (std::tolower((unsigned char)clean[i+j]) !=
                            std::tolower((unsigned char)term[j])) {
                            same = false; break;
                        }
                    }
                    if (same) {
                        // Check character boundary
                        result += "<mark>";
                        for (size_t j = 0; j < term.size(); j++)
                            result += clean[i+j];
                        result += "</mark>";
                        i += term.size();
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    char c = clean[i];
                    switch (c) {
                        case '&': result += "&amp;"; break;
                        case '<': result += "&lt;"; break;
                        case '>': result += "&gt;"; break;
                        case '"': result += "&quot;"; break;
                        default: result += c;
                    }
                    i++;
                }
            }

            return "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
                   "<title>Snapshot: " + url + "</title>"
                   "<style>body{font-family:-apple-system,sans-serif;max-width:800px;"
                   "margin:20px auto;padding:0 20px;line-height:1.8;color:#333}"
                   "mark{background:#ffeb3b;color:#000;padding:0 2px}"
                   "h2{font-size:18px;color:#666;margin-bottom:16px}"
                   ".url-bar{color:#006621;font-size:14px;margin-bottom:20px;"
                   "word-break:break-all;border-bottom:1px solid #eee;padding-bottom:12px}"
                   "</style></head><body>"
                   "<h2>Cached Snapshot</h2>"
                   "<div class='url-bar'>" + url + "</div>"
                   "<div>" + result + "</div></body></html>";
        }

        return "<html><body><h2>Snapshot not available</h2></body></html>";
    }

    // Convert HTML to clean plain text (strip tags, scripts, styles)
    static std::string HtmlToPlainText(const std::string& html) {
        std::string text;
        bool in_tag = false, in_style = false, in_script = false;
        for (size_t i = 0; i < html.size(); i++) {
            char c = html[i];
            if (c == '<') {
                in_tag = true;
                if (i + 8 <= html.size()) {
                    std::string t = html.substr(i, 8);
                    auto tolower = [](unsigned char ch) { return std::tolower(ch); };
                    std::transform(t.begin(), t.end(), t.begin(), tolower);
                    if (t.find("<style") == 0) in_style = true;
                    if (t.find("<script") == 0) in_script = true;
                    if (t.find("</style") == 0) { in_style = false; in_tag = false; i += 7; continue; }
                    if (t.find("</script") == 0) { in_script = false; in_tag = false; i += 8; continue; }
                }
                continue;
            }
            if (in_tag) {
                if (c == '>') in_tag = false;
                continue;
            }
            if (!in_tag && !in_style && !in_script) {
                // Decode common HTML entities
                if (c == '&') {
                    if (html.compare(i, 5, "&amp;") == 0)  { text += '&';  i += 4; continue; }
                    if (html.compare(i, 4, "&lt;") == 0)   { text += '<';  i += 3; continue; }
                    if (html.compare(i, 4, "&gt;") == 0)   { text += '>';  i += 3; continue; }
                    if (html.compare(i, 6, "&quot;") == 0) { text += '"';  i += 5; continue; }
                    if (html.compare(i, 6, "&nbsp;") == 0) { text += ' ';  i += 5; continue; }
                    if (html.compare(i, 5, "&#39;") == 0)  { text += '\''; i += 4; continue; }
                }
                text += c;
            }
        }

        // Collapse whitespace
        std::string clean;
        bool last_was_space = false;
        for (char c : text) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                if (!last_was_space) { clean += ' '; last_was_space = true; }
            } else {
                clean += c;
                last_was_space = false;
            }
        }
        // Trim
        size_t s = clean.find_first_not_of(' ');
        if (s == std::string::npos) return "";
        return clean.substr(s);
    }

private:
    std::string data_dir_;
    std::vector<DocInfo> doc_infos_;
    std::unordered_map<std::string, std::vector<int>> inverted_index_;
    Dictionary dict_;
    std::unique_ptr<Segmenter> segmenter_;
    std::vector<std::string> raw_files_;

    bool LoadDocIndex(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs) return false;

        std::string line;
        while (std::getline(ifs, line)) {
            DocInfo di{};
            std::istringstream iss(line);
            if (iss >> di.doc_id >> di.offset >> di.checksum) {
                doc_infos_.push_back(di);
            }
        }
        return !doc_infos_.empty();
    }

    bool LoadInvertedIndex(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs) return false;

        std::string line;
        while (std::getline(ifs, line)) {
            size_t tab = line.find('\t');
            if (tab == std::string::npos) continue;

            std::string term = line.substr(0, tab);
            std::string posting_str = line.substr(tab + 1);

            std::vector<int> postings;
            std::istringstream pss(posting_str);
            int doc_id;
            while (pss >> doc_id) {
                postings.push_back(doc_id);
            }

            // Remove very short terms (noise) - same heuristic as original
            if (term.size() >= 2) {
                inverted_index_[term] = std::move(postings);
            }
        }
        return !inverted_index_.empty();
    }

    void FindRawFiles() {
        // Look for Tianwang.raw.* files in data directory
        // Simple approach: check common names
        std::vector<std::string> candidates = {
            data_dir_ + "/Tianwang.raw.2559638448",
            data_dir_ + "/Tianwang.raw",
        };

        for (const auto& path : candidates) {
            std::ifstream ifs(path);
            if (ifs) {
                raw_files_.push_back(path);
            }
        }

        // Also scan for any Tianwang.raw.* files
        // (skip full directory scan for simplicity)
    }

    std::vector<int> Intersect(const std::vector<int>& a, const std::vector<int>& b) {
        if (b.empty()) return {};
        std::vector<int> result;
        size_t i = 0, j = 0;
        while (i < a.size() && j < b.size()) {
            if (a[i] < b[j]) i++;
            else if (b[j] < a[i]) j++;
            else { result.push_back(a[i]); i++; j++; }
        }
        return result;
    }

    SearchResult GetResult(int doc_id) {
        SearchResult sr;

        // Look up offset in Doc.idx
        long offset = -1;
        for (const auto& di : doc_infos_) {
            if (di.doc_id == doc_id) {
                offset = di.offset;
                break;
            }
        }

        if (offset >= 0) {
            // Read from raw files
            for (const auto& raw_file : raw_files_) {
                std::ifstream ifs(raw_file, std::ios::binary);
                if (!ifs) continue;

                ifs.seekg(offset);
                std::string record;
                // Read until next record or EOF (heuristic: ~100KB per record)
                char buf[102400];
                ifs.read(buf, sizeof(buf));
                size_t n = ifs.gcount();
                record.assign(buf, n);

                // Parse: version/url/date/ip/length/headers/content
                auto get_line = [](const std::string& s, const std::string& key) -> std::string {
                    size_t p = s.find(key);
                    if (p == std::string::npos) return "";
                    p += key.size();
                    size_t end = s.find('\n', p);
                    return s.substr(p, end - p);
                };

                std::string url = get_line(record, "url: ");
                if (url.empty()) continue;

                // Strip version line
                size_t body_start = record.find("\n\n");
                if (body_start == std::string::npos) continue;
                body_start += 2;
                size_t body = record.find("\n\n", body_start);
                if (body != std::string::npos) body += 2;
                else body = body_start;

                std::string content = record.substr(body);

                sr.url = url;
                sr.title = ExtractTitle(content);
                sr.snippet = GenerateSnippet(content, sr.title);
                sr.size = static_cast<long>(content.size());
                break;
            }
        }

        if (sr.url.empty()) {
            sr.url = "(document " + std::to_string(doc_id) + ")";
            sr.title = "(unknown)";
            sr.snippet = "";
        }

        return sr;
    }

    std::string ExtractTitle(const std::string& html) {
        // Try <title>...</title>
        size_t ts = html.find("<title");
        if (ts == std::string::npos) ts = html.find("<TITLE");
        if (ts != std::string::npos) {
            ts = html.find('>', ts);
            if (ts != std::string::npos) {
                ts++;
                size_t te = html.find("</title>", ts);
                if (te == std::string::npos) te = html.find("</TITLE>", ts);
                if (te != std::string::npos) {
                    std::string title = html.substr(ts, te - ts);
                    // Strip HTML tags and entities
                    return StripHtml(title);
                }
            }
        }
        return "(no title)";
    }

    // Remove HTML tags and entities, return clean text
    static std::string StripHtml(const std::string& input) {
        std::string result;
        bool in_tag = false;
        for (size_t i = 0; i < input.size(); i++) {
            if (input[i] == '<') {
                in_tag = true;
                continue;
            }
            if (in_tag) {
                if (input[i] == '>') in_tag = false;
                continue;
            }
            // Decode common HTML entities
            if (input[i] == '&') {
                if (input.compare(i, 5, "&amp;") == 0) { result += '&'; i += 4; continue; }
                if (input.compare(i, 4, "&lt;") == 0)  { result += '<'; i += 3; continue; }
                if (input.compare(i, 4, "&gt;") == 0)  { result += '>'; i += 3; continue; }
                if (input.compare(i, 6, "&quot;") == 0) { result += '"'; i += 5; continue; }
                if (input.compare(i, 6, "&nbsp;") == 0) { result += ' '; i += 5; continue; }
            }
            result += input[i];
        }
        // Trim
        size_t s = result.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) return "";
        size_t e = result.find_last_not_of(" \t\r\n");
        return result.substr(s, e - s + 1);
    }

    std::string GenerateSnippet(const std::string& html, const std::string& title) {
        // Strip all HTML: tags, <style> blocks, <script> blocks
        std::string text;
        bool in_tag = false, in_style = false, in_script = false;
        for (size_t i = 0; i < html.size(); i++) {
            char c = html[i];
            if (c == '<') {
                in_tag = true;
                if (i + 8 <= html.size()) {
                    std::string t = html.substr(i, 8);
                    auto tolower = [](unsigned char ch) { return std::tolower(ch); };
                    std::transform(t.begin(), t.end(), t.begin(), tolower);
                    if (t.find("<style") == 0) in_style = true;
                    if (t.find("<script") == 0) in_script = true;
                    if (t.find("</style") == 0) { in_style = false; in_tag = false; i += 7; continue; }
                    if (t.find("</script") == 0) { in_script = false; in_tag = false; i += 8; continue; }
                }
                continue;
            }
            if (in_tag) {
                if (c == '>') in_tag = false;
                continue;
            }
            if (!in_tag && !in_style && !in_script) {
                text += c;
            }
        }

        // Collapse whitespace
        std::string clean;
        bool last_was_space = false;
        for (char c : text) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
                if (!last_was_space) { clean += ' '; last_was_space = true; }
            } else {
                clean += c;
                last_was_space = false;
            }
        }
        // Trim and limit
        size_t s = clean.find_first_not_of(' ');
        if (s == std::string::npos) return "";
        clean = clean.substr(s);
        if (clean.size() > 200) clean = clean.substr(0, 200);
        return clean;
    }
};

} // namespace tse
