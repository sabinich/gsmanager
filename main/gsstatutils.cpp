#include <mgr/mgrstr.h>
#include <mgr/mgrdate.h>
#include <mgr/mgrdb.h>
#include <mgr/mgrlog.h>
#include <api/xmlcache.h>
#include "gsstatutils.h"

MODULE("gsstatutils");

namespace stat_utils {

string GetTitleMessage(Session &ses, const string& func, const string& msg, const string& name) {
	auto msgs = GetMessages(ses.conn.lang(), func);
	string title_message = msgs.GetNode("//msg[@name='" + msg + "']");
	StringMap params;
	params["__name__"] = name;
	return str::Replace(title_message, params);
}

string BuildDatePeriodCondition(const string& date_begin, const string& date_end, const string& table_pseudoname) {
	time_t begin = 0;
	time_t end = 0;
	if (date_begin.empty() && date_end.empty()) {
		string dt = mgr_date::DateTime().AddDays(-1).AsDate();
		begin = mgr_date::DateTime(dt + " 00:00:00");
		end = mgr_date::DateTime(dt + " 23:59:59");
	}
	if (!date_begin.empty())
		begin = mgr_date::DateTime(date_begin + " 00:00:00");
	if (!date_end.empty())
		end = mgr_date::DateTime(date_end + " 23:59:59");
	if (begin && end)
		return str::Format("%s.begin BETWEEN %ld AND %ld", table_pseudoname.c_str(), begin, end);
	if (begin)
		return str::Format("%s.begin > %ld", table_pseudoname.c_str(), begin);
	if (end)
		return str::Format("%s.begin < %ld", table_pseudoname.c_str(), end);
	return "";
}

string BuildTopQuery(const string& report_type, const string& db_table, const string& report_subject,
                     Session& ses) {
	string where = str::Format("WHERE s.node IN (SELECT name from %s", report_type.c_str());
	where += ") AND " + BuildDatePeriodCondition(ses.IParam("periodstart"), ses.IParam("periodend"), "s");
	string field = report_subject;
	if (report_subject == "cpu")
		field = "cpuuser+cpusystem";
	else if (report_subject == "iops")
		field = "hddioreq";
	else if (report_subject == "mem")
		field = "memused";
	else if (report_subject == "loadavg")
		field = "loadavg_5";

	string fields = str::Format("SUM(%s) AS %s", field.c_str(), report_subject.c_str());
	if (report_subject == "iops") {
		fields += ", SUM(hdd_rd) as hdd_rd, SUM(hdd_wr) as hdd_wr";
	}

	string sql = str::Format(
	            "SELECT %s, s.node, MIN(s.begin) start, MAX(s.begin) end "
	            "FROM %s s %s "
	            "GROUP BY node "
	            "ORDER BY %s DESC LIMIT 10",
	            fields.c_str(), db_table.c_str(), where.c_str(), field.c_str());

	return sql;
}

string BuildHostnodeQuery(Session& ses, const string& name, const string db_table) {
	string period_condition = BuildDatePeriodCondition(ses.IParam("periodstart"), ses.IParam("periodend"), "s");
	return "SELECT s.begin, SUM(rx) AS rx, SUM(tx) AS tx, SUM(cpuuser) AS cpuuser, "
	        "SUM(cpusystem) AS cpusystem, SUM(cpuidle) AS cpuidle, SUM(cpuiowait) AS cpuiowait, "
	        "SUM(memused) AS memused, SUM(memmax) AS memmax, SUM(hddioreq) AS hddioreq, "
	        "SUM(hdd_rd) as hdd_rd, SUM(hdd_wr) as hdd_wr, "
	        "AVG(loadavg_5) AS loadavg, SUM(memrej) AS memrej, SUM(numprocrej) AS numprocrej, "
	        "SUM(numfilerej) AS numfilerej, SUM(numproc) AS numproc, SUM(numfile) AS numfile "
	        "FROM " + db_table + " s WHERE s.node='" + name + "' AND " + period_condition + " GROUP BY s.begin ORDER BY s.begin";
}

static string GetDataValueTime(Session &ses, time_t date) {
	if (ses.Param("type") == "day") {
		return mgr_date::DateTime(date).AsDate();
	} else {
		return mgr_date::DateTime(date).AsDateTime();
	}
}

time_t getInterval(Session &ses) {
	string interval = ses.Param("interval");
	if (interval == "min") {
		return GSSTAT_MIN_INTERVAL;
	} else if (interval == "day") {
		return GSSTAT_DAY_INTERVAL;
	}
	return GSSTAT_HOUR_INTERVAL;
}

static string GetDataValueTime(Session &ses, mgr_db::QueryPtr dataQuery) {
	return GetDataValueTime(ses, dataQuery->AsInt("begin"));
}

static string trafficToGb(const string & bytes) {
	long long llBytes = str::Int64(bytes);
	long long gb = llBytes >> 30;
	long long rest = llBytes - (gb << 30);
	double result = gb + (double)rest / (double)(1024*1024*1024);
	return str::Format("%0.4f GiB", result);
}

string calcCpuStat(long long user, long long system, long long iowait, long long idle) {
	long long sum = user + system + iowait + idle;
	if (sum == 0)
		return "";
	return str::Format("%0.4f%%", (double)(user+system+iowait)/(double)sum*100.0);
}

string calcCpuStatQ(mgr_db::Query& nav) {
	return calcCpuStat(
			str::Int64(nav["cpuuser"]),
			str::Int64(nav["cpusystem"]),
			str::Int64(nav["cpuiowait"]),
			str::Int64(nav["cpuidle"]));
}

string calcMemStat(long long used, long long max) {
	if (max == 0)
		return "";
	return str::Format("%0.2f%%", (double)(used) / (double)max * 100.0);
}

string calcMemStatQ(mgr_db::Query& nav) {
	return calcMemStat(
			str::Int64(nav["memused"]),
			str::Int64(nav["memmax"]));
}

string BytesToHuman(const string &src) {
	uint64_t isrc = strtoull(src.c_str(), NULL, 10);
	uint64_t max_size = 1024;
	const char *sfx[] = {"B", "KB", "MB", "GB", "TB", "PB"};
	size_t cnt = 5;
	size_t current = 0;
	uint64_t rest = 0;
	while (isrc > max_size && current < cnt) {
		rest = isrc & 1023;
		isrc >>= 10;
		current ++;
	}
	double result = isrc + ((double) rest) /1024;
	return str::Format("%0.2f %s", result, sfx[current]);
}

string calcDiskStat(int64_t rd, int64_t wr, int64_t io, time_t period) {
	double iops = (double)io / (double)period;
	double speed = (double)(rd + wr) / (double)period;
	string speedStr = ((speed > 1024) ? BytesToHuman(str::Str(speed, 2)) : (str::Str(speed, 2) + " B"));
	if (iops > 0)
		return str::Format("%0.4f iops / %s/s", iops, speedStr.c_str());
	else
		return str::Format("%s/s", speedStr.c_str());
}

string calcDiskStatQ(mgr_db::Query& nav, time_t period) {
	return calcDiskStat(
	        str::Int64(nav["hdd_rd"]),
	        str::Int64(nav["hdd_wr"]),
	        str::Int64(nav["hddioreq"]),
	        period);
}

static string GetDataValue(Session &ses, const string & report_subject, mgr_db::QueryPtr data_query) {
	if (report_subject == "rx") {
		return trafficToGb(data_query->AsString("rx"));
	} else if (report_subject == "tx") {
		return trafficToGb(data_query->AsString("tx"));
	} else if (report_subject == "cpu") {
		return calcCpuStatQ((*data_query));
	} else if (report_subject == "mem") {
		return calcMemStatQ((*data_query));
	} else if (report_subject == "iops") {
		return calcDiskStatQ((*data_query), getInterval(ses));
	}

	else if (report_subject == "loadavg") {
		return str::Format("%0.4f", str::Double(data_query->AsString("loadavg")));
	} else if (report_subject == "memrej") {
		return data_query->AsString("memrej");
	} else if (report_subject == "numprocrej") {
		return data_query->AsString("numprocrej");
	} else if (report_subject == "numfilerej") {
		return data_query->AsString("numfilerej");
	} else if (report_subject == "numproc") {
		return data_query->AsString("numproc");
	} else if (report_subject == "numfile") {
		return data_query->AsString("numfile");
	}
	return "";
}

struct TopReportLine {
	time_t Time;
	string Data[10];
};

void FillTopReportData(Session& ses, const string& report_type, const string& report_subject, const string& db_table,
                       const string& func, mgr_xml::XmlNode& report_data_node) {
	string report_band_name = "report_top_" + report_subject;
	auto band = report_data_node.AppendChild(report_band_name.c_str());
	auto data_elem = band.AppendChild("elem");
	string title = GetTitleMessage(ses, func, report_subject, "");
	data_elem.AppendChild(report_subject.c_str(), title);

	string sql_top = BuildTopQuery(report_type, db_table, report_subject, ses);
	mgr_xml::XmlNode data_band = data_elem.AppendChild("data");

	std::map<time_t, TopReportLine> report_data;
	int vm_count = -1;

	string sql;
	ForEachQuery (db, sql_top, query_item) {
		++vm_count;
		string name = query_item->AsString("node");
		string col_name = str::Format("col_%s_%i", report_subject.c_str(), vm_count);
		string tmp =str::Format("//msg[@name='%s']", col_name.c_str());
		auto msg_col_node = ses.xml.GetNode(tmp);
		if (msg_col_node) {
			msg_col_node.SetContent(name);
		} else {
			Warning ("Node %s not found", col_name.c_str());
		}
		sql = BuildHostnodeQuery(ses, name, db_table);
		ForEachQuery(db, sql, item) {
			time_t row_time = item->AsInt("begin");
			if (report_data.find(row_time) == report_data.end()) {
				report_data[row_time].Time = row_time;
			}
			report_data[row_time].Data[vm_count] = GetDataValue(ses, report_subject, item);
		}
	}

	for (int i = vm_count + 1; i < 10; ++i) {
		ses.xml.RemoveNodes(
		            str::Format("//band[@name='%s']//line[@data='col_%s_%i']", report_band_name.c_str(),
		                        report_subject.c_str(), i));
		ses.xml.RemoveNodes(
		            str::Format("//band[@name='%s']//col[@name='col_%s_%i']", report_band_name.c_str(),
		                        report_subject.c_str(), i));
	}

	ForEachI(report_data, row) {
		mgr_xml::XmlNode elem = data_band.AppendChild("elem");
		elem.AppendChild("time", GetDataValueTime(ses, row->first));
		for(int i = 0; i <= vm_count; ++i)
			elem.AppendChild(("col_" + report_subject + "_" + str::Str(i)).c_str(), row->second.Data[i]);
	}
}

void FillReportData(Session &ses, const string& sql, const string& title) {
	auto reportdata = ses.NewNode("reportdata");
	auto band = reportdata.AppendChild("single_report");
	auto data_elem = band.AppendChild("elem");
	data_elem.AppendChild("report_name", title);
	mgr_xml::XmlNode data_band = data_elem.AppendChild("data");
	ForEachQuery(db, sql, dataelem) {
		mgr_xml::XmlNode elem = data_band.AppendChild("elem");
		elem.AppendChild("time", GetDataValueTime(ses, dataelem));
		elem.AppendChild("rx", GetDataValue(ses, "rx", dataelem));
		elem.AppendChild("tx", GetDataValue(ses, "tx", dataelem));
		elem.AppendChild("cpu", GetDataValue(ses, "cpu", dataelem));
		elem.AppendChild("mem", GetDataValue(ses, "mem", dataelem));
		elem.AppendChild("iops", GetDataValue(ses, "iops", dataelem));

		elem.AppendChild("loadavg", GetDataValue(ses, "loadavg", dataelem));
		elem.AppendChild("memrej", GetDataValue(ses, "memrej", dataelem));
		elem.AppendChild("numprocrej", GetDataValue(ses, "numprocrej", dataelem));
		elem.AppendChild("numfilerej", GetDataValue(ses, "numfilerej", dataelem));
		elem.AppendChild("numproc", GetDataValue(ses, "numproc", dataelem));
		elem.AppendChild("numfile", GetDataValue(ses, "numfile", dataelem));
	}
}

} //end of namespace stat_utils
