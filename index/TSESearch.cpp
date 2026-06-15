#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <list>

#include "Comm.h"
#include "Query.h"
#include "Document.h"
#include "StrFun.h"
#include "ChSeg/Dict.h"
#include "ChSeg/HzSeg.h"
#include "DisplayRst.h"

using namespace std;

/*
 * A inverted file(INF) includes a term-index file & a inverted-lists file.
 * A inverted-lists consists of many bucks(posting lists).
 * The term-index file is stored at vecTerm, and
 * the inverted-lists is sored at mapBuckets.
 */
int main(int argc, char* argv[])
{
	struct timeval begin_tv, end_tv;
	struct timezone tz;

	CDict iDict;
	map<string, string> dictMap, mapBuckets;
	vector<DocIdx> vecDocIdx;

	CQuery iQuery;
	iQuery.GetInputs();
	// current query & result page number
	iQuery.SetQuery();
	iQuery.SetStart();

	// begin to search
	gettimeofday(&begin_tv,&tz); 

	iQuery.GetInvLists(mapBuckets); 
	iQuery.GetDocIdx(vecDocIdx); 
	
	CHzSeg iHzSeg; 
	iQuery.m_sSegQuery = iHzSeg.SegmentSentenceMM(iDict,iQuery.m_sQuery); 
	
	vector<string> vecTerm; 
	iQuery.ParseQuery(vecTerm); 
	
	set<string> setRelevantRst; 
	iQuery.GetRelevantRst(vecTerm, mapBuckets, setRelevantRst); 
	
	gettimeofday(&end_tv,&tz);
	// search end

	CDisplayRst iDisplayRst; 
	iDisplayRst.ShowTop(); 

	float used_msec = (end_tv.tv_sec-begin_tv.tv_sec)*1000 
		+((float)(end_tv.tv_usec-begin_tv.tv_usec))/(float)1000; 

	iDisplayRst.ShowMiddle(iQuery.m_sQuery,used_msec, 
			setRelevantRst.size(), iQuery.m_iStart);

	iDisplayRst.ShowBelow(vecTerm,setRelevantRst,vecDocIdx,iQuery.m_iStart); 

	return 0;

}
