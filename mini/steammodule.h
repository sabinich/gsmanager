#ifndef STEAMMODULE_H
#define STEAMMODULE_H

#include "gamemodule.h"
#include <mgr/mgrnet.h>

class SteamGameModule : public GameModuleBase {
public:
	class pShowDamage : public GamePlugin {
	public:
		pShowDamage(GameModuleBase & module) : GamePlugin(module) {
			ExecutableName = "plugins/showdamage.smx";
		}
		void SetParams(isp_api::Session & ses, const string & id) {
			ServerInstanceInfo inst(m_module.Name,id);
			mgr_file::Write( GetConfigPath( inst ), ses.Param("config") );
			inst.NeedRestart();
		};
		void GetParams(isp_api::Session & ses, const string & id) {
			ServerInstanceInfo inst( m_module.Name, id );
			string current_conf = mgr_file::ReadE( GetConfigPath( inst ) );
			if( current_conf.empty() ) {
				//default config
				current_conf = "sm_show_damage 1\n"
						"sm_show_damage_ff 0\n"
						"sm_show_damage_own_dmg 0\n"
						"sm_show_damage_text_area 1\n";
			}
			ses.NewNode( "config", current_conf );
		};
		virtual void Uninstall( const string& id ) {
			GamePlugin::Uninstall( id );
			ServerInstanceInfo inst( m_module.Name, id );
			string dir = mgr_file::ConcatPath( inst.GetInstancePath(), m_module.PluginsPath );
			mgr_file::RemoveFile( mgr_file::ConcatPath( dir, "translations/showdamage.phrases.txt" ) );
			mgr_file::RemoveFile( mgr_file::ConcatPath( dir, "scripting/showdamage.sp" ) );
		}
	private:
		string GetConfigPath( ServerInstanceInfo & inst ) {
			return mgr_file::ConcatPath( inst.GetInstancePath(), reinterpret_cast<SteamGameModule*>( &m_module )->GameFolderName, "cfg/sourcemod/plugin.showdamage.cfg" );
		}
	};

	class pREGEXWordFilter : public GamePlugin {
	public:
		pREGEXWordFilter(GameModuleBase & module) : GamePlugin(module) {
			ExecutableName = "plugins/sm_regexfilter.smx";
		}
		void SetParams(isp_api::Session & ses, const string & id) {
			ServerInstanceInfo inst(m_module.Name,id);
			mgr_file::Write( GetConfigPath( inst ), ses.Param("config") );
			inst.NeedRestart();
		};
		void GetParams(isp_api::Session & ses, const string & id) {
			ServerInstanceInfo inst( m_module.Name, id );
			ses.NewNode( "config", GetConfigPath( inst ) );
		};
	private:
		string GetConfigPath( ServerInstanceInfo & inst ) {
			return mgr_file::ConcatPath( inst.GetInstancePath(), m_module.PluginsPath, "config/regexrestrict.cfg" );
		}
	};
	
	const string GameFolderName;

	SteamGameModule(const string& name, const string& srv_proc_name, const string& game_folder_name ) : GameModuleBase(name),
														GameFolderName( game_folder_name ),
														ServerProcName( srv_proc_name ) {
		RegisterPlugin<pShowDamage>( "ShowDamage" );
		RegisterPlugin<pREGEXWordFilter>( "REGEXWordFilter" );
	}
	virtual void SetParams(isp_api::Session & ses, const string & id) = 0;

	virtual void CreateServer( isp_api::Session& ses, const string& id );
	bool IsUp( const string& srv_id );
protected:
	const string ServerProcName;
	void GetMaps( ServerInstanceInfo& instance, StringVector& maps );
	void BuildRegionsSelect( isp_api::Session& ses );
};

class SteamServerQuery {
public:
	SteamServerQuery(string ip, int port);
	int OnlinePlayers();
	int OnlinePlayersArma();
	~SteamServerQuery();
private:
#ifdef WIN32
	SOCKET SocketDesc;
#else
	int SocketDesc;
#endif
	mgr_net::SockAddr ServerAddress;
	void SendToSocket(const string& message);
	void RecieveFromSocket(string& answer);
};


class SteamServerConfig {
public:
	SteamServerConfig( const string& cfg_path, const string& user_name );
	void ReadConf();

	void SetParams(StringMap & params);
	virtual void SetParamsFromSes(isp_api::Session & ses);
	virtual void ConfToSes(isp_api::Session & ses);
	void SetParam(const string & param, const string & value);

	void Save();
	
	string GetParam(const string & param);
	
	string &operator[] (const string & param);
	void DebugParams();
protected:
	string m_path;
	string m_user;
	StringMap m_conf;
	StringSet m_str_params;
	StringSet m_num_params;
};

class aMapCycleList : public isp_api::ListAction {
public:
	aMapCycleList( const string& game, const string& cfg_relative_path, const string& relative_maps_path );

	void List ( isp_api::Session& ses ) const;
	virtual bool CheckAccess( const isp_api::Authen& ) const;

	const string& GetGameName() const;
	const string& GetCfgRelativePath() const;
	const string& GetRelativeMapsPath() const;
private:
	class aMapCycleEdit : public isp_api::FormAction {
	public:
		aMapCycleEdit( aMapCycleList * parent );

		virtual void Get( isp_api::Session& ses, const string& elid ) const;

		virtual void Set( isp_api::Session& ses, const string& elid ) const;
		virtual bool CheckAccess( const isp_api::Authen& ) const;
	} aMapCycleEditObj;

	class aMapCycleDelete : public isp_api::Action {
	public:
		aMapCycleDelete( aMapCycleList * parent );

		virtual void Execute( isp_api::Session& ses ) const;
		virtual bool CheckAccess( const isp_api::Authen& ) const;
	} aMapCycleDeleteObj;

	class aMapCycleMove : public isp_api::Action {
	public:
		enum Direction{
			dirUP,
			dirDOWN
		};

		aMapCycleMove( aMapCycleList * parent, Direction dir );

		virtual void Execute( isp_api::Session& ses ) const;
		virtual bool CheckAccess( const isp_api::Authen& ) const;
	private:
		Direction MoveDirection; 
	} aMapCycleUpObj, aMapCycleDownObj;

	const string GameName, CfgRelativePath, RelativeMapsPath;
};

#endif
