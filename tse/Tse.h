#ifndef _TSE_H_030719_
#define _TSE_H_030719_
#pragma once

#include <string>

using namespace std;

const string DATA_FILE_NAME("WebData.db");
const string INDEX_FILE_NAME("WebData.idx");

const string DATA_TIANWANG_FILE("Tianwang.raw");
const string DATA_LINK4SE_FILE("Link4SE.raw");

const string VISITED_FILE("visited.url");
const string UNVISITED_FILE("tse_unvisited.url");
const string UNREACH_HOST_FILE("tse_unreachHost.list");
const string LINK4SE_FILE("link4SE.url");
const string LINK4History_FILE("link4History.url");
const string URL_MD5_FILE("tse_md5.visitedurl");
const string PAGE_MD5_FILE("tse_md5.visitedpage");

const string IP_BLOCK_FILE("tse_ipblock");

constexpr unsigned int NUM_WORKERS      = 10;
//constexpr unsigned int NUM_WORKERS    = 1;
constexpr unsigned int NUM_WORKERS_ON_A_SITE = 4;

/*============== Standard C++ includes ==================*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <cctype>
#include <vector>
#include <iterator>
#include <list>
#include <deque>
#include <map>
#include <set>
#include <cassert>
#include <signal.h>

/*==========================================================================*/

#endif /* _TSE_H_030719_ */
