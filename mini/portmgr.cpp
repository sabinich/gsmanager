#include <api/module.h>
#include <mgr/mgrlog.h>
#include <api/action.h>
#include <api/stdconfig.h>
#include "../main/defines.h"
#include "gmmini.h"
#include <api/auth_method.h>
#include "portmgr.h"

#ifdef WIN32
#include "win_common.h"
#endif

ConfRef PortMgrConf;
MODULE("gsmini");

std::map<string, std::pair<int,int> > PortsPoll;

mgr_thread::SafeLock PortMgrLock;

bool PortMgr::PortRec::Parse(Cf cf)
{
	if (cf->token() != "Port") return false;
	cf.CacheStart(this);
	cf->GetToken();
	cf.CacheField(this->Ip);
	cf->GetToken();
	cf.CacheField(this->Port);
	cf->GetToken();
	cf.CacheField(this->Owner);
	try{
			cf->GetToken();
	} catch(...){}
	cf.CacheDone(this,true);
	return true;
}

bool PortMgr::FreePort(const string& owner, const string& ip, int port)
{
	mgr_thread::SafeSection sf(PortMgrLock);
	auto cPort = PortMgrConf.Get<PortMgr::PortRec>();
	if (ip.empty()) {
		ForEachRecord(cPort) {
			if (cPort->Owner==owner) {
#ifdef WIN32
				RemoveFirewallRule(owner+"_udp");
				RemoveFirewallRule(owner+"_tcp");
#endif
				cPort->Delete();
			}
		}
	}
	if (cPort->Find(ip+"_"+str::Str(port)) && cPort->Owner == owner) {
#ifdef WIN32
		RemoveFirewallRule(owner+"_udp");
		RemoveFirewallRule(owner+"_tcp");
#endif
		cPort->Delete();
		return true;
	} else 
		return false;
}

int PortMgr::AllocPort(const string& owner, const string& ip, int port, string poll)
{
	if (PortsPoll.find(poll) == PortsPoll.end()) {
		Warning("Unknown ports poll '%s' get port from 'main' poll", poll.c_str());
		poll = "main";
	}
	
	mgr_thread::SafeSection sf(PortMgrLock);
	auto cPort = PortMgrConf.Get<PortMgr::PortRec>();
	if (port == 0) {
// 		ForEachRecord(cPort) {
// 			if (cPort->Owner == owner)
// 				cPort->Delete();
// 		}

		for (int i = PortsPoll[poll].first; i <= PortsPoll[poll].second; i++) {
			if (!cPort->Find(ip+"_"+str::Str(i))) {
				cPort->New();
				cPort->Port = i;
				cPort->Owner = owner;
				cPort->Ip = ip;
				cPort->Post();
#ifdef WIN32
				FirewallOpenLocalPort(owner+"_udp",cPort->Ip, cPort->Port, "udp");
				FirewallOpenLocalPort(owner+"_tcp",cPort->Ip, cPort->Port, "tcp");
#endif
				return i;
			}
		}
		throw mgr_err::Error("no_free_ports");
	} else {
		if (cPort->Find(ip+"_"+str::Str(port))) {
			if (cPort->Owner == owner)
				return port;
			else 
				throw mgr_err::Error("port_occupied", cPort->Ip+" - "+cPort->Port.Str());
		}
		ForEachI(PortsPoll, poll) {
			if (port < poll->second.first || port > poll->second.second)
				throw mgr_err::Value("port", str::Str(port));
		}
		cPort->New();
		cPort->Port = port;
		cPort->Owner = owner;
		cPort->Ip = ip;
		cPort->Post();
#ifdef WIN32
		FirewallOpenLocalPort(owner+"_udp",cPort->Ip, cPort->Port, "udp");
		FirewallOpenLocalPort(owner+"_tcp",cPort->Ip, cPort->Port, "tcp");
#endif
		ForEachRecord(cPort) {
			if (cPort->Owner == owner && cPort->Port != port)
				cPort->Delete();
		}
		return port;
	}
}

int PortMgr::AllocPortDecade(const string& owner, const string& ip, string poll) 
{
	mgr_thread::SafeSection sf(PortMgrLock);
	auto cPort = PortMgrConf.Get<PortMgr::PortRec>();

	for (int i = PortsPoll[poll].first; i <= PortsPoll[poll].second; i++) {
		if (i % 10 == 0) {
			bool free = true;
			for (int j = 0 ; j < 10; j++) {
				if (cPort->Find(ip+"_"+str::Str(i+j)))
					free = false;
			}
			if (free) {
				for (int j = 0 ; j < 10; j++) {
					cPort->New();
					cPort->Port = i+j;
					cPort->Owner = owner;
					cPort->Ip = ip;
					cPort->Post();
#ifdef WIN32
					FirewallOpenLocalPort(owner+"_udp",cPort->Ip, cPort->Port, "udp");
					FirewallOpenLocalPort(owner+"_tcp",cPort->Ip, cPort->Port, "tcp");
#endif
				}
				return i;
			}
		}
	}
	throw mgr_err::Error("no_free_ports");
}

MODULE_INIT(portmgr, "gsmini")
{
	PortMgrConf = PortMgr::PortConfig::Config("var/port.list", true);
	PortsPoll["main"] = std::pair<int,int>(33000,43000);
	PortsPoll["steam"] = std::pair<int,int>(8766,9766);
	PortsPoll["steamquery"] = std::pair<int,int>(27516,29016);
}