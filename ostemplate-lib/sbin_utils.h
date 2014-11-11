#ifndef SBIN_UTILS_H
#define SBIN_UTILS_H
#include <vector>
#include <string>
#include <memory>
#include <mgr/mgrdb.h>
#include <mgr/mgrdb_struct.h>
#include <mgr/mgrclient.h>
namespace sbin {

std::shared_ptr<mgr_db::Cache> DB();
mgr_client::Local & Client();
mgr_client::Result ClientQuery(const string &query);
mgr_client::Result ClientQuery(const string &func, const StringMap & params);
string GetMgrConfParam(const string& name, const string& def_val = "");
void Init(const string module_name);
void Init(const string module_name, void (*SignalWatcher)(int));
bool TermSignalRecieved();
void SetTermSignalReceived();


}

#endif // SBIN_UTILS_H
