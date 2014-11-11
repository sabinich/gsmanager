#include "utils.h"

void SimpleConfigBase::SetParams(StringMap & params)
{
	ForEachI(params, i) {
		string param = i->first;
		string val = i->second;
		if (m_params.find(param) != m_params.end()) {
			if (val == "on") val = "true";
			if (val == "off") val ="false";
			m_params[param]=val;
		}
	}

}

void SimpleConfigBase::SetParansFormSes(isp_api::Session & ses) 
{
	StringVector params;
	ses.GetParams(params);
	
	ForEachI(params, i) {
		string param = *i;
		string val = ses.Param(*i);
		if (m_params.find(param) != m_params.end()) {
			if (val == "on") val = "true";
			if (val == "off") val ="false";
			m_params[param] = val;
		}
	}
}

void SimpleConfigBase::ConfToSes(isp_api::Session & ses) {
	ForEachI(m_params, param) {
		string val = param->second;
		if (val == "true") val = "on";
		if (val == "false") val ="off";
		ses.NewNode(param->first.c_str(),val);
	}
}

void SimpleConfigBase::SetParam(const string & param, const string & value)
{
	if (m_params.find(param) != m_params.end())
		m_params[param]=value;
}