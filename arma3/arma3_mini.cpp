#include <api/module.h>
#include <api/stdconfig.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrrpc.h>
#include "../mini/gmmini.h"
#include "../main/defines.h"
#include "../mini/steammodule.h"
#include "../mini/utils.h"

#define GAME_NAME "arma3"
MODULE(GAME_NAME);
using namespace isp_api;
using namespace mgr_file;

class Arma3Config: public SimpleConfigBase{
private:
	string m_path;
	StringSet m_str_params;
	StringSet m_erase_if_empty;
public:
	Arma3Config(ServerInstanceInfo & srv, const string &config = "config.cfg")
	{
		m_path = mgr_file::ConcatPath(srv.GetInstancePath(),config);
		m_params["password"];
		m_params["hostname"];
		m_params["passwordAdmin"];
		m_str_params.insert("password");
		m_str_params.insert("passwordAdmin");
		m_str_params.insert("hostname");
		m_erase_if_empty.insert("passwordAdmin");
		m_erase_if_empty.insert("password");
	}
	virtual void ReadConf()
	{
		string data = mgr_file::Read(m_path);
		StringList lines;
		str::Split(data, "\n", lines);
		ForEachI(lines, line) {
			string tmp = str::Trim(*line);
			size_t double_slash_pos = tmp.find("//");
			if(double_slash_pos != 0) {
				tmp = str::GetWord(tmp, "//");
				str::inpl::Trim(tmp);
				size_t eq_pos = tmp.find("=");
				size_t end_pos = tmp.find(";");
				if (eq_pos == string::npos || end_pos == string::npos) {
					Warning("Can`t  parse string '%s'", line->c_str());
					continue;
				}
				string name = str::Trim(tmp.substr(0, eq_pos));
				string val = str::Trim(tmp.substr(eq_pos+1, end_pos-eq_pos-1));
				
				if (m_str_params.find(name) != m_str_params.end()) {
					if (val[0]=='\"')
						val.erase(0, 1);
					if (val[val.length()-1]=='\"')
						val.erase(val.length()-1, 1);
				}
				m_params[name] = val;
			}
		}
	}
	virtual void SaveConf()
	{
		string data = mgr_file::Read(m_path);
		StringList lines;
		str::Split(data, "\n", lines);
		ForEachI(lines, line) 
			*line = str::Trim(*line);
			
		ForEachI(m_params, param) {
			
			bool found = false;
			string val = m_str_params.find(param->first) != m_str_params.end()?"\""+param->second+"\"":param->second;
			Debug("param '%s' val '%s'",param->first.c_str(),param->second.c_str());
			ForEachI(lines, line) {
				Debug("\tline '%s'",line->c_str());
				if (line->find(param->first) == 0) {
					string tmp = *line;
					string paramname = str::GetWord(tmp);
					if (paramname == param->first) {
						found = true;
						Debug("\t\tFound");
						if (param->second.empty() && m_erase_if_empty.find(param->first) != m_str_params.end()) {
							line->clear();
						} else {
							string comments;
							size_t pos = tmp.find("//");
							if (pos != string::npos) {
								comments = tmp.substr(pos);
								comments = str::Trim(comments);
								if (!comments.empty()) comments="\t"+comments;
							}
							*line = paramname+"\t = \t"+val+";\t"+comments;
						}
						break;
					}
				}
			}
			if (!found)
				lines.push_back(param->first+"\t = \t"+val+";");
		}
		
		data.clear();
		data = str::Join(lines,"\n");
		mgr_file::Write(m_path, data+"\n");
	}
};


class Arma3Mini : public GameModuleBase {
public:

	Arma3Mini() : GameModuleBase(GAME_NAME) {
	}

	virtual bool IsUp(const string& id)
	{
		ServerInstanceInfo inst(Name,id);
		ScreenHandler screen(Name, id, inst.UserName);
		return screen.IsRunnig();
	}

	virtual int PlayersOnline(const string& id)
	{
		ServerInstanceInfo srv(GAME_NAME,id);
		SteamServerQuery sq(srv.GetIp(), str::Int(srv.GetParam("steamqueryport")));
		return sq.OnlinePlayers();
	}

	virtual void AfterReinstall(const string& id, DistInfo& dist)
	{
		ServerInstanceInfo srv(GAME_NAME,id);
		mgr_file::Attrs instanceatrs(0744, mgr_user::Uid(srv.UserName), mgr_user::GroupGid(srv.UserName));
		string path = mgr_file::ConcatPath("/home",srv.UserName,".local");
		path = mgr_file::ConcatPath(path, "share");
		mgr_file::Remove(path);
		mgr_file::MkDir(mgr_file::ConcatPath(path, "Arma 3"), &instanceatrs);
		mgr_file::MkDir(mgr_file::ConcatPath(path, "Arma 3 - Other Profiles", srv.UserName), &instanceatrs);
		mgr_file::MkLink(mgr_file::ConcatPath(srv.GetInstancePath(), "difficulty.Arma3Profile"), mgr_file::ConcatPath(path, "Arma 3 - Other Profiles", srv.UserName+"/"+srv.UserName+".Arma3Profile"));
	}

	virtual void GetParams(Session& ses, const string & id)
	{
		ServerInstanceInfo inst(Name,id);
		Arma3Config conf(inst);
		conf.ReadConf();
		conf.ConfToSes(ses);
		ses.NewNode("addparams",inst.GetParam("additional_params"));
	}

	virtual void SetParams(Session& ses, const string & id)
	{
		ServerInstanceInfo inst(Name,id);
		Arma3Config conf(inst);
		conf.ReadConf();
		conf.SetParansFormSes(ses);
		conf.SaveConf();
		inst.SetParam("additional_params",ses.Param("addparams"));
		inst.SetNeedRestart();
		if (ses.Checked("restart")) {
			StartServer(ses, id);
		}
	}

	virtual void StartServerImpl(Session& ses, const string & id)
	{
		Debug("StartServer");
		ServerInstanceInfo inst(Name,id);
		ScreenHandler screen(Name, id, inst.UserName);
		Arma3Config conf(inst);
		conf.ReadConf();
		conf.SetParam("steamport", inst.GetParam("steamport"));
		conf.SetParam("steamqueryport", inst.GetParam("steamqueryport"));
		conf.SetParam("maxPlayers", str::Str(inst.GetSlots()));
		conf.SaveConf();
		try {
			if(screen.IsRunnig())
				StopServer(ses, id);
			screen.SetWorkDir(inst.GetInstancePath());
			string run_cmd = "./arma3server";
			run_cmd += " -port="+str::Str(inst.GetPort());
			run_cmd += " -name="+inst.UserName;
			run_cmd += " -ip="+inst.GetIp();
			run_cmd += " -config=config.cfg";
			run_cmd += " -noSound";
			string add_params = inst.GetParam("additional_params");
			if (!add_params.empty())
				run_cmd += " "+add_params;
			screen.Open(run_cmd);
		} catch (...) {}
		inst.SetNeedRestart(false);
	}

	virtual void StopServerImpl(Session& ses, const string & id)
	{
		Debug("StopServer");
		ServerInstanceInfo inst(Name,id);
		ScreenHandler screen(Name, id, inst.UserName);
		screen.SendCtrl_C();
		mgr_proc::Sleep(2000);
		screen.Close(10);
	}

	virtual void DeleteServer(Session& ses, const string & id)
	{
		ServerInstanceInfo srv(GAME_NAME, id);
		PortMgr::FreePort(srv.UserName);
		GameModuleBase::DeleteServer(ses, id);
	}
	
	virtual void CreateServer(Session& ses, const string& id)
	{
		m_AllocPortDecade = true;
		GameModuleBase::CreateServer(ses,id);
		
		string username = Name+"_"+id;
		ServerInstanceInfo srv(username);
		int steamport = srv.GetPort() + 5;
		int steamqueryport = srv.GetPort() + 6;
		srv.SetParam("steamport", str::Str(steamport));
		srv.SetParam("steamqueryport", str::Str(steamqueryport));
	}
};

class aArma3State: public StdListAction {
public:
	aArma3State():StdListAction("arma3.state",MinLevel(lvUser)){}
	void AddRow(Session & ses, const string & name, const string & value) const
	{
		ses.NewElem();
		ses.AddChildNode("name", name);
		ses.AddChildNode("value", value);
	}
	virtual void List(Session& ses) const
	{
		ServerInstanceInfo srv(ses);

		bool serverIsUp = GameModules[GAME_NAME]->IsUp(srv.Id);

		AddRow(ses, "version", srv.GetParam("version"));
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

	virtual bool CheckAccess(const isp_api::Authen& auth) const
	{
		if (auth.level() == mgr_access::lvUser) 
		{
			return auth.name().find(GAME_NAME "_") == 0;
		}
		return false;
	}
};

MODULE_INIT (arma3mini, "gsmini")
{
	RegModule<Arma3Mini>();
	new aArma3State();
}

#undef GAME_NAME
