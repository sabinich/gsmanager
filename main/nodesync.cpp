#include <mgr/mgrlog.h>
#include <mgr/mgrjob.h>
#include <mgr/mgrthread.h>
#include <mgr/mgrxml.h>
#include <api/module.h>
#include <api/action.h>
#include <api/problems.h>
#include <api/autotask.h>
#include "dbobject.h"
#include "gsutils.h"
#include "nodes.h"

MODULE("nodeinfo");

namespace {

using namespace isp_api;
using namespace gsutils;

class NodeSyncThread : gsutils::NodeThread {
public:
	NodeSyncThread(int node_id) : gsutils::NodeThread(node_id) {}
	void operator()() {
		try {
			Node node(NodeId());
			mgr_xml::Xml xml;
			try {
				xml = node.MiniQuery("func=serversstate&out=xml",40).xml;
			} catch (...) {
				Warning("MiniQuery to node %s failed, trying to kill core....", node.Name().c_str());
				try {
					auto ssh = node.ssh();
					ssh.Connect();
					string res = ssh.Execute("killall -9 core").Str();
					Warning("Core on node %s killed with res '%s'", node.Name().c_str(), res.c_str());
				} catch (...) {
					LogError("Failed to kill core on node %s", node.Name().c_str());
				}
				throw ;
			}
			auto elems = xml.GetNodes("//elem");
			ForEachI(elems, elem) {
				auto server_table = db->Get<ServerTable>();
				if (!server_table->DbFind("id=" + elem->FindNode("id").Str())) {
					if (!HasProblem("unknown_game_instance", elem->FindNode("id").Str()))
						RegisterProblem("unknown_game_instance", elem->FindNode("id").Str(),isp_api::plWarning, 60*24*3)
					        .add_param("node_id", str::Str(node.ID()))
					        .add_param("node_name", node.Name())
					        .add_param("node_ip", node.Ip())
					        .add_param("type", elem->FindNode("type").Str())
					        .add_param("ip", elem->FindNode("ip").Str())
					        .add_param("port", elem->FindNode("port").Str())
					        .add_param("slots", elem->FindNode("slots").Str())
					        .add_param("playersonline", elem->FindNode("playersonline").Str())
					        .add_param("status", elem->FindNode("status").Str());
					continue;
				}
				DeleteProblem("unknown_game_instance", server_table->Id.AsString());
				server_table->Active = elem->FindNode("status").Str() == "on";
				server_table->Broken = elem->FindNode("status").Str() == "broken";
				server_table->OnlinePlayers = str::Int(elem->FindNode("playersonline").Str());
				server_table->RamUsed = str::Int(elem->FindNode("ramused").Str())/1024;
				server_table->CpuUsed = str::Int(elem->FindNode("cpuused").Str());
				server_table->DiskUsed = ((float)str::Int(elem->FindNode("diskused").Str()))/1024.0/1024.0;
				if (server_table->Node != node.ID()) {
					try {
						if (server_table->Moving && server_table->Disabled) {
							string query = "func=" + server_table->Type+".setparams";
							query += "&disabled=off";
							query += "&id="+server_table->Id;
							node.MiniQuery(query);
							server_table->Disabled = false;
						}
					} catch(...) {
					}
					server_table->Moving = false;
				}
				server_table->Node = node.ID();
				server_table->Ip = elem->FindNode("ip").Str();
				server_table->Port = str::Int(elem->FindNode("port").Str());
				server_table->Post();
			}
			mgr_job::Commit();
			DeleteProblem("lost_connection_to_node", str::Str(NodeId()));
		} catch (...) {
			if (!HasProblem("lost_connection_to_node", str::Str(NodeId()))) {
				Node node(NodeId());
				RegisterProblem("lost_connection_to_node", str::Str(NodeId()), isp_api::plError).add_param("node_name", node.Name()).add_param("node_ip", node.Ip());
			}
			mgr_job::Rollback();
		}
	}
};

class NodeSyncAction : public Action {
public:
	NodeSyncAction() : Action("nodesync", MinLevel(lvAdmin)) {}

	void Execute(Session &) const {
		mgr_thread::TryLockWrite lock(m_mutex);
		if (!lock)
			return;
		gsutils::NodeThreadPool<NodeSyncThread> pool;
		ForEachQuery(db, "SELECT * FROM node", node) {
			pool.StartThread(node->AsInt("id"));
		}
	}
private:
	mutable mgr_thread::RwLock m_mutex;
};

void InitScheduler() {
	string cmd = mgr_file::ConcatPath(mgr_file::GetCurrentDir(), "sbin/mgrctl -m gsmgr nodesync");
	try {
		isp_api::task::Schedule(cmd, "*/1 * * * *", "ISPsystem GSmanager node synchronization");
	} catch (const mgr_err::Error &e) {
		Warning("Failed to create node synchronization task schedule, reason: %s", e.value().c_str());
	}
}

class UnknownInstancwProblem : public isp_api::Problem {
public:
	UnknownInstancwProblem () : isp_api::Problem("unknown_game_instance", MinLevel(lvAdmin)) {}

	bool Solve(Session &ses, const string &id, ProblemParams &params) const 
	{
		try {
			Node node(str::Int(params.GetParam("node_id")));
			StringMap table_values;
			string query = "func="+params.GetParam("type")+".delete&id="+id;
			node.MiniQuery(query);
			return true;
		} catch(...) {
		}
		return false;
	}
};

MODULE_INIT(nodeinfo, "gsmgr") {
	STrace();
	new NodeSyncAction;
	new UnknownInstancwProblem;
	InitScheduler();
}
} //end of private namespace
