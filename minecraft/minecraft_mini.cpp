#include <api/module.h>
#include <api/stdconfig.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrrpc.h>
#include <mgr/mgrfile.h>
#include <mgr/mgrrpc.h>
#include <mgr/mgrerr.h>
#include "../mini/gmmini.h"
#include "../main/defines.h"
#include <unistd.h>
#include <netinet/in.h>
#include "minecraft.h"
#include <cstring>
#include <algorithm>
//Модуль будет называться "minecraft"
#define GAME_NAME "minecraft"
MODULE(GAME_NAME);
using namespace isp_api;
using namespace mgr_file;
//Имя исполняемого файла
const string MinecraftExecutable = GAME_NAME "_server.jar";
//Функция возвращает настройки java в зависимости от количества слотов на сервере
void CalcSrvStartParams(int slots, int & ramMin, int & ramMax, int & threads) 
{
	if (slots > 0 and slots <= 10)  			{ramMin=1024;	ramMax=3072;	threads=1;}
	else if (slots >= 11 and slots <= 15)  		{ramMin=1536;	ramMax=4096;	threads=1;}
	else if (slots >= 16 and slots <= 20)  		{ramMin=2048;	ramMax=5120;	threads=1;}
	else if (slots >= 21 and slots <= 40)  		{ramMin=3072;	ramMax=7168;	threads=2;}
	else if (slots >= 41 and slots <= 60)  		{ramMin=4096;	ramMax=9216;	threads=3;}
	else if (slots >= 61 and slots <= 80)  		{ramMin=6144;	ramMax=13312;	threads=4;}
	else if (slots >= 81 and slots <= 100)  	{ramMin=8192;	ramMax=17408;	threads=5;}
	else if (slots >= 101 and slots <= 120)  	{ramMin=9216;	ramMax=18432;	threads=5;}
	else if (slots >= 121 and slots <= 140)  	{ramMin=10240;	ramMax=19456;	threads=6;}
	else if (slots >= 141 and slots <= 160)  	{ramMin=11264;	ramMax=20480;	threads=6;}
	else if (slots >= 161 and slots <= 180)  	{ramMin=12288;	ramMax=21504;	threads=7;}
	else if (slots >= 181 and slots <= 200)  	{ramMin=13312;	ramMax=22528;	threads=7;}
	else throw mgr_err::Value("slots");
}

MinecraftServerInfo::MinecraftServerInfo(const string & ip, int port) : m_ServerAddr(ip, port)
{
	
}

MinecraftServerInfo::~MinecraftServerInfo() {
	close(m_Socket);
}

void MinecraftServerInfo::GetInfo()
{
	//Для получения информации о состоянии сервера Minecraft используется нативный протокол.
	m_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_Socket == -1)
		throw mgr_err::Error("minecraft_srv_sock", "socket", str::Str(errno));
	if (connect(m_Socket, m_ServerAddr.addr(), m_ServerAddr.addrsize()) == -1)
		throw mgr_err::Error("minecraft_srv_connect", "connect", str::Str(errno));
	
	string out;
	out.push_back('\xFE');
	out.push_back('\x01');
	SendToSocket(out);
	string res;
	RecieveFromSocket(res);
	res.erase(0,3);
	string trimmed;
	bool skip = true;
	for (auto i = res.begin(); i != res.end(); ++i) {
		if (!skip)
			trimmed += *i;
		skip = ! skip;
	}
	
	StringVector data;
	while(!trimmed.empty()) {
		string tmp = str::GetWord(trimmed,'\0');
		Debug("Str '%s'",tmp.c_str());
		data.push_back(tmp);
	}
	
	if (data.size() > 5) {
		this->Version = data[2];
		this->Message = data[3];
		this->OnlinePlayers = str::Int(data[4]);
		this->MaxPlayers = str::Int(data[5]);
	} else {
		throw mgr_err::Error("minecraft_srv_info", "bad_data");
	}
	
}

void MinecraftServerInfo::SendToSocket(const string& message) {
	if (send(m_Socket, message.c_str(), message.size(), 0) == -1)
		throw mgr_err::Error("minecraft_srv_send", "send", str::Str(errno));
}

void MinecraftServerInfo::RecieveFromSocket(string& answer) {
	answer.clear();
	const int buff_len = 1024;
	char buffer[buff_len];
	fd_set readset;
	FD_ZERO(&readset);
	FD_SET(m_Socket, &readset);
	while (true) {
		timeval timeout = {5, 0};
		if (select(m_Socket + 1, &readset, NULL, NULL, &timeout) > 0) {
			memset(buffer, 0, buff_len);
			ssize_t recieved = recv(m_Socket, buffer, sizeof(buffer), 0);
			Debug("recv from socket %d",recieved);
			if (recieved < 0)
				recieved = 0;
			string tmp(buffer, recieved);
			answer = tmp;
			return;
		} else {
			Debug("errno %d", errno);
				answer.clear();
			break;
		}
	}
}

// Класс реализующий модуль "minecraft"
class MinecraftMini : public GameModuleBase {
public:
	//Класс реализующий плагин для minecraft под названием MobHealth
	class pMobHealth : public GamePlugin {
	public:
		pMobHealth(GameModuleBase & module) : GamePlugin(module)
		{
			//Имя исполняемого файла плагина
			ExecutableName = "MobHealth.jar";
		}
		
        virtual void GetParams(Session& ses, const string& id)
		{
			ServerInstanceInfo srv(GAME_NAME,id); //Получаем Instance сервера
			ses.NewNode("config", mgr_file::Read(mgr_file::ConcatPath(srv.GetInstancePath(),m_module.PluginsPath,"MobHealth/config.yml"))); //Выводим значение конфига плагина на форму в поле "config"
		}
		
        virtual void SetParams(Session& ses, const string& id)
		{
			ServerInstanceInfo srv(GAME_NAME,id); //Получаем Instance сервера
			mgr_file::Write(mgr_file::ConcatPath(srv.GetInstancePath(),m_module.PluginsPath,"MobHealth/config.yml"), ses.Param("config")); //Записываем в конфиг плагина данные из поля "config"
		}
	};
	
	//Конструктор игрового модуля
	MinecraftMini() : GameModuleBase(GAME_NAME) {
		//Регистрация плагина MobHealth
		this->RegisterPlugin<pMobHealth>("MobHealth");
	}
	
	// Создаёт и возвращает обработик плагина 
	virtual std::shared_ptr<GamePlugin> GetDefaultPluginHandler(string name)
	{
		std::shared_ptr<GamePlugin> ptr = std::shared_ptr<GamePlugin>(new GamePlugin(*this));
		ptr->ExecutableName = name+".jar";
		ptr->Name = name;
		return ptr;
	}
	
	//Возвращает состояние сервера
    virtual bool IsUp(const string& id)
	{
		ServerInstanceInfo inst(GAME_NAME "_"+id); // Получаем инстанс сервера
		MinecraftServerInfo info(inst.GetIp(),inst.GetPort()); // Созадём объект для запроса информации о состоянии сервера по сети
		try {
			info.GetInfo();// Пробуем запросить информацию
		} catch (...) {
			return false;
		}
		return true;
	}
	// Возвращает количество игроков Online
    virtual int PlayersOnline(const string& id)
	{
		ServerInstanceInfo inst(GAME_NAME "_"+id); // Получаем инстанс сервера
		MinecraftServerInfo info(inst.GetIp(),inst.GetPort());  // Созадём объект для запроса информации о состоянии сервера по сети
		info.GetInfo(); // запрашиваем информацияю
		Debug("MinecraftInfo for server %s:%d version '%s' info '%s' players %d/%d",inst.GetIp().c_str(), inst.GetPort(), info.Version.c_str(), info.Message.c_str(), info.OnlinePlayers, info.MaxPlayers);
		return info.OnlinePlayers; // Возвращаем значение
	}
	// Запуск сервера Minecraft
	void StartMinecraft(const string & id)
	{	
		ServerInstanceInfo inst(Name,id);
		ScreenHandler screen(Name, id, this->Name+"_"+id); // Создаём обработик screen для сервера
		
		MinecraftConfig conf(inst); // Получаем конфиг minecraft 
		conf.SetParam("server-ip", inst.GetIp()); // Устанавливаем IP
		conf.SetParam("server-port", str::Str(inst.GetPort())); // Устанавливаем порт сервера
		conf.SetParam("max-players", str::Str(inst.GetSlots())); // Устанавливаем максимальное количество игроков
		conf.SetParam("rcon.port", inst.GetParam("rconport")); // Устанавливаем порт RCON
		conf.SetParam("query.port", inst.GetParam("queryport"));  // Устанавливаем порт удалённых запросов
		conf.Save(); // Записываем конфиг на диск
		screen.SetWorkDir(inst.GetInstancePath()); // Устанавливаем рабочий каталог

		if (screen.IsRunnig()) StopMinecraft(id); // Если скрин для этого сервера уже запущен - останавливаем сервер
		
		Debug("Starting screen for Minecraft server...");
		int ramMin, ramMax, threads;
		CalcSrvStartParams(inst.GetSlots(), ramMin, ramMax, threads); // Получаем параметры для java 
		//Запускаем screen с Minecraft
		int res = screen.Open("/usr/bin/java -Xms"+str::Str(ramMin)+"M -Xmx"+str::Str(ramMax)+"M -XX:ParallelGCThreads="+str::Str(threads)+" -jar " + ConcatPath(inst.GetInstancePath(), MinecraftExecutable));
		Debug("Started with result %d", res);
		inst.SetNeedRestart(false); //Помечаем что сервер перезапущен
	}
	
	//Остановка сервера Minecraft
	void StopMinecraft(const string & id)
	{
		ServerInstanceInfo inst(Name,id);
		ScreenHandler screen(Name,id, inst.UserName); // Создаём обработик screen для сервера
		Debug("Stoping screen for Minecraft server...");
		{
			ScreenTerminal term(screen); //Создаём скрин терминал для ScreenHandler-а
			term.WaitInputCall(">"); // Ждём приглашения ввода
			term.Send("stop\r"); // Шлём команду остановки
			term.Wait(Name+"_"+id); // Ждём пока сервера выйдет и будет предложен ввод пользователю
		}
		screen.Close(10); // Закрываем screen
		inst.SetParam("status", "off"); // Меняем состояние сервера
		Debug("Stoped");
	}
	
	//Настройка инстанса после переустановки сервера
    virtual void AfterReinstall(const string& id, DistInfo& dist)
	{
		ServerInstanceInfo inst(Name, id);  
		mgr_file::Dir dir(inst.GetInstancePath());
		//Читаем содержимое дириктории
		while(dir.Read()) { 
			// Переименоаваем исполняемый файл
			if (dir.Info().IsFile() && dir.name() != MinecraftExecutable && dir.name().find(".jar") != string::npos) {
				mgr_file::Move(dir.FullName(), mgr_file::ConcatPath(inst.GetInstancePath(), MinecraftExecutable));
				break;
			}
		}
		//переустанавливаем плагины
		ForEachI(Plugins, plg) {
			if (plg->second->IsInstalled(id)) {
				try {
					plg->second->Install(id);
				} catch (...){}
			}	
		}
	}
	
	//Выводит параметры на форму настроей из конфига minecraft
    virtual void GetParams(Session& ses, const string & id)
	{
		ses.NewSelect("difficulty");
		ses.AddChildNode("msg","Peaceful").SetProp("key", "0");
		ses.AddChildNode("msg","Easy").SetProp("key", "1");
		ses.AddChildNode("msg","Normal").SetProp("key", "2");
		ses.AddChildNode("msg","Hard").SetProp("key", "3");
		ses.NewSelect("gamemode");
		ses.AddChildNode("msg","Survival").SetProp("key", "0");
		ses.AddChildNode("msg","Creative").SetProp("key", "1");
		ses.AddChildNode("msg","Adventure").SetProp("key", "2");
		ses.NewSelect("op-permission-level");
		ses.AddChildNode("msg","oplevel_1").SetProp("key", "1");
		ses.AddChildNode("msg","oplevel_2").SetProp("key", "2");
		ses.AddChildNode("msg","oplevel_3").SetProp("key", "3");
		ses.AddChildNode("msg","oplevel_4").SetProp("key", "4");
		ses.NewSelect("level-type");
		ses.AddChildNode("msg","DEFAULT").SetProp("key", "DEFAULT");
		ses.AddChildNode("msg","FLAT").SetProp("key", "FLAT");
		ses.AddChildNode("msg","LARGEBIOMES").SetProp("key", "LARGEBIOMES");
		ServerInstanceInfo inst(Name, id);// Получаем инстанс сервера
		MinecraftConfig conf(inst); // Загружаем конфиг
		conf.ConfToSes(ses); // Выгружаем конфиг из памятм в сессию
	}
	
	// Сохраняет параметры установленные на форме настроек в конфиг
    virtual void SetParams(Session& ses, const string & id)
	{
		StringVector allparams;
		ses.GetParams(allparams); // Получаем все параметрв сессии
		ServerInstanceInfo inst(Name, id); // Получаем инстанс сервера
		MinecraftConfig conf(inst); // Загружаем конфиг

		conf.SetParansFormSes(ses); // Записываем параметры в конфиг из сесии

		// Если включет RCON то выделяем для него свободный порт
		if (conf["enable-rcon"] == "true") {
			string oldRconPort =inst.GetParam("rconport");
			conf["rcon.port"] = str::Str(PortMgr::AllocPort(inst.UserName+"_rcon", inst.GetIp(), str::Int(oldRconPort))); // Выделение свобдного порта
			if (oldRconPort.empty())
				inst.SetParam("rconport", conf["rcon.port"]);
		} else {
			// Освобождаем порт
			PortMgr::FreePort(inst.UserName+"_rcon");
			inst.SetParam("rconport", "");
		}

		// Если включены удалённые запросы то выделяем для них свободный порт
		if (conf["enable-query"] == "true") {
			string oldQueryPort =inst.GetParam("queryport");
			conf["query.port"] = str::Str(PortMgr::AllocPort(inst.UserName+"_query", inst.GetIp(), str::Int(oldQueryPort)));
			if (oldQueryPort.empty())
				inst.SetParam("queryport", conf["query.port"]);
		} else {
			// Освобождаем порт
			PortMgr::FreePort(inst.UserName+"_query");
			inst.SetParam("queryport", "");
		}

		
		// Сохораняем конфиг на диск
		conf.Save();
		inst.SetNeedRestart(); // Помечаес что сервер необходимо перезапустить
		//Перезапускаем сервер если пользователь отметил checkbox "restart"
		if (ses.Checked("restart")) {
			StartMinecraft(inst.Id);
		}
	}
	// Заупуск сервера
    virtual void StartServerImpl(Session& ses, const string & id)
	{
		Debug("StartServer");
		StartMinecraft(id);
	}
	// Остановка сервера
    virtual void StopServerImpl(Session& ses, const string & id)
	{
		Debug("StopServer");
		StopMinecraft(id);
	}
	// Удаление сервера
    virtual void DeleteServer(Session& ses, const string & id)
	{
		GameModuleBase::DeleteServer(ses, id);
		// Освобождаем дополнительные порты
		PortMgr::FreePort(Name+"_"+id+"_rcon"); 
		PortMgr::FreePort(Name+"_"+id+"_query");
	}	
};

// Класс реализует action который выводит информацию о состоянии сервера на главную форму
class aMinecraftState: public StdListAction {
public:
	//Конструктор action-а
	aMinecraftState():StdListAction("minecraft.state",MinLevel(lvUser)){}
	void AddRow(Session & ses, const string & name, const string & value) const
	{
		ses.NewElem();
		ses.AddChildNode("name", name);
		ses.AddChildNode("value", value);
	}
	// Вывод списка информации
    virtual void List(Session& ses) const
    {
		ServerInstanceInfo srv(ses); //Получение инстанса смервера
		MinecraftConfig conf(srv); // Читаем конфиг
		
		bool serverIsUp = GameModules[GAME_NAME]->IsUp(srv.Id); // Запущен ли сервер ?
		
		AddRow(ses, "version", srv.GetParam("version")); // Выводим версию сервера
		ses.NewElem();
		if (serverIsUp) { // Если сервер запущен
			ses.AddChildNode("name", "server_online");
			ses.AddChildNode("status", "on");
		} else {
			ses.AddChildNode("name", "server_offline");
			ses.AddChildNode("status", "off");
		}
		AddRow(ses, "ip", srv.GetIp()); // Выводим IP-адресс сервера
		AddRow(ses, "port", str::Str(srv.GetPort())); // Выводи порт сервера
		string rport = srv.GetParam("rconport");
		if (!rport.empty()) AddRow(ses, "rconport", rport); // Если включен RCON выводим порт RCON
		string qport = srv.GetParam("queryport");
		if (!qport.empty()) AddRow(ses, "queryport", qport); // Если включены удалённые запросы выводим для них порт 
		AddRow(ses, "players_online", serverIsUp?str::Str(GameModules[GAME_NAME]->PlayersOnline(srv.Id)):"-"); // выводи количесво игроков
		
	}
	// Проверка доступнлости функционала. 
	virtual bool CheckAccess(const isp_api::Authen& auth) const 
    {
		if (auth.level() == mgr_access::lvUser) 
		{
			return auth.name().find(GAME_NAME "_") == 0; // Проверка соответсвия типа игры
		}
		return false;
	}
};

// Данный класс реализует Action который выводит список игроков находящихся на сервере
class aMinecraftPlayersList: public StdListAction {
public:
	aMinecraftPlayersList():StdListAction("minecraft.playerslist",MinLevel(lvUser)){}
    virtual void List(Session& ses) const
    {
		ServerInstanceInfo srv(ses);
		string list = srv.GetParam("playerslist");
		StringList players;
		str::Split(list,"\n",players);
		ForEachI(players, p) {
			if (p->empty()) continue;
			ses.NewElem();
			ses.AddChildNode("name", *p);
		}
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

//Класс раелизующий уравление файлом содержащим спосок пользователей
class aMinecraftUserListFile: public StdListAction {
private:
	string m_file;
public:
	aMinecraftUserListFile(const string & actionname,const string & file):StdListAction(actionname, MinLevel(lvUser)),m_file(file){}
    virtual void List(Session& ses) const
    {
		ServerInstanceInfo srv(ses);
		string file = mgr_file::ConcatPath(srv.GetInstancePath(), m_file);
		if (mgr_file::Exists(file)) {
			string userdata = mgr_file::Read(file);
			StringList users;
			str::Split(userdata, "\n", users);
			ForEachI(users, i) {
				ses.NewElem();
				ses.AddChildNode("name", *i);
			}
		}
	}
    virtual void Get(Session& ses, const string& elid) const
    {
		ses.NewNode("name", elid);
	}
	
    virtual void Set(Session& ses, const string& elid) const
    {
		ServerInstanceInfo srv(ses);
		string file = mgr_file::ConcatPath(srv.GetInstancePath(), m_file);
		string userdata = mgr_file::Read(file);
		StringList users;
		str::Split(userdata , "\n", users);
		if (!elid.empty()) {
			ForEachI(users, user) {
				if (*user == elid)
					*user = ses.Param("name");
			}
		} else {
			users.push_back(ses.Param("name"));
		}
		userdata = str::Join(users, "\n");
		mgr_file::Write(file, userdata);
		srv.SetNeedRestart();
	}
	
    virtual void Del(Session& ses, const string& elid) const
    {
		ServerInstanceInfo srv(ses);
		string file = mgr_file::ConcatPath(srv.GetInstancePath(), m_file);
		string userdata = mgr_file::Read(file);
		StringList users;
		str::Split(userdata, "\n", users);
		users.remove(elid);
		userdata = str::Join(users, "\n");
		mgr_file::Write(file, userdata);
		srv.SetNeedRestart();
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
// Класс реализует упраление списком операторов сервер
class aMinecraftOpers: public aMinecraftUserListFile
{
public:
	aMinecraftOpers():aMinecraftUserListFile("minecraftopers", "ops.txt"){}
};
// Класс реализует управление whitelist-ом
class aMinecraftWhiteList: public aMinecraftUserListFile
{
public:
	aMinecraftWhiteList():aMinecraftUserListFile("minecraftwhitelist", "white-list.txt"){}
};

MODULE_INIT (minecraftmini, "gsmini")
{
	RegModule<MinecraftMini>(); // Резистрация модуля minecraft
	//Регистрация второстепенных action-ов
	new aMinecraftState(); 
	new aMinecraftOpers();
	new aMinecraftWhiteList();
	new aMinecraftPlayersList();
}

#undef GAME_NAME
