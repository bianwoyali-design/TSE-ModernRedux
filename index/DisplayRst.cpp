#include "Comm.h"
#include "DisplayRst.h"
#include "StrFun.h"


using namespace std;

CDisplayRst::CDisplayRst()
{
}

CDisplayRst::~CDisplayRst()
{
}
		
bool CDisplayRst::ShowMiddle(string strQuery, 
		float fUsedMsec, unsigned iRstNum, unsigned start)
{
	unsigned iPageNum = 0;
	if (iRstNum%RstPerPage == 0){
		iPageNum = iRstNum/RstPerPage;
	} else {
		iPageNum = iRstNum/RstPerPage + 1;
	}

	cout << "<title>TSE Search</title>\n"
		<< "<font  color=#008080 size=2>" << endl
		<< "查找: <b><font color=\"#000000\" size=\"2\">" 
		<< strQuery << "</b></font>" << endl
		<< "费时<b><font color=\"#000000\" size=\"2\">"
		<< fUsedMsec
		<< "</font></b> 毫秒,共找到<b><font color=\"#000000\" size=\"2\">"
		<< iRstNum
		<< "</font></b> 篇文档,下面是第 <b><font color=\"#000000\" size=\"2\">";

	if (iRstNum == 0){
		cout << "0</font></b>到第 <b><font color=\"#000000\" size=\"2\">"
			<< "0</font></b>个<br>" << endl;

		return true;
	}

	cout << (start-1)*RstPerPage + 1
		<< "</font></b>到第 <b><font color=\"#000000\" size=\"2\">";

	if (iRstNum >= start*RstPerPage ) {
		cout << start*RstPerPage << "</font></b>个&nbsp;&nbsp;&nbsp;&nbsp;" << endl;
	} else {
		cout << iRstNum << "</font></b>个&nbsp;&nbsp;&nbsp;&nbsp;" << endl;
	}

	cout << "选择页面: " ;

		
	for (unsigned i=0; i<iPageNum; i++) {
		if (i+1 == start ) {
			cout << i+1 << "</a>&nbsp;&nbsp;";
		}else{
			cout << "<a href=\"/yc-cgi-bin/index/TSESearch?word=" 
				<< strQuery << "&start=" << i+1 << "\">"
				<< i+1 << "</a>&nbsp;&nbsp;";
		}
	}


	return true;
}
	

bool CDisplayRst::ShowBelow(vector<string>&vecQuery, set<string> &setRelevantRst, 
		vector<DocIdx> &vecDocIdx, unsigned start)
{
	cout << "<ol>" << endl;

	set<string>::iterator it= setRelevantRst.begin();

	unsigned iDocNumber=0;


	unsigned iRstBegin = (start-1)*RstPerPage;
	unsigned iRstEnd = start*RstPerPage - 1;

	//cout << "<table width=100% align=center border=1 cellpadding=5 cellspacing=0 >";

	vector<string> vecRefUrl;
	vector<string>::iterator itVecRefUrl;
	cout << "<tr bgcolor=#e7eefc>";
	//cout << "<td valign=top align=center width=250><font size=-1>" ;
	bool bColor = true;

	ifstream ifs(RAWPAGE_FILE_NAME.c_str());
	if (!ifs) {
		cout << "Cannot open " << RAWPAGE_FILE_NAME << " for input\n";
		return false;
	}

	for ( ; it!=setRelevantRst.end(); ++it,iDocNumber++ ){

		if (iDocNumber < iRstBegin ) continue;
		if (iDocNumber > iRstEnd ) break;

		cout << "<li><font color=black size=2>" << endl ;
		int docId = atoi( (*it).c_str() );
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

		if (length > 400*1024) {    // if more than 400KB
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

		CDocument iDocument;
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

	return true;
}


bool CDisplayRst::ShowTop()
{
	string strHost = string(getenv("HTTP_HOST"));
cout << "<body bgcolor=#ffffff topmargin=2 marginheight=2>"
	<< "<table class=border=0 width=100% cellspacing=0 cellpadding=0 height=29>" << endl
	<< "<tr>" << endl
	<< "<td width=36% rowspan=2 height=1>" << "<a href=http://" << strHost << "/yc/TSE/><img border=0 src=/yc/TSE/tsetitle.JPG width=308 height=65></a></td>" << endl
	<< "<td width=64% height=33 ><font size=2><a href=http://" << strHost << "/yc/TSE/>搜索主页</a>| <a href=http://e.pku.edu.cn/gbhelp.htm>使用帮助</a> </font><br></td>" << endl
	<< "</tr>" << endl;

cout << "<tr>" << endl
	<< "<td><p align=\"left\">" << endl
	<< "<form method=\"get\" action=\"/yc-cgi-bin/index/TSESearch\" name=\"tw\">" << endl
	<< "<input type=\"text\" name=\"word\" size=\"55\">" << endl
	<< "<INPUT TYPE=\"submit\" VALUE=\" 新查询 \">&nbsp;" << endl
	<< "<input type=\"hidden\" name=\"start\" value=\"1\">" << endl
	<< "</form>" << endl
	<< "</tr>" << endl
	<< "</table>" << endl;

cout << "<table border=0 width=100% cellspacing=1 cellpadding=0 height=1>" << endl
	<< "<tr>" << endl
	<< "<td width=68 align=center bgcolor=#000066 valign=middle><font size=2><b><font color=#FFFFFF>图  片</font></b></font></td>"
	//<< "<td valign=bottom align=left width=943 height=18><font size=2 color=#808080></font></td>"
 	<< endl
	<< "</tr>" << endl
	<< "<tr>"
	<< "<td width=100% align=left colspan=3 height=0>"
    	//<< "<img border=0 src=/yc/TSE/line.gif width=100% height=1 align=top>"
	<< "</td></tr>" << endl
	<< "</table>" << endl;

	return true;

}
