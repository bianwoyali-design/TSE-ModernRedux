/**
 * SimpleServer.hpp — Minimal HTTP/1.0 server for TSE
 *
 * Uses POSIX sockets (available on macOS). Single-threaded, blocking.
 * Serves static files and dispatches API routes to handlers.
 */
#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <fstream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

namespace tse {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> params;
};

struct HttpResponse {
    int status = 200;
    std::string content_type = "text/html; charset=utf-8";
    std::string body;

    std::string ToString() const {
        std::ostringstream oss;
        oss << "HTTP/1.0 " << status << " " << StatusText() << "\r\n";
        oss << "Content-Type: " << content_type << "\r\n";
        oss << "Content-Length: " << body.size() << "\r\n";
        oss << "Connection: close\r\n";
        oss << "Access-Control-Allow-Origin: *\r\n";
        oss << "\r\n";
        oss << body;
        return oss.str();
    }

private:
    const char* StatusText() const {
        if (status == 200) return "OK";
        if (status == 404) return "Not Found";
        if (status == 400) return "Bad Request";
        if (status == 500) return "Internal Server Error";
        return "Unknown";
    }
};

using RouteHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

class SimpleServer {
public:
    SimpleServer(uint16_t port, const std::string& web_root)
        : port_(port), web_root_(web_root) {}

    void AddRoute(const std::string& prefix, RouteHandler handler) {
        routes_[prefix] = std::move(handler);
    }

    void Run() {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "Failed to create socket\n";
            return;
        }

        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Failed to bind to port " << port_ << "\n";
            close(sock);
            return;
        }

        if (listen(sock, 10) < 0) {
            std::cerr << "Failed to listen\n";
            close(sock);
            return;
        }

        std::cout << "\n  TSE Server listening on http://localhost:" << port_ << "/\n\n";

        while (true) {
            struct sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client = accept(sock, (struct sockaddr*)&client_addr, &client_len);
            if (client < 0) continue;

            HandleClient(client);
            close(client);
        }

        close(sock);
    }

private:
    uint16_t port_;
    std::string web_root_;
    std::map<std::string, RouteHandler> routes_;

    void HandleClient(int client) {
        char buf[8192];
        ssize_t n = recv(client, buf, sizeof(buf) - 1, 0);
        if (n <= 0) return;
        buf[n] = '\0';

        HttpRequest req = ParseRequest(buf, n);
        HttpResponse res;

        // Try API routes first (prefix match)
        bool handled = false;
        for (const auto& [prefix, handler] : routes_) {
            if (req.path.find(prefix) == 0) {
                handler(req, res);
                handled = true;
                break;
            }
        }

        // Fall through to static file serving
        if (!handled) {
            ServeStaticFile(req.path, res);
        }

        std::string response = res.ToString();
        send(client, response.c_str(), response.size(), 0);
    }

    HttpRequest ParseRequest(const char* raw, size_t len) {
        HttpRequest req;
        std::string data(raw, len);

        // Parse first line: GET /path?query HTTP/1.0
        size_t pos = data.find("\r\n");
        if (pos == std::string::npos) return req;
        std::string first_line = data.substr(0, pos);

        size_t sp1 = first_line.find(' ');
        size_t sp2 = first_line.rfind(' ');
        if (sp1 == std::string::npos || sp2 == std::string::npos) return req;

        req.method = first_line.substr(0, sp1);
        std::string full_path = first_line.substr(sp1 + 1, sp2 - sp1 - 1);

        // Split path and query string
        size_t qpos = full_path.find('?');
        if (qpos != std::string::npos) {
            req.path = full_path.substr(0, qpos);
            req.query_string = full_path.substr(qpos + 1);
            ParseQueryString(req.query_string, req.params);
        } else {
            req.path = full_path;
        }

        // URL decode path
        req.path = UrlDecode(req.path);

        return req;
    }

    void ParseQueryString(const std::string& qs,
                          std::map<std::string, std::string>& params) {
        size_t pos = 0;
        while (pos < qs.size()) {
            size_t eq = qs.find('=', pos);
            size_t amp = qs.find('&', pos);
            if (amp == std::string::npos) amp = qs.size();
            if (eq != std::string::npos && eq < amp) {
                std::string key = qs.substr(pos, eq - pos);
                std::string val = qs.substr(eq + 1, amp - eq - 1);
                params[UrlDecode(key)] = UrlDecode(val);
            }
            pos = amp + 1;
        }
    }

    static std::string UrlDecode(const std::string& src) {
        std::string result;
        for (size_t i = 0; i < src.size(); i++) {
            if (src[i] == '%' && i + 2 < src.size()) {
                int val;
                sscanf(src.substr(i + 1, 2).c_str(), "%x", &val);
                result += static_cast<char>(val);
                i += 2;
            } else if (src[i] == '+') {
                result += ' ';
            } else {
                result += src[i];
            }
        }
        return result;
    }

    void ServeStaticFile(const std::string& path, HttpResponse& res) {
        // Prevent directory traversal
        if (path.find("..") != std::string::npos) {
            res.status = 404;
            res.body = "Not Found";
            return;
        }

        std::string file_path = web_root_ + path;
        if (path == "/") file_path = web_root_ + "/index.html";

        struct stat st{};
        if (stat(file_path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
            res.status = 404;
            res.body = "Not Found";
            return;
        }

        std::ifstream ifs(file_path, std::ios::binary);
        if (!ifs) {
            res.status = 404;
            res.body = "Not Found";
            return;
        }

        std::ostringstream oss;
        oss << ifs.rdbuf();
        res.body = oss.str();

        // Set content type based on extension
        if (file_path.find(".css") != std::string::npos)
            res.content_type = "text/css; charset=utf-8";
        else if (file_path.find(".js") != std::string::npos)
            res.content_type = "application/javascript; charset=utf-8";
        else if (file_path.find(".html") != std::string::npos)
            res.content_type = "text/html; charset=utf-8";
        else if (file_path.find(".jpg") != std::string::npos)
            res.content_type = "image/jpeg";
        else if (file_path.find(".gif") != std::string::npos)
            res.content_type = "image/gif";
    }
};

} // namespace tse
