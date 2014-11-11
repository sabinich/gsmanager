/*
 * sysctl.cpp
 *
 *  Created on: 12.07.2013
 *      Author: boom
 */

#include "sysctl.h"
#include <classes.h>
#include <mgr/mgrfile.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrssh.h>
#include <mgr/mgrproc.h>

#define SYSCTLCONF "/etc/sysctl.conf"
#define TMP_SYSCTLCONF "/tmp/sysctl.conf"
#define SYSCTL_EXEC "sysctl -p"

namespace sysctl {

class ConnectionLocal : public ConnectionBase
{
public:
	string ReadConf()
	{
		return mgr_file::Read(SYSCTLCONF);
	}
	void WriteConf(const string & content)
	{
		mgr_file::Write(SYSCTLCONF, content);
	}
	void ExecSysctl()
	{
		mgr_proc::Execute (SYSCTL_EXEC);
	}
};

class ConnectionRemote : public ConnectionBase
{
public:
	ConnectionRemote (mgr_rpc::SSH ssh): m_ssh (ssh) { }
	string ReadConf()
	{
		return m_ssh.Execute("cat " SYSCTLCONF).Str();
	}
	void WriteConf(const string & content)
	{
		m_ssh.Execute("cat > " SYSCTLCONF).In(content);
	}
	void ExecSysctl()
	{
		m_ssh.Execute(SYSCTL_EXEC);
	}
private:
	mgr_rpc::SSH m_ssh;
};

ConnectionBase::Ptr Local()
{
	return ConnectionBase::Ptr (new ConnectionLocal);
}
ConnectionBase::Ptr Remote(mgr_rpc::SSH ssh)
{
	return ConnectionBase::Ptr (new ConnectionRemote (ssh));
}

Conf::Conf(ConnectionBase::Ptr connection) :
		m_Connection (connection)
{
	try {
		m_OldContent = m_Connection->ReadConf();
	} catch (...) { }
}

Conf& Conf::SetParamValue(const string& param, int value) {
	m_values[param] = str::Str(value);
	return *this;
}

Conf& Conf::SetParamValue(const string& param, const string& value) {
	m_values[param] = value;
	return *this;
}

void Conf::Run() {
	string confContent, line, old_line, contentResult, key;
	confContent = m_Connection->ReadConf();
	while (!confContent.empty()) {
		line = str::GetWord(confContent, '\n');
		if (line.empty() || (line[0] == '#') || m_values.empty()) {
			contentResult += line + "\n";
			continue;
		}
		old_line = line;
		key = str::Trim(str::GetWord(line, "="));
		if (!key.empty() && key[0] == '#')
			key = str::RGetWord(key, "#");
		auto val = m_values.find(key);
		if (val != m_values.end()) {
			contentResult += key + " = " + val->second + "\n";
			m_values.erase(val);
		} else
			contentResult += old_line + "\n";
	}
	ForEachI(m_values, val) {
		contentResult += val->first + " = " + val->second + "\n";
	}
	m_Connection->WriteConf(contentResult);
	m_Connection->ExecSysctl();
}

void Conf::onRollback() throw () {
	m_Connection->WriteConf(m_OldContent);
}


} /* namespace sysctl */
