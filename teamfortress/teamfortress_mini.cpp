#include <api/module.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrlog.h>
#include "../mini/gmmini.h"
#include "../main/defines.h"
#include "../mini/steammodule.h"

#define DIST_PATH GSDISTR_PATH"teamfortress/"
#define INST_PATH GSINSTANCE_PATH"teamfortress/"

#define GAME_NAME "teamfortress"
MODULE(GAME_NAME);
using namespace isp_api;

class TeamFortress2Config : public SteamServerConfig {
public:
	TeamFortress2Config( ServerInstanceInfo& instance ) : SteamServerConfig( mgr_file::ConcatPath( instance.GetInstancePath(), "tf/cfg/server.cfg" ), instance.UserName ) {
		m_conf["hostname"] = "FirstGAME Team Fortress 2 Server";
		m_conf["sv_contact"];
		m_conf["rcon_password"];
		m_conf["sv_region"] = "255";
		m_conf["sv_pausable"] = "0";
		m_conf["mp_timelimit"] = "0";
		m_conf["mp_winlimit"] = "0";
		m_conf["mp_bonusroundtime"] = "15";
		m_conf["mp_maxrounds"] = "0";
		m_conf["mp_allowspectators"] = "1";
		m_conf["mp_autoteambalance"] = "1";
		m_conf["mp_teams_unbalance_limit"] = "2";
		m_conf["mp_falldamage"] = "0";
		m_conf["mp_forcecamera"] = "1";
		m_conf["mp_idlemaxtime"] = "3";
		
		m_str_params.insert("hostname");
		m_str_params.insert("rcon_passowrd");
		m_str_params.insert("sv_password");
		ReadConf();
	}
};

class TeamFortressMini : public SteamGameModule {
public:
	TeamFortressMini() : SteamGameModule(GAME_NAME, "srcds_linux", "tf" ) {
		PluginsPath = "tf/addons/sourcemod";
	}

	virtual void GetParams(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		TeamFortress2Config conf(inst);
		
		StringVector maps;
		GetMaps( inst, maps );
		ses.BuildSelect( maps.begin(), maps.end(), "map" );
		ses.NewNode( "map", inst.GetParam( "map" ) );
		BuildRegionsSelect( ses );

		conf.ConfToSes(ses);
	}
	
	virtual void SetParams(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		TeamFortress2Config conf(inst);
		conf.SetParamsFromSes(ses);

		conf.Save();
		inst.SetParam( "map", ses.Param( "map" ) );
		if (ses.Checked("restart")) {
			StartServer(ses, id);
		} else inst.SetNeedRestart();
	}

	virtual void StartServerImpl(Session& ses, const string & id) {
		ServerInstanceInfo inst( Name, id );
		ScreenHandler screen( Name, id, inst.UserName );
		screen.SetWorkDir( inst.GetInstancePath() );
		TeamFortress2Config conf( inst );
		conf["sv_lan"] = "0";
		string run_command = "./srcds_run -game " + GameFolderName
					+ " -insecure -norestart -nohltv +ip " + inst.GetIp() + " +maxplayers " + str::Str( inst.GetSlots() )
					+ " +map " + inst.GetParam( "map" ) + " -port " + str::Str( inst.GetPort() )
					+ " -autoupdate -steam_dir " + mgr_file::ConcatPath( inst.GetInstancePath(), "steamcmd" )
					+ " -steamcmd_script " + mgr_file::ConcatPath( inst.GetInstancePath(), "steamcmd_script.txt" );
		
		if (screen.IsRunnig())
			StopServer(ses,id);

		Debug("Starting screen for CS server...");
		conf.Save();
		int res = screen.Open(run_command);

		Debug("Started with result %d", res);
		inst.SetNeedRestart( false );
	}


	virtual int PlayersOnline( const string& id ) {
		Debug( "Players online stat" );
		ServerInstanceInfo inst( Name, id );
		return SteamServerQuery( inst.GetIp(), inst.GetPort() ).OnlinePlayers();
	}

	virtual void AfterReinstall(const string & id, DistInfo & dist) {
		ServerInstanceInfo inst( Name, id );
		uid_t uid = mgr_user::Uid( inst.UserName );
		string user_home_dir = getpwuid( uid )->pw_dir;
		mgr_file::Attrs attrs(0744, uid, mgr_user::GroupGid(inst.UserName));
		const string link_dir = mgr_file::ConcatPath( user_home_dir, ".steam/sdk32/" );
		mgr_file::MkDir( link_dir, &attrs );
		mgr_file::MkLink( mgr_file::ConcatPath( inst.GetInstancePath(), "bin/steamclient.so" ),
					mgr_file::ConcatPath( link_dir, "steamclient.so" ) );
		Debug( "Link to steamclient.so created" );
		
		StringVector maps;
		GetMaps(inst, maps);
		if( !maps.empty() )
			inst.SetParam( "map", maps.front() );

	}

	virtual void StopServerImpl(Session& ses, const string & id) {
		ServerInstanceInfo inst( Name, id );
		ScreenHandler screen( Name, id, inst.UserName );

		Debug("Team fortress server is stoping now.");
		screen.SendCtrl_C();
		screen.Close(10);
		Debug("Closed now.");
	}
};


class aTF2State: public ListAction {
public:
	aTF2State():ListAction("teamfortress.state",MinLevel(lvUser)){}
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

MODULE_INIT (teamfortressmini, "gsmini")
{
	RegModule<TeamFortressMini>();
	new aTF2State();
	new aMapCycleList( GAME_NAME, "tf/cfg/mapcycle.txt", "tf/maps" );
}
