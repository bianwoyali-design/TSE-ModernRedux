/* URL handling
 */

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netdb.h>

#include "Tse.h"
#include "Url.h"
#include "Http.h"
#include "Md5.h"
#include "StrFun.h"

/* Is X "."?  */
#define DOTP(x) ((*(x) == '.') && (!*(x + 1)))
/* Is X ".."?  */
#define DDOTP(x) ((*(x) == '.') && (*(x + 1) == '.') && (!*(x + 2)))

map<string,string> mapCacheHostLookup;
extern vector<string> vsUnreachHost;
pthread_mutex_t mutexCacheHost = PTHREAD_MUTEX_INITIALIZER;
extern set<string> setVisitedUrlMD5;
extern map<unsigned long,unsigned long> mapIpBlock;
typedef map<string,string>::value_type valTypeCHL;

struct scheme_data
{
	const char *leading_string;
	int default_port;
	int enabled;
};

/* Supported schemes: */
static struct scheme_data supported_schemes[] =
{
	{ "http://",  DEFAULT_HTTP_PORT,  1 },
	{ "https://", DEFAULT_HTTPS_PORT, 1 },
	{ "ftp://",   DEFAULT_FTP_PORT,   1 },

	/* SCHEME_INVALID */
	{ NULL,       -1,                 0 }
};

/* Returns the scheme type if the scheme is supported, or
   SCHEME_INVALID if not.  */
void CUrl::ParseScheme (const char *url)
{
	int i;

	for (i = 0; supported_schemes[i].leading_string; i++)

		if (0 == strncasecmp (url, supported_schemes[i].leading_string,
                          strlen (supported_schemes[i].leading_string))) {

			if (supported_schemes[i].enabled){
				this->m_eScheme = (enum url_scheme) i;
				return;
			}else{
				this->m_eScheme = SCHEME_INVALID;
				return;
			}
		}

	this->m_eScheme = SCHEME_INVALID;
	return;
}

/************************************************************************
 *  Function name: ParseUrlEx
 *  Input argv:
 *  	-- strUrl: url
 *  Output argv:
 *  	--
 *  Return:
   	true: success
   	false: fail
 *  Fucntion Description: break an URL into scheme, host, port and request.
 *  			result as member variants
 *  Be careful:	release the memory by the client
************************************************************************/
bool CUrl::ParseUrlEx(string strUrl)
{
	char protocol[10];
	char host[HOST_LEN];
	char request[256];
	int port = -1;

	memset( protocol, 0, sizeof(protocol) );
	memset( host, 0, sizeof(host) );
	memset( request, 0, sizeof(request) );

	this->ParseScheme(strUrl.c_str());
	if( this->m_eScheme != SCHEME_HTTP && this->m_eScheme != SCHEME_HTTPS ){
		return false;
	}

	ParseUrlEx(strUrl.c_str(),
			protocol, sizeof(protocol),
			host, sizeof(host),
			request, sizeof(request),
			&port);

	m_sUrl  = strUrl;
	m_sHost = host;
	m_sPath = request;

	if( m_eScheme == SCHEME_HTTPS && port == DEFAULT_HTTP_PORT ){
		m_nPort = DEFAULT_HTTPS_PORT;
	} else if( port > 0 ){
		m_nPort = port;
	}

	return true;
}

/************************************************************************
 *  Function name: ParseUrlEx
 *  Input argv:
 *  	-- url: host name
 *  	-- protocol: result protocol
 *  	-- lprotocol: protocol length
 *  	-- host: result host
 *  	-- lhost: host length
 *  	-- request: result request
 *  	-- lrequest: request length
 *  Output argv:
 *  	--
 *  Return:
   	true: success
   	false: fail
 *  Fucntion Description: break an URL into scheme, host, port and request.
 *  			result as argvs
 *  Be careful:
************************************************************************/
void CUrl::ParseUrlEx(const char *url,
		char *protocol, int lprotocol,
		char *host, int lhost,
		char *request, int lrequest,
		int *port)
{
	char *work,*ptr,*ptr2;

	*protocol = *host = *request = 0;
	*port = 80;

	int len = strlen(url);
	//pthread_mutex_lock(&mutexMemory);
	work = new char[len + 1];
	//pthread_mutex_unlock(&mutexMemory);
	memset(work, 0, len+1);
	strncpy(work, url, len);

	// find protocol if any
	ptr = strchr(work, ':');
	if( ptr != NULL ){
		*(ptr++) = 0;
		strncpy( protocol, work, lprotocol );
	} else {
		strncpy( protocol, "HTTP", lprotocol );
		ptr = work;
	}

	// skip past opening /'s
	if( (*ptr=='/') && (*(ptr+1)=='/') )
		ptr+=2;

	// find host
	ptr2 = ptr;
	while( IsValidHostChar(*ptr2) && *ptr2 )
		ptr2++;
	*ptr2 = 0;
	strncpy( host, ptr, lhost );

	// find the request
	int offset = ptr2 - work;
	const char *pStr = url + offset;
	strncpy( request, pStr, lrequest );

	// find the port number, if any
	ptr = strchr( host, ':' );
	if( ptr != NULL ){
		*ptr = 0;
		*port = atoi(ptr+1);
	}

	//pthread_mutex_lock(&mutexMemory);
	delete [] work;
	//pthread_mutex_unlock(&mutexMemory);
	work = NULL;
}


/* scheme://user:pass@host[:port]... 
 *                    ^              
 * We attempt to break down the URL into the components path,
 * params, query, and fragment.  They are ordered like this:
 * scheme://host[:port][/path][;params][?query][#fragment] 
 */

/*
bool CUrl::ParseUrl(string strUrl)
{
	string::size_type idx;

	this->ParseScheme(strUrl.c_str());	
	if( this->m_eScheme != SCHEME_HTTP )
		return false;

	// get host name
	this->m_sHost = strUrl.substr(7);
	idx = m_sHost.find('/');
	if(idx != string::npos){
		m_sHost = m_sHost.substr(0,idx);
	}

	this->m_sUrl = strUrl;

	return true;
}
*/

CUrl::CUrl()
{
	this->m_sUrl = ""; 
	this->m_eScheme= SCHEME_INVALID;
        
	this->m_sHost = "";  
	this->m_nPort = DEFAULT_HTTP_PORT; 
        
	this->m_sPath = "";
	/*
	this->m_sParams = "";
	this->m_sQuery = "";
	this->m_sFragment = "";

	this->m_sDir = "";
	this->m_sFile = "";
        
        this->m_sUser = "";
	this->m_sPasswd = "";
	*/

}

CUrl::~CUrl()
{

}


/****************************************************************************
 *  Function name: GetIpByHost
 *  Input argv:
 *  	-- host: host name
 *  Output argv:
 *  	--
 *  Return:
   	ip: sucess
   	NULL: fail
 *  Function Description: get the ip address by host name
 *  Be careful: release the memory by the client
****************************************************************************/
char * CUrl::GetIpByHost(const char *host)
{
	if( !host ){	// null pointer
		return NULL;
	}

	if( !IsValidHost(host) ){	// invalid host
		return NULL;
	}

	unsigned long inaddr = 0;
	char *result = NULL;
	int len = 0;


	inaddr = (unsigned long)inet_addr( host );
	//if ( (int)inaddr != -1){ 
	if ( inaddr != INADDR_NONE){ // host is just ip
		len = strlen(host);
		//pthread_mutex_lock(&mutexMemory);
		result = new char[len+1];
		//pthread_mutex_unlock(&mutexMemory);
		memset(result, 0, len+1);
		memcpy(result, host, len);

		return result;

        } else {
		//firt find from cache
		
		map<string,string>::iterator it  = mapCacheHostLookup.find(host);

		if( it != mapCacheHostLookup.end() ){	// find in host lookup cache
			const char * strHostIp;

			strHostIp = (*it).second.c_str();

			inaddr = (unsigned long)inet_addr( strHostIp );
			//if ( (int)inaddr != -1){ 
			if ( inaddr != INADDR_NONE ){ 
				len = strlen(strHostIp);
				//pthread_mutex_lock(&mutexMemory);
				result = new char[len+1];
				//pthread_mutex_unlock(&mutexMemory);
				memset( result, 0, len+1 );
				memcpy( result, strHostIp, len );

				//cout << ":)" ;
				
				return result;
        		}
		}
	}

	// if still not find, then try by DNS server
	struct hostent *hp;	/* Host entity */
	hp = gethostbyname(host); // deprecated but kept for legacy compat
	if(hp == NULL) { 
		//cout << "gethostbyname() error in GetIpByHost: " << host << endl;
		return NULL;
	}

	// cache host lookup
        struct  in_addr in;

	bcopy(*(hp->h_addr_list), (caddr_t)&in, hp->h_length);
		
	char    abuf[INET_ADDRSTRLEN];
        if( inet_ntop(AF_INET, (void *)&in,abuf, sizeof(abuf)) == NULL ){
		cout << "inet_ntop() return error in GetIpByHost" << endl;
		return NULL;

	} else {

		pthread_mutex_lock(&mutexCacheHost);
		//if( mapCacheHostLookup.count(host) == 0){
		if( mapCacheHostLookup.find(host) == mapCacheHostLookup.end() ){
		
			//cout << endl << host << " and " << abuf << endl;
			mapCacheHostLookup.insert( valTypeCHL ( host, abuf));
		}
		pthread_mutex_unlock(&mutexCacheHost);

	}

	// return result
	len = strlen(abuf);
	//pthread_mutex_lock(&mutexMemory);
	result = new char[len + 1];
	//pthread_mutex_unlock(&mutexMemory);
	memset( result, 0, len+1 );
	memcpy( result, abuf, len );

	return result;
}

/**********************************************************************************
 *  Function name: IsValidHostChar
 *  Input argv:
 *  	-- ch: the character for testing
 *  Output argv:
 *  	-- 
 *  Return:
   	true: is valid
   	false: is invalid
 *  Function Description: test the specified character valid
 *  			for a host name, i.e. A-Z or 0-9 or -.:
**********************************************************************************/
bool CUrl::IsValidHostChar(char ch)
{
	return( isalpha(ch) || isdigit(ch)
		|| ch=='-' || ch=='.' || ch==':' || ch=='_');
}

/**********************************************************************************
 *  Function name: IsValidHost
 *  Input argv:
 *  	-- ch: the character for testing
 *  Output argv:
 *  	-- 
 *  Return:
   	true: is valid
   	false: is invalid
 *  Function Description: test the specified character valid
 *  			for a host name, i.e. A-Z or 0-9 or -.:
 *  Be careful:
**********************************************************************************/
bool CUrl::IsValidHost(const char *host)
{
	if( !host ){
		return false;
	}

	if( strlen(host) < 6 ){ // in case host like "www", "pku", etc.
		return false;
	}

	char ch;
	for(unsigned int i=0; i<strlen(host); i++){
		ch = *(host++);
		if( !IsValidHostChar(ch) ){
			return false;
		}
	}

	return true;
}

/**********************************************************************************
 *  Function name: IsVisitedUrl
 *  Input argv:
 *  	-- url: url
 *  Output argv:
 *  	-- 
 *  Return:
   	true: is visited
   	false: not visited
 *  Function Description: test the url visited by the MD5
 *  Be careful:
**********************************************************************************/
bool CUrl::IsVisitedUrl(const char *url)
{
	if( !url ){
		return true; // if be null, we think it have been visited
	}

	CMD5 iMD5;
	iMD5.GenerateMD5( (unsigned char*)url, strlen(url) );
	string strDigest = iMD5.ToString();

	if( setVisitedUrlMD5.find(strDigest) != setVisitedUrlMD5.end() ) {
		return true;
	} else {
		return false;
	}

}


/**********************************************************************************
 *  Function name: IsValidIp
 *  Input argv:
 *  	-- ip: ip
 *  Output argv:
 *  	-- 
 *  Return:
   	true: inside the ip block
   	false: outside the ip block
 *  Function Description: decide teh ip whether or not inside the ip block
 *  Be careful:
**********************************************************************************/
bool CUrl::IsValidIp(const char *ip)
{
	if( ip == NULL ){
		return false;
	}

	unsigned long inaddr = (unsigned long)inet_addr(ip);
	if( inaddr == INADDR_NONE ){	// invalid ip
		return false;
	}

	if( mapIpBlock.size() > 0 ){
		map<unsigned long,unsigned long>::iterator pos;
		for(pos=mapIpBlock.begin(); pos!=mapIpBlock.end(); ++pos){
			unsigned long ret;

			ret = inaddr & ~((*pos).second);
			if( ret == (*pos).first ){	// inside
				return true;
			}
		}

		// outside
		return false;
	}

	// if block range is not given, we think it inside also
	return true;
}

/*
 * If it is, return true; otherwise false
 * not very precise
 */
bool CUrl::IsForeignHost(string host)
{
	if( host.empty() ) return true;
	if( host.size() > HOST_LEN ) return true;

	unsigned long inaddr = 0;

	inaddr = (unsigned long)inet_addr( host.c_str() );
	if ( inaddr != INADDR_NONE){ // host is just ip
		return false;
	}

	string::size_type idx = host.rfind('.');
	string tmp;
	if( idx != string::npos ){
		tmp = host.substr(idx+1);
	}

	CStrFun::Str2Lower( tmp, tmp.size() );
	const char *home_host[] ={
		"cn","com","net","org","info",
		"biz","tv","cc", "hk", "tw"
	};

	int home_host_num = 10;

	for(int i=0; i<home_host_num; i++){
		if( tmp == home_host[i] )
			return false;
	}

	return true;
}
	
	
bool CUrl::IsImageUrl(string url)
{
	if( url.empty() ) return false;
	if( url.size() > HOST_LEN ) return false;

	string::size_type idx = url.rfind('.');
	string tmp;
	if( idx != string::npos ){
		tmp = url.substr(idx+1);
	}

	CStrFun::Str2Lower( tmp, tmp.size() );
	const char *image_type[] ={
		"gif","jpg","jpeg","png","bmp",
		"tif","psd"
	};

	int image_type_num = 7;

	for (int i=0; i<image_type_num; i++)
	{
		if( tmp == image_type[i] )
			return true;
	}

	return false;
}
	
	
