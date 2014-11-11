#include <api/module.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrlog.h>
#include "../mini/gmmini.h"
#include "../main/defines.h"
#include "../mini/steammodule.h"

#define DIST_PATH GSDISTR_PATH"left4dead2/"
#define INST_PATH GSINSTANCE_PATH"left4dead2/"

#define GAME_NAME "left4dead2"
MODULE(GAME_NAME);
using namespace isp_api;

class Left4Dead2Config : public SteamServerConfig {
public:
	Left4Dead2Config( ServerInstanceInfo& instance ) : SteamServerConfig( mgr_file::ConcatPath( instance.GetInstancePath(), "left4dead2/cfg/server.cfg" ), instance.UserName ) {
		m_conf["hostname"] = "FirstGAME Left4Dead 2 Server";
		m_conf["sv_contact"];
		m_conf["rcon_password"];
		m_conf["sv_gametypes"] = "coop,realism,survival,versus,teamversus,scavenge,teamscavenge";
		m_conf["z_difficulty"] = "normal";
		m_conf["sv_alltalk"] = "0";
		m_conf["sv_region"] = "255";
		m_conf["sv_password"] = "";
		
		m_str_params.insert("hostname");
		m_str_params.insert("rcon_passowrd");
		m_str_params.insert("sv_password");
		ReadConf();
	}
};

class Left4Dead2Mini : public SteamGameModule {
public:
	
	void BuildMapSelect(Session & ses, ServerInstanceInfo & inst) {
		
		StringList mapDirs;
		mapDirs.push_back("left4dead2/maps");
		mapDirs.push_back("left4dead2_dlc1/maps");
		mapDirs.push_back("left4dead2_dlc2/maps");
		mapDirs.push_back("left4dead2_dlc3/maps");
		mapDirs.push_back("left4dead2_lv/maps");
		ses.NewSelect("map");
		ForEachI(mapDirs, dir) {
			string path = mgr_file::ConcatPath(inst.GetInstancePath(), *dir);
			Debug("looking for maps in %s",path.c_str());
			if (!mgr_file::Exists(path)) continue;
			mgr_file::Dir MapsDir(path);
			while(MapsDir.Read()) {
				const string bsp_ext = ".bsp";
				string cur_file = MapsDir.name();
				size_t ext_pos = cur_file.rfind(bsp_ext);
				if(ext_pos != string::npos && cur_file.size() - ext_pos == bsp_ext.size())
					//maps.push_back(str::GetWord(cur_file, bsp_ext));
					ses.AddChildNode("val",str::GetWord(cur_file, bsp_ext));
			}
		}
	}
	
	Left4Dead2Mini() : SteamGameModule(GAME_NAME, "srcds_linux", "left4dead2" ) {
		PluginsPath = "left4dead2/addons/sourcemod";
	}

	virtual void GetParams(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		
		StringVector maps;
		BuildMapSelect(ses,inst);
/*		ses.BuildSelect( maps.begin(), maps.end(), "map" );*/
		ses.NewNode("map", inst.GetParam("map"));
		BuildRegionsSelect( ses );

		ses.NewSelect("z_difficulty");
		ses.AddChildNode("msg","easy");
		ses.AddChildNode("msg","normal");
		ses.AddChildNode("msg","hard");
		ses.AddChildNode("msg","impossible");

		ses.NewSelect("gamemode");
		ses.AddChildNode("msg","coop");
		ses.AddChildNode("msg","realism");
		ses.AddChildNode("msg","survival");
		ses.AddChildNode("msg","versus");
		ses.AddChildNode("msg","teamversus");
		ses.AddChildNode("msg","scavenge");
		ses.AddChildNode("msg","teamscavenge");
	
		ses.NewNode("gamemode", inst.GetParam("gamemode"));
		Left4Dead2Config conf(inst);
		conf.DebugParams();
		conf.ConfToSes(ses);
	}
	
	virtual void SetParams(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		Left4Dead2Config conf(inst);
		conf.SetParamsFromSes(ses);
		conf.Save();
		
		inst.SetParam("gamemode", ses.Param("gamemode"));
		inst.SetParam("map", ses.Param("map"));
		if (ses.Checked("restart")) {
			StartServer(ses, id);
		} else inst.SetNeedRestart();
	}

	virtual void StartServerImpl(Session& ses, const string & id) {
		ServerInstanceInfo inst( Name, id );
		ScreenHandler screen( Name, id, inst.UserName );
		screen.SetWorkDir( inst.GetInstancePath() );
		Left4Dead2Config conf( inst );
		conf["sv_lan"] = "0";
		string run_command = "./srcds_run -game " + GameFolderName
					+ " -norestart +ip " + inst.GetIp() + " +maxplayers " + str::Str(inst.GetSlots())
					+ " +map " + inst.GetParam("map") + " -port " + str::Str(inst.GetPort())
					+ " +mp_gamemode "+ inst.GetParam("gamemode")
					+ " -autoupdate -steam_dir " + mgr_file::ConcatPath(inst.GetInstancePath(), "steamcmd")
					+ " -steamcmd_script " + mgr_file::ConcatPath(inst.GetInstancePath(), "steamcmd/steamcmd_script.txt");
		
		if (screen.IsRunnig())
			StopServer(ses,id);

		Debug("Starting screen for Left4Dead server...");
		conf.Save();
		int res = screen.Open(run_command);

		Debug("Started with result %d", res);
		inst.SetNeedRestart( false );
	}


	virtual int PlayersOnline( const string& id ) {
		Debug( "Players online stat" );
		ServerInstanceInfo inst( Name, id );
		return SteamServerQuery(inst.GetIp(), inst.GetPort()).OnlinePlayers();
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
		inst.SetParam("map", "c1m1_hotel");
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

class aLeft4Dead2State: public ListAction {
public:
	aLeft4Dead2State():ListAction("left4dead2.state",MinLevel(lvUser)){}
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

MODULE_INIT (left4dead2mini, "gsmini")
{
	RegModule<Left4Dead2Mini>();
	new aLeft4Dead2State();
	//new aMapCycleList( GAME_NAME, "tf/cfg/mapcycle.txt", "tf/maps" );
}
