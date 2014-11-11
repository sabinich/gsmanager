#ifndef DBOBJECT_H
#define DBOBJECT_H

#include <mgr/mgrdb.h>
#include <mgr/mgrdb_struct.h>

class UserTable : public mgr_db::Table {
public:
	mgr_db::StringField Password;
	mgr_db::IntField Level;
	mgr_db::BoolField Enabled;

	UserTable();
};

class NodeTable : public mgr_db::Table {
public:
	typedef std::shared_ptr<NodeTable> Cursor;
	mgr_db::StringField Ip;
	mgr_db::BoolField Status;
	mgr_db::StringField MiniPassword;
	mgr_db::IntField MiniMgrPort;
	mgr_db::BoolField LocalNode;
	mgr_db::StringField Type;

	mgr_db::LongField MemSize;
	mgr_db::LongField MemUsed;
	mgr_db::StringField CpuUsage;
	mgr_db::StringField HddSpeed;
	mgr_db::LongField HddRd;
	mgr_db::LongField HddWr;
	mgr_db::DateTimeField MeasureEndTime;
	mgr_db::DateTimeField MeasureStartTime;
	NodeTable();
};

class ServerTable : public mgr_db::Table
{
public:
	mgr_db::ReferenceField Node;
	mgr_db::ReferenceField User;
	mgr_db::StringField Type;
	mgr_db::StringField Ip;
	mgr_db::IntField Port;
	mgr_db::IntField MaxPlayers;
	mgr_db::BoolField Active;	//Состояние сервера - запущен/остановлени
	mgr_db::BoolField Moving;
	mgr_db::BoolField Broken;
	mgr_db::BoolField Disabled;	//Состояние сервера - заблокирован(т.е. пользователь больше не может пользоваться этой услугой)
	mgr_db::IntField OnlinePlayers;
	mgr_db::FloatField DiskUsed;
	mgr_db::IntField RamUsed;
	mgr_db::IntField CpuUsed;	
	ServerTable ();
	
    virtual bool CheckAccess(int level, const string& id) const;
};

class TeamSpeakServerTable : public mgr_db::Table
{
public:
 	mgr_db::IntField SID;
	mgr_db::ReferenceField User;
	mgr_db::StringField Ip;
	mgr_db::IntField Port;
	mgr_db::IntField MaxPlayers;
	mgr_db::BoolField Active;	//Состояние сервера - запущен/остановлени
	mgr_db::BoolField Disabled;	//Состояние сервера - заблокирован(т.е. пользователь больше не может пользоваться этой услугой)
	mgr_db::IntField OnlinePlayers;
	mgr_db::StringField AdminToken;
	TeamSpeakServerTable ();
	
    virtual bool CheckAccess(int level, const string& id) const;
};

class ModuleTable : public mgr_db::IdTable
{
public:
	mgr_db::ReferenceField Server;
	ModuleTable (const string & name) : mgr_db::IdTable (name),
		Server (this, "server", mgr_db::rtRestrict)
	{}
	
    virtual bool CheckAccess(int level, const string& id) const;
};

class AllowedGamesTable : public mgr_db::IdTable
{
public:
	mgr_db::ReferenceField Node;
	mgr_db::StringField GameName;
	AllowedGamesTable();
};

extern std::shared_ptr<mgr_db::Cache> db;
#endif // DBOBJECT_H
