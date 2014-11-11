#include <api/module.h>
#include <mgr/mgrlog.h>
#include <api/action.h>
#include <api/stdconfig.h>
#include "dbobject.h"
#include "defines.h"
#include <api/auth_method.h>
#include <mgr/mgrdb.h>
#include <mgr/mgrdb_struct.h>
#include <api/dbaction.h>
#include <api/problems.h>
#include <api/action.h>
#include <api/longtask.h>
#include "nodes.h"
#include "gmmain.h"
#include "gsutils.h"
#include "teamspeak.h"
#include "../mini/screen.h"
#include <fcntl.h>
#include <unistd.h>

MODULE("gsmgr");

using namespace isp_api;
using namespace gsutils;
std::shared_ptr<mgr_db::Cache> db;

class gsAuth : public AuthMethod {
public:
	gsAuth() : AuthMethod("gsmgr") {}
	string Lookup(const string &name, bool checkactive = true) const
	{
		auto user = db->Get<UserTable>();

		if (user->FindByName(name) && !user->Password.AsString().empty() && (!checkactive || user->Enabled))
			return user->Password;
		return "";
	}

	void FillupParams(const string &name, mgr_xml::Xml &xml) const
	{
		auto user = db->Get<UserTable>();

		if (user->FindByName(name)) {
			XmlNode n = xml.GetRoot().AppendChild("ok");
			n.SetProp("level", str::Str(user->Level));
			n.SetProp("name", user->Name);
			n.SetProp("method", this->name());
			n.AppendChild("ext", user->Id).SetProp("name", "uid");
			n.AppendChild("ext", user->Id).SetProp("name", "id");
		}
	}

	virtual void AuthenByName(mgr_xml::Xml &xml, const string &name) const
	{
		if (!Lookup(name, false).empty())
			FillupParams(name, xml);
	}

	virtual void AuthenByPassword(Xml &res, const string &name, const string &passwd) const
	{
//		string user = ses.Param("username");
		string pass = Lookup(name);

		if (!pass.empty() && pass == str::Crypt(passwd, pass))
			FillupParams(name, res);
	}

	bool IsOwner(const Authen& cur, const string& user) const
	{
		return (cur.level() == lvAdmin);
	}

	void SetPassword(const string &user, const string &pass) const
		{
			auto usertable = db->Get<UserTable>();

			if (!usertable->FindByName(user))
					throw mgr_err::Missed("user");

			usertable->Password = str::Crypt(pass);
			usertable->Post();
		}
};

class GameModulesList : public ListAction {
public:
	GameModulesList() : ListAction("gamelist", MinLevel(lvAdmin)) {}

	virtual void List(Session &ses) const {
		StringMap games = GameModules::Get();
		for (auto i = games.begin(); i != games.end(); ++i) {
			ses.NewElem();
			ses.AddChildNode("module", i->first);
		}
	}
};

class aServers: public isp_api::ExtTableIdListAction<ServerTable> {
public:
	aServers() :
		isp_api::ExtTableIdListAction<ServerTable>("server", mgr_access::MinLevel(mgr_access::lvUser), *db),ConfigureServer(this), EnableServer(this), DisableServer(this), MigrateServer(this), MigrateServerInfo(this)
	{
	}
	
	class ServersListCallBack : public SmartDbListCallBack {
	public:
		virtual void OnRow(Session& ses, FieldsList& row)
		{
			if (HasProblem("lost_connection_to_node",row.GetParam("nodeid"))) {
				row.SetParam("connection_lost","on");
			}
		}
	};
	
	virtual void List(isp_api::Session& ses) const
    {
		string q_where;
		if (ses.auth.level() != mgr_access::lvAdmin)
			q_where+=" WHERE s.user="+ses.auth.ext("uid");
		SmartDbList<ServersListCallBack>(ses, db->Query("SELECT s.*, s.maxplayers as players_total, s.onlineplayers as players_used, "
		                               "u.name as 'owner', n.name as 'nodename', n.id as 'nodeid' "
		                               "FROM server s "
		                               "LEFT JOIN user u ON u.id=s.user "
		                               "LEFT JOIN node n ON n.id=s.node"+q_where));
	}

    virtual void TableFormTune(Session& ses, TableIdListAction< ServerTable >::Cursor& table) const
    {
		DbSelect(ses, "user", db->Query("SELECT id, name FROM user where level=" + str::Str(isp_api::lvUser) + " order by name"));
		ses.NewSelect("game");
		StringMap games = GameModules::Get();
		ForEachI(games, game) {
			ses.AddChildNode("val",game->first);
		}
		
		auto res = db->Query("SELECT g.gamename, g.node, n.name FROM allowedgames g LEFT JOIN node n ON n.id=g.node");
		ses.NewSelect("node");
		ses.AddChildNode("val","auto");
		ForEachRecord(res) {
			ses.AddChildNode("val",res->AsString("name")).SetProp("key",res->AsString("node")).SetProp("depend",res->AsString("gamename"));
		}
	}
	
    virtual void TableSet(Session& ses, TableIdListAction< ServerTable >::Cursor& table) const
	{
		if (ses.auth.level() == mgr_access::lvUser &&  table->Disabled)	throw mgr_err::Access("server");
		if (table->IsNew()) {
			if (ses.Param("node") == "auto")  {
				auto newNodeQuery = db->Query("SELECT ig.node, count(ig.node) cnt FROM allowedgames ig LEFT JOIN server s ON s.node=ig.node WHERE ig.gamename='"+ses.Param("game")+"' GROUP BY ig.node ORDER BY cnt LIMIT 1");
				table->Node = newNodeQuery->AsInt("node");
			}
			table->Type = ses.Param("game");
			
			
			Node node (table->Node);

			if (ses.Param("ip").empty()) {
				table->Ip = node.Ip();
			}
			
			StringMap table_values;
			gsutils::DbGetValues(table, table_values);
			string query = "func=" + table->Type+".create";
			MakeQuery(query, table_values);
			auto res = node.MiniQuery(query);
			table->Active = false;
			auto port = res.xml.GetNode("/doc/port");
			if (port)
				table->Port = str::Int(port.Str());
		} else {
			Node node (table->Node);
			string query = "func=" + table->Type+".setparams";
			query+="&slots="+table->MaxPlayers;
			query+="&id="+table->Id;
			node.MiniQuery(query);
		}
	}
	
	virtual void TableAfterDelete(isp_api::Session& ses, TableIdListAction< ServerTable >::Cursor& table) const {
		Node node (table->Node);
		StringMap table_values;
		gsutils::DbGetValues(table, table_values);
		string query = "func=" + table->Type+".delete";
		MakeQuery(query, table_values);
		node.MiniQuery(query);
	}

	virtual void TableResume(isp_api::Session & ses, TableIdListAction< ServerTable >::Cursor& table) const
	{
		if (ses.auth.level() == mgr_access::lvUser &&  table->Disabled)	throw mgr_err::Access("server");
		Node node (table->Node);
		StringMap table_values;
		gsutils::DbGetValues(table, table_values);
		string query = "func=" + table->Type+".resume&sok=ok";
		MakeQuery(query, table_values);
		node.MiniQuery(query);
		table->Active = true;
		table->Broken = false;
	}

	virtual void TableSuspend(isp_api::Session & ses, TableIdListAction< ServerTable >::Cursor& table) const
	{
		if (ses.auth.level() == mgr_access::lvUser &&  table->Disabled)	throw mgr_err::Access("server");
		Node node (table->Node);
		StringMap table_values;
		gsutils::DbGetValues(table, table_values);
		string query = "func=" + table->Type+".suspend&sok=ok";
		MakeQuery(query, table_values);
		node.MiniQuery(query);
		table->Active = false;
		table->Broken = false;
	}
	
	class aConfigureServer: public Action {
	public:
		aConfigureServer(aServers * parent) :Action("configure", mgr_access::MinLevel(mgr_access::lvUser),parent){}
        virtual void Execute(Session& ses) const
        {
			string redirect;
			auto cServer = db->Get<ServerTable>(ses.Param("elid"));
			if (ses.auth.level() == mgr_access::lvUser &&  cServer->Disabled) throw mgr_err::Access("server");
			auto cNode = db->Get<NodeTable>(cServer->Node);
			Node node (cServer->Node);
			string key = str::hex::Encode(str::Random(10));
			string query = "func=session.newkey&username="+cServer->Type+"_"+cServer->Id+"&key="+key;
			node.MiniQuery(query);
			
			string solveip = gsutils::ResolveHostname(cNode->Name);
			Debug("Resolve hostname '%s' get result '%s'", cNode->Name.AsString().c_str(), solveip.c_str());
			
			redirect += "https://" + (solveip == cNode->Ip?cNode->Name:cNode->Ip)+ ":" + cNode->MiniMgrPort + "/gsmini?func=auth&key="+key;//+"&redirect=func=desktop";
			ses.Ok(mgr_session::BaseSession::okBlank, redirect);
		}
	}ConfigureServer;
	
	class aEnableServer: public GroupAction{
	public:
		aEnableServer(aServers * parent) :GroupAction("enable", mgr_access::MinLevel(mgr_access::lvAdmin), parent){}
        virtual void ProcessOne(Session& ses, const string& elid) const
        {
			auto cServer = db->Get<ServerTable>(elid);
			Node node (cServer->Node);
			string query = "func=" + cServer->Type+".setparams";
			query += "&disabled=off";
			query += "&id="+cServer->Id;
			node.MiniQuery(query);
			cServer->Disabled = false;
			cServer->Post();
		}
	}EnableServer;
	
	class aDisableServer: public GroupAction{
	public:
		aDisableServer(aServers * parent) :GroupAction("disable", mgr_access::MinLevel(mgr_access::lvAdmin), parent){}
        virtual void ProcessOne(Session& ses, const string& elid) const
        {
			auto cServer = db->Get<ServerTable>(elid);
			Node node (cServer->Node);
			string query = "func=" + cServer->Type+".setparams";
			query += "&disabled=on";
			query += "&id="+cServer->Id;
			node.MiniQuery(query);
			cServer->Disabled = true;
			cServer->Active = false;
			cServer->Post();
		}
	}DisableServer;
	
	class aMigrateServer: public FormAction {
	public:
		aMigrateServer(aServers * parent) :FormAction("migrate", mgr_access::MinLevel(mgr_access::lvAdmin), parent){}
		virtual void Get(Session& ses, const string& elid) const
		{
			auto res = db->Query("SELECT name, id FROM node");
			ses.NewSelect("node");
			ForEachRecord(res) {
				ses.AddChildNode("val",res->AsString("name")).SetProp("key",res->AsString("id"));
			}
		}
		virtual void Set(Session& ses, const string& elid) const
		{
			auto cServer = db->Get<ServerTable>(elid);
			if (cServer->Node == str::Int(ses.Param("node")))
				throw mgr_err::Error("migrate_same_node");
			
				StringVector params;
				params.push_back("--server");
				params.push_back(elid);
				params.push_back("--node");
				params.push_back(ses.Param("node"));
				lt::LongTask longtask("sbin/" CMD_MIGRATE_SERVER, elid, "COPYDISTR");
				longtask.SetParams(params);
				new mgr_job::RunLongTask(longtask);
		}
	} MigrateServer;
	
	class aMigrateServerInfo: public Action{
	public:
		aMigrateServerInfo(aServers * parent) :Action("migrateinfo", mgr_access::MinLevel(mgr_access::lvAdmin), parent){}
		virtual void Execute(Session& ses) const
		{
			auto cServer = db->Get<ServerTable>(ses.Param("server"));
			auto cOldNode = db->Get<NodeTable>(cServer->Node);
			auto cNewNode = db->Get<NodeTable>(ses.Param("node"));
			
			ses.NewNode("game", cServer->Type);
			ses.NewNode("oldnode", cOldNode->Id);
			ses.NewNode("oldnode_ip", cOldNode->Ip);
			ses.NewNode("oldnode_is_local", cOldNode->LocalNode?"on":"off");
			ses.NewNode("newnode_is_local", cNewNode->LocalNode?"on":"off");
			ses.NewNode("newnode_ip", cNewNode->Ip);
			ses.NewNode("privkey", mgr_cf::GetParam(ConfsshPrivatKey));
			ses.NewNode("pubkey", mgr_cf::GetParam(ConfsshPublicKey));
			ses.NewNode("knownhosts", mgr_cf::GetParam(ConfsshKnownHosts));
			ses.NewNode("maxplayers", cServer->MaxPlayers);
			cServer->Moving = true;
			cServer->Post();
		}
	} MigrateServerInfo;
};


class aTeamSpeakServers: public isp_api::ExtTableIdListAction<TeamSpeakServerTable> {
public:
	aTeamSpeakServers() :
		isp_api::ExtTableIdListAction<TeamSpeakServerTable>("tsserver", mgr_access::MinLevel(mgr_access::lvUser), *db), EnableServer(this), DisableServer(this)
	{
	}
	virtual void List(isp_api::Session& ses) const
    {
		string q_where;
		if (ses.auth.level() != mgr_access::lvAdmin)
			q_where+=" WHERE s.user="+ses.auth.ext("uid");
		string srvip = mgr_cf::GetParam(TeamSpeakNodePublicIp);
		isp_api::DbList(ses, db->Query("SELECT s.id, s.name, '"+srvip+"' as ip, s.port, s.active, s.disabled  , s.maxplayers as players_total, s.onlineplayers as players_used, "
		                               "u.name as 'user' "
		                               "FROM tsserver s "
		                               "LEFT JOIN user u ON u.id=s.user " + q_where));

	}
	
    virtual void TableFormTune(Session& ses, TableIdListAction< TeamSpeakServerTable >::Cursor& table) const
    {
		DbSelect(ses, "user", db->Query("SELECT id, name FROM user where level=" + str::Str(isp_api::lvUser) + " order by name"));
		ses.NewSelect("game");
		StringMap games = GameModules::Get();
		ForEachI(games, game) {
			ses.AddChildNode("val",game->first);
		}
	}
	
    virtual void TableSet(Session& ses, TableIdListAction< TeamSpeakServerTable >::Cursor& table) const
	{
		TeamSpeakNode tsnode;
		if (table->IsNew()) {
			auto res = tsnode.CreateServer(table->Name, table->MaxPlayers);
			if (!res) throw mgr_err::Error("teamspeak","servercreate",res.Result["msg"]);
			auto lastdata = res.Data.rbegin();
			table->Port = str::Int((*lastdata)["virtualserver_port"]);
			table->AdminToken = (*lastdata)["token"];
			table->SID = str::Int((*lastdata)["sid"]);
			ses.NewNode("admintoken",table->AdminToken.AsString());
		} else {
			auto res = tsnode.SetServer(table->SID,table->Name, table->MaxPlayers);
			if (!res) throw mgr_err::Error("teamspeak","serveredit",res.Result["msg"]);
		}
	}
	
    virtual void TableBeforeDelete(Session& ses, TableIdListAction< TeamSpeakServerTable >::Cursor& table) const
    {
		TeamSpeakNode tsnode;
		auto res = tsnode.DeleteServer(table->SID);
		if (!res) throw mgr_err::Error("teamspeak","serverdelete",res.Result["msg"]);
	}
	
	virtual void TableResume(isp_api::Session & ses, TableIdListAction< TeamSpeakServerTable >::Cursor& table) const
	{
		TeamSpeakNode tsnode;
		auto res = tsnode.StartServer(table->SID);
		if (!res) throw mgr_err::Error("teamspeak","serverstart",res.Result["msg"]);
		table->Active = true;
	}

	virtual void TableSuspend(isp_api::Session & ses, TableIdListAction< TeamSpeakServerTable >::Cursor& table) const
	{
		TeamSpeakNode tsnode;
		auto res = tsnode.StopServer(table->SID);
		if (!res) throw mgr_err::Error("teamspeak","serverstop",res.Result["msg"]);
		table->Active = false;
	}
	
	class aEnableServer: public GroupAction{
	public:
		aEnableServer(aTeamSpeakServers * parent) :GroupAction("enable", mgr_access::MinLevel(mgr_access::lvAdmin), parent){}
        virtual void ProcessOne(Session& ses, const string& elid) const
        {
			auto cServer = db->Get<TeamSpeakServerTable>(elid);
			TeamSpeakNode tsnode;
			auto res = tsnode.StartServer(cServer->SID);
			if (!res) throw mgr_err::Error("teamspeak","serverstart",res.Result["msg"]);
			cServer->Disabled = false;
			cServer->Active = true;
			cServer->Post();
		}
	}EnableServer;
	
	class aDisableServer: public GroupAction{
	public:
		aDisableServer(aTeamSpeakServers * parent) :GroupAction("disable", mgr_access::MinLevel(mgr_access::lvAdmin), parent){}
        virtual void ProcessOne(Session& ses, const string& elid) const
        {
			auto cServer = db->Get<TeamSpeakServerTable>(elid);
			cServer->Disabled = true;
			TeamSpeakNode tsnode;
			auto res = tsnode.StopServer(cServer->SID);
			if (!res) throw mgr_err::Error("teamspeak","serverstop",res.Result["msg"]);
			cServer->Active = false;
			cServer->Post();
		}
	}DisableServer;
};

StringMap GameModules::m_game_modules = StringMap();

MODULE_INIT(gsmgr,"")
{
	Debug("Loading GSmanager...");

	mgr_cf::AddParam(ConfDBHost, DefaultDBHost);
	mgr_cf::AddParam(ConfDBUser, DefaultDBUser);
	mgr_cf::AddParam(ConfDBPassword);
	mgr_cf::AddParam(ConfDBName, DefaultDBName);
	mgr_cf::SetPathDefault(ConfGameInstallDir, DefaultGameInstallDir);

	mgr_db::ConnectionParams params;
	params.type = "mysql";
	params.host = mgr_cf::GetParam(ConfDBHost);
	params.user = mgr_cf::GetParam(ConfDBUser);
	params.password = mgr_cf::GetParam(ConfDBPassword);
	params.db = mgr_cf::GetParam(ConfDBName);

	mgr_cf::AddParam(ConfsshPublicKey, DefaultsshPublicKey);
	mgr_cf::AddParam(ConfsshPrivatKey, DefaultsshPrivateKey);
	mgr_cf::AddParam(ConfsshKnownHosts, DefaultsshKnownHosts);
	
	mgr_cf::AddParam(TeamSpeakNodeIp, "127.0.0.1");
	mgr_cf::AddParam(TeamSpeakNodePublicIp, "192.168.56.10");
	mgr_cf::AddParam(TeamSpeakNodePort, "10011");
	mgr_cf::AddParam(TeamSpeakNodePassword, "6zzUSEEW");
	
	db.reset(new mgr_db::JobCache(params));
	if (db->Query("show variables like 'have_innodb'")->AsString(1) != "YES") {
		throw mgr_err::Error("not_have_innodb");
	}

	db->Register<UserTable>();
	db->Register<NodeTable>();
	db->Register<ServerTable>();
	db->Register<AllowedGamesTable>();
	db->Register<TeamSpeakServerTable>();

	new gsAuth();
	new GameModulesList();
	new aServers();
	new aTeamSpeakServers();
	mgr_job::Commit();
	
	//XXX Убрать потом
	
	GameModules::AddGame("minecraft","Minecraft");
	GameModules::AddGame("teamfortress","TeamFortress");
	GameModules::AddGame("gta","GTA:SA");
	GameModules::AddGame("cs16","Counter-Strike 1.6");
	GameModules::AddGame("csgo","Counter-Strike: Global Offensive");
	GameModules::AddGame("left4dead2","Left4Dead 2");
	GameModules::AddGame("arma3","ARMA 3");
	GameModules::AddGame("rust","RUST");
}

void _headerDebug (int line, const string &file, const string &msg)
{
	Trace ("\%d:\%s: %s", line, file.c_str(), msg.c_str());

}

