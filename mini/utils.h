#ifndef GMMINI_UTILS_H
#define GMMINI_UTILS_H
#include <mgr/mgrstr.h>
#include <mgr/mgrfile.h>
#include <api/action.h>

class SimpleConfigBase {
protected:
	StringMap m_params;
public:
	void SetParams(StringMap & params);
	void SetParansFormSes(isp_api::Session & ses);
	void ConfToSes(isp_api::Session & ses);
	void SetParam(const string & param, const string & value);
	string GetParam(const string & param) 
	{
		return m_params[param];
	}	
	string &operator[] (const string & param)
	{
		return m_params[param];
	}
	virtual void ReadConf() = 0;
	virtual void SaveConf() = 0;
};

#endif