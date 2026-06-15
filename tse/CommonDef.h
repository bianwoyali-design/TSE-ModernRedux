#ifndef _COMMON_DEF_H_030717_
#define _COMMON_DEF_H_030717_
#pragma once

#include <map>
#include <set>
#include <string>

using namespace std;

constexpr int PORT_NUMBER = 80;
#define HTTP_VERSION        "HTTP/1.1"
#define DEFAULT_USER_AGENT  "Tse"
#define VERSION             "1.0"
constexpr int DEFAULT_TIMEOUT = 30;
constexpr int REQUEST_BUF_SIZE = 1024;
constexpr int HEADER_BUF_SIZE = 1024;
constexpr int DEFAULT_PAGE_BUF_SIZE = 1024 * 200;
constexpr int MAX_PAGE_BUF_SIZE = 5 * 1024 * 1024;

/////////////////////////////
// collections (defined in Crawl.cpp)
extern map<string, string> mapCacheHostLookup;
extern map<unsigned long, unsigned long> mapIpBlock;
using valTypeIpBlock = map<unsigned long, unsigned long>::value_type;
extern set<string> setVisitedUrlMd5;
////////////////////////////

#endif /* _COMMON_DEF_H_030717_ */
