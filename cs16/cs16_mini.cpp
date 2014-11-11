#include <api/module.h>
#include "../mini/gmmini.h"
#include "../main/defines.h"
#include <mgr/mgrproc.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrstr.h>
#include <mgr/mgruser.h>
#include <pwd.h>
#include "../mini/steammodule.h"

#define GAME_NAME "cs16"
MODULE(GAME_NAME);

using namespace isp_api;

class CS16Config : public SteamServerConfig {
public:
	CS16Config( ServerInstanceInfo& instance ) : SteamServerConfig( mgr_file::ConcatPath( instance.GetInstancePath(), "cstrike/server.cfg" ), instance.UserName ) {
		m_conf["hostname"] = "\"Counter-Strike 1.6 Server\"";
		m_conf["rcon_password"];
		m_conf["sv_password"];
		m_conf["sv_region"] = "255";
		m_conf["mp_timelimit"] = "25";
		m_conf["mp_autokick"] = "1";
		m_conf["mp_autoteambalance"] = "0";
		m_conf["mp_c4timer"] = "45";
		m_conf["mp_freezetime"] = "6";
		m_conf["mp_friendlyfire"] = "0";
		m_conf["mp_hostagepenalty"] = "5";
		m_conf["mp_limitteams"] = "2";
		m_conf["mp_roundtime"] = "5";
		m_conf["allow_spectators"] = "0";
		m_conf["mp_startmoney"] = "800";
		m_conf["pausable"] = "0";
		m_conf["mp_falldamage"] = "1";
		ReadConf();
	}
};

class CS16Mini : public SteamGameModule {
public:
	CS16Mini() : SteamGameModule(GAME_NAME, "hlds_linux", "cstrike" ) {
		PluginsPath = "cstrike/addons/sourcemod";
	}
	
	virtual void GetParams(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		CS16Config conf(inst);
		conf.ConfToSes(ses);

		StringVector maps;
		GetMaps( inst, maps );
		ses.BuildSelect( maps.begin(), maps.end(), "map" );
		BuildRegionsSelect( ses );
		ses.NewNode( "map", inst.GetParam( "map" ) );
	}
	
	virtual void SetParams(Session& ses, const string & id) {
		static char string_params[][20] = { "hostname", "rcon_passowrd", "sv_password" };
		StringVector allparams;
		ses.GetParams(allparams);
		ServerInstanceInfo inst(Name, id);
		CS16Config conf(inst);
		for( size_t i = 0; i < sizeof( string_params ); ++i )
			if( !conf[string_params[i]].empty() && ( conf[string_params[i]][0] != '"' || conf[string_params[i]][conf[string_params[i]].length()-1] != '"' ) )
				conf[string_params[i]] = "\"" + conf[string_params[i]] + "\"";
		inst.SetParam( "map", ses.Param( "map" ) );
		conf.SetParamsFromSes(ses);
		conf.Save();
		if (ses.Checked("restart")) {
			StartServer(ses, id);
		} else inst.SetNeedRestart();
	}

	virtual void StartServerImpl(Session& ses, const string & id) {
		ServerInstanceInfo inst( Name, id );
		ScreenHandler screen( Name, id, inst.UserName );
		screen.SetWorkDir( inst.GetInstancePath() );
		CS16Config conf( inst );
		string run_command = "./hlds_run -console +sv_lan 0 -game " + GameFolderName + " -sport " + inst.GetParam("vacport")
					+ " -norestart +map " + inst.GetParam( "map" ) + " +ip " + inst.GetIp() + " +maxplayers " + str::Str(inst.GetSlots())
					+ " -port " + str::Str(inst.GetPort())+" "+inst.GetParam("additional_params");

		if (screen.IsRunnig())
			StopServer(ses,id);

		Debug("Starting screen for CS server...");

		int res = screen.Open(run_command);

		Debug("Started with result %d", res);
		inst.SetNeedRestart( false );
	}

	virtual void CreateServer(isp_api::Session & ses, const string & id) {
		SteamGameModule::CreateServer( ses, id );
		ServerInstanceInfo inst( Name, id );
		inst.SetParam( "vacport", str::Str( PortMgr::AllocPort( inst.UserName + "_vacport", inst.GetIp() ) ) ); 
	}

	virtual void DeleteServer(isp_api::Session & ses, const string & id) {
		GameModuleBase::DeleteServer( ses, id );
		ServerInstanceInfo inst( Name, id );
		PortMgr::FreePort( inst.UserName + "_vacport" );
	}

	virtual void StopServerImpl(Session& ses, const string & id) {
		ServerInstanceInfo inst( Name, id );
		ScreenHandler screen( Name, id, inst.UserName );
		Debug("Stoping screen for CS server...");
		screen.SendCtrl_C();
		screen.Close(10);
		Debug("Stoped");
	}

	virtual void AfterReinstall(const string & id, DistInfo & dist) {
		ServerInstanceInfo inst( Name, id );
		uid_t uid = mgr_user::Uid( inst.UserName );
		string user_home_dir = getpwuid( uid )->pw_dir;
		mgr_file::Attrs attrs(0744, uid, mgr_user::GroupGid(inst.UserName));
		const string link_dir = mgr_file::ConcatPath( user_home_dir, ".steam/sdk32/" );
		mgr_file::MkDir(link_dir, &attrs);
		string link_name = mgr_file::ConcatPath(link_dir, "steamclient.so");
		if (mgr_file::Exists(link_name)) 
			mgr_file::RemoveFile(link_name);
		mgr_file::MkLink(mgr_file::ConcatPath( inst.GetInstancePath(), "steamclient.so"), link_name);
		Debug( "Link to steamclient.so created" );
		StringVector maps;
		GetMaps( inst, maps );
		if( !maps.empty() )
			inst.SetParam( "map", maps.front() );
	}

	virtual int PlayersOnline( const string& id ) {
		Debug( "Players online stat" );
		ServerInstanceInfo inst( Name, id );
		return SteamServerQuery( inst.GetIp(), inst.GetPort() ).OnlinePlayers();
	}
	
	virtual bool IsUp(const string& id) {
		ServerInstanceInfo inst(Name, id);
		if (mgr_proc::Execute("pgrep -U " + inst.UserName + " hlds_linux").Result() == 0) return true;
		if (mgr_proc::Execute("pgrep -U " + inst.UserName + " hlds_i686").Result() == 0) return true;
		return false;
	}
};

class aCS16State: public ListAction {
public:
	aCS16State():ListAction("cs16.state",MinLevel(lvUser)){}
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
		AddRow(ses, "players_online", serverIsUp?str::Str(GameModules[GAME_NAME]->PlayersOnline(srv.Id)):"-");
	}
	
	virtual bool CheckAccess(const isp_api::Authen& auth) const {
		if (auth.level() == mgr_access::lvUser) {
			return auth.name().find(GAME_NAME "_") == 0;
		}
		return false;
	}
};

MODULE_INIT (cs16mini, "gsmini")
{
	RegModule<CS16Mini>();
	new aCS16State();
	new aMapCycleList( GAME_NAME, "cstrike/mapcycle.txt", "cstrike/maps/" );
}
