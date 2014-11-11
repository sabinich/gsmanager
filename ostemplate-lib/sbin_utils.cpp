#include "sbin_utils.h"
#include <mgr/mgrlog.h>
#include <mgr/mgrproc.h>
#include "../main/defines.h"
#include <signal.h>
#include <strings.h>

MODULE ("sbin_utils");
namespace sbin {
static string ModuleName = "sbinutils";
static bool bTermSignalRecieved = false;

bool TermSignalRecieved() {
	return bTermSignalRecieved;
}

void SetTermSignalReceived()
{
	bTermSignalRecieved = true;
}

static void TermSignal(int sig) {
	LogInfo("Signal %d recieved. Preparing to stop", sig);
	bTermSignalRecieved = true;
}

void Init(const string module_name, void (*SignalWatcher)(int)) {
	ModuleName = module_name;
	mgr_log::Init(module_name);

	LogExtInfo("Set signal hanlers");
	struct sigaction sa;
	bzero(&sa, sizeof(sa));
	sigaddset(&sa.sa_mask, SIGTERM);
	sigaddset(&sa.sa_mask, SIGINT);
	sa.sa_handler = SignalWatcher;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGTERM, &sa, 0);
	sigaction(SIGINT, &sa, 0);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_DFL);
}

void Init(const string module_name) {
	Init (module_name, TermSignal);
}
std::shared_ptr<mgr_db::Cache> DB()
{
	static bool initialized = false;
	static std::shared_ptr<mgr_db::Cache> db;
	static mgr_db::ConnectionParams params;
	if (!initialized) {
		Debug ("Initialize db");
		initialized = true;

		params.type = "mysql";
		params.host = GetMgrConfParam(ConfDBHost, DefaultDBHost);
		params.user = GetMgrConfParam(ConfDBUser, DefaultDBUser);
		params.password = GetMgrConfParam(ConfDBPassword);
		params.db = GetMgrConfParam(ConfDBName, DefaultDBName);

		db.reset(new mgr_db::JobCache(params));
	}
	return db;
}

mgr_client::Local & Client()
{
	static mgr_client::Local client (MGR, ModuleName);
	return client;
}
string BuildMgrQuery (const string & func, const StringMap & params)
{
	string result="func=" + func;
	ForEachI(params, param) {
		result += "&" + param->first + "=" + param->second;
	}
	return result;
}

mgr_client::Result ClientQuery(const string &query)
{
	LogInfo ("QUERY: %s", query.c_str ());
	mgr_client::Result result = Client().Query (query);
	Debug ("QUERY_RESULT: \n%s", result.xml.Str (true).c_str ());
	return result;
}
mgr_client::Result ClientQuery(const string &func, const StringMap & params)
{
	return ClientQuery(BuildMgrQuery(func, params));
}

string GetMgrConfParam(const string& name, const string& def_val) {
	static bool initialized = false;
	static StringMap params;
	if (!initialized) {
		auto paramList = ClientQuery ("out=xml&func=otparamlist").xml;
		mgr_xml::XPath xpath = paramList.GetNodes ("/doc/elem");
		ForEachI(xpath, elem) {
			mgr_xml::XmlNode childNode = elem->FirstChild ();
			params[childNode.Name ()] = childNode.Str ();
		}
		initialized = true;
	}
	string res = params[name];
	if (res.empty())
		res = def_val;
	return res;
}

}
