#include "gamemodule.h"
#include <mgr/mgrlog.h>
#include "../main/defines.h"
#include "serverstat.h"
#include <api/vault.h>
#include <api/stdconfig.h>
MODULE("gsmini");

#ifdef WIN32
#include "win_common.h"
#endif



void GameModuleBase::CreateServer(isp_api::Session& ses, const string& id)
{
	//Create User
	string username = Name+"_"+id;
	ServerInstanceInfo instance(username);
#ifdef WIN32
	CreateWindowsUser(username);
#else
	string cmd = "useradd -m "+username+" -s /bin/bash";
	mgr_proc::Execute(cmd).Result();
#endif
	//Create Directories and set rights
#ifdef WIN32
	mgr_file::Attrs instanceatrs(0744, mgr_user::WinSid(username).str(), mgr_user::WinSid(username).str());
	mgr_file::Attrs mgrdataatrs(0700, mgr_user::WinSid(mgr_user::GetDefaultAdministrator()).str(),	mgr_user::WinSid(WinBuiltinAdministratorsSid).str());
#else
	mgr_file::Attrs instanceatrs(0744, mgr_user::Uid(username), mgr_user::GroupGid(username));
	mgr_file::Attrs mgrdataatrs(0700, mgr_user::Uid("root"), mgr_user::GroupGid("root"));
#endif
	mgr_file::MkDir(instance.GetInstancePath(), &instanceatrs);
	mgr_file::MkDir(instance.GetMgrDataPath(), &mgrdataatrs);

	// Alloc IP and ports
	string ip = ses.Param("ip");
	int slots = str::Int(ses.Param("maxplayers"));
	int port = str::Int(ses.Param("port"));
	instance.SetIp(ip);
	instance.SetSlots(slots);
	try {
		if (m_AllocPortDecade) 
			port = PortMgr::AllocPortDecade(instance.UserName, ip);
		else
			port = PortMgr::AllocPort(instance.UserName, ip, port);
		instance.SetPort(port);
	} catch  (const char * s) {
		Debug("TT[%s]",s);
		throw;
	}
	
	// Get Dist
	DistInfoList distinfo;
	GetDistInfoList(distinfo, Name);
	DistInfo distToInstall = *distinfo.begin();
	ForEachI(distinfo, dist) {
		Debug("Dist [%s] '%s' <%s> _%s_", dist->Game.c_str(), dist->Name.c_str(), dist->Path.c_str(), dist->Info.c_str());
		if (dist->IsDefault) distToInstall = *dist;
	}
	
	// Install Dist
	instance.InstallDist(distToInstall);
	
	ses.NewNode("port", str::Str(instance.GetPort()));
	AfterReinstall(id,*distinfo.begin());
}

void GameModuleBase::DeleteServer(isp_api::Session& ses, const string& id)
{
	try {
		this->StopServer(ses, id);
	} catch(...) {}
	//Delete User
	string username = Name + "_" + id;
#ifdef WIN32
	try {
		DeleteWindowsUser(username);
	} catch (...) {}
	try {
		mgr_file::Remove(sys_path::UserHomeSecurator(username));
	} catch(...) {}
#else
	string cmd = "userdel -f -r " + username;
	mgr_proc::Execute(cmd).Result();
#endif
	
	try {
		PortMgr::FreePort(username);
	} catch(...) {
	}
	
	try {
		mgr_file::Remove(mgr_file::ConcatPath("/home",username));
	} catch (...) {
	}
}
#ifdef WIN32 
void GameModuleBase::CreateWindowsUser(const string & name)
{
	mgr_user::WinUser user(name);
	if (user.Exists()) DeleteWindowsUser(name);
	try {
		const string password = str::hex::Encode(str::Random(5))+"1sd@sd1"+str::hex::Encode(str::Random(5));
		user.Create(password, ISP_USER_COMMENT_PREFIX + name);
		isp_api::vault::SavePassword(name, password);
		mgr_user::WinGroup isp(ISP_USERS_GROUP);
		isp.AddMember(name);
		mgr_user::WinGroup user_group(ISP_USER_GROUP_PREFIX + name);
		if (!user_group.Exists())
			user_group.Create(ISP_USER_GROUP_COMMENT_PREFIX + name);
		user_group.AddMember(name);

		mgr_file::Attrs attr_sec;
		attr_sec.SetAccess(name, FILE_LIST_DIRECTORY);
		const string path_sec = sys_path::UserHomeSecurator(name);
		Debug("User home securator: '%s'", path_sec.c_str());
		//проверим существование домашней директории
		if (mgr_file::Exists(path_sec))
			throw mgr_err::Error("user", "home_exists", path_sec).add_param("user", name);
		new mgr_job::CreateDir(path_sec); //удалит диру, если таковая была
		//права назначатся на новую диру. При откате вернется старая со старыми правами
		attr_sec.Set(path_sec, true);

		mgr_file::Attrs attr(0770, mgr_user::WinSid(name).str(), mgr_user::WinSid(user_group.Name()).str());
		const string path = sys_path::ServerInstanceDir(name);
		Debug("ServerInstanceDir directory: '%s'", path.c_str());
		new mgr_job::CreateDir(path); //удалит диру, если таковая была
		//права назначатся на новую диру. При откате вернется старая со старыми правами
		attr.Set(path, true);
		Debug("Set");
		user.SetHome(path);
		Debug("Set Home");
		//cur->Uid = mgr_user::WinSid(name).str();
	} catch(const mgr_err::Error&) {
		throw mgr_err::Error("user", "create");
	}
	Debug("End");
}

void GameModuleBase::DeleteWindowsUser(const string & name)
{
	mgr_user::WinUser user(name);
	try {
		string u_name = name;
		try {
			if (!user.Enabled())
				user.Enable();
		} catch(const mgr_err::Error& e) {
			Warning("Failed to delete all user '%s' tasks. Error: %s", u_name.c_str(), e.what());
		}

		string path = user.GetHome();
		try {
			if (!path.empty() && mgr_file::Exists(path))
				mgr_file::Remove(path);
		} catch(const mgr_err::Error& e) {
			Warning("Failed to delete user '%s' home '%s'. Error: %s", u_name.c_str(), path.c_str(), e.what());
		}

		user.Delete();

		mgr_user::WinGroup user_group(ISP_USER_GROUP_PREFIX + u_name);
		try {
			if (user_group.Exists())
				user_group.Delete();
		} catch(const mgr_err::Error& e) {
			Warning("Failed to delete user '%s' primary group '%s'. Error: %s", u_name.c_str(), user_group.Name().c_str(), e.what());
		}
	} catch(const mgr_err::Error&) {
		throw mgr_err::Error("user", "delete");
	}
}
#endif


class ServerStatusCollectThread {
public:
	string Username;
	GameModuleBase * Module;
	time_t now;
	ServerStatusCollectThread(const string & username, GameModuleBase * module, time_t date):Username(username), Module(module), now(date)
	{
	}
	void operator()() {
		ServerInstanceInfo srv(Username);
		bool isUp = Module->IsUp(srv.Id);
		int players = isUp ? Module->PlayersOnline(srv.Id) : 0;
		srv.SetParam("status", isUp ? "on" : (str::Int(srv.GetParam("restart_cnt"))==5?"broken":"off"));
		srv.SetParam("playersonline", str::Str(players));

		string ramused = "0";
		string cpuused = "0";
		if (isUp) {
			try {
				ramused = str::Trim(mgr_proc::Execute(str::Format("smem -uHU %s -c rss", srv.UserName.c_str())).Str());
			} catch (...) {
				ramused = "0";
			}
			try {
				cpuused = str::Trim(mgr_proc::Execute(
				            str::Format("ps -U %s -u %s -o pcpu | awk '{sum += $1;} END {print sum;}'",
                                        srv.UserName.c_str(), srv.UserName.c_str())).Str());
			} catch (...) {
				cpuused = "0";
			}
		}
		
		srv.SetParam("ramused", ramused);
		srv.SetParam("cpuused", cpuused);
		if (now % 300 == 0)
			AddServerStat(now, srv, players, str::Int(ramused));
	}
};

std::list< std::shared_ptr<mgr_thread::Handle> > GameModuleBase::SaveServersStatus()
{
#ifdef WIN32
	time_t now = mgr_date::DateTime();
#else
	time_t now = time(0);
#endif
	now = now - (now%60);
	StringList ilist;
	GetInstanceList(ilist, Name);
	
	std::list< std::shared_ptr<mgr_thread::Handle> > threadList;
	ForEachI(ilist, i) {
		threadList.push_back(std::shared_ptr<mgr_thread::Handle>(new mgr_thread::Handle(ServerStatusCollectThread(*i, this, now))));
	}
	
	return threadList;
}

std::map<string, int> GetDiskQuota()
{
	std::map<string, int> res;
	string data;
	mgr_proc::Execute rephome("repquota /home");
	mgr_proc::Execute reproot("repquota /");
	data = rephome.Str();
	if (rephome.Result() != 0)
		data = reproot.Str();
	StringList lines;
	str::Split(data, "\n", lines);
	bool header = true;
	ForEachI(lines, line) {
		*line = str::Trim(*line);
		Debug("line '%s'",line->c_str());
		if (!header) {
			string user = str::GetWord(*line);
			str::GetWord(*line);
			string used = str::GetWord(*line);
			res[user] = str::Int64(used);
		}
		if (line->find("----------") == 0)
			header = false;
	}
	return res;
}


void GameModuleBase::GetServersStatus(isp_api::Session& ses)
{
	mgr_thread::SafeSection statLock(StatusLock);
	StringList ilist;
	GetInstanceList(ilist, Name);
	ForEachI(ilist, i) {
		ServerInstanceInfo srv(*i);
		ses.NewElem();
		ses.AddChildNode("id", srv.Id);
		ses.AddChildNode("type", srv.Game);
		ses.AddChildNode("ip", srv.GetIp());
		ses.AddChildNode("port", str::Str(srv.GetPort()));
		ses.AddChildNode("slots", str::Str(srv.GetSlots()));
		ses.AddChildNode("playersonline", srv.GetParam("playersonline"));
		ses.AddChildNode("status", srv.GetParam("status"));
		ses.AddChildNode("ramused", srv.GetParam("ramused"));
		ses.AddChildNode("cpuused", srv.GetParam("cpuused"));
		ses.AddChildNode("diskused", srv.GetParam("diskused"));
	}
}

void GetInstanceList(StringList& list, const string & game)
{
#ifdef WIN32
	mgr_file::Dir home(sys_path::UsersRoot());
#else
	mgr_file::Dir home("/home");
#endif
	while(home.Read()) {
		if (!game.empty() && home.name().find(game) != 0) continue;
		if (mgr_user::Exists(home.name()) && mgr_file::Exists(mgr_file::ConcatPath(home.FullName(), "mgrdata"))) {
			list.push_back(home.name());
		}
	}
}

void GameModuleBase::PluginsList(isp_api::Session& ses, const string& id)
{
	ServerInstanceInfo srv(this->Name, id);
	string CurrDist = srv.GetParam("version");
	string plugpath = mgr_file::ConcatPath(mgr_cf::GetPath("dist_path"), Name, CurrDist+"/plugins");
		
	if (mgr_file::Exists(plugpath)) {
		mgr_file::Dir plugdir(plugpath);
		while(plugdir.Read()) {
			string plginfo;
			if (plugdir.Info().IsDir() && mgr_file::Exists(mgr_file::ConcatPath(plugdir.FullName(),"/dist.tgz"))) {
				string infofile = mgr_file::ConcatPath(plugdir.FullName(),"/info");
				if (mgr_file::Exists(infofile))
					plginfo = mgr_file::Read(infofile);
				ses.NewElem();
				ses.AddChildNode("name", plugdir.name());
				ses.AddChildNode("info", plginfo);
				auto plg = GetPlugin(plugdir.name());
				ses.AddChildNode("installed", plg->IsInstalled(id) ? "on" : "off");
			}
		}
	}
}

void GamePlugin::Install(const string& id)
{
	ServerInstanceInfo srv(m_module.Name,id);
	string CurrDist = srv.GetParam("version");
	string plugpath = mgr_file::ConcatPath(mgr_cf::GetPath("dist_path"),m_module.Name,CurrDist+"/plugins");
	if (mgr_file::Exists(plugpath)) {
		mgr_file::Dir plugdir(plugpath);
		while(plugdir.Read()) {
			string plugdistr = mgr_file::ConcatPath(plugdir.FullName(),"/dist.tgz");
			if (str::Upper(plugdir.name()) == str::Upper(Name) && plugdir.Info().IsDir() && mgr_file::Exists(plugdistr)) {
				mgr_proc::Execute tar("tar xzf " + mgr_proc::Escape(plugdistr));
				string plugins_destination= mgr_file::ConcatPath(srv.GetInstancePath(), m_module.PluginsPath);
				tar.SetDir(plugins_destination);
				if (tar.Result() != 0) throw mgr_err::Error("plugin_unpack", Name.c_str());
#ifdef WIN32
				mgr_file::Attrs instanceatrs(0744, mgr_user::WinSid(srv.UserName).str(), mgr_user::WinSid(srv.UserName).str());
#else
				mgr_file::Attrs instanceatrs(0744, mgr_user::Uid(srv.UserName), mgr_user::GroupGid(srv.UserName));
#endif
				instanceatrs.Set(plugins_destination, true);
				srv.SetNeedRestart();
				return;
			}
		}
	}
}

bool GamePlugin::IsInstalled(const string& id)
{
	ServerInstanceInfo srv(m_module.Name, id);
	return (!ExecutableName.empty()) && mgr_file::Exists(mgr_file::ConcatPath(srv.GetInstancePath(), m_module.PluginsPath, ExecutableName));
}

void GamePlugin::Uninstall(const string& id)
{
	ServerInstanceInfo srv(m_module.Name, id);
	if (!ExecutableName.empty()) {
		mgr_file::Remove(mgr_file::ConcatPath(srv.GetInstancePath(), m_module.PluginsPath, ExecutableName));
	}
	srv.SetNeedRestart();
}

void GameModuleBase::StartServer(isp_api::Session& ses, const string& id)
{
	ServerInstanceInfo srv(this->Name+"_"+id);
	srv.SetParam("desired_state", "on");
	srv.SetParam("restart_cnt", "0");
	StartServerImpl(ses, id);
}

void GameModuleBase::StopServer(isp_api::Session& ses, const string& id)
{
	ServerInstanceInfo srv(this->Name+"_"+id);
	srv.SetParam("desired_state", "off");
	srv.SetParam("restart_cnt", "0");
	StopServerImpl(ses, id);
}


void GameModuleBase::CheckServerState(isp_api::Session& ses)
{
	//mgr_thread::SafeSection statLock(StatusLock);
	StringList ilist;
	GetInstanceList(ilist, Name);
	ForEachI(ilist, i) {
		try {
			ServerInstanceInfo srv(*i);
			string state = srv.GetParam("status");
			string desired_state = srv.GetParam("desired_state");
			int autostart_cnt = str::Int(srv.GetParam("restart_cnt"));
			if (state != desired_state) {
				if (desired_state == "on") {
					if (autostart_cnt < 5) {
						autostart_cnt++;
						srv.SetParam("restart_cnt", str::Str(autostart_cnt));
						this->StartServerImpl(ses, srv.Id);
					}
				}
			}
		} catch (...) {}
	}
}

