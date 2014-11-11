#include <mgr/mgrstr.h>
#include <mgr/mgrfile.h>
#include <api/action.h>
#include "../mini/gamemodule.h"
#include <mgr/mgrnet.h>
//Данный класс получает информацию о состоянии сервера по сети.
class MinecraftServerInfo {
private:
	int m_Socket;
	mgr_net::SockAddr m_ServerAddr;
public:
	MinecraftServerInfo(const string & ip, int port);
	~MinecraftServerInfo();
	void GetInfo(); // Получить информацию о состоянии сервера
	string Version; // Версия сервера
	string Message; // Сообщение сервера
	int MaxPlayers; // Колисество слотов на сервере
	int OnlinePlayers; // Количество игроков online
	
	void SendToSocket(const string& message);
	void RecieveFromSocket(string& answer);
	string RollUp(string value)
	{
		string result;
		for (str::utf8::iterator i = value.begin(); i != value.end(); ++i) {
			result += *i;
		}
		return result;
	}
};

//Класс для работы с конфигурационным файлом Minecraft
class MinecraftConfig {
private:
	string m_path;
	string m_user;
	StringMap m_conf;
	StringSet m_knowm_params;
public:
	MinecraftConfig(ServerInstanceInfo & instance): m_path(mgr_file::ConcatPath(instance.GetInstancePath(), "server.properties")), m_user(instance.UserName)
	{
		m_conf["allow-flight"]="false";
		m_conf["allow-nether"]="true";
		m_conf["announce-player-achievements"]="true";
		m_conf["enable-command-block"]="false";
		m_conf["difficulty"]="1";
		m_conf["enable-query"]="false";
		m_conf["enable-rcon"]="falese";
		m_conf["force-gamemode"]="true";
		m_conf["gamemode"]="0";
		m_conf["generate-structures"]="true";
		m_conf["generator-settings"];
		m_conf["hardcore"]="false";
		m_conf["level-name"]="world";
		m_conf["level-seed"];
		m_conf["level-type"]="DEFAULT";
		m_conf["max-build-height"]="256";
		m_conf["max-players"]="20";
		m_conf["motd"]="A Minecraft Server on GSmanager";
		m_conf["online-mode"]="true";
		m_conf["op-permission-level"]="true";
		m_conf["player-idle-timeout"]="0";
		m_conf["query.port"];
		m_conf["resource-pack"];
		m_conf["pvp"]="true";
		m_conf["rcon.password"];
		m_conf["rcon.port"];
		m_conf["server-ip"];
		m_conf["server-name"];
		m_conf["server-port"];
		m_conf["snooper-enabled"]="true";
		m_conf["spawn-animals"]="true";
		m_conf["spawn-monsters"]="true";
		m_conf["spawn-npcs"]="true";
 		m_conf["spawn-protection"];
		m_conf["view-distance"]="10";
		m_conf["white-list"]="false";
		ReadConf();
	}
	//Прочитать конфигурационный файл в память
	void ReadConf() 
	{
		if (mgr_file::Exists(m_path)) {
			string conf = mgr_file::Read(m_path);
			StringList confLines;
			str::Split(conf, "\n", confLines);
			ForEachI(confLines, line) {
				if((*line)[0] != '#') {
					string tmp = *line;
					m_conf[str::GetWord(tmp,"=")]=tmp;
				}
			}
// 			ForEachI(m_conf, i) {
// 				Debug("ReadConf '%s'='%s'", i->first.c_str(), i->second.c_str());
// 			}
		} else {
			mgr_file::Write(m_path);
			mgr_file::Attrs attrs(0755, mgr_user::Uid(m_user), mgr_user::GroupGid(m_user));
			attrs.Set(m_path);
		}
	}
	
	//Записать в память параметры из StringMap
	void SetParams(StringMap & params)
	{
		ForEachI(params, i) {
// 			string param = str::Replace(i->first,"_","-");
			string param = i->first;
			string val = i->second;
			if (m_conf.find(param) != m_conf.end()) {
				if (val == "on") val = "true";
				if (val == "off") val ="false";
				m_conf[param]=val;
			}
		}

	}
	
	//Записать параметры в память из сессии
	void SetParansFormSes(isp_api::Session & ses) 
	{
		StringVector params;
		ses.GetParams(params);
		
		ForEachI(params, i) {
			string param = *i;
			string val = ses.Param(*i);
			if (m_conf.find(param) != m_conf.end()) {
				if (val == "on") val = "true";
				if (val == "off") val ="false";
				m_conf[param] = val;
			}
		}
	}
	//Выгрузить данные из памяти в сессию
	void ConfToSes(isp_api::Session & ses) {
		ForEachI(m_conf, param) {
			string val = param->second;
			if (val == "true") val = "on";
			if (val == "false") val ="off";
			ses.NewNode(param->first.c_str(),val);
		}
	}
	//Уствновить определённый параметр
	void SetParam(const string & param, const string & value)
	{
		if (m_conf.find(param) != m_conf.end())
			m_conf[param]=value;
	}
	//Записать конфиг на диск
	void Save()
	{
		string conf;
		ForEachI(m_conf, i) {
			conf+=i->first+"="+i->second+"\n";
		}
		mgr_file::Write(m_path,conf);
	}
	
	// Получить определённый параметр
	string GetParam(const string & param) 
	{
		return m_conf[param];
	}
	// Получить определённый параметр
	string &operator[] (const string & param)
	{
		return m_conf[param];
	}
};