#include <api/module.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrlog.h>
#include "../mini/gmmini.h"
#include "../main/defines.h"
#include "../mini/steammodule.h"

#define DIST_PATH GSDISTR_PATH"csgo/"
#define INST_PATH GSINSTANCE_PATH"csgo/"

#define GAME_NAME "csgo"
MODULE(GAME_NAME);
using namespace isp_api;


class CSGOConfig : public SteamServerConfig {
private:
	
public:
	CSGOConfig(ServerInstanceInfo & instance, const string & confname): SteamServerConfig(mgr_file::ConcatPath(instance.GetInstancePath(), "csgo/cfg", confname), instance.UserName) {
		m_num_params.insert("bot_difficulty");
		m_num_params.insert("bot_quota");
		m_num_params.insert("mp_maxrounds");
		m_num_params.insert("mp_buytime");
		m_num_params.insert("mp_freezetime");
		m_num_params.insert("mp_maxmoney");
		m_num_params.insert("mp_roundtime");
	};
	
    void ConfToSesGameMode(Session& ses, const string & gamemode)
	{
		ForEachI(m_conf, param) {
			string val = param->second; 
			if (m_num_params.find(param->first) == m_num_params.end()) {
				if (val == "1") val = "on";
				if (val == "0") val = "off";
			}
			ses.NewNode((gamemode+"_"+param->first).c_str(), val);
		}
	}
	
    virtual void SetParamsFromSesGameMode(Session& ses, const string & gamemode)
	{
		StringVector params;
		ses.GetParams(params);
		
		ForEachI(params, i) {
			string param = *i;
			string mode = str::GetWord(param,"_");
			if (mode != gamemode) continue;
			if(*i == "map") continue;
			Debug("Set [%s]", i->c_str());
			string val = ses.Param(*i);
			if (m_conf.find(param) != m_conf.end()) {
				if (val == "on") val = "1";
				if (val == "off") val ="0";
				m_conf[param] = val;
			}
		}
	}
};

class CSGOServerConfig : public CSGOConfig {
public:
    CSGOServerConfig(ServerInstanceInfo& instance) : CSGOConfig(instance,"server.cfg") {
		m_conf["hostname"] = "1stGAME CS:Global Offensive Server";
		m_conf["rcon_password"];
		m_conf["sv_password"];
		
		m_str_params.insert("hostname");
		m_str_params.insert("rcon_password");
		m_str_params.insert("sv_password");
		ReadConf();
	}
};

class CsGOMini : public SteamGameModule {
public:
	CsGOMini() : SteamGameModule(GAME_NAME, "srcds_linux", "csgo" ) {
		PluginsPath = "csgo/addons/sourcemod";
	}

	virtual void GetParams(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		
		ses.NewSelect("gamemode");
		ses.AddChildNode("msg","competitive");
		ses.AddChildNode("msg","casual");
		ses.AddChildNode("msg","deathmatch");
		ses.AddChildNode("msg","armrace");
		ses.AddChildNode("msg","demolition");
		
		CSGOConfig compcfg(inst, "gamemode_competitive.cfg");
		compcfg.ReadConf();
		compcfg.ConfToSesGameMode(ses, "competitive");

		CSGOConfig demolcfg(inst, "gamemode_demolition.cfg");
		demolcfg.ReadConf();
		demolcfg.ConfToSesGameMode(ses, "demolition");

		CSGOConfig armcfg(inst, "gamemode_armsrace.cfg");
		armcfg.ReadConf();
		armcfg.ConfToSesGameMode(ses, "armrace");

		CSGOConfig deathcfg(inst, "gamemode_deathmatch.cfg");
		deathcfg.ReadConf();
		deathcfg.ConfToSesGameMode(ses, "deathmatch");

		CSGOConfig casualcfg(inst, "gamemode_casual.cfg");
		casualcfg.ReadConf();
		casualcfg.ConfToSesGameMode(ses, "casual");
		
		CSGOServerConfig servercfg(inst);
		servercfg.ReadConf();
		servercfg.ConfToSes(ses);
		
		ses.NewNode("gamemode", inst.GetParam("gamemode"));
	}
	
	virtual void SetParams(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		
		CSGOConfig compcfg(inst, "gamemode_competitive.cfg");
		compcfg.ReadConf();
		compcfg.SetParamsFromSesGameMode(ses, "competitive");
		compcfg.Save();

		CSGOConfig demolcfg(inst, "gamemode_demolition.cfg");
		demolcfg.ReadConf();
		demolcfg.SetParamsFromSesGameMode(ses, "demolition");
		demolcfg.Save();

		CSGOConfig armcfg(inst, "gamemode_armsrace.cfg");
		armcfg.ReadConf();
		armcfg.SetParamsFromSesGameMode(ses, "armrace");
		armcfg.Save();

		CSGOConfig deathcfg(inst, "gamemode_deathmatch.cfg");
		deathcfg.ReadConf();
		deathcfg.SetParamsFromSesGameMode(ses, "deathmatch");
		deathcfg.Save();

		CSGOConfig casualcfg(inst, "gamemode_casual.cfg ");
		casualcfg.ReadConf();
		casualcfg.SetParamsFromSesGameMode(ses, "casual");
		casualcfg.Save();
		
		CSGOServerConfig servercfg(inst);
		servercfg.ReadConf();
		servercfg.SetParam("hostname", ses.Param("hostname"));
		servercfg.SetParam("rcon_password", ses.Param("rcon_password"));
		servercfg.SetParam("sv_password", ses.Param("sv_password"));
		servercfg.Save();

		inst.SetParam("gamemode", ses.Param("gamemode"));
		if (ses.Checked("restart")) {
			StartServer(ses, id);
		} else inst.SetNeedRestart();
	}

	virtual void StartServerImpl(Session& ses, const string & id) {
		ServerInstanceInfo inst( Name, id );
		ScreenHandler screen( Name, id, inst.UserName );
		screen.SetWorkDir( inst.GetInstancePath() );

		int type=0;
		int mode=0;
		
		string gamemode = inst.GetParam("gamemode");
		string mapgroup;
		if (gamemode=="casual") {
			type = 0;
			mode = 0;
			mapgroup = "mg_bomb";
		} else if (gamemode=="competitive") {
			type = 0;
			mode = 1;
			mapgroup = "mg_bomb_se";
		} else if (gamemode=="armrace") {
			type = 1;
			mode = 0;
			mapgroup = "mg_armsrace";
		} else if (gamemode=="demolition") {
			type = 1;
			mode = 1;
			mapgroup = "mg_demolition";
		} else if (gamemode=="deathmatch") {
			type = 1;
			mode = 2;
			mapgroup = "mg_allclassic";
		}
		
		string gamemodestr="+game_type "+str::Str(type)+" +game_mode "+str::Str(mode);
		
		string run_command = "./srcds_run -game " + GameFolderName
					+ " -console -usercon -norestart +ip " + inst.GetIp() + " +maxplayers " + str::Str(inst.GetSlots())
					+ " -port " + str::Str(inst.GetPort())
					+ " -autoupdate -steam_dir " + mgr_file::ConcatPath(inst.GetInstancePath(), "steamcmd")
					+ " -steamcmd_script " + mgr_file::ConcatPath(inst.GetInstancePath(), "steamcmd/steamcmd_script.txt")
					+ " "+gamemodestr;
					//+ " "+gamemodestr+" +mapgroup "+mapgroup;
		
		if (screen.IsRunnig())
			StopServer(ses,id);

		Debug("Starting screen for CS GO server...");
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
	}

	virtual void StopServerImpl(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		ScreenHandler screen(Name, id, inst.UserName);

		Debug("CSGO server is stoping now.");
		screen.SendCtrl_C();
		screen.Close(10);
		Debug("Closed now.");
	}
};


class aCSGOState: public ListAction {
public:
	aCSGOState():ListAction("csgo.state",MinLevel(lvUser)){}
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

MODULE_INIT (csgomini, "gsmini")
{
	RegModule<CsGOMini>();
	new aCSGOState();
}
