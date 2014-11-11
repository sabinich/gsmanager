#include <signal.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrproc.h>
#include <api/action.h>
#include <mgr/mgrnet.h>
//#include <api/ipdb.h>
#include <api/stdconfig.h>
//#include <api/problems.h>
//#include <mgr/mgrdate.h>
#include <mgr/mgrproc.h>
#include "sbin_utils.h"
#include "stat_utils.h"
#include "traff_table.h"
#include "traff_nflow.h"
#include "csvfile.h"
#include "traff.h"

MODULE("netflow");

#define NFACCTD "nfacctd"
#define NFACCTD_CONFIG_PATH "etc/nfacctd.conf"

#define NFACCTD_BIN_PATH "libexec/nfacctd"

#define FLOWDIR_PATH "var/flow"

#define ConfNetFlowPort "NetFlowPort"
#define ConfNetFlowIp "NetFlowIp"
#define DefaultNetFlowPort "9995"
#define DefaultNetFlowIp "0.0.0.0"
#define ConvEnableNFACCTDLog "EnableNFACCTDLog"



namespace
{
int Terminate(const string &name)
{
	mgr_proc::PidVector pidlist;
	if (mgr_proc::List(name, pidlist))
			ForEachI(pidlist, item)
					kill(*item,SIGTERM);
	return (int)pidlist.size();
}

}

using namespace isp_api;
namespace {
	class ActionCheckFlow: public Action {
	public:
		ActionCheckFlow():Action("checkflow", MinLevel(lvAdmin)){};
		virtual void Execute(Session &) const
		{
			LogInfo ("checking neflow collector daemon");
			try {
				if (!mgr_cf::FindOption(ConfUseNetflow)) {
					netflow::daemon::Stop();
					return;
				}
				netflow::daemon::Check();
			} catch(...) {};
		}
	};
};

void netflow::daemon::Start()
{
	WriteConfig();
	string res = mgr_proc::Execute(NFACCTD_BIN_PATH " -f "+ mgr_file::ConcatPath(mgr_file::GetCurrentDir(),NFACCTD_CONFIG_PATH),mgr_proc::Execute::efOutErr|mgr_proc::Execute::efLeaveLdPath).Str();
	Debug("NFACCTD START RESULT %s", res.c_str());

}



void netflow::daemon::Stop()
{
	Terminate(NFACCTD);
}


void netflow::daemon::WriteConfig()
{
	string netflow_conf = 	"daemonize: true\n"
			"plugins: print\n"
			"aggregate: src_host,dst_host\n"
			"nfacctd_ip: __ip__\n"
			"nfacctd_port: __port__\n"
			"print_refresh_time: 300\n"
			"print_output: csv\n"
			"print_output_file: __flowdir__/%Y-%m-%d %H:%M:00\n";
	netflow_conf = str::Replace(netflow_conf, "__port__", mgr_cf::GetParam(ConfNetFlowPort));
	netflow_conf = str::Replace(netflow_conf, "__ip__", mgr_cf::GetParam(ConfNetFlowIp));
	netflow_conf = str::Replace(netflow_conf, "__flowdir__", mgr_file:: ConcatPath(mgr_file::GetCurrentDir(),FLOWDIR_PATH));

	if (mgr_cf::FindOption(ConvEnableNFACCTDLog))
		netflow_conf +="logfile: "+mgr_file::ConcatPath(mgr_file::GetCurrentDir(),"var/nfacctd.log\n");


	if (!mgr_file::Exists(FLOWDIR_PATH))
		mgr_file::MkDir(FLOWDIR_PATH);
	mgr_file::Write(NFACCTD_CONFIG_PATH, netflow_conf);
}


void netflow::daemon::Check() {
	if (!mgr_proc::Exists(NFACCTD)) {
		Debug ("need to start " NFACCTD);
		Start();
	}
}


netflow::ObjRecord::ObjRecord()
	: Rx(0), Tx(0), Rp(0), Tp(0)
{}

void netflow::ObjRecord::Add(uint64_t rx, uint64_t tx, uint64_t rp, uint64_t tp)
{
	Rx += rx;
	Tx += tx;
	Rp += rp;
	Tp += tp;
}

void netflow::ObjRecord::Add(const netflow::ObjRecord &other)
{
	Name = other.Name;
	Parent = other.Parent;
	Rx += other.Rx;
	Tx += other.Tx;
	Rp += other.Rp;
	Tp += other.Tp;
}

void netflow::ObjRecord::DumpDbg() const
{
	LogNote ("Object: ['%s', '%s', '%llu', '%llu']",
			 Name.c_str(), Parent.c_str(), Rx, Tx);
}


netflow::FilterRecord::FilterRecord(const string &object, int direction, const string &ipRange)
	: Object (object)
	, Direction (direction)
	, IpRange (ipRange)
{}

void netflow::FilterRecord::DumpDbg()
{
	LogNote ("Filter: ['%s', '%d', '%s']",
			 Object.c_str(), Direction, IpRange.FirstIp().Str().c_str());
}


void netflow::Collector::FillObjects()
{
	OnFillObjects(m_Objects);
}

void netflow::Collector::FillFilters()
{
	m_Filters.clear();
	ForEachQuery(StatUtils::DB(), "SELECT * FROM traffignore", filter) {
		FilterRecord record(filter->AsString("object"),
							filter->AsInt("direction"),
							filter->AsString("ip"));
		m_Filters.push_back(record);
	}
}

bool netflow::Collector::NeedDrop(const netflow::ObjRecord &record, const netflow::FilterRecord &filter, int direction)
{
	// is filter for this object?
	Debug ("Check filter: filter: '%s', object: %s, parent: %s", filter.Object.c_str(),
		   record.Name.c_str(),
		   record.Parent.c_str());
	Debug ("%d", filter.Object != record.Name && filter.Object != record.Parent);
	if (filter.Object != record.Name && filter.Object != record.Parent)
		return false;
	bool goodName = record.Name == filter.Object
			||  record.Parent == filter.Object
			|| filter.Object == "*";
	Debug ("good name: %d", goodName);
	Debug ("result %d", (direction & filter.Direction) && goodName);
	return (direction & filter.Direction) && goodName;
}

std::vector<netflow::ObjRecord> FoldObjects (const std::vector<netflow::ObjRecord> & objects)
{
	std::map<string, netflow::ObjRecord> resultMap;
	ForEachI (objects, obj) {
		resultMap[obj->Name].Add(*obj);
	}
	std::vector<netflow::ObjRecord> result;
	ForEachI (resultMap, elem) {
		result.push_back(elem->second);
	}
	return result;
}

void netflow::Collector::ProcessFlowData()
{
	//		string FlowDir;
	mgr_file::Dir dir(FLOWDIR_PATH);
	// fill object records...
	m_Objects.clear();
	std::vector<ObjRecord> tmpObjects;
	FillObjects();
	FillFilters();
	while (dir.Read()) {
		tmpObjects = m_Objects;
		try {
			if (dir.Info(mgr_file::Info::ifLink).IsFile() && !dir.Info(mgr_file::Info::ifLink).IsLink()) {
				LogNote ("Processing %s file (%s)", dir.name().c_str(), dir.FullName().c_str());
				time_t date = mgr_date::DateTime(dir.name());
				CSVFile data(dir.FullName());
				while (!data.Eof()) {
					DataRecord record;
					record.SrcIp = data["SRC_IP"];
					record.DstIp = data["DST_IP"];
					record.Packets = str::Int64(data["PACKETS"]);
					record.Bytes = str::Int64(data["BYTES"]);

					record.DumpDbg();
					ForEachI (tmpObjects, obj) {
						uint64_t rx = 0;
						uint64_t tx = 0;
						uint64_t rp = 0;
						uint64_t tp = 0;

						int direction = DIRECTION_UNKNOWN;
						if (obj->Ip == record.SrcIp) {				//Outgowing
							direction = DIRECTION_OUTGOWING;
							tx = record.Bytes;
							tp = record.Packets;
						} else if (obj->Ip == record.DstIp) {		// incoming...
							direction = DIRECTION_INCOMING;
							rx = record.Bytes;
							rp = record.Packets;
						}
						obj->DumpDbg();
						if (direction != DIRECTION_UNKNOWN) {
							// check filter...
							bool dropData = false;
							ForEachI(m_Filters, filter) {
								filter->DumpDbg();
								if (NeedDrop(*obj, *filter, direction)) {
									dropData = true;
									break;
								}
							}
							if (!dropData) {
								obj->Add(rx,tx,rp,tp);
							}
						}
					}
					data.Next();
				}
				OnProcessFile (date, FoldObjects(tmpObjects));
				mgr_file::RemoveFile(dir.FullName());
			}
		} catch(...) {}
	}
}


MODULE_INIT(netflow, "")
{
	if (!mgr_cf::FindOption(ConfUseNetflow))
		return;
	Debug("Init NetFlow module");
	mgr_cf::AddParam(ConfNetFlowIp, DefaultNetFlowIp);
	mgr_cf::AddParam(ConfNetFlowPort, DefaultNetFlowPort);
	new ActionCheckFlow();
}


void netflow::DataRecord::DumpDbg()
{
	LogNote ("SRC IP: '%s', DST IP: '%s', BYTES: '%llu', PACKETS: '%llu'",
			 SrcIp.Str().c_str(),
			 DstIp.Str().c_str(),
			 Bytes,
			 Packets);
}
