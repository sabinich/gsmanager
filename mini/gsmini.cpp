#include <api/module.h>
#include <mgr/mgrlog.h>
#include <mgr/mgruser.h>
#include <mgr/mgrscheduler.h>
#include <api/action.h>
#include <api/stdconfig.h>
#include "../main/defines.h"
#include "gmmini.h"
#include <api/auth_method.h>
#include "gamemodule.h"
#include "serverstat.h"


#ifdef WIN32
#include "win_common.h"
//определение всего, что нужно для Windows
#include <winlibcntl.h>
#include <winsock2.h>
USECORELIB("libmgr")
USECORELIB("ispapi")
USECORELIB("parser")
USELIBBASE

USELIB("Ws2_32")
USELIB("Shlwapi")
#include <api/autotask.h>
#endif

MODULE("gsmini");

ModulesMap GameModules;

using namespace isp_api;

string GetInstanceId(isp_api::Session& ses)
{
	string id = ses.auth.ext("instance");
	if (ses.auth.level() == mgr_access::lvUser && !id.empty())
		return id;
	else 
		id = ses.Param("id");
	if (id.empty()) throw mgr_err::Error("unknown_server","can`t detect instance");
	return id;
}

class GSMiniAuth : public AuthMethod {
public:
	GSMiniAuth() : AuthMethod("gsmini") {}

	virtual void AuthenByName(mgr_xml::Xml &res, const string &name) const {
		if (name == "admin" || name == "root")
			FillupParams(name, res, mgr_access::lvSuper);
		else if (mgr_user::Exists(name) && !ServerInstanceInfo(name).IsDisabled())
			FillupParams(name, res, mgr_access::lvUser);
	}

	virtual void AuthenByPassword(mgr_xml::Xml &res, const string &name, const string &pass) const {
		if (name == "admin") {
			string md5 = str::base64::Decode(mgr_cf::GetParam(ConfMiniPasswordParam));
			string tmp = str::Crypt(pass, md5);
			if (tmp == md5)
				FillupParams(name, res, mgr_access::lvSuper);
		}
	}
private:
	void FillupParams(const string& name, mgr_xml::Xml &xml, int level) const {
		XmlNode n = xml.GetRoot().AppendChild("ok");
		n.SetProp("level", str::Str(level));
		n.SetProp("method", this->name());
		n.SetProp("name", name);
		string tmp = name;
		str::GetWord(tmp,"_");
		if (!tmp.empty())
			n.AppendChild("ext", tmp).SetProp("name", "instance");
	}
};

class aPeriodicTasks: public Action {
public:
	aPeriodicTasks() : Action("periodic", MinLevel(lvSuper)) {};
	virtual void Execute(Session& ses) const
	{
	}
};

class aServersState: public Action {
public:
	aServersState() : Action("serversstate", MinLevel(lvAdmin)) {};
	virtual void Execute(Session& ses) const
	{
		ForEachI(GameModules, game) {
			Debug("Get module stat %s",game->first.c_str());
			auto ptr = game->second;
			ptr->GetServersStatus(ses);
		}	
	}
};

class aCheckServersState: public Action {
public:
	aCheckServersState() : Action("checkserverstate", MinLevel(lvAdmin)) {};
	virtual void Execute(Session& ses) const	
	{
		ForEachI(GameModules, game) {
			auto ptr = game->second;
			ptr->CheckServerState(ses);
		}
	}
};

class eStatistics: public Event {
public:
	eStatistics():Event("periodic", "estat"){};
	virtual void AfterExecute(Session& ses) const
	{
		ServerStateList list;
		std::list< std::shared_ptr<mgr_thread::Handle> > threadList;
		ForEachI(GameModules, game) {
			Debug("Collect module %s", game->first.c_str());
			auto ptr = game->second;
			mgr_thread::SafeSection statLock(ptr->StatusLock);
			auto thlist = ptr->SaveServersStatus();
			threadList.merge(thlist);
		}
		
		ForEachI(threadList, thread) {
			(*thread)->join();
		}
		
		StringList ilist;
		GetInstanceList(ilist);
		std::map<string, int> quota = GetDiskQuota();
		ForEachI(ilist, i) {
			ServerInstanceInfo srv(*i);
			if (quota.find(srv.UserName) != quota.end()) {
				srv.SetParam("diskused", str::Str(quota[srv.UserName]));
			} else {
				srv.SetParam("diskused", "0");
			}
		}
	}
};

class eGlobalServerRestartBanner : public GlobalEvent {
public:
	eGlobalServerRestartBanner():GlobalEvent("globalserverrestartbanner") {}
    virtual void BeforeAction(Session& ses) const
    {
		if (ses.auth.level() != mgr_access::lvUser) return; 
		try {
			ServerInstanceInfo srv(ses);
			if (srv.NeedRestart())
				ses.NewBanner("server_need_restart",mgr_session::BaseSession::Banner::blNotify);
		} catch (...) {
		}
	}
};

class aPlayersReport: public ReportAction {
public:
    aPlayersReport():ReportAction("playersdaystat",MinLevel(lvUser)) {}
    virtual void ReportExecute(Session& ses) const
    {
		ServerInstanceInfo srv(ses);
		GetDayStat(srv, ses);
	}
};

class aMemoryReport: public ReportAction {
public:
    aMemoryReport():ReportAction("memdaystat",MinLevel(lvUser)) {}
    virtual void ReportExecute(Session& ses) const
    {
		ServerInstanceInfo srv(ses);
		GetDayStat(srv, ses);
	}
};

class EventFileAuthInfo : public Event {
public:
 	EventFileAuthInfo() : Event("file.authinfo", MGR, epBefore) {}
 	virtual void BeforeExecute(Session &ses) const {
		const string auth_user = ses.Param("auth_user"); // параметр auth_user содержит имя пользователя, авторизованного в панели
 		ses.NewNode("username", auth_user); // запускать менеджер файлов от имени пользователя, авторизованного в панели
		ServerInstanceInfo srv(auth_user);
 		ses.NewNode("homedir", srv.GetUserAccessPath());
	}
 };
 
class EventFileUploadFix : public Event {
public:
 	EventFileUploadFix() : Event("file.upload", MGR, epBefore) {}
 	virtual void BeforeExecute(Session &ses) const {
		if (ses.Param("sok") != "ok") {
			ServerInstanceInfo srv(ses);
			Debug("%s",ses.xml.Str(true).c_str());
			string plid = ses.Param("plid");
// 			Debug("plid [%s] add [%s]", plid.c_str(), srv.GetUserAccessPath(false).c_str());
// 			plid = mgr_file::ConcatPath(plid, srv.GetUserAccessPath(false));
 			ses.NewNode("plid", mgr_file::ConcatPath(srv.GetUserAccessPath(false), plid));
		}
	}
};

class aSetAdminPassword : public FormAction {
public:
	aSetAdminPassword():FormAction("adminpass",MinLevel(lvSuper)) {}
	virtual void Set(Session& ses, const string& elid) const
	{
		string pass = ses.Param("passwd");
		string tmp = str::base64::Encode(str::Crypt(pass));
		mgr_cf::SetParam(ConfMiniPasswordParam, tmp);
	}
};

#ifdef WIN32
class WinSockInit {
public:
	WinSockInit() {
		WSADATA wsaData = {0};
		int iResult = 0;
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0)
			Warning("Can`t init Windows sockets");
	}

	~WinSockInit() {
		WSACleanup();
	}
};
static WinSockInit WinSockInitInstance;
#endif


MODULE_INIT(gsmini,"")
{
	Debug("Loading GSmini...");
	mgr_cf::AddParam(ConfMiniPasswordParam);
	mgr_cf::SetPathDefault("screen","screen");
	mgr_cf::SetPathDefault("dist_path", GS_DEFAULT_DISTR_PATH);
	new GSMiniAuth();
	new aSetAdminPassword();
#ifdef WIN32
	//пример как сделать то, что ниже для линухов написано, одной строчкой
	const string mgrctl = WinUnix("mgrctl.exe", "sbin/mgrctl");
	const string param = " -m gsmini periodic";
	const string per_cmd = "\"" + mgr_file::ConcatPath(mgr_file::GetCurrentDir(), mgrctl) + "\"" + param;
	//под линухами создаст под юзером, от которого запущен манагер, под виндой создаст специального админа
	isp_api::task::Schedule(per_cmd, "* * * * *", "ISPsystem GSmini periodic task");
	sys_path::CheckUsersBase();
	sys_path::CheckUsersRoot();
	mgr_cf::SetPathDefault("7z_path","C:\\Program Files\\7-Zip\\7z.exe");

	mgr_user::WinGroup isp(ISP_USERS_GROUP);
	Debug("Check system group '%s' existence", ISP_USERS_GROUP);
	if (!isp.Exists()) {
		Debug("Create system group '%s'", ISP_USERS_GROUP);
		isp.Create(ISP_USERS_COMMENT);
		mgr_win_perms::UserLocalSecurityPolicy lsp(isp.Name());
		lsp.AddRight(SE_DENY_INTERACTIVE_LOGON_NAME);
		lsp.AddRight(SE_DENY_NETWORK_LOGON_NAME);
		lsp.AddRight(SE_DENY_REMOTE_INTERACTIVE_LOGON_NAME);
		lsp.AddRight(SE_DENY_SERVICE_LOGON_NAME);
		lsp.AddRight(SE_BATCH_LOGON_NAME);
	}

#else
	//AddCron Task for periodic actions;
	string command;
	command = mgr_file::ConcatPath(mgr_file::GetCurrentDir(), "sbin/mgrctl -m gsmini periodic");
	mgr_scheduler::Scheduler PeriodicTask("root");
	if (!PeriodicTask.LocateByCommand(command)) {
		PeriodicTask.CreateTask(command);
		PeriodicTask.SetMinute("*");
		PeriodicTask.Post();
	}
	
	command = mgr_file::ConcatPath(mgr_file::GetCurrentDir(), "sbin/mgrctl -m gsmini checkserverstate");
	mgr_scheduler::Scheduler CheckServerTask("root");
	if (!CheckServerTask.LocateByCommand(command)) {
		CheckServerTask.CreateTask(command);
		CheckServerTask.SetMinute("*");
		CheckServerTask.Post();
	}

#endif
	new aPeriodicTasks();
	new eStatistics();
	new aServersState();
	new aCheckServersState();
	new eGlobalServerRestartBanner();
	new aPlayersReport();
	new aMemoryReport();
	new EventFileAuthInfo();
	new EventFileUploadFix();
	mgr_cf::SetOption("FirstStart",false);
	Debug("Dist path: %s", mgr_cf::GetPath("dist_path").c_str());
}
