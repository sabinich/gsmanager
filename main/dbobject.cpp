#include "dbobject.h"
#include <mgr/mgrlog.h>
#include <api/auth.h>
MODULE("gsmgr");
UserTable::UserTable() :
	mgr_db::Table("user", 64),
	Password(this, "password", 128),
	Level(this, "level"),
	Enabled(this, "enabled")
{
	Password.info().access_read = mgr_access::AccessNone;
	Password.info().access_write = mgr_access::AccessNone;
	Enabled.info().access_write = mgr_access::AccessNone;
}

NodeTable::NodeTable()
	: mgr_db::Table		("node", 64)
	, Ip				(this, "ip", 64)
	, Status			(this, "status")
	, MiniPassword		(this, "minipassword", 128)
	, MiniMgrPort		(this, "minimgrport")
	, LocalNode			(this, "islocal")
	, Type				(this, "type", 32)
    , MemSize			(this, "mem_total")
    , MemUsed			(this, "mem_used")
    , CpuUsage			(this, "cpu", 10)
    , HddSpeed			(this, "hdd", 64)
    , HddRd				(this, "hdd_rd")
    , HddWr				(this, "hdd_wr")
    , MeasureEndTime	(this, "end_time")
    , MeasureStartTime	(this, "start_time")
{
	Status.info().access_read = mgr_access::AccessNone;
	Status.info().def_value = "off";
	Type.info().def_value = "linux";
	MiniPassword.info().access_read = mgr_access::AccessNone;
	MiniMgrPort.info().set_default("1500");
	LocalNode.info().def_value = "off";
}

ServerTable::ServerTable() : mgr_db::Table ("server", 128),
	Node (this, "node", mgr_db::rtRestrict),
	User (this, "user", mgr_db::rtRestrict),
	Type (this, "type", 64),
	Ip(this,"ip", 64),
	Port(this,"port"),
	MaxPlayers(this,"maxplayers"),
	Active(this, "active"),
	Moving(this, "moving"),
	Broken(this, "broken"),
	Disabled(this, "disabled"),
    OnlinePlayers(this, "onlineplayers"),
    DiskUsed(this, "diskused"),
    RamUsed(this, "ramused"),
    CpuUsed(this, "cpuused")
{
	Active.info().def_value = "off";
	Broken.info().def_value = "off";
}

bool TeamSpeakServerTable::CheckAccess(int level, const string& id) const
{
    Debug("TeamSpeakServerTable::CheckAccess");
	if (level == mgr_access::lvAdmin) 
		return true;
	if (level == mgr_access::lvUser) {
		if (this->Disabled) return false;
		auto cUser = db->Get<UserTable>(id);
		return cUser->Id == (int)User;
	} else 
		return false;
	return true;
}

TeamSpeakServerTable::TeamSpeakServerTable() : mgr_db::Table ("tsserver", 128),
	SID(this,"sid"),
	User (this, "user", mgr_db::rtRestrict),
	Ip(this,"ip", 64),
	Port(this,"port"),
	MaxPlayers(this,"maxplayers"),
	Active(this, "active"),
	Disabled(this, "disabled"),
    OnlinePlayers(this, "onlineplayers"),
    AdminToken(this,"admintoken",128)
{
	Active.info().def_value = "off";
}

AllowedGamesTable::AllowedGamesTable() : mgr_db::IdTable("allowedgames"),
	Node (this, "node", mgr_db::rtCascade),
	GameName (this, "gamename", 64)
{
}
bool ServerTable::CheckAccess(int level, const string& id) const
{
	Debug("ServerTable::CheckAccess");
	if (level == mgr_access::lvAdmin) 
		return true;
	if (level == mgr_access::lvUser) {
		if (this->Disabled) return false;
		auto cUser = db->Get<UserTable>(id);
		return cUser->Id == (int)User;
	} else 
		return false;
	return true;
}


bool ModuleTable::CheckAccess(int level, const string& id) const
{
   return db->Get<ServerTable>(this->Server)->CheckAccess(level,id);
}
