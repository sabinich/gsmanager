#include "gsutils.h"
#include "dbobject.h"
#include <mgr/mgrproc.h>
#include <mgr/mgrtest.h>

namespace  gsutils {

void DbGetValues(std::shared_ptr<mgr_db::CustomTable> table, StringMap& values) {
	int i = 0;
	auto desc = table->desc();
	ForEachI(desc.field, field) {
			values[(*field)->name] = table->AsString(i);
		++i;
	}
}

string ResolveHostname(const string& name)
{
	//mgr_proc::Execute host("host "+name+" | awk '/^[[:alnum:].-]+ has address/ { print $4 }'");
	mgr_proc::Execute host("host "+name);
	string res = str::Trim(host.Str());
	string ip = str::RGetWord(res);
	if (!test::Ip(ip)) return "";
	return ip;
}

void MakeQuery(string& query, StringMap& values) {
	ForEachI(values, e) {
		string val;
		val = e->first;
		val += "=" + e->second;
		str::inpl::Append(query, val, "&");
	}
}

}


