#include "stat.h"
#include <api/module.h>
#include <api/action.h>
#include <api/stdconfig.h>
#include <mgr/mgrscheduler.h>
#include <mgr/mgrjob.h>
#include <api/xmlcache.h>
#include "gmmain.h"
#include <fstream>

MODULE("gsmgr");

using namespace isp_api;

class GameServersStatRecordSet : public Stat::Recordset 
{
public:
	Stat::KeyField Game;
	Stat::KeyField Server;
	Stat::KeyField Node;
	Stat::DataField PlayersOnline;
	Stat::DataField MemoryUsed;
	Stat::DataField HddUsed;
	GameServersStatRecordSet() :
		Game(this,"game"),
		Server(this,"server"),
		Node(this,"node"),
		PlayersOnline(this, "players", false, Stat::dftMax),
		MemoryUsed(this, "memory", false, Stat::dftMax),
		HddUsed(this, "hdd", false, Stat::dftMax)
		{} 
	string Name () const
	{
		return "servers_stat";
	}
};

class ServerDayStat : public Stat::DataTable<GameServersStatRecordSet>
{
public:
	ServerDayStat()
		: Stat::DataTable <GameServersStatRecordSet>("servers_stat_day", 730)
	{}
};
class ServerHourStat : public Stat::DataTable<GameServersStatRecordSet>
{
public:
	ServerHourStat()
		: Stat::DataTable <GameServersStatRecordSet>("servers_stat_hour", 720)
	{}
};

void AddServerStat(time_t date, const string & game, const string & server, const string & node, int players, long long memory, long long hdd) {
	GameServersStatRecordSet module;
	module.Game = game;
	module.Server = server;
	module.Node = node;
	module.PlayersOnline = players;
	module.MemoryUsed = memory;
	module.HddUsed = hdd;
	Stat::MinutelyProcess(date, module);
}

class eCollectStatEvent : public Event {
public:
	eCollectStatEvent(): Event("collectstat", "comman_srv_stat") {}
	virtual void AfterExecute(Session&) const
	{
		try {
			time_t now = time(0);
			Stat::HourlyProcess<ServerHourStat>(now);
			Stat::DailyProcess<ServerDayStat, ServerHourStat>(now);
			ForEachQuery(db,"SELECT id, name, node, type, ramused, diskused, onlineplayers FROM server", res) {
				Debug("AddStat for server %s %s",res->AsString("id").c_str(),res->AsString("name").c_str());
				AddServerStat(now,res->AsString("type"), res->AsString("id"), res->AsString("node"), res->AsInt("onlineplayers"), res->AsLong("ramused"), res->AsDouble("diskused")*1024);
			}
		} catch(...) {
		}
	}
};

class aPlayersActivityReport : public ReportAction {
public:
	aPlayersActivityReport(): ReportAction("overallplayers", mgr_access::MinLevel(lvAdmin))
	{
	}
	virtual void ReportExecute(Session& ses) const
	{
		string day = ses.Param("day");
		auto msgnode = mgr_xml::XPath(ses.xml, "//messages").begin();
		auto bandnode = mgr_xml::XPath(ses.xml, "//band[@name='data']").begin();
		auto diagnode = mgr_xml::XPath(ses.xml, "//diagram[@name='diagram_players']").begin();
		
		StringMap games = GameModules::Get();
		
		ForEachI(games, game) {
			msgnode->AppendChild("msg", game->second).SetProp("name", game->first);
			bandnode->AppendChild("col").
			SetProp("name", game->first).
			SetProp("type", "data").
			SetProp("sort", "digit");
			diagnode->AppendChild("line").SetProp("data", game->first);
		}
		
		string date_filter="";
		if (!day.empty()) {
			Debug("Day is '%s'",day.c_str());
			time_t end_date = mgr_date::DateTime(day + " 23:59:59");
			time_t start_date = mgr_date::DateTime(day + " 00:00:00");
			date_filter = "WHERE begin >=" + str::Str(start_date) + " AND begin <=" + str::Str(end_date);
		}

		string query = "SELECT SUM(players) AS 'players' FROM servers_stat_hour WHERE game='__GAME__' ";

		auto repdata = ses.NewNode("reportdata");
		auto srvinfo = repdata.AppendChild("players");
		auto info = srvinfo.AppendChild("elem");
		Messages messages(ses.conn.lang());
		string info_msg = messages.Load("overallplayers").Msg("info");
		info.AppendChild("info", info_msg);
		auto traf = info.AppendChild("data");
		
		ForEachQuery(db,"SELECT begin FROM servers_stat_hour "+date_filter+" GROUP BY begin",list) {
			auto node = traf.AppendChild("elem");
			mgr_date::DateTime tmp(list->AsLong("begin"));
			node.AppendChild("date", tmp.AsDateTime());

			ForEachI(games, game) {
				auto res = db->Query(str::Replace(query, "__GAME__", game->first)+ "AND begin = "+list->AsString("begin"));
				ForEachRecord(res) {
					node.AppendChild(game->first.c_str(), str::Str(res->AsInt("players")));
				}
			}
		}
	}
};

MODULE_INIT(GSStat, "gsmgr nodestat") {
	db->Register<ServerHourStat>();
	db->Register<ServerDayStat>();
	StatUtils::InitStatsDB(db);
	new eCollectStatEvent;
	new aPlayersActivityReport;
	Debug("Init GSStat");
}