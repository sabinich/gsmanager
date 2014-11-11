#include <mgr/mgrproc.h>
#include <mgr/mgrfile.h>
#include <mgr/mgrrpc.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrerr.h>
#include "../main/defines.h"
#include "steammodule.h"
#ifndef WIN32
#include <unistd.h>
#include <netinet/in.h>
#endif
#include <cstring>
#include <algorithm>

MODULE("gsmini");

#define STEAMCMDURL "http://media.steampowered.com/client/steamcmd_linux.tar.gz"

using namespace std;
using namespace mgr_file;
using namespace mgr_proc;
using namespace mgr_rpc;
using namespace mgr_err;

void GetMapsTool(const string& path, StringVector& maps) {
	mgr_file::Dir MapsDir(path);
	while(MapsDir.Read()) {
		const string bsp_ext = ".bsp";
		string cur_file = MapsDir.name();
		size_t ext_pos = cur_file.rfind(bsp_ext);
		if(ext_pos != string::npos && cur_file.size() - ext_pos == bsp_ext.size())
			maps.push_back(str::GetWord(cur_file, bsp_ext));
	}
}

void SteamGameModule::CreateServer(isp_api::Session& ses, const string& id) {
	GameModuleBase::CreateServer(ses, id);
	ServerInstanceInfo inst(Name, id);
//	StringVector maps;
// 	GetMaps(inst, maps);
// 	if(!maps.empty())
// 		inst.SetParam("map", maps.front());
}


bool SteamGameModule::IsUp(const string& id) {
	ServerInstanceInfo inst(Name, id);
	return !mgr_proc::Execute("pgrep -U " + inst.UserName + " " + ServerProcName).Result();
}

void SteamGameModule::GetMaps(ServerInstanceInfo& instance, StringVector& maps) {
	::GetMapsTool(mgr_file::ConcatPath(instance.GetInstancePath(), GameFolderName, "maps"), maps);
}

void SteamGameModule::BuildRegionsSelect(isp_api::Session& ses) {
	ses.NewSelect("sv_region");
	StringMap regions_map;
	regions_map["reg_us_east_coast"] = "0";
	regions_map["reg_us_west_coast"] = "1";
	regions_map["reg_south_america"] = "2";
	regions_map["reg_europe"] = "3";
	regions_map["reg_asia"] = "4";
	regions_map["reg_australia"] = "5";
	regions_map["reg_middle_east"] = "6";
	regions_map["reg_africa"] = "7";
	regions_map["reg_world"] = "255";

	ForEachI(regions_map, rm)
		ses.AddChildNode("msg", rm->first).SetProp("key", rm->second);
}

SteamServerQuery::SteamServerQuery(string ip, int port) : ServerAddress(ip, port) {
	Debug("Try to connect to %s:%d", ip.c_str(), port);
	SocketDesc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (-1 == SocketDesc)
		throw mgr_err::Error("steam_srv_sock", "socket", str::Str(errno));
	if (-1 == connect(SocketDesc, ServerAddress.addr(), ServerAddress.addrsize()))
		throw mgr_err::Error("steam_srv_connect", "connect", str::Str(errno));
}

//

int SteamServerQuery::OnlinePlayersArma() {
	string query("\x20\x00\x01\x08\x81\x80\xd5\x20\x36\x43\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xac\x10\xa1\xce\x35\x2d\x21\x27",32);
	SendToSocket(query);
	string answer;
	RecieveFromSocket(answer);
	Debug("Result length %d [%s]", answer.length(), answer.c_str());
	mgr_date::DateTime date;
	mgr_file::Append("/tmp/arma3"+date.AsTime(), answer);
	return 0;
}

int SteamServerQuery::OnlinePlayers() {
	string query = "\xFF\xFF\xFF\xFF\x54Source Engine Query";//A2S_INFO
	query.push_back('\x00');
	SendToSocket(query);
	string answer;
	RecieveFromSocket(answer);
	Debug("Result length %d [%s]",answer.length(), answer.c_str());
	if (answer.length() == 0) return 0;
	string header = answer.substr(0, 4);
	Debug("header is '%s'",str::hex::Encode(header).c_str());
//	Debug("data is '%s'",str::hex::Encode(answer).c_str());
	if (header != "\xFF\xFF\xFF\xFF" && header != "\xFF\xFF\xFF\xFE") {
		Warning("Unknown packet header");
		return 0;
	}
	answer.erase(0,4);
	char packtype = answer[0];
	string address;
	
	if (packtype == 'I') {
		Debug("Protocol ver %d pack type %c",answer[1], packtype);
		answer.erase(0,2); //Header + ver
	} else if (packtype == 'm') {
		Debug("pack type %c", packtype);
		answer.erase(0,1); //Header 
		address = str::GetWord(answer, '\x00');
	} else {
		Warning("Unknown packet type");
		return 0;
	}
	
	string name = str::GetWord(answer, '\x00');
	string map = str::GetWord(answer, '\x00');
	string folder = str::GetWord(answer, '\x00');
	string game = str::GetWord(answer, '\x00');
	Debug("Server info Address[%s] Name[%s] Map[%s] Folder[%s] Game[%s]", address.c_str(), name.c_str(), map.c_str(), folder.c_str(), game.c_str());
	
	if (packtype == 'I')
		answer.erase(0,2); // App ID
	unsigned int players  = (unsigned char)answer[0];
	unsigned int maxplayers  = (unsigned char)answer[1];
	unsigned int bots = 0;
	if (packtype == 'I')
		bots  = answer[2];
	
	Debug("Players on server %d/%d bots[%d]",players,maxplayers,bots);
	
	return players;
	
	// 	int result = -1;
// 	int string_counter = 4;
// 	
// 	for(size_t i = 5; i < answer.size(); ++i) {
// 		if(string_counter) {
// 			if(!answer[i])
// 				string_counter--;
// 			continue;
// 		}
// 		result = answer[i + sizeof(short)];
// 		break;
// 	}
// 	if(-1 == result) {
// 		Warning("Incorrect packet from server. It is possible that protocol was changed.");
// 		return 0;
// 	} else return result;
}

SteamServerQuery::~SteamServerQuery() {
#ifdef WIN32
	closesocket(SocketDesc);
#else
	close(SocketDesc);
#endif
}

void SteamServerQuery::SendToSocket(const string& message) {
	if(-1 == send(SocketDesc, message.c_str(), message.size(), 0))
		throw mgr_err::Error("steam_srv_sock", "send", str::Str(errno));
}

void SteamServerQuery::RecieveFromSocket(string& answer) {
	answer.clear();
	const int buff_len = 1024;
	char buffer[buff_len];
	fd_set readset;
	FD_ZERO(&readset);
	FD_SET(SocketDesc, &readset);
	while (true) {
		timeval timeout = {5, 0};
		if (select(SocketDesc + 1, &readset, NULL, NULL, &timeout) > 0) {
			memset(buffer, 0, buff_len);
			ssize_t recieved = recv(SocketDesc, buffer, sizeof(buffer), 0);
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

SteamServerConfig::SteamServerConfig(const string& cfg_path, const string& user_name) : m_path(cfg_path), m_user(user_name) {

}

void SteamServerConfig::ReadConf() {
	if (mgr_file::Exists(m_path)) {
		string conf = mgr_file::Read(m_path);
		StringList confLines;
		str::Split(conf, "\n", confLines);

		ForEachI(confLines, line) {
			size_t double_slash_pos = line->find("//");
			if(double_slash_pos != 0) {
				*line = str::GetWord(*line, "//");
				str::inpl::Trim(*line);
				string opt_value = *line;
				string opt_name = str::GetWord(opt_value);
				if(opt_name == "exec")
					continue;
				opt_value = str::Trim(opt_value);
				
				if (m_str_params.find(opt_name) != m_str_params.end()) {
					if (opt_value[0]=='\"')
						opt_value.erase(0,1);
					if (opt_value[opt_value.length()-1]=='\"')
						opt_value.erase(opt_value.length()-1,1);
				}
				m_conf[opt_name] = opt_value;
			}
		}
	} else {
#ifdef WIN32
		mgr_file::Attrs attrs(0744, mgr_user::WinSid(m_user).str(), mgr_user::WinSid(m_user).str());
#else
		mgr_file::Attrs attrs(0744, mgr_user::Uid(m_user), mgr_user::GroupGid(m_user));
#endif

		string conf_dir = m_path;
		str::RGetWord(conf_dir, '.');
		if(!mgr_file::Exists(conf_dir))
			mgr_file::MkDir(conf_dir, &attrs);

		mgr_file::Write(m_path);
		attrs.Set(m_path);
	}
}

void SteamServerConfig::Save() {
	StringList lines;
	
	string conf = mgr_file::Read(m_path);
	
	str::Split(conf, "\n", lines);
	ForEachI(lines, line) 
		*line = str::Trim(*line);
		
	ForEachI(m_conf, param) {
		bool found = false;
		if (m_str_params.find(param->first) != m_str_params.end())
			param->second = "\""+param->second+"\"";
		ForEachI(lines, line) {
			if (line->find(param->first) == 0) {
				string tmp = *line;
				string paramname = str::GetWord(tmp);
				if (paramname == param->first) {
					string comments;
					size_t pos = tmp.find("//");
					if (pos != string::npos) {
						comments = tmp.substr(pos);
						comments = str::Trim(comments);
						if (!comments.empty()) comments="\t"+comments;
					}					
					*line = paramname+"\t\t\t"+param->second+comments;
					found = true;
				}
				break;
			}
		}
		if (!found)
			lines.push_back(param->first+"\t\t\t"+param->second);
	}
	
	conf.clear();
	conf = str::Join(lines,"\n");
	mgr_file::Write(m_path, conf);
}

void SteamServerConfig::SetParams(StringMap & params) {
	ForEachI(params, i) {
		string param = i->first;
		string val = i->second;
		if (m_conf.find(param) != m_conf.end()) {
			if (val == "on") val = "1";
			if (val == "off") val ="0";
			m_conf[param]=val;
		}
	}
}

void SteamServerConfig::SetParamsFromSes(isp_api::Session & ses) {
	StringVector params;
	ses.GetParams(params);
	
	ForEachI(params, i) {
		if(*i == "map")
			continue;
		string param = *i;
		string val = ses.Param(*i);
		if (m_conf.find(param) != m_conf.end()) {
			if (val == "on") val = "1";
			if (val == "off") val ="0";
			m_conf[param] = val;
		}
	}
}

void SteamServerConfig::ConfToSes(isp_api::Session & ses) {
	ForEachI(m_conf, param) {
		string val = param->second;
		if (m_num_params.find(param->first) == m_num_params.end()) {
			if (val == "1") val = "on";
			if (val == "0") val = "off";
		}
		ses.NewNode(param->first.c_str(),val);
	}
}

void SteamServerConfig::SetParam(const string & param, const string & value) {
	if (m_conf.find(param) != m_conf.end())
		m_conf[param]=value;
}

string SteamServerConfig::GetParam(const string & param) {
	return m_conf[param];
}

void SteamServerConfig::DebugParams()
{
	ForEachI(m_conf, param) {
		Debug("[%s]\t\t[%s]",param->first.c_str(), param->second.c_str());
	}
}


string& SteamServerConfig::operator[] (const string & param) {
	return m_conf[param];
}

aMapCycleList::aMapCycleList(const string& game, const string& cfg_relative_path, const string& relative_maps_path) : ListAction (game + ".mapcycle", 
										mgr_access::MinLevel(mgr_access::lvUser)),
										aMapCycleEditObj(this),
										aMapCycleDeleteObj(this),
										aMapCycleUpObj(this, aMapCycleMove::dirUP),
										aMapCycleDownObj(this, aMapCycleMove::dirDOWN),
										GameName(game),
										CfgRelativePath(cfg_relative_path),
										RelativeMapsPath(relative_maps_path) {}

void aMapCycleList::List (isp_api::Session& ses) const {
	ServerInstanceInfo inst(ses);
	string current_cycle = ReadE(mgr_file::ConcatPath(inst.GetInstancePath(), CfgRelativePath));
	int cur_order = 0;
	while(!current_cycle.empty()) {
		ses.NewElem();
		if(!cur_order)
			ses.AddChildNode("is_boundary", "first");
		ses.AddChildNode("order", str::Str(cur_order++));
		ses.AddChildNode("map", str::GetWord(current_cycle, '\n'));
	}
	if(cur_order)
		ses.AddChildNode("is_boundary", "last");
}

const string& aMapCycleList::GetGameName() const {
	return GameName;
}

const string& aMapCycleList::GetCfgRelativePath() const {
	return CfgRelativePath;
}

const string& aMapCycleList::GetRelativeMapsPath() const {
	return RelativeMapsPath;
}

bool aMapCycleList::CheckAccess (const isp_api::Authen& auth) const {
	return GameModules[GameName]->CheckAccess(auth);
}


#define CHILD_CHECK_ACCESS_DEF(X) \
bool X::CheckAccess (const isp_api::Authen& auth ) const { \
	return GameModules[reinterpret_cast<const aMapCycleList*>(parent())->GetGameName()]->CheckAccess(auth); \
}

aMapCycleList::aMapCycleEdit::aMapCycleEdit(aMapCycleList * parent) : FormAction("edit", mgr_access::MinLevel(mgr_access::lvUser), parent) {}

void aMapCycleList::aMapCycleEdit::Get(isp_api::Session& ses, const string& elid) const {
	ServerInstanceInfo inst(ses);
	StringVector maps;
	GetMapsTool(mgr_file::ConcatPath(inst.GetInstancePath(), reinterpret_cast<const aMapCycleList*>(parent())->GetRelativeMapsPath()), maps);
	ses.BuildSelect(maps.begin(), maps.end(), "map");
	if(!elid.empty()) {
		StringVector maps_in_cycles;
		str::Split(mgr_file::ReadE(mgr_file::ConcatPath(inst.GetInstancePath(), reinterpret_cast<const aMapCycleList*>(parent())->GetCfgRelativePath())), "\n", maps_in_cycles);
		ses.NewNode("map", maps_in_cycles[str::Int(elid)]);
	}
}

void aMapCycleList::aMapCycleEdit::Set(isp_api::Session& ses, const string& elid) const {
	ServerInstanceInfo inst(ses);
	const string map_cycle_path = mgr_file::ConcatPath(inst.GetInstancePath(), reinterpret_cast<const aMapCycleList*>(parent())->GetCfgRelativePath());
	if(elid.empty())
		Append(map_cycle_path, ses.Param("map") + '\n');
	else {
		StringVector maps_in_cycle;

		str::Split(Read(map_cycle_path), "\n", maps_in_cycle);
		maps_in_cycle[str::Int(elid)] = ses.Param("map");
		string new_cycle;
		ForEachI(maps_in_cycle, map)
			new_cycle += *map + "\n";
		Write(map_cycle_path, new_cycle);
	}
}

CHILD_CHECK_ACCESS_DEF(aMapCycleList::aMapCycleEdit)


aMapCycleList::aMapCycleDelete::aMapCycleDelete (aMapCycleList* parent) : Action("delete", mgr_access::MinLevel(mgr_access::lvUser), parent) {}

void aMapCycleList::aMapCycleDelete::Execute(isp_api::Session& ses) const {
	ServerInstanceInfo inst(ses);
	const string path = mgr_file::ConcatPath(inst.GetInstancePath(), reinterpret_cast<const aMapCycleList*>(parent())->GetCfgRelativePath());
	StringVector maps_in_cycle;
	str::Split(ReadE(path), "\n", maps_in_cycle);
	StringSet elids;
	str::Split(ses.Param("elid"), ", ", elids);

	size_t deleted = 0;
	ForEachI(elids, elid)
		maps_in_cycle.erase(maps_in_cycle.begin() + str::Int(*elid) - deleted++);

	string new_cycle;
	ForEachI(maps_in_cycle, map)
		new_cycle += *map + '\n';
	mgr_file::Write(path, new_cycle);
	inst.NeedRestart();
}

CHILD_CHECK_ACCESS_DEF(aMapCycleList::aMapCycleDelete)

aMapCycleList::aMapCycleMove::aMapCycleMove(aMapCycleList * parent, Direction dir) : Action((dir == dirUP ? "up" : "down"), parent) {
	MoveDirection = dir;
}

void aMapCycleList::aMapCycleMove::Execute(isp_api::Session& ses) const {
	ServerInstanceInfo inst(ses);
	size_t old_elid = str::Int(ses.Param("elid"));
	const string path = mgr_file::ConcatPath(inst.GetInstancePath(), reinterpret_cast<const aMapCycleList*>(parent())->GetCfgRelativePath());
	StringVector maps_in_cycle;
	str::Split(ReadE(path), "\n", maps_in_cycle);
	StringSet elids;
	str::Split(ses.Param("elid"), ", ", elids);

	int new_elid;
	if(dirUP == MoveDirection && old_elid > 0)
		new_elid = old_elid - 1;
	else if(dirDOWN == MoveDirection && old_elid < maps_in_cycle.size() - 1)
		new_elid = old_elid + 1;
	else {
		ses.Ok(mgr_session::BaseSession::okList, "func=" + reinterpret_cast<const aMapCycleList*>(parent())->GetGameName() + ".mapcycle");
		return;
	};

	std::swap(maps_in_cycle[old_elid], maps_in_cycle[new_elid]);
	string new_cycle;
	ForEachI(maps_in_cycle, map)
		new_cycle += *map + '\n';
	mgr_file::Write(path, new_cycle);
	ses.Ok(mgr_session::BaseSession::okList, "func=" + reinterpret_cast<const aMapCycleList*>(parent())->GetGameName() + ".mapcycle");
	inst.NeedRestart();
} 

CHILD_CHECK_ACCESS_DEF(aMapCycleList::aMapCycleMove)
