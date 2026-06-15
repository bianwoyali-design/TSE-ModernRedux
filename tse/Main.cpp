/*
 * Main.cpp -- the main function of Tiny Search Engine
 * Created: YAN Hongfei, Network lab of Peking University. <yhf@net.pku.edu.cn>
 * Created: July 15 2003. version 0.1.1
 *		# Can crawl web pages with a process
 * Updated: Aug 20 2003. version 1.0.0 !!!!
 *		# Can crawl web pages with multithreads
 * Updated: Nov 08 2003. version 1.0.1 !!!!
 *		# more classes in the codes
 */

#include "Tse.h"
#include "Crawl.h"
#include "Search.h"

void help();

int main(int argc, char* argv[])
{
	if( argc==2 && !strncmp(argv[1],"-s",2) ){	// search
		CSearch iSearch;
		iSearch.DoSearch();
	} else if( argc==3 && !strncmp(argv[1],"-c",2) ){ // crawl pages
		CCrawl iCrawl(argv[2], "visited.all");
		iCrawl.DoCrawl();
	} else {	// invalid argv
		help();
	}

	exit(0);
}

void help()
{
	cout << "Tse 1.0" << endl;
	cout << "Tiny Web Searching Engine, July 2003" << endl;
	cout << "Syntax:" << endl;
	cout << "\tTse -c inputFileName" << endl;
	cout << "\tTse -s" << endl;
	cout << "The input file contains a list of URLs for crawling.-c is to crawl all urls of inutFileName, and to save crawled links in 'visited.all' file. -s is a searching mode.In the extended mode,all the hypertext links in the webpages will be stored in the outputfile in the same format as the input file." << endl;

	exit(0);
}
