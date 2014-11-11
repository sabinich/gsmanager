#include "ssh_utils.h"
#include <mgr/mgrenv.h>
#include <classes.h>
#include <mgr/mgrssh.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrtest.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrproc.h>
#include <fstream>
#include <iostream>
#include <mgr/mgrlog.h>

MODULE("sshutils");

namespace ssh_utils
{
namespace os_info
{

mgr_env::OsTypeEnum Type(mgr_rpc::SSH & connection) {
	mgr_env::OsTypeEnum OsType = mgr_env::osUnknown;
	if (file::Exists(connection, "/etc/redhat-release")) {
		OsType = mgr_env::osRedHat;
		string osname = file::Read(connection, "/etc/redhat-release");
		if (osname.find("Fedora release") == 0)
			OsType = mgr_env::osFedora;
		if (osname.find("CentOS release") == 0)
			OsType = mgr_env::osCentOS;
		if (osname.find("CentOS Linux release") == 0)
			OsType = mgr_env::osCentOS;
		if (osname.find("Scientific Linux release") == 0)
			OsType = mgr_env::osCentOS;
	} else if (file::Exists(connection, "/etc/debian_version")) {
		OsType = mgr_env::osDebian;
		string osname;
		if (file::Exists(connection, "/etc/lsb-release")) {
			osname = file::Read(connection, "/etc/lsb-release");
			if (osname.find("Ubuntu") != string::npos)
				OsType = mgr_env::osUbuntu;
		}
	} else if (file::Exists(connection, "/etc/gentoo-release")) {
		OsType = mgr_env::osGentoo;
	}
	LogInfo("os version: %06x", (int)OsType);
	return OsType;
}

string Version(mgr_rpc::SSH &connection) {
	string osver;
	mgr_env::OsTypeEnum osFamily = Type(connection);
	if ((osFamily & mgr_env::osDebian) == mgr_env::osDebian) {
		string ver = file::Read(connection, "/etc/debian_version");
		if (!ver.empty() && ver[ver.length()-1] == '\n')
			ver.resize(ver.length()-1);
		string major = str::GetWord(ver, '.');
		string minor = str::GetWord(ver, '.');
		if (minor.empty())
			minor = "0";
		if (test::Numeric(major) && test::Numeric(minor))
			osver = major + "." + minor;
		else {
			ver = file::Read(connection, "/etc/issue");
			if (!ver.empty()) {
				string buff;
				while (!(buff = str::GetWord(ver)).empty()) {
					string testnum(buff.substr(0,1));
					if (test::Numeric(testnum)) {
						osver = buff;
						break;
					}
				}
			}
		}
	} else if ((osFamily & mgr_env::osRedHat) == mgr_env::osRedHat) {
		string ver = file::Read(connection, "/etc/redhat-release");
		str::GetWord(ver, "release ");
		osver = str::GetWord(ver);
	}
	return osver;
}

string GetOs(mgr_rpc::SSH & connection) {
	switch (Type(connection)) {
			return "RedHat " + Version(connection);
		case mgr_env::osCentOS:
			return "CentOS " + Version(connection);
		case mgr_env::osDebian:
			return "Debian " + Version(connection);
		case mgr_env::osUbuntu:
			return "Ubuntu " + Version(connection);
		default:
			return "";
	}
}

string Arch(mgr_rpc::SSH & connection) {
	return str::Trim(connection.Execute("uname -m").Str());
}

} /*end of namespace os_info*/


namespace file
{

DownloadWatcher::DownloadWatcher(size_t streamLength)
	: m_ReadenTotal(0)
	, m_ReadenTotalPreviousPercent(0)
	, m_Length (streamLength)
{
	Debug ("writing %llu bytes", m_Length);
}

void DownloadWatcher::OnGetData(char *buf, size_t len)
{
	size_t readenTotalPercent = (m_ReadenTotal * 100) / m_Length;
	m_ReadenTotal += len;
	if ((readenTotalPercent % 5 == 0)
			&& readenTotalPercent != m_ReadenTotalPreviousPercent) {
		Debug ("readen %d%% (%llu of %llu)", readenTotalPercent, m_ReadenTotal, m_Length);
		m_ReadenTotalPreviousPercent = readenTotalPercent;
	}
}

bool DownloadWatcher::Finished()
{
	return m_ReadenTotal == m_Length;
}

DownloadWatcher::~DownloadWatcher() {}


string Read (mgr_rpc::SSH & connection, const string & fileName)
{
	string result = connection.Execute("dd if=" + mgr_proc::Escape(fileName) + " 2>/dev/null").Str();
	if (connection.Result() != 0) {
		throw mgr_err::Error ("remote_read");
	}
	return result;
}

void Write (mgr_rpc::SSH & connection, const string & fileName, const string & data)\
{
	connection.Execute("dd of=" + mgr_proc::Escape(fileName) + " 2>/dev/null").In(data);
	if (connection.Result() != 0) {
		throw mgr_err::Error("remote_write");
	}
}

bool Exists (mgr_rpc::SSH & connection, const string & fileName, string type)
{
	if (type.empty() || type.find_first_not_of("fdesp") != string::npos)
		type = "e";
	return connection.Execute("test -" + type + " " + mgr_proc::Escape(fileName)).Result() == 0;
}

void MkDir(mgr_rpc::SSH &connection, const string &dirName)
{
	connection.Execute("mkdir -p " + mgr_proc::Escape(dirName));
}

void Remove(mgr_rpc::SSH &connection, const string &fileName)
{
	connection.Execute("rm -f " + mgr_proc::Escape(fileName));
}

void CopyTo(const string &fileName, mgr_rpc::SSH &connection, const string &remoteFileName)
{
	size_t fileSize = mgr_file::Info(fileName).Size();
	DownloadWatcher watcher(fileSize);
	std::ifstream input (fileName.c_str(), std::ios::binary);
	std::ostream & stream = connection.Execute("dd of=" + remoteFileName + " 2>/dev/null").In();
	char buf[65536];
	std::streambuf * readbuffer = input.rdbuf();
	std::streamsize readen = 0;
	do {
		readen = readbuffer->sgetn(buf, sizeof (buf));
		if (readen < (std::streamsize)sizeof (buf)) {
			Debug ("readen %d", readen);
		}
		if (readen > 0) {
			watcher.OnGetData(buf, readen);
			stream.write(buf, readen);
		}
	} while (readen > 0 && !watcher.Finished());
	Debug ("Finished: %d", watcher.Finished());
}

size_t Size(mgr_rpc::SSH &connection, const string &fileName)
{
	size_t fileSize = str::Int64 (connection.Execute ("stat -c '%s' " + mgr_proc::Escape(fileName)).Str ());
	return fileSize;
}

} /*end of namespace file*/

} /*end of namespace ssh_utils*/


