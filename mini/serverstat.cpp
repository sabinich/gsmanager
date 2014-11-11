#include "serverstat.h"
#define STAT_PATH "stat"
#define DAY_STAT_FILE "daystat"
#define MONTH_STAT_FILE "monthstat"
#include "mgr/mgrdate.h"
#include <mgr/mgrlog.h>
MODULE("gsmini");

void AddServerStat(time_t now, ServerInstanceInfo& srv, int players, int memory)
{
	time_t day = 60*60*24;
	time_t weak = day*7;
	time_t month = day*31;
	string statdir = mgr_file::ConcatPath(srv.GetMgrDataPath(), STAT_PATH);
	string daystatpath = mgr_file::ConcatPath(statdir, DAY_STAT_FILE);
	string monthstatpath = mgr_file::ConcatPath(statdir, MONTH_STAT_FILE);
	
	if (!mgr_file::Exists(statdir)) mgr_file::MkDir(statdir);
	//Day
	time_t startdate = now - weak;
	StringList data;
	if (mgr_file::Exists(daystatpath)) {
		string tmp = mgr_file::Read(daystatpath);
		str::Split(tmp, "\n", data);
	}
	
	for(auto line = data.begin(); line != data.end();) {
		string tmp = *line;
		time_t date = str::Int64(str::GetWord(tmp, "|"));
		if (date < startdate)
			data.erase(line++);
		else 
			++line;
	}
	data.push_back(str::Str(now)+"|"+str::Str(players)+"|"+str::Str(memory));
	mgr_file::Write(daystatpath, str::Join(data,"\n"));
	
	//Month
	data.clear();
	if (mgr_file::Exists(monthstatpath)) {
		string tmp = mgr_file::Read(monthstatpath);
		str::Split(tmp, "\n", data);
	}
	now = now - (now % (day));
	startdate = now - month;
	for(auto line = data.begin(); line != data.end();) {
		string tmp = *line;
		time_t date = str::Int64(str::GetWord(tmp, "|"));
		if (date < startdate)
			data.erase(line++);
		else 
			++line;
	}
	
	string date = str::Str(now);
	string last_line;
	if (data.size() > 0) {
		last_line = data.back();
	}
	if (date == str::GetWord(last_line,"|")) {
		int pl = str::Int(str::GetWord(last_line,"|"));
		int mem = str::Int(str::GetWord(last_line,"|"));
		data.back() = (str::Str(now)+"|"+str::Str(players > pl?players:pl)+"|"+str::Str(memory>mem?memory:mem));
	} else {
		data.push_back(str::Str(now)+"|"+str::Str(players)+"|"+str::Str(memory));
	}
	mgr_file::Write(monthstatpath, str::Join(data,"\n"));
}

void GetDayStat(ServerInstanceInfo& srv, isp_api::Session& ses)
{
	string statdir = mgr_file::ConcatPath(srv.GetMgrDataPath(), STAT_PATH, DAY_STAT_FILE);
	Debug("File to read %s", statdir.c_str());
	StringList data;
	if (mgr_file::Exists(statdir)) {
		string tmp = mgr_file::Read(statdir);
	//	Debug("File content %s", tmp.c_str());
		str::Split(tmp, "\n", data);
	}
	auto rdata = ses.NewNode("reportdata");
	auto eldata = rdata.AppendChild("data");
	ForEachI(data, line) {
		auto elem = eldata.AppendChild("elem");
		time_t date = str::Int64(str::GetWord(*line,"|"));
		string players = str::GetWord(*line,"|");
		int mem = str::Int(str::GetWord(*line,"|"));
		mgr_date::DateTime strdate(date);
		elem.AppendChild("date",strdate.AsDateTime());
		elem.AppendChild("players", players);
		elem.AppendChild("memory",str::Str(mem/1000));
	}
}

void GetMonthStat(ServerInstanceInfo& srv, isp_api::Session& ses)
{
	string statdir = mgr_file::ConcatPath(srv.GetMgrDataPath(), STAT_PATH, MONTH_STAT_FILE);
	StringList data;
	if (mgr_file::Exists(statdir)) {
		string tmp = mgr_file::Read(statdir);
		str::Split(tmp, "\n", data);
	}
	auto rdata = ses.NewNode("reportdata");
	auto eldata = rdata.AppendChild("data");
	ForEachI(data, line) {
		auto elem = eldata.AppendChild("elem");
		time_t date = str::Int64(str::GetWord(*line,"|"));
		string players = str::GetWord(*line,"|");
		int mem = str::Int(str::GetWord(*line,"|"));
		mgr_date::DateTime strdate(date);
		elem.AppendChild("date", strdate.AsDate());
		elem.AppendChild("players", players);
		elem.AppendChild("memory", str::Str(mem/1000));
	}
}
