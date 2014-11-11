#ifndef GMMINI_H
#define GMMINI_H
#include <api/action.h>
#include "gamemodule.h"
#ifndef WIN32
#include "screen.h"
#endif
#include "portmgr.h"

string GetInstanceId(isp_api::Session& ses);

class GameModuleMini : public isp_api::FormAction
{
private:
	std::shared_ptr<GameModuleBase> m_module;
public:
	GameModuleMini(std::shared_ptr<GameModuleBase> module) : isp_api::FormAction (module->Name, mgr_access::MinLevel(mgr_access::lvUser)), m_module(module),
	StopServer(module, this),
	StartServer(module, this),
	DeleteServer(module, this),
	CreateServer(module, this),
	SetParams(module, this),
	ReinstallDist(module, this),
	Plugins(module, this)
	{
	}
	
    bool CheckAccess(const isp_api::Authen& auth) const {return m_module->CheckAccess(auth);}
	
    virtual void Get(isp_api::Session& ses, const string& elid) const
    {
		m_module->GetParams(ses,GetInstanceId(ses));
	}
	
    virtual void Set(isp_api::Session& ses, const string& elid) const
    {
		m_module->SetParams(ses, GetInstanceId(ses));
	}
	
	class aStopServer : public isp_api::FormAction {
	private:
		std::shared_ptr<GameModuleBase> m_module;
	public:
		bool CheckAccess(const isp_api::Authen& auth) const {return m_module->CheckAccess(auth);}
		aStopServer(std::shared_ptr<GameModuleBase> module, GameModuleMini * parent): isp_api::FormAction("suspend", mgr_access::MinLevel(mgr_access::lvUser),parent), m_module(module) {}
        virtual void Set(isp_api::Session& ses, const string& elid) const {
			m_module->StopServer(ses, GetInstanceId(ses));
			ses.Ok(mgr_session::BaseSession::okTop);
		}
	}StopServer;
	
	class aStartServer: public isp_api::FormAction {
	private:
		std::shared_ptr<GameModuleBase> m_module;
	public:
		bool CheckAccess(const isp_api::Authen& auth) const {return m_module->CheckAccess(auth);}
		aStartServer(std::shared_ptr<GameModuleBase> module, GameModuleMini * parent): isp_api::FormAction("resume", mgr_access::MinLevel(mgr_access::lvUser),parent), m_module(module) {}
        virtual void Set(isp_api::Session& ses, const string& elid) const
        {			
			m_module->StartServer(ses, GetInstanceId(ses));
			ses.Ok(mgr_session::BaseSession::okTop);
		}
	}StartServer;
	
	class aDeleteServer : public isp_api::Action {
	private:
		std::shared_ptr<GameModuleBase> m_module;
	public:
		aDeleteServer(std::shared_ptr<GameModuleBase> module, GameModuleMini * parent): isp_api::Action("delete",mgr_access::MinLevel(mgr_access::lvAdmin),parent), m_module(module) {}
        virtual void Execute(isp_api::Session& ses) const
        {
			m_module->DeleteServer(ses, GetInstanceId(ses));
		}
	}DeleteServer;
	
	class aCreateServer : public isp_api::Action {
	private:
		std::shared_ptr<GameModuleBase> m_module;
	public:
		aCreateServer(std::shared_ptr<GameModuleBase> module, GameModuleMini * parent): isp_api::Action("create",mgr_access::MinLevel(mgr_access::lvAdmin),parent), m_module(module) {}
        virtual void Execute(isp_api::Session& ses) const
        {
			m_module->CreateServer(ses, GetInstanceId(ses));
		}
	}CreateServer;

	class aSetParams : public isp_api::Action {
	private:
		std::shared_ptr<GameModuleBase> m_module;
	public:
		aSetParams(std::shared_ptr<GameModuleBase> module, GameModuleMini * parent): isp_api::Action("setparams",mgr_access::MinLevel(mgr_access::lvAdmin),parent), m_module(module) {}
        virtual void Execute(isp_api::Session& ses) const
        {
			ServerInstanceInfo srv(m_module->Name, GetInstanceId(ses));
			string disabled = ses.Param("disabled");
			string ip = ses.Param("ip");
			string reinit = ses.Param("reinit");
			string slots = ses.Param("slots");
			if (disabled.empty()) {
				if (!slots.empty()) srv.SetSlots(str::Int(slots));
				if (!ip.empty()) srv.SetIp(ip);
				if (m_module->IsUp(srv.Id))
					m_module->StartServer(ses, srv.Id);
				if (reinit=="on") {
					srv.ApplyRight();
					srv.ReinitPort();
				}
			} else if (disabled == "on") {
				srv.SetDisabled();
				m_module->StopServer(ses, srv.Id);
				isp_api::DropAuthen(srv.UserName);
			} else if (disabled == "off") {
				srv.SetDisabled(false);
			}
		}
	}SetParams;
	
	class aReinstallDist : public isp_api::FormAction {
	private:
		std::shared_ptr<GameModuleBase> m_module;
	public:
		bool CheckAccess(const isp_api::Authen& auth) const {return m_module->CheckAccess(auth);}
		aReinstallDist(std::shared_ptr<GameModuleBase> module, GameModuleMini * parent): isp_api::FormAction("reinstall",mgr_access::MinLevel(mgr_access::lvUser), parent), m_module(module) {}
        virtual void Get(isp_api::Session& ses, const string& elid) const
        {
			ServerInstanceInfo srv(m_module->Name, GetInstanceId(ses));
			ses.NewNode("installeddist", srv.GetParam("version"));
			ses.NewSelect("dist");
			DistInfoList list;
			GetDistInfoList(list, m_module->Name);
			ForEachI(list, i) {
				ses.AddChildNode("val", i->Name);
			}
		}
		
        virtual void Set(isp_api::Session& ses, const string& elid) const
        {
			ServerInstanceInfo srv(m_module->Name, GetInstanceId(ses));
			DistInfo newdist = GetDistInfo(srv.Game, ses.Param("dist"));
			m_module->StopServer(ses, srv.Id);
 			m_module->BeforeReinstall(srv.Id, newdist);
			if (ses.Checked("clear_on_reinstall"))
				srv.ClearInstance();
			srv.InstallDist(newdist);
			m_module->AfterReinstall(srv.Id, newdist);
		}
	} ReinstallDist;
	
	class aPlugins: public isp_api::StdListAction {
	private:
		std::shared_ptr<GameModuleBase> m_module;
	public:
		aPlugins(std::shared_ptr<GameModuleBase> module, GameModuleMini * parent): isp_api::StdListAction("plugin", mgr_access::MinLevel(mgr_access::lvUser), parent), m_module(module), Install(module, this) {}
		bool CheckAccess(const isp_api::Authen& auth) const {return m_module->CheckAccess(auth);}
        virtual void List(isp_api::Session& ses) const
        {
			m_module->PluginsList(ses,GetInstanceId(ses));
		}
		
        virtual void Get(isp_api::Session& ses, const string& elid) const
        {
			ses.Ok(mgr_session::BaseSession::okForm, "func="+m_module->Name+".plugin."+str::Lower(elid));
		}
		
		class aInstall: public isp_api::Action {
		private:
			std::shared_ptr<GameModuleBase> m_module;
		public:
			aInstall(std::shared_ptr<GameModuleBase> module, aPlugins * parent): isp_api::Action("install", mgr_access::MinLevel(mgr_access::lvUser), parent), m_module(module) {}
			bool CheckAccess(const isp_api::Authen& auth) const {return m_module->CheckAccess(auth);}
			virtual void Execute(isp_api::Session& ses) const
			{
				auto plugin = m_module->GetPlugin(ses.Param("elid"));
				if (!plugin) throw mgr_err::Error("unknown_plugin", ses.Param("elid"));
				plugin->Install(GetInstanceId(ses));
			}
		}Install;
		
		virtual void Del(isp_api::Session& ses, const string& elid) const
		{
			auto plugin = m_module->GetPlugin(elid);
			if (!plugin) throw mgr_err::Error("unknown_plugin", elid);
			plugin->Uninstall(GetInstanceId(ses));
		}
	} Plugins;
};

template <class Module>
void RegModule() {
	std::shared_ptr<Module> modptr = std::shared_ptr<Module>(new Module);
	new GameModuleMini(modptr);
	GameModules[modptr->Name] = modptr;
}

#endif