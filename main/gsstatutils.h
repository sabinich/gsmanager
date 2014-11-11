#ifndef STATUTILS_H
#define STATUTILS_H

#include <mgr/mgrxml.h>
#include <api/action.h>
#include "dbobject.h"

namespace stat_utils {

using namespace isp_api;

#define GSSTAT_MIN_INTERVAL		300
#define GSSTAT_HOUR_INTERVAL	3600
#define GSSTAT_DAY_INTERVAL		3600*24

string GetTitleMessage(Session &ses, const string& func, const string& msg, const string& name);
string BuildHostnodeQuery(Session& ses, const string& name, const string db_table);
void FillTopReportData(Session& ses, const string& report_type, const string& report_subject, const string& db_table,
                       const string& func, mgr_xml::XmlNode& report_data_node);
void FillReportData(Session &ses, const string& sql, const string& title);

string calcCpuStat(long long user, long long system, long long iowait, long long idle);
string calcDiskStat(int64_t rd, int64_t wr, int64_t io, time_t period);

} //end of namespace stat_utils

#endif // STATUTILS_H
