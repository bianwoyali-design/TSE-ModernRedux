#include <iostream>
#include <fstream>
#include <cstring>
#include "Md5.h"
#include "Url.h"
#include "Document.h"

using namespace std;

int main(int argc, char* argv[])
{
        ifstream ifs("Tianwang.raw.2559638448");
        if (!ifs) {
                cerr << "Cannot open " << "tianwang.img.info" << " for input\n";
                return -1;
        }

	ofstream ofsUrl("Url.idx", ios::in|ios::out|ios::trunc|ios::binary);
        if( !ofsUrl ){
                cout << "error open file " << endl;
        } 

	ofstream ofsDoc("Doc.idx", ios::in|ios::out|ios::trunc|ios::binary);
        if( !ofsDoc ){
                cout << "error open file " << endl;
        } 

	ofstream ofsDocId2Url("DocId2Url.idx", ios::in|ios::out|ios::trunc|ios::binary);
        if( !ofsDocId2Url ){
                cout << "error open file " << endl;
        } 

	int cnt=0;
	string strLine,strPage;
	CUrl iUrl;
	CDocument iDocument;
	CMD5 iMD5;

	int nOffset = ifs.tellg();
	while (getline(ifs, strLine)) {
		if (strLine[0]=='\0' || strLine[0]=='#' || strLine[0]=='\n'){
			nOffset = ifs.tellg();
			continue;
		}

		if (!strncmp(strLine.c_str(), "version: 1.0", 12)){	
			if(!getline(ifs, strLine)) break;

			if (!strncmp(strLine.c_str(), "url: ", 4)){
				iUrl.m_sUrl = strLine.substr(5);
				iMD5.GenerateMD5( (unsigned char*)iUrl.m_sUrl.c_str(), iUrl.m_sUrl.size() );
				iUrl.m_sChecksum = iMD5.ToString();

			} else {
				continue;
			}

			while (getline(ifs, strLine)) {
				if (!strncmp(strLine.c_str(), "length: ", 8)){
					sscanf(strLine.substr(8).c_str(), "%d", &(iDocument.m_nLength));
					break;
				}
			}

			getline(ifs, strLine);

			iDocument.m_nDocId = cnt;
			iDocument.m_nPos = nOffset;
			char *pContent = new char[iDocument.m_nLength+1];

			memset(pContent, 0, iDocument.m_nLength+1);
			ifs.read(pContent, iDocument.m_nLength);
			iMD5.GenerateMD5( (unsigned char*)pContent, iDocument.m_nLength );
			iDocument.m_sChecksum = iMD5.ToString();
			
			delete[] pContent;
			
			ofsUrl << iUrl.m_sChecksum ;
			ofsUrl << "\t" << iDocument.m_nDocId << endl;

			ofsDoc << iDocument.m_nDocId ;
			ofsDoc << "\t" << iDocument.m_nPos ;
			//ofsDoc << "\t" << iDocument.m_nLength ;
			ofsDoc << "\t" << iDocument.m_sChecksum << endl;

			ofsDocId2Url << iDocument.m_nDocId ;
			ofsDocId2Url << "\t" << iUrl.m_sUrl << endl;

			cnt++;
		}

		nOffset = ifs.tellg();

	}

	ofsDoc << cnt ; 
	ofsDoc << "\t" << nOffset << endl;


	return(0);
}
