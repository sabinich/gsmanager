#ifndef GMAMEMODULE_H
#define GMAMEMODULE_H
#include <api/action.h>
#include <mgr/mgrfile.h>
#include <mgr/mgruser.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrstr.h>
#include "portmgr.h"
#include "distr.h"
#include "instance.h"
class GameModuleBase;
typedef std::map< string, std::shared_ptr<GameModuleBase> > ModulesMap;

extern ModulesMap GameModules;

struct ServerState {
	string Id;
	string Game;
	int Slots;
	int PlayersOnline;
	int RamUsed;
	int ProcUsed;
	string Status;
};
typedef std::list<ServerState> ServerStateList;

std::map<string, int> GetDiskQuota();

class GamePlugin {
protected:
	GameModuleBase & m_module;
public:
	GamePlugin(GameModuleBase & module):m_module(module){};
	string PluginPath;
	string Name;
	string ExecutableName;
	virtual void GetParams(isp_api::Session & ses, const string & id){};
	virtual void SetParams(isp_api::Session & ses, const string & id){};
	virtual void Install(const string & id);
	virtual void Uninstall(const string & id);
	virtual bool IsInstalled(const string & id);
};

// Базовый класс для игровых модулей
class GameModuleBase {
	friend class GameModuleMini;
protected:
	
	std::map<string, std::shared_ptr<GamePlugin> > Plugins;
	std::shared_ptr<GamePlugin> DefaultPluginHandler;
	class PluginEditAction : isp_api::FormAction {
	protected:
		GameModuleBase * m_module;
	public:
		string Name;
		PluginEditAction(const string & name, const string & actionname, GameModuleBase * module) :isp_api::FormAction(actionname, isp_api::MinLevel(mgr_access::lvUser)), m_module(module), Name(name){}
		virtual void Get(isp_api::Session& ses, const string& elid) const
		{
			ServerInstanceInfo srv(ses);
			std::shared_ptr<GamePlugin> plugin = m_module->GetPlugin(Name);
			if (!plugin) throw mgr_err::Error("unknown_plugin", Name);
			plugin->GetParams(ses, srv.Id);
		}
		
		virtual void Set(isp_api::Session& ses, const string& elid) const
		{
			ServerInstanceInfo srv(ses);
			std::shared_ptr<GamePlugin> plugin = m_module->GetPlugin(Name);
			if (!plugin) throw mgr_err::Error("unknown_plugin", Name);
			plugin->SetParams(ses, srv.Id);
			srv.SetNeedRestart();
		}
	};
	bool m_AllocPortDecade;
public:
	mgr_thread::SafeLock StatusLock;
	string Name;
	string PluginsPath;
	bool CheckAccess(const isp_api::Authen& auth) {
		if (auth.level() == mgr_access::lvUser) 
		{
			return auth.name().find(Name+"_") == 0;
		}
		return true;
	}
	std::shared_ptr<GamePlugin> GetPlugin(string name)
	{
		string upname = str::Upper(name);
		if (Plugins.find(upname) != Plugins.end()) {
			return Plugins[upname];
		}
		return GetDefaultPluginHandler(name);
	}
	
	virtual std::shared_ptr<GamePlugin> GetDefaultPluginHandler(string name) 
	{
		return std::shared_ptr<GamePlugin>();
	}
	
	GameModuleBase(const string & name) : m_AllocPortDecade(false), Name(name), PluginsPath("plugins") {
		
	}
	template <class PlClass>
	void RegisterPlugin(string name) {
		string upname = str::Upper(name);
		Plugins[upname] = std::shared_ptr<GamePlugin>(new PlClass(*this));
		Plugins[upname]->Name = name;
		string actionname = Name+".plugin."+str::Lower(name);
		new PluginEditAction(name, actionname, this);
	}
	
	virtual void SetParams(isp_api::Session & ses, const string & id) = 0; // Вызывается при изменении настроек игрового сервера
	virtual void GetParams(isp_api::Session & ses, const string & id) = 0; // Вызывается при вывоед настроек игрового сервер пользователю
	
	void StartServer(isp_api::Session & ses, const string & id);
	void StopServer(isp_api::Session & ses, const string & id);
	
	virtual void StartServerImpl(isp_api::Session & ses, const string & id) = 0; // Запусе игрового сервера
	virtual void StopServerImpl(isp_api::Session & ses, const string & id) = 0; // Остановка игрового сервера
	
	virtual void CreateServer(isp_api::Session & ses, const string & id); // Создание игрового сервера
	virtual void DeleteServer(isp_api::Session & ses, const string & id); // Удаление игрового сервера
	virtual void BeforeReinstall(const string & id, DistInfo & dist){}; // Выполняется перед переустановкой сервера
	virtual void AfterReinstall(const string & id, DistInfo & dist){}; // Выполняется после переустановки сервера
	
	virtual void PluginsList(isp_api::Session & ses, const string & id); // Выводит список плагинов
	
	virtual std::list< std::shared_ptr<mgr_thread::Handle> >  SaveServersStatus();
	virtual void GetServersStatus(isp_api::Session & ses);
	virtual void CheckServerState(isp_api::Session & ses);
	virtual bool IsUp(const string & id){return false;};	// Выводит состояние игрового сервера
	virtual int PlayersOnline(const string & id){return 0;}; // Выводи количество игроков на игровом сервере
#ifdef WIN32
	void CreateWindowsUser(const string & name);
	void DeleteWindowsUser(const string & name);
#endif
};

#endif
