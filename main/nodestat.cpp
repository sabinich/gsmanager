#include <mgr/mgrlog.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrjob.h>
#include <api/module.h>
#include <api/action.h>
#include <api/autotask.h>
#include <api/stdconfig.h>
#include "gsutils.h"
#include "dbobject.h"
#include "nodes.h"
#include "gsstatutils.h"
#include "../ostemplate-lib/stat_raw.h"
#include "teamspeak.h"

MODULE("stat");

namespace {

using namespace isp_api;

class StatRecordset : public Stat::Recordset {
public:
	Stat::KeyField Object;
	Stat::DataField LoadAvg_1;
	Stat::DataField LoadAvg_5;
	Stat::DataField LoadAvg_15;
	Stat::DataField MemUsed;
	Stat::DataField MemMax;
	Stat::DataField MemRejected;
	Stat::DataField NumProc;
	Stat::DataField NumProcMax;
	Stat::DataField NumProcRejected;
	Stat::DataField NumFile;
	Stat::DataField NumFileMax;
	Stat::DataField NumFileRejected;
	Stat::DataField CpuUser;
	Stat::DataField CpuNiceUser;
	Stat::DataField CpuSystem;
	Stat::DataField CpuIdle;
	Stat::DataField CpuIowait;
	Stat::DataField Rx;
	Stat::DataField Tx;
	Stat::DataField HddIORequest;
	Stat::DataField RDBytes;
	Stat::DataField WRBytes;
	Stat::DataField RDReq;
	Stat::DataField WRReq;

	StatRecordset() :
	    Object(this, "node"),
	    LoadAvg_1(this, "loadavg_1", false, Stat::dftNop),
	    LoadAvg_5(this, "loadavg_5", false, Stat::dftNop),
	    LoadAvg_15(this, "loadavg_15", false, Stat::dftNop),
	    MemUsed(this, "memused", false, Stat::dftNop),
	    MemMax(this, "memmax", false, Stat::dftNop),
	    MemRejected(this, "memrej", true),
	    NumProc(this, "numproc", false, Stat::dftNop),
	    NumProcMax(this, "numprocmax", false, Stat::dftNop),
	    NumProcRejected(this, "numprocrej"),
	    NumFile(this, "numfile", false, Stat::dftNop),
	    NumFileMax(this, "numfilemax", false, Stat::dftNop),
	    NumFileRejected(this, "numfilerej", true),
	    CpuUser(this, "cpuuser", true),
	    CpuNiceUser(this, "cpuniceuser", true),
	    CpuSystem(this, "cpusystem", true),
	    CpuIdle(this, "cpuidle", true),
	    CpuIowait(this, "cpuiowait", true),
	    Rx(this, "rx", true),
	    Tx(this, "tx", true),
	    HddIORequest(this, "hddioreq", true),
	    RDBytes(this, "hdd_rd", true),
	    WRBytes(this, "hdd_wr", true),
	    RDReq(this, "hdd_rd_req", true),
	    WRReq(this, "hdd_wr_req", true)
	{}


	string Name () const{
		return "mainstat";
	}
};

class DayStat : public Stat::DataTable<StatRecordset> {
public:
	DayStat() : Stat::DataTable<StatRecordset>("statday", 730) {}
};

class HourStat : public Stat::DataTable<StatRecordset> {
public:
	HourStat() : Stat::DataTable<StatRecordset>("stathour", 720) {}
};

class NodeStatThread : public gsutils::NodeThread {
public:
	NodeStatThread(int node_id) : gsutils::NodeThread(node_id) {}

	void operator ()() {
		try {
			mgr_date::AccurateDateTime now;
			Node node(NodeId());
			auto result = node.MiniQuery("func=nodestat&out=xml", MINI_QUERY_TIMEOUT);
			StatRecordset stats;
			stats.Object = node.Name();
			if (result.is_ok()) {
				stats.CpuUser = str::Int64(result.xml.GetNode("//user").Str());
				stats.CpuNiceUser = str::Int64(result.xml.GetNode("//nice").Str());
				stats.CpuSystem = str::Int64(result.xml.GetNode("//system").Str());
				stats.CpuIdle = str::Int64(result.xml.GetNode("//idle").Str());
				stats.CpuIowait = str::Int64(result.xml.GetNode("//iowait").Str());
				stats.MemRejected = str::Int64(result.xml.GetNode("//rejmem").Str());
				stats.NumProcRejected = str::Int64(result.xml.GetNode("//rejproc").Str());
				stats.NumFileRejected = str::Int64(result.xml.GetNode("//rejfile").Str());
				stats.Rx = str::Int64(result.xml.GetNode("//rx").Str());
				stats.Tx = str::Int64(result.xml.GetNode("//tx").Str());
				stats.HddIORequest = str::Int64(result.xml.GetNode("//iops").Str());
				stats.RDBytes = str::Int64(result.xml.GetNode("//rd_bytes").Str());
				stats.WRBytes = str::Int64(result.xml.GetNode("//wr_bytes").Str());
				stats.RDReq = str::Int64(result.xml.GetNode("//rd_req").Str());
				stats.WRReq = str::Int64(result.xml.GetNode("//wr_req").Str());

				stats.LoadAvg_1 = str::Double(result.xml.GetNode("//loadavg1").Str()) * 100;
				stats.LoadAvg_5 = str::Double(result.xml.GetNode("//loadavg5").Str()) * 100;
				stats.LoadAvg_15 = str::Double(result.xml.GetNode("//loadavg15").Str()) * 100;
				stats.MemUsed = str::Int64(result.xml.GetNode("//mem").Str());
				stats.MemMax = str::Int64(result.xml.GetNode("//maxmem").Str());
				stats.NumProc = str::Int64(result.xml.GetNode("//proc").Str());
				stats.NumProcMax = str::Int64(result.xml.GetNode("//maxproc").Str());
				stats.NumFile = str::Int64(result.xml.GetNode("//file").Str());
				stats.NumFileMax = str::Int64(result.xml.GetNode("//maxfile").Str());

				auto node_table = db->Get<NodeTable>();
				node_table->AssertByName(stats.Object.Value());
				node_table->MemSize = stats.MemMax.Int() / 1024;
				node_table->MemUsed = stats.MemUsed.Int() / 1024;
				node_table->CpuUsage = stat_utils::calcCpuStat(stats.CpuUser.Int(),
				                                               stats.CpuSystem.Int(),
				                                               stats.CpuIowait.Int(),
				                                               stats.CpuIdle.Int());

				if(node_table->MeasureEndTime.IsNull())
					node_table->MeasureStartTime.SetNull();
				else
					node_table->MeasureStartTime = node_table->MeasureEndTime;
				node_table->MeasureEndTime = mgr_date::DateTime();
				time_t period = 0;
				if (!node_table->MeasureStartTime.IsNull()) {
					mgr_date::DateTime enddate = node_table->MeasureEndTime;
					mgr_date::DateTime startdate = node_table->MeasureStartTime;
					period = enddate - startdate;
				}
				node_table->HddSpeed =
				        period == 0 ?
				            "0 Mb/s" :
				            stat_utils::calcDiskStat(stats.RDBytes.Int() - node_table->HddRd,
				                                     stats.WRBytes.Int() - node_table->HddWr,
				                                     0,
				                                     period);
				node_table->HddRd = stats.RDBytes.Int();
				node_table->HddWr = stats.WRBytes.Int();

				node_table->Post();
				stats.Normalize();
				Stat::MinutelyProcess(now, stats);
			}
			mgr_job::Commit();
		} catch (...) {
			mgr_job::Rollback();
		}
	}
};

class NodeStatistics : public Action {
public:
	NodeStatistics() : Action("collectstat", MinLevel(lvAdmin)) {}

	void Execute(Session &ses) const {
		mgr_thread::TryLockWrite lock(m_mutex);
		if (!lock)
			return;
		mgr_date::AccurateDateTime now;
		Stat::HourlyProcess<HourStat>(now);
		Stat::DailyProcess<DayStat, HourStat>(now);
		gsutils::NodeThreadPool<NodeStatThread> pool;
		ForEachQuery(db, "SELECT * FROM node", node) {
			pool.StartThread(node->AsInt("id"));
		}
		//TeamSpeak Node Request;
		string start_scritp_path = mgr_cf::GetPath("teamspeak_start_script");
		try {
			mgr_proc::Execute status(start_scritp_path+" status");
			if (status.Str().find("Server is running") == string::npos) {
				mgr_proc::Execute start(start_scritp_path+" start");
				start.Result();
			}
			TeamSpeakNode node;
			node.GetServerStat();
		} catch(...) {}

		Stat::DailyProcess<DayStat, HourStat>(now);
	}
private:
	mutable mgr_thread::RwLock m_mutex;
};

class ReportHostnode : public ReportAction {
public:
	ReportHostnode() : ReportAction("reporthostnode", MinLevel(lvAdmin)) {}

	void ReportFormTune(Session &ses) const {
		ses.NewSelect("interval");
		ses.AddChildNode("msg", "hour");
		ses.AddChildNode("msg", "day");

		string sql = "SELECT id, name FROM node ORDER BY name";
		ses.NewSelect("hostnode");
		ses.AddChildNode("msg", "top10").SetProp("key", "*top10");
		int hostnode_count = 0;
		ForEachQuery(db, sql, db_item) {
			ses.AddChildNode("val", db_item->AsString("name")).SetProp("key", db_item->AsString("id"));
			++hostnode_count;
		}
		if (!hostnode_count)
			ses.xml.RemoveNodes("//field[@name='hostnode']");

		if (!ses.Param("interval").empty())
			ses.NewNode("interval", ses.Param("interval"));
		if (!ses.Param("hostnode").empty())
			ses.NewNode("hostnode", ses.Param("hostnode"));
	}

	void ReportExecute(Session &ses) const {
		string elid = ses.Param("elid");
		if (!elid.empty()) {
			ses.SetParam("hostnode", elid);
			ses.SetParam("interval", "hour");
		}
		string hostnode = ses.Param("hostnode");
		string interval = ses.Param("interval");
		if (!hostnode.empty() && !interval.empty()) {
			ses.NewNode("hostnode", hostnode);
			ses.NewNode("interval", interval);

			//удаляем ненужные diagram'ы и колонки
			ses.xml.RemoveNodes("//diagram[@name='diagram_rejects' or @name='diagram_proc' or @name='diagram_file']");
			ses.xml.RemoveNodes("//col[contains(@name, 'rej') or @name='numproc' or @name='numfile']");

			string stat_table_name = "stat" + ses.Param ("interval");
			if (!hostnode.find("*top10")) {
				ses.xml.RemoveNodes("//band[@name='single_report']");

				auto reportdata = ses.NewNode("reportdata");
				stat_utils::FillTopReportData(ses, "node", "rx", stat_table_name, "reporthostnode", reportdata);
				stat_utils::FillTopReportData(ses, "node", "tx", stat_table_name, "reporthostnode", reportdata);
				stat_utils::FillTopReportData(ses, "node", "cpu", stat_table_name, "reporthostnode", reportdata);
				stat_utils::FillTopReportData(ses, "node", "iops", stat_table_name, "reporthostnode", reportdata);
				stat_utils::FillTopReportData(ses, "node", "mem", stat_table_name, "reporthostnode", reportdata);
				stat_utils::FillTopReportData(ses, "node", "loadavg", stat_table_name, "reporthostnode", reportdata);
			} else {
				ses.xml.RemoveNodes("//band[contains(@name, 'report_top')]");

				auto hostnode_table = db->Get<NodeTable>(hostnode);
				//report title:
				string title = stat_utils::GetTitleMessage(ses, "reporthostnode", "report_name", hostnode_table->Name);
				string sql = stat_utils::BuildHostnodeQuery(ses, hostnode_table->Name, stat_table_name);
				stat_utils::FillReportData(ses, sql, title);
			}
		} else {
			ses.xml.RemoveNodes("//band");
		}
	}
};

void InitScheduler() {
	string cmd = mgr_file::ConcatPath(mgr_file::GetCurrentDir(), "sbin/mgrctl -m gsmgr collectstat");
	try {
		isp_api::task::Schedule(cmd, "*/5 * * * *", "ISPsystem GSmanager node statistics");
	} catch (const mgr_err::Error &e) {
		Warning("Failed to create node statistics task schedule, reason: %s", e.value().c_str());
	}
}

MODULE_INIT(nodestat, "gsmgr") {
	db->Register<DayStat>();
	db->Register<HourStat>();
	//StatUtils::InitStatsDB(db);
	new NodeStatistics;
	new ReportHostnode;
	InitScheduler();
}

} //end of private namespace
