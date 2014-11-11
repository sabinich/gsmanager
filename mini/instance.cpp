#include "instance.h"
#include <mgr/mgrlog.h>
#include "../main/defines.h"
#include "serverstat.h"
#include <api/vault.h>
#include <api/stdconfig.h>
MODULE("gsmini");

#ifdef WIN32
#include "win_common.h"
#endif

ServerInstanceInfo::ServerInstanceInfo(isp_api::Session & ses) 
{
	if (ses.auth.level() == mgr_access::lvUser) {
		UserName = ses.auth.name();
		string tmp = UserName;
		Game = str::GetWord(tmp,"_");
		Id = tmp;
	} else 
		throw mgr_err::Error("Can`t create instance from non user Level");
}

ServerInstanceInfo::ServerInstanceInfo(const string & game, const string & id)
{
	UserName = game+"_"+id;
	Game = game;
	Id = id;
}

ServerInstanceInfo::ServerInstanceInfo(const string & user) {
	UserName = user;
	string tmp = user;
	Game = str::GetWord(tmp,"_");
	Id = tmp;
}

string ServerInstanceInfo::GetParam(const string& param)
{
	try {
		return mgr_file::Read(mgr_file::ConcatPath(GetMgrDataPath(), param));
	} catch(...) {
	}
	return "";
}

void ServerInstanceInfo::SetParam(const string& param, const string& val)
{
	return mgr_file::Write(mgr_file::ConcatPath(GetMgrDataPath(), param), val);
}

string ServerInstanceInfo::GetIp()
{
	return GetParam("ip");
}

void ServerInstanceInfo::SetIp(const string& ip)
{
	SetParam("ip", ip);
}

string ServerInstanceInfo::GetStatus()
{
	return GetParam("status");
}

void ServerInstanceInfo::SetStatus(const string& status)
{
	SetParam("status", status);
}

int ServerInstanceInfo::GetSlots()
{
	return str::Int(GetParam("slots"));
}

void ServerInstanceInfo::SetSlots(int slots)
{
	SetParam("slots", str::Str(slots));
}

int ServerInstanceInfo::GetPort()
{
	return str::Int(GetParam("port"));
}

void ServerInstanceInfo::SetPort(int slots)
{
	SetParam("port", str::Str(slots));
}

string ServerInstanceInfo::GetInstancePath()
{
#ifdef WIN32
	return sys_path::ServerInstanceDir(UserName);
#else
	const string m_HomePath = "/home";
	const string m_InstanceDir = "instance";
	return mgr_file::ConcatPath(m_HomePath, UserName, m_InstanceDir);
#endif
}
string ServerInstanceInfo::GetMgrDataPath()
{
	const string m_MgrDataDir = "mgrdata";
#ifdef WIN32
	return mgr_file::ConcatPath(sys_path::UserHomeSecurator(UserName), m_MgrDataDir);
#else
	const string m_HomePath = "/home";	
	return mgr_file::ConcatPath(m_HomePath, UserName, m_MgrDataDir);
#endif	
}

void ServerInstanceInfo::InstallDist(DistInfo& dist)
{
#ifdef WIN32
	Debug("arch path is '%s'", dist.Path.c_str());
	mgr_proc::Execute arch("\""+mgr_proc::Escape(mgr_cf::GetPath("7z_path"))+"\" x -y "+ mgr_proc::DecorateEnvParam("ARCH_PATH"), mgr_proc::Execute::efOutErr);
	arch.SetEnv("ARCH_PATH", mgr_proc::Escape(dist.Path));
#else
	mgr_proc::Execute arch("tar xzf " + mgr_proc::Escape(dist.Path));
#endif
	arch.SetDir(GetInstancePath());
	Debug("arch res %s", arch.Str().c_str());
	if (arch.Result() != 0) throw mgr_err::Error("dist_extract");
	ApplyRight();
	SetParam("version", dist.Name);	
}

void ServerInstanceInfo::ApplyRight()
{
#ifdef WIN32
	mgr_file::Attrs attrs(0744, mgr_user::WinSid(UserName).str(), mgr_user::WinSid(UserName).str());
#else
	mgr_file::Attrs attrs(0744, mgr_user::Uid(UserName), mgr_user::GroupGid(UserName));
#endif
	attrs.Set(GetInstancePath(), true);
}

void ServerInstanceInfo::ReinitPort()
{
	PortMgr::FreePort(UserName);
	SetPort(PortMgr::AllocPort(UserName,GetIp()));
}


bool ServerInstanceInfo::NeedRestart()
{
	return this->GetParam("needrestart") == "on";
}

void ServerInstanceInfo::SetNeedRestart(bool restart)
{
	if (restart)
		this->SetParam("needrestart","on");
	else 
		this->SetParam("needrestart","off");
}

bool ServerInstanceInfo::IsDisabled()
{
	return this->GetParam("disabled") == "on";
}

void ServerInstanceInfo::SetDisabled(bool disabled)
{
	
	if (disabled) {
		this->SetParam("disabled","on");
	} else { 
		this->SetParam("disabled","off");
	}
}

void ServerInstanceInfo::ClearInstance()
{
	mgr_file::Dir dir(GetInstancePath());
	while (dir.Read()) {
		if (dir.Info().IsDir())
			mgr_file::Remove(dir.FullName());
		else
			mgr_file::RemoveFile(dir.FullName());
	}
}
