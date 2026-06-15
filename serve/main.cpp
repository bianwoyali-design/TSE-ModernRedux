/**
 * TSE Server — Embedded HTTP search server
 *
 * Usage: ./tse-serve [--port PORT] [--data DATA_DIR] [--web WEB_DIR]
 */
#include <iostream>
#include <string>
#include "SimpleServer.hpp"
#include "SearchEngine.hpp"

using namespace tse;

static void PrintUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --port PORT     Listen on PORT (default: 8888)\n"
              << "  --data DIR      Index/data directory (default: ./Data)\n"
              << "  --web DIR       Web root directory (default: ./web)\n";
}

/**
 * Build JSON search response.
 * Minimal hand-rolled JSON to avoid external dependencies.
 */
static std::string EscapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c;
        }
    }
    return out;
}

int main(int argc, char* argv[]) {
    uint16_t port = 8888;
    std::string data_dir = "./Data";
    std::string web_dir = "./web";

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--data" && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (arg == "--web" && i + 1 < argc) {
            web_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "=== TSE Server ===\n";
    std::cout << "Data directory: " << data_dir << "\n";
    std::cout << "Web directory:  " << web_dir << "\n\n";

    // Load search engine
    SearchEngine engine;
    if (!engine.Load(data_dir)) {
        std::cerr << "WARNING: Failed to load index data. Server will start, "
                  << "but search may not work until you build an index.\n"
                  << "Run: ./tse crawl --seeds seeds.txt\n"
                  << "Then: ./tse-docindex && ./tse-docsegment ... && ./tse-iidx ...\n\n";
    }

    // Create HTTP server
    SimpleServer server(port, web_dir);

    // Search API
    server.AddRoute("/api/search", [&](const HttpRequest& req, HttpResponse& res) {
        std::string query;
        int page = 1;

        auto qit = req.params.find("q");
        if (qit != req.params.end()) query = qit->second;

        auto pit = req.params.find("p");
        if (pit != req.params.end()) {
            try { page = std::stoi(pit->second); } catch (...) { page = 1; }
        }
        if (page < 1) page = 1;

        auto result = engine.Search(query, page);

        // Build JSON response
        std::ostringstream json;
        json << "{\n";
        json << "  \"query\": \"" << EscapeJson(result.query) << "\",\n";
        json << "  \"total\": " << result.total << ",\n";
        json << "  \"time_ms\": " << result.time_ms << ",\n";
        json << "  \"terms\": [";
        for (size_t i = 0; i < result.terms.size(); i++) {
            if (i > 0) json << ", ";
            json << "\"" << EscapeJson(result.terms[i]) << "\"";
        }
        json << "],\n";
        json << "  \"results\": [\n";
        for (size_t i = 0; i < result.results.size(); i++) {
            if (i > 0) json << ",\n";
            const auto& r = result.results[i];
            json << "    {";
            json << "\"url\": \"" << EscapeJson(r.url) << "\", ";
            json << "\"title\": \"" << EscapeJson(r.title) << "\", ";
            json << "\"snippet\": \"" << EscapeJson(r.snippet) << "\", ";
            json << "\"size\": " << r.size;
            json << "}";
        }
        json << "\n  ]\n";
        json << "}\n";

        res.content_type = "application/json; charset=utf-8";
        res.body = json.str();
    });

    // Snapshot API
    server.AddRoute("/api/snapshot", [&](const HttpRequest& req, HttpResponse& res) {
        std::string url, word;
        auto uit = req.params.find("url");
        if (uit != req.params.end()) url = uit->second;
        auto wit = req.params.find("word");
        if (wit != req.params.end()) word = wit->second;

        res.content_type = "text/html; charset=utf-8";
        res.body = engine.GetSnapshot(url, word);
    });

    // Run server
    server.Run();

    return 0;
}
