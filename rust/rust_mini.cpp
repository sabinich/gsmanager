#include <api/module.h>
#include "../mini/gmmini.h"
#include "../main/defines.h"
#include <mgr/mgrproc.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrstr.h>
#include <mgr/mgruser.h>
#include <pwd.h>
#include "../mini/steammodule.h"

#define GAME_NAME "rust"
MODULE(GAME_NAME);

using namespace isp_api;

class RustMini : public SteamGameModule {
public:
	RustMini() : SteamGameModule(GAME_NAME, "hlds_linux", "rust" ) {
		PluginsPath = "rust/addons/sourcemod";
	}
	
	virtual void GetParams(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		ses.NewNode("hostname",inst.GetParam("hostname"));
		ses.NewNode("rcon_password",inst.GetParam("rconpassword"));
	}
	
	virtual void SetParams(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		inst.SetParam("hostname", ses.Param("hostname"));
		inst.SetParam("rconpassword", ses.Param("rcon_password"));
	}

	virtual void StartServerImpl(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		ScreenHandler screen(Name, id, inst.UserName);
		screen.SetWorkDir(inst.GetInstancePath());
		string run_command = string("./RustDedicated -batchmode ")+
		"+server.hostname \"FirstGAME\" "+
		"+server.ip "+inst.GetIp()+" "+
		"+server.port "+str::Str(inst.GetPort())+" "+
		"+server.identity \"qrustdat\" "+
		"+server.level \"Procedural Map\" "+
		"+server.seed 6738 "+
		"+rcon.password \""+inst.GetParam("rconpassword")+"\" "+
		"+rcon.port "+inst.GetParam("rconport")+" "+
		"+server.maxplayers "+str::Str(inst.GetSlots());
		if (screen.IsRunnig())
			StopServer(ses,id);

		Debug("Starting screen for RUST server...");

		int res = screen.Open(run_command);

		Debug("Started with result %d", res);
		inst.SetNeedRestart(false);
	}

	virtual void CreateServer(isp_api::Session & ses, const string & id) {
		SteamGameModule::CreateServer(ses, id);
		ServerInstanceInfo inst(Name, id);
		inst.SetParam("hostname", "FirstGAME");
		inst.SetParam("rconport", str::Str(PortMgr::AllocPort(inst.UserName + "_rconport", inst.GetIp()))); 
	}

	virtual void DeleteServer(isp_api::Session & ses, const string & id) {
		GameModuleBase::DeleteServer(ses, id);
		ServerInstanceInfo inst(Name, id);
		PortMgr::FreePort(inst.UserName + "_rconport");
	}

	virtual void StopServerImpl(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		ScreenHandler screen(Name, id, inst.UserName);
		Debug("Stoping screen for RUST server...");
		screen.SendCtrl_C();
		screen.Close(10);
		Debug("Stoped");
	}

	virtual void AfterReinstall(const string & id, DistInfo & dist) {
		ServerInstanceInfo inst( Name, id );
		uid_t uid = mgr_user::Uid( inst.UserName );
		string user_home_dir = getpwuid( uid )->pw_dir;
		mgr_file::Attrs attrs(0744, uid, mgr_user::GroupGid(inst.UserName));
	}

	virtual int PlayersOnline(const string& id) {
		Debug("Players online stat");
		ServerInstanceInfo inst(Name, id);
		return SteamServerQuery(inst.GetIp(), inst.GetPort()).OnlinePlayers();
	}
	
	virtual bool IsUp(const string& id) {
		ServerInstanceInfo inst(Name, id);
		ScreenHandler screen(Name, id, inst.UserName);
		return screen.IsRunnig();
	}
};

class aRustState: public ListAction {
public:
	aRustState():ListAction("rust.state",MinLevel(lvUser)){}
	void AddRow(Session & ses, const string & name, const string & value) const {
		ses.NewElem();
		ses.AddChildNode("name", name);
		ses.AddChildNode("value", value);
	}
	virtual void List(Session& ses) const {
		ServerInstanceInfo srv(ses);
		
		bool serverIsUp = GameModules[GAME_NAME]->IsUp(srv.Id);
		
		ses.NewElem();
		if (serverIsUp) {
			ses.AddChildNode("name", "server_online");
			ses.AddChildNode("status", "on");
		} else {
			ses.AddChildNode("name", "server_offline");
			ses.AddChildNode("status", "off");
		}
		
		AddRow(ses, "ip", srv.GetIp());
		AddRow(ses, "port", str::Str(srv.GetPort()));
		AddRow(ses, "rconport", srv.GetParam("rconport"));
		AddRow(ses, "players_online", serverIsUp?str::Str(GameModules[GAME_NAME]->PlayersOnline(srv.Id)):"-");
	}
	
	virtual bool CheckAccess(const isp_api::Authen& auth) const {
		if (auth.level() == mgr_access::lvUser) {
			return auth.name().find(GAME_NAME "_") == 0;
		}
		return false;
	}
};

MODULE_INIT (rustmini, "gsmini")
{
	RegModule<RustMini>();
	new aRustState;
}
