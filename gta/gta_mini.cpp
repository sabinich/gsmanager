#include <api/module.h>
#include <mgr/mgrfile.h>
#include "../mini/gmmini.h"
#include "../main/defines.h"
#include <api/action.h>
#include <sstream>
#include <classes.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrnet.h>
#include "mgr/mgrrpc.h"
#include <cstring>
#include <netinet/in.h>
#include <algorithm>
#include <mgr/mgrtest.h>

#define GAME_NAME "gta"
MODULE(GAME_NAME);

using namespace std;
using namespace isp_api;

const string GTAExecutable = "samp03svr";
const string GamemodeStr = "gamemode";

class GTAConfig {
private:
	string m_path;
	string m_user;
	StringMap m_conf;

	bool IsBoolField(const string& field_name) {
		static StringSet int_bool_filter, special_bool_filter;
		if (int_bool_filter.empty() && special_bool_filter.empty()) {
			int_bool_filter.insert("lan-mode");
			int_bool_filter.insert("announce");
			int_bool_filter.insert("query");
			int_bool_filter.insert("rcon");
			int_bool_filter.insert("logqueries");
			int_bool_filter.insert("chatlogging");

			special_bool_filter.insert("timestamp");
			special_bool_filter.insert("output");
		}
		return int_bool_filter.end() != int_bool_filter.find(field_name) || special_bool_filter.end() != special_bool_filter.find(field_name);
	}

	void ConvertToBool(string& field_name) {
		static StringSet special_bool_filter;
		if(special_bool_filter.empty()) {
			special_bool_filter.insert("timestamp");
			special_bool_filter.insert("output");
		}
		if(special_bool_filter.end() == special_bool_filter.find(field_name)) {
			if("on" == m_conf[field_name])
				m_conf[field_name] = "1";
			else
				m_conf[field_name] = "0";
		} else {
			if("on" == m_conf[field_name])
				m_conf[field_name] = "Enable";
			else
				m_conf[field_name] = "Disable";
		}
	}

	class GamemodeCompare {
	public:
		bool operator ()(string gm1, string gm2) {
			str::GetWord(gm1, GamemodeStr);
			str::GetWord(gm2, GamemodeStr);
			return str::Int(gm1) < str::Int(gm2);
		}
	} GamemodeComparer;
public:
	GTAConfig(ServerInstanceInfo & instance): m_path(mgr_file::ConcatPath(instance.GetInstancePath(), "samp03", "server.cfg")), m_user(instance.UserName) {
		m_conf["echo"]="Executing Server Config...";
		m_conf["lan-mode"]="0";
		m_conf["maxplayers"] = "1";
		m_conf["announce"] = "0";
		m_conf["query"] = "1";
		m_conf["port"];
		m_conf["hostname"] = "SA-MP 0.3 Server";
		m_conf["weburl"] = "www.sa-mp.com";
		m_conf["rcon_password"] = str::Random(20);
		m_conf["filterscripts"];
		m_conf["plugins"];
		m_conf["password"];
		m_conf["mapname"] = "San Andreas";
		m_conf["bind"];
		m_conf["rcon"] = "1";
		m_conf["maxnpc"] = "100";
		m_conf["onfoot_rate"];
		m_conf["incar_rate"];
		m_conf["weapon_rate"];
		m_conf["stream_distance"];
		m_conf["stream_rate"];
		m_conf["timestamp"] = "Enable";
		m_conf["logqueries"] = "0";
		m_conf["logtimeformat"] = "[%d/%m/%Y %H:%M:%S]";
		m_conf["output"] = "Disable";
		m_conf["gamemodetext"] = "Unknown";
		m_conf["chatlogging"] = "1";

		ReadConf();
	}
	
	void ReadConf() {
		if (mgr_file::Exists(m_path)) {
			string conf = mgr_file::Read(m_path);
			StringList confLines;
			str::Split(conf, "\n", confLines);

			ForEachI(confLines, line) {
				string opt_value = *line;
				string opt_name = str::GetWord(opt_value);
				m_conf[opt_name] = str::Trim(opt_value);
			}
		} else {
			mgr_file::Write(m_path);
			mgr_file::Attrs attrs(0740, mgr_user::Uid(m_user), mgr_user::GroupGid(m_user));
			attrs.Set(m_path);
		}
	}
	
	void SetParams(StringMap & params) {
		ForEachI(params, i) {
			string param = i->first;
			string val = i->second;
			if (m_conf.find(param) != m_conf.end()) {
				m_conf[param]=val;
				if(IsBoolField(param))
					ConvertToBool(param);
			}
		}

	}

	void SetParamsFromSes(isp_api::Session & ses) 	{
		StringVector params;
		ses.GetParams(params);
		
		ForEachI(params, i) {
			string param = *i;
			string val = ses.Param(*i);
			if (m_conf.find(param) != m_conf.end()) {
				m_conf[param]=val;
				if(IsBoolField(param))
					ConvertToBool(param);
			}
		}
	}
	
	void ConfToSes(isp_api::Session & ses) {
		ForEachI(m_conf, param) {
			string val = param->second;
			if(IsBoolField(param->first)) {
				if (val == "1" || val == "Enable") val = "on";
				if (val == "0" || val == "Disable") val ="off";
			}
			ses.NewNode(param->first.c_str(),val);
		}
	}
	
	void SetParam(const string & param, const string & value) {
		if (m_conf.find(param) != m_conf.end()) {
			m_conf[param]=value;
		}
	}

	void Save() {
		string conf;
		ForEachI(m_conf, i)
			if(!str::inpl::Trim(i->second).empty())
				conf+=i->first+" "+i->second+"\n";
		mgr_file::Write(m_path,conf);
	}
	
	string GetParam(const string & param) {
		return m_conf[param];
	}
	
	string &operator[] (const string & param) {
		return m_conf[param];
	}

	void GetGamemodes(StringVector& gamemodes) {
		ForEachI(m_conf, conf_pair) {
			if(conf_pair->first.find(GamemodeStr) == 0) {
				string order_str = conf_pair->first.substr(GamemodeStr.size());
				if(!test::Numeric(order_str))
					continue;
				gamemodes.push_back(GamemodeStr + order_str);
			}
		}
		std::sort(gamemodes.begin(), gamemodes.end(), GamemodeComparer);
	}
};

class GtaMini : public GameModuleBase {
public:
	GtaMini() : GameModuleBase("gta") {}
	
	virtual void GetParams(Session& ses, const string & id) {
		ServerInstanceInfo inst(Name, id);
		GTAConfig conf(inst);
		conf.ConfToSes(ses);
	}
	
	virtual void SetParams(Session& ses, const string & id) {
		StringVector allparams;
		ses.GetParams(allparams);
		ServerInstanceInfo inst(Name, id);
		GTAConfig conf(inst);

		conf.SetParamsFromSes(ses);
		conf.Save();
		if (ses.Checked("restart")) {
			StartServer(ses, id);
		} else inst.SetNeedRestart();
	}

	void StartServerImpl(isp_api::Session & ses, const string & id) {
		ServerInstanceInfo inst(Name,id);
		ScreenHandler screen(Name, id, inst.UserName);
		try {
			if(screen.IsRunnig())
				StopServer(ses, id);
			GTAConfig config(inst);
			config.SetParam("bind", inst.GetIp());
			config.SetParam("port", str::Str(inst.GetPort()));
			config.SetParam("maxplayers", str::Str(inst.GetSlots()));
			config.SetParam("rcon", "1");
			config.SetParam("query", "1");
			config.SetParam("lanmode", "0");
			if (config["rcon_password"] == "changeme")
				config["rcon_password"] = str::base64::Encode(str::Random(20));
			config.Save();

			Debug("Dir %s, binary %s", inst.GetInstancePath().c_str(), mgr_file::ConcatPath(inst.GetInstancePath(), GTAExecutable).c_str());
			screen.SetWorkDir(mgr_file::ConcatPath(inst.GetInstancePath(), "samp03"));
			screen.Open(mgr_file::ConcatPath(inst.GetInstancePath(), "samp03", GTAExecutable));
		} catch (...) {}
		inst.SetNeedRestart(false);
	}

	void StopServerImpl(isp_api::Session & ses, const string & id) {
		ServerInstanceInfo inst(Name,id);
		ScreenHandler screen(Name, id, inst.UserName);
		try {
			GTAConfig config(inst);
			GTAServerCommander(inst.GetIp(), inst.GetPort(), config["rcon_password"]).Execute("exit", false);
			screen.Close(10);
		} catch (...) {
			
		}
		inst.SetParam("status", "off");
	}

	virtual int PlayersOnline(const string & id) {
		try {
			Debug(">>>>>>>>>>>>>>>>>>>>>>>GTA PLAYERS ONLINE BEGIN<<<<<<<<<<<<<<<<<<<<<<<<<");
			ServerInstanceInfo inst(Name,id);
			GTAConfig config(inst);
			string answer = GTAServerCommander(inst.GetIp(), inst.GetPort(), config["rcon_password"]).Execute("players");
			Debug("Players:\n%s", answer.c_str());
			str::inpl::Trim(answer);
			Debug(">>>>>>>>>>>>>>>>>>>>>>>GTA PLAYERS ONLINE END<<<<<<<<<<<<<<<<<<<<<<<<<");
			return answer.empty() ? 0 : std::count(answer.begin(), answer.end(), '\n');
		} catch (...) {
			
		}
		return 0;
	}

	virtual bool IsUp(const string & id) {
		ServerInstanceInfo inst(Name, id);
		string path = mgr_file::ConcatPath(inst.GetInstancePath(), "samp03", GTAExecutable);
		return !mgr_proc::Execute("pgrep -U " + inst.UserName + " " + GTAExecutable).Result();
	}
private:
	class GTAServerCommander {
	public:
		GTAServerCommander(string ip, int port, const string& rcon_passwd) : ServerAddress(ip, port), RCONPassword(rcon_passwd) {
			SocketDesc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if(-1 == SocketDesc)
				throw mgr_err::Error("gta_srv_sock", "socket", str::Str(errno));

			PacketHead = "SAMP";
			while(!ip.empty())
				PacketHead += static_cast<unsigned char>(str::Int(str::GetWord(ip, '.')));
			PacketHead += static_cast<unsigned char>(port & 0xFF);
			PacketHead += static_cast<unsigned char>(port >> 8 & 0xFF);
		}

		string Execute(const string& command, bool wait_answer = true) {
			string send_packet = PacketHead;
			send_packet += 'x';
			send_packet += static_cast<unsigned char>(RCONPassword.size() & 0xFF);
			send_packet += static_cast<unsigned char>(RCONPassword.size() >> 8 & 0xFF);
			send_packet += RCONPassword;

			send_packet += static_cast<unsigned char>(command.size() & 0xFF);
			send_packet += static_cast<unsigned char>(command.size() >> 8 & 0xFF);
			send_packet += command;

			SendToSocket(send_packet);
			string answer;
			if(wait_answer)
				RecieveFromSocket(answer);
			return answer;
		}

		~GTAServerCommander() {
			close(SocketDesc);
		}
	private:
		int SocketDesc;
		mgr_net::SockAddr ServerAddress;
		const string RCONPassword;
		string PacketHead;

		void SendToSocket(const string& message) {
			if(-1 == sendto(SocketDesc, message.c_str(), message.size(), 0, ServerAddress.addr(), ServerAddress.addrsize()))
				throw mgr_err::Error("gta_srv_sock", "send", str::Str(errno));
		}

		void RecieveFromSocket(string& answer) {
			answer.clear();
			const int buff_len = 1024;
			char buffer[buff_len];
			fd_set readset;
			FD_ZERO(&readset);
			FD_SET(SocketDesc, &readset);
			const int head_size = 12;
			const char start_boundary = 32, end_boundary = 126;
			socklen_t fromlen = ServerAddress.addrsize();
			while(true) {
				timeval timeout = { 0, 10000 };
				if(select(SocketDesc + 1, &readset, nullptr, nullptr, &timeout) > 0) {
					memset(buffer, 0, buff_len);
					ssize_t recieved = recvfrom(SocketDesc, buffer, sizeof(buffer), MSG_DONTWAIT, ServerAddress.addr(), &fromlen);
					if(recieved > head_size) {
						for(int i = head_size + 1; i < recieved; ++ i)
							if(buffer[i] >= start_boundary && buffer[i] <= end_boundary)
								answer += buffer[i];
							else
								answer += ' ';
						answer += "\n";
					}
				} else break;
			}
		}
	};
};

class aGTAState: public ListAction {
public:
	aGTAState():ListAction("gta.state",MinLevel(lvUser)){}
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

class aGamemodes : public StdListAction {
public:
	aGamemodes() : StdListAction("gta.gamemode", MinLevel(lvUser)) {}
	void AddRow(Session & ses, const string& gamemode_name, string value) const {
		ses.NewElem();
		ses.AddChildNode("order", gamemode_name.substr(GamemodeStr.size()));
		ses.AddChildNode("name", str::GetWord(value));
		ses.AddChildNode("repeat", value);
	}
	virtual void Get(Session& ses, const string& elid) const {
		if(!elid.empty()) {
			ServerInstanceInfo inst(ses);
			GTAConfig conf(inst);
			ses.NewNode("order", elid);
			string tmp = conf[GamemodeStr + elid];
			ses.NewNode("name", str::GetWord(tmp));
			ses.NewNode("repeat", tmp);
		}
	}
	virtual void Set(Session& ses, const string& elid) const {
		ServerInstanceInfo inst(ses);
		GTAConfig conf(inst);
		StringVector gamemodes;
		conf.GetGamemodes(gamemodes);
		string new_order = ses.Param("order");
		if(new_order != elid) {
			StringVector::iterator iter = find(gamemodes.begin(), gamemodes.end(), GamemodeStr + new_order);
			if(iter != gamemodes.end() && !iter->empty())
				throw mgr_err::Exists("order", new_order);
			if(!elid.empty())
				conf[GamemodeStr + elid].clear();
		}
		conf[GamemodeStr + new_order] = ses.Param("name") + " " + ses.Param("repeat");
		conf.Save();
		inst.SetNeedRestart();
	}
	virtual void Del(Session& ses, const string& elid) const {
		if("0" == elid)
			return;
		ServerInstanceInfo inst(ses);
		GTAConfig conf(inst);
		conf[GamemodeStr+elid].clear();
		conf.Save();
		inst.SetNeedRestart();
	}
	virtual void List(Session& ses) const {
		ServerInstanceInfo inst(ses);
		GTAConfig conf(inst);

		StringVector gamemodes;
		conf.GetGamemodes(gamemodes);
		ForEachI(gamemodes, gm)
			AddRow(ses, *gm, conf[*gm]);
	}

	virtual bool CheckAccess(const isp_api::Authen& auth) const {
		if (auth.level() == mgr_access::lvUser) {
			return auth.name().find(GAME_NAME "_") == 0;
		}
		return false;
	}
};

MODULE_INIT (gtamini, "gsmini") {
	RegModule<GtaMini>();
	new aGTAState();
	new aGamemodes();
}

