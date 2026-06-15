#ifndef _DATA_ENGINE_H_031104
#define _DATA_ENGINE_H_031104

#include "Tse.h"

enum dataengine_type
{
	FILE_ENGINE,
	DATABASE_ENGINE
};

class CDataEngine
{
public:
	string m_str;	// database engine ---connecting string
			// file engine ---file path & name

public:
	CDataEngine(string str);
	CDataEngine();
	virtual ~CDataEngine();

	virtual int GetEngineType() = 0;
	virtual bool Write(void *arg) = 0;
	virtual bool Open(string str) = 0;
	virtual void Close() = 0;
};

#endif /*_DATA_ENGINE_H_031104*/
