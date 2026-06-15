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

using namespace std;

int GetInputs();
int PrintResults();

CDict iDict;

map<string, string> mapIdxTerm;

CDocument iDocument;

vector<DocIdx> vecDocIdx;
bool LoadDocIdxFile();

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
	map<string, string> dictMap, mapBuckets, mapDocidUrl;

	CQuery iQuery;
	iQuery.GetInputs();
	// current query & result page number
	iQuery.SetQuery();
	iQuery.SetStart();

	// begin to search
	gettimeofday(&begin_tv,&tz); 

	iQuery.GetInvLists(mapBuckets); 
	iQuery.GetDocidUrl(mapDocidUrl); 
	
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

	iDisplayRst.ShowBelow(setRelevantRst,mapDocidUrl,iQuery.m_iStart); 

	return 0;

}

	unsigned int j;
	FILE *fp;
	time_t current_time;
	char tmpbuf[20];


	GetInputs();

	//fp=fopen("/yc/yc/TSE/tsesubmit.list","a");
	//if(fp==NULL){
		//printf("Content-type:text/html\n\nopen error.\n");
		//exit(1);
	//}


	// print result
	//PrintResults();

cout << "<table class=border=0 width=100% cellspacing=0 cellpadding=0 height=29>" << endl;
cout << "<tr>" << endl
	<< "<td width=36% rowspan=2 height=1><a href=http://162.105.80.60/yc/TSE/><img border=0 src=/yc/TSE/tsetitle.JPG width=308 height=65></a></td>" << endl
	<< "<td width=64% height=33 ><font size=2><a href=http://162.105.80.60/yc/TSE/>TSE主页</a>| <a href=http://e.pku.edu.cn/gbhelp.htm>使用帮助</a> </font><br></td>" << endl
	<< "</tr>" << endl;

cout << "<tr>" << endl
	<< "<td><p align=\"left\">" << endl
	<< "<form method=\"get\" action=\"/yc-cgi-bin/index/TSESearch\" name=\"tw\">" << endl
	<< "<input type=\"text\" name=\"word\" size=\"55\">" << endl
	<< "<INPUT TYPE=\"submit\" VALUE=\" 新查询 \">&nbsp;" << endl
	<< "<input type=\"hidden\" name=\"cdtype\" value=\"GB\">" << endl
	<< "</form>" << endl
	<< "</tr>" << endl;

cout << "</table>" << endl;

cout << "<table border=0 width=100% cellspacing=1 cellpadding=0 height=1>" << endl;
cout << "<tr>" << endl
	<< "<td width=68 align=center bgcolor=#000066 valign=middle><font size=2><b><font color=#FFFFFF>网 页</font></b></font></td>"
	<< "<td valign=bottom align=left width=943 height=18><font size=2 color=#808080></font></td>"
 	<< endl
	<< "</tr>" << endl
	<< "<tr>"
	<< "<td width=100% align=left colspan=3 height=0>"
    	<< "<img border=0 src=/yc/TSE/line.gif width=100% height=1 align=top>"
	<< "</td></tr>" << endl;

cout << "</table>" << endl;



	ifstream ifs("Tianwang.raw.2559638448");
        if (!ifs) {
                cerr << "Cannot open tianwang.img.info for input\n";
                return -1;
        }

	ifstream ifsInvInfo("sun.iidx");
	if (!ifsInvInfo) {
		cerr << "Cannot open " << "sun.iidx" << " for input\n";
		return -1;
	}

	// begin to search
	gettimeofday(&begin_tv,&tz);

	string strLine, strWord, strDocNum;
	//int cnt = 0;
	while (getline(ifsInvInfo, strLine)) {
		string::size_type idx;
		string tmp;


		idx = strLine.find("\t");
		strWord = strLine.substr(0,idx);
		strDocNum = strLine.substr(idx+1);

		mapIdxTerm.insert(map<string,string>::value_type (strWord, strDocNum));

	}

	LoadDocIdxFile();



	// cur query
	string query = HtmlInputs[0].Value;
	CHzSeg iHzSeg;
	query = iHzSeg.SegmentSentenceMM(iDict,query);
	//cout << query << endl;

	vector<string> vecQuery;
	string::size_type idx;
	while ( (idx = query.find("/  ")) != string::npos ) {
		vecQuery.push_back(query.substr(0,idx));
		query = query.substr(idx+3);
	}

//=====================
  set<string> setFResult,setSResult;

  bool bFirst=true;
  vector<string>::iterator itQuery = vecQuery.begin();
  for ( ; itQuery != vecQuery.end(); ++itQuery ){

	setSResult.clear();
	set<string>::iterator itFResult = setFResult.begin();
	for ( ; itFResult != setFResult.end(); ++itFResult ){
		string docid = (*itFResult);
		setSResult.insert(docid);
	}


	map<string,string>::iterator itIdxTerm = mapIdxTerm.find(*itQuery);

	map<string,int> mapRstDoc;
	string docid;
	int doccnt;


	if (itIdxTerm != mapIdxTerm.end()){
		string tmp = (*itIdxTerm).second;
		string::size_type idx;
		idx = tmp.find_first_not_of(" ");
		tmp = tmp.substr(idx);

		while ( (idx = tmp.find(" ")) != string::npos ) {
			docid = tmp.substr(0,idx);
			doccnt = 0;

			if (docid.empty()) continue;

			map<string,int>::iterator it = mapRstDoc.find(docid);
			if ( it != mapRstDoc.end() ){
				mapRstDoc.erase(it);
				doccnt = (*it).second + 1;
			}
			mapRstDoc.insert( pair<string,int>(docid,doccnt) );
				
		
			tmp = tmp.substr(idx+1);
		}

		docid = tmp;
		doccnt = 0;
		mapRstDoc.insert( pair<string,int>(docid,doccnt) );
	}



	multimap<int, string, greater<int> > newRstDoc;
	map<string,int>::iterator it0 = mapRstDoc.begin();
	for ( ; it0 != mapRstDoc.end(); ++it0 ){
		newRstDoc.insert( pair<int,string>((*it0).second,(*it0).first) );
	}

	multimap<int,string>::iterator itNewRstDoc = newRstDoc.begin();
	setFResult.clear();
	for ( ; itNewRstDoc != newRstDoc.end(); ++itNewRstDoc ){
		string docid = (*itNewRstDoc).second;

		if (bFirst==true) {
			setFResult.insert(docid);
			continue;
		}

		if ( setSResult.find(docid) != setSResult.end() ){	
			setFResult.insert(docid);
		}
	}

	//cout << "setFResult.size(): " << setFResult.size() << "<br>";
	bFirst = false;
  }

//=====================

	gettimeofday(&end_tv,&tz);
	float used_msec;
	used_msec = (end_tv.tv_sec-begin_tv.tv_sec)*1000+((float)(end_tv.tv_usec-begin_tv.tv_usec))/(float)1000;

	cout << "<title>TSE Search</title>\n";

	cout << "<font  color=\"#008080\" size=\"2\">" << endl;
	cout << "查找: <b><font color=\"#000000\" size=\"2\">" 
		<< HtmlInputs[0].Value << "</b></font>" << endl;
cout << "费时<b><font color=\"#000000\" size=\"2\">"
	<< used_msec 
	<< "</font></b> 毫秒,共找到<b><font color=\"#000000\" size=\"2\">"
	<< setFResult.size()
	<< "</font></b> 篇文档,下面是第 <b><font color=\"#000000\" size=\"2\">";

	if (setFResult.size()){
		cout << "1</font></b>篇到第 <b><font color=\"#000000\" size=\"2\">";
	}else{
		cout << "0</font></b>篇到第 <b><font color=\"#000000\" size=\"2\">";
	}

cout << setFResult.size()
	<< "</font></b>篇&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" << endl;
	


	cout << "<ol>" << endl;
	set<string>::iterator itFResult_F= setFResult.begin();

	for ( ; itFResult_F != setFResult.end(); ++itFResult_F ){
		cout << "<li><font color=black size=2>" << endl ;
		//cout << (*it1).first + 1 << "\t";	// term frequency
		//cout << (*it1).second << "\t";	// docId

		//int docId = atoi(((*it1).second).c_str());
		int docId = atoi( (*itFResult_F).c_str() );
		int length = vecDocIdx[docId+1].offset - vecDocIdx[docId].offset;


                char *pContent = new char[length+1];
                memset(pContent, 0, length+1);
                ifs.seekg(vecDocIdx[docId].offset);
                ifs.read(pContent, length);


                char *s;
                s = pContent;

		string url,tmp = pContent;
        	string::size_type idx1 = 0, idx2=0;	

		idx1 = tmp.find("url: ");
        	if( idx1 == string::npos ) continue;
		idx2 = tmp.find("\n", idx1);
        	if( idx1 == string::npos ) continue;
		url = tmp.substr(idx1+5, idx2 - idx1 - 5);

		string word;
		for(unsigned int i=0; i< vecQuery.size(); i++){
			word = word + "+" + vecQuery[i];
		}
		word = word.substr(1);

		cout << "<a href=" << url << ">" << url << "</a>,&nbsp;" 
			<< length << "<font  color=#008080>字节</font>" << ",&nbsp;" 
			<< "<a href=/yc-cgi-bin/index/Snapshot?"
			<< "word=" << word << "&"
			<< "url="<< url 
			<< " target=_blank>"
			<< "[网页快照]</a>" 
			<< endl << "<br>";

		if (length > 400*1024) { 	// if more than 400KB
                	delete[] pContent;
			continue;	
		}

                // skip HEAD 
                int bytesRead = 0,newlines = 0;
                while (newlines != 2 && bytesRead != HEADER_BUF_SIZE-1) {
                        if (*s == '\n')
                                newlines++;
                        else
                                newlines = 0;
                        s++;
                        bytesRead++;
                }
                if (bytesRead == HEADER_BUF_SIZE-1) continue;

                // skip header
                bytesRead = 0,newlines = 0;
                while (newlines != 2 && bytesRead != HEADER_BUF_SIZE-1) {
                        if (*s == '\n')
                                newlines++;
                        else
                                newlines = 0;
                        s++;
                        bytesRead++;
                }
                if (bytesRead == HEADER_BUF_SIZE-1) continue;

		//iDocument.m_sBody = s;
		iDocument.RemoveTags(s);
		iDocument.m_sBodyNoTags = s;

                delete[] pContent;
                string line = iDocument.m_sBodyNoTags;

		CStrFun::ReplaceStr(line, "&nbsp;", " ");
                CStrFun::EmptyStr(line); // set " \t\r\n" to " "


		// abstract
		string reserve;

		if ((unsigned char)line.at(48) < 0x80) {
		 	reserve = line.substr(0,48);
		}else{
			reserve = line.substr(0,48+1);
		}
		reserve = "[" + reserve + "]";

		unsigned int resNum = 128;
		if (vecQuery.size() == 1) resNum = 256;
	    	for(unsigned int i=0; i< vecQuery.size(); i++){
			string::size_type idx = 0, cur_idx;

        		idx = line.find(vecQuery[i],idx);
			if (idx == string::npos) continue;
			if (idx > resNum ) {
				cur_idx = idx - resNum;
				while ((unsigned char)line.at(cur_idx) > 0x80 && cur_idx!=idx) {
					cur_idx ++;
				}

				reserve += line.substr(cur_idx+1, resNum*2);
			}else{
				reserve += line.substr(idx, resNum*2);
			}

			reserve += "...";

			// highlight
			string newKey = "<font color=#e10900>" + vecQuery[i] + "</font>";
			CStrFun::ReplaceStr(reserve, vecQuery[i], newKey);
	    	}

		line = reserve;
		cout << line << endl << endl;
	}

	cout << "</ol>";
	cout << "<br><br><hr><br>";
        cout << "&copy 2004 北大网络实验室<br><br>\n";
        cout << "</center></body>\n<html>";
	return 0;

}

bool LoadDocIdxFile()
{

	ifstream ifsDoc("Doc.idx");
        if (!ifsDoc) {
                cerr << "Cannot open Doc.idx for input\n";
                return -1;
        }

	string strLine;
	while (getline(ifsDoc,strLine)){
		//int docid,offset;
		//char chksum[33];
		DocIdx di;

		//memset(chksum, 0, 33);
		//sscanf( strLine.c_str(), "%d%d%s", &docid, &pos, chksum );
		sscanf( strLine.c_str(), "%d%d", &di.docid, &di.offset );
		//iDocument.m_sChecksum = chksum;
		vecDocIdx.push_back(di);
        }


	return true;
}

/* 
 * Get form information throught environment varible.
 * return 0 if succeed, otherwise exit.
 */
int GetInputs()
{
        int i,j;
	char *mode = getenv("REQUEST_METHOD");
        char *tempstr;
	char *in_line;
	int length;

	printf("Content-type: text/html\n\n");
	printf("<html>\n");
	printf("<head>\n");
	//cout <<"<LINK href=\"style.css\" rel=stylesheet>" << endl;

	if (mode==NULL) return 1;

	if (strcmp(mode, "POST") == 0) {

		length = atoi(getenv("CONTENT_LENGTH"));
		if (length==0 || length>=256)
			return 1;
		in_line = (char*)malloc(length + 1);
		read(STDIN_FILENO, in_line, length);
		in_line[length]='\0';

	} else if (strcmp(mode, "GET") == 0) {
		char* inputstr = getenv("QUERY_STRING");
		length = strlen(inputstr);
		if (inputstr==0 || length>=256)
			return 1;
		in_line = (char*)malloc(length + 1);
		strcpy(in_line, inputstr);
	}


	tempstr = (char*)malloc(length + 1);
	if(tempstr == NULL){
		printf("<title>Error Occurred</title>\n");
		printf("</head><body>\n");
		printf("<p>Major failure #1;please notify the webmaster\n");
		printf("</p></body></html>\n");
		fflush(stdout);
		exit(2);
	}

        j=0;
        for (i=0; i<length; i++)
        {
                if (in_line[i] == '=')
                {
                        tempstr[j]='\0';
                        Translate(tempstr);
                        strcpy(HtmlInputs[HtmlInputCount].Name,tempstr);
                        if (i == length - 1)
                        {
                                strcpy(HtmlInputs[HtmlInputCount].Value,"");
                                HtmlInputCount++;
                        }
                        j=0;
                }
                else if ((in_line[i] == '&') || (i==length-1))
                {
                        if (i==length-1)
                        {
                                if(in_line[i] == '+')tempstr[j]=' ';
                                else tempstr[j] = in_line[i];
                                j++;
                        }
                        tempstr[j]='\0';
                        Translate(tempstr);
                        strcpy(HtmlInputs[HtmlInputCount].Value,tempstr);
                        HtmlInputCount++;
                        j=0;
                } else if (in_line[i] == '+') {
                        tempstr[j]=' ';
                        j++;
                } else {
                        tempstr[j]=in_line[i];
                        j++;
                }
        }

	if(in_line) free(in_line);
	if(tempstr) free(tempstr);

        return 0;
}

int PrintResults()
{
        printf("<title>提交成功</title>\n");
	printf("<meta http-equiv=\"refresh\" content=\"5;url=http://e.pku.edu.cn/\">\n");
        printf("<center>\n");
	printf("提交成功!<br><br>\n");
	printf("谢谢您的支持<br><br>\n");
	printf("注意: 收录过程不含人工干预，天网不保证收录您提交的网站。<br><br>\n");
	printf("本页 5 秒后自动转到 相关网页 ......<br><br>\n");
	printf("&copy 2003 北大网络实验室.<br><br>\n");
	printf("</center></body>\n<html>");

	return 0;
}
