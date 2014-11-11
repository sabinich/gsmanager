#include <mgr/mgrlog.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrclient.h>
#include <ispbin.h>
#include "cmdarg.h"
#include "../main/defines.h"

#define VER							"0.0.1"

MODULE(CMD_MIGRATE_SERVER);

void MigrateServer(const string &server, const string &node) {
	
	auto info = mgr_client::Local("gsmgr", CMD_MIGRATE_SERVER).Query("func=server.migrateinfo&server="+server+"&node="+node);
	//if (!info.is_ok()) throw mgr_err::Error("local_query_fail");
	
	string game = info.xml.GetNode("//game").Str();
	string oldnode = info.xml.GetNode("//oldnode").Str();
	string oldnode_ip = info.xml.GetNode("//oldnode_ip").Str();
	string newnode_ip = info.xml.GetNode("//newnode_ip").Str();
	bool old_ndoe_is_local = info.xml.GetNode("//oldnode_is_local").Str() == "on";
	bool new_ndoe_is_local = info.xml.GetNode("//newnode_is_local").Str() == "on";
	string privkey = info.xml.GetNode("//privkey").Str();
	string pubkey = info.xml.GetNode("//pubkey").Str();
	string knownhosts = info.xml.GetNode("//knownhosts").Str();
	string maxplayers = info.xml.GetNode("//maxplayers").Str();
	
	
	LogInfo("server %s",server.c_str());
	LogInfo("node %s",node.c_str());
	LogInfo("game %s",game.c_str());
	LogInfo("oldnode %s",oldnode.c_str());
	LogInfo("oldnode_ip %s",oldnode_ip.c_str());
	LogInfo("newnode_ip %s",newnode_ip.c_str());
	LogInfo("privkey %s",privkey.c_str());
	LogInfo("pubkey %s",pubkey.c_str());
	
	string full_ssh_private_key = privkey;
	if (full_ssh_private_key.find(DIRSEP) != 0)
		full_ssh_private_key = mgr_file::ConcatPath(mgr_file::GetCurrentDir(), full_ssh_private_key);
	string full_ssh_public_key = pubkey;
	if (full_ssh_public_key.find(DIRSEP) != 0)
		full_ssh_public_key = mgr_file::ConcatPath(mgr_file::GetCurrentDir(), full_ssh_public_key);
	string full_ssh_knownhosts = knownhosts;
	if (full_ssh_knownhosts.find(DIRSEP) != 0)
		full_ssh_knownhosts = mgr_file::ConcatPath(mgr_file::GetCurrentDir(), full_ssh_knownhosts);
	
	
	//Disable server
	mgr_client::Local("gsmgr", CMD_MIGRATE_SERVER).Query("func=server.disable&elid="+server);
	
	//Create server instance on new node
	string node_query = "func="+game+".create&id="+server+"&ip="+newnode_ip+"&maxplayers="+maxplayers;
	node_query = str::base64::Encode(node_query);
	mgr_client::Local("gsmgr", CMD_MIGRATE_SERVER).Query("func=node.query&elid="+node+"&query="+node_query);	
	
	//Take Server Instance Backup
	string from;
	string to;
	if (old_ndoe_is_local)
		from = "/home/"+game+"_"+server;
	else 
		from = "root@"+oldnode_ip+":/home/"+game+"_"+server;
	
	to  = "/gsmgr/backups";
	
	if (!mgr_file::Exists(to))  {
		mgr_file::MkDir(to);
	} else {
		mgr_file::Remove("/gsmgr/backups/"+game+"_"+server);
	}
	
	string cmd = "scp -i "+full_ssh_private_key+" -o UserKnownHostsFile="+full_ssh_knownhosts+" -r "+from+" "+to;
	mgr_proc::Execute exec(cmd);
	if (exec.Result() != 0)
		throw mgr_err::Error("scp_backup_fail");
	
	from = "/gsmgr/backups/"+game+"_"+server+"/*";

	if (new_ndoe_is_local)
		to = "/home/"+game+"_"+server;
	else 
		to = "root@"+newnode_ip+":/home/"+game+"_"+server;
	
	if (!mgr_file::Exists(to)) mgr_file::MkDir(to);
	
	cmd = "scp -i "+full_ssh_private_key+" -o UserKnownHostsFile="+full_ssh_knownhosts+" -r "+from+" "+to;
	mgr_proc::Execute exec1(cmd);
	if (exec1.Result() != 0)
		throw mgr_err::Error("scp_tonode_fail");
	
	
	//Set new ip
	node_query = "func="+game+".setparams&id="+server+"&ip="+newnode_ip;
	node_query = str::base64::Encode(node_query);
	mgr_client::Local("gsmgr", CMD_MIGRATE_SERVER).Query("func=node.query&elid="+node+"&query="+node_query);	
	
	//Apply rigths to server
	node_query = "func="+game+".setparams&id="+server+"&reinit=on";
	node_query = str::base64::Encode(node_query);
	mgr_client::Local("gsmgr", CMD_MIGRATE_SERVER).Query("func=node.query&elid="+node+"&query="+node_query);	

	//Delete Old Server
	node_query = "func="+game+".delete&id="+server;
	node_query = str::base64::Encode(node_query);
	mgr_client::Local("gsmgr", CMD_MIGRATE_SERVER).Query("func=node.query&elid="+oldnode+"&query="+node_query);	
	
}

class Arguments : public opts::Args {
public:
	opts::Arg Server;
	opts::Arg Node;

	Arguments () :
	    Server("server", 's', this),
	    Node("node", 'n', this)
		{}

	void OnUsage(const string &argv0) {
		using namespace std;
		cout << endl << "Usage: " << mgr_file::Name (argv0) << " [OPTION]" << endl;
		cout << "GSmanager dist copy utility, version " << VER << endl;
		cout << endl << mgr_file::Name (argv0) << endl;
		cout << "\t-s | --server <server_id>\tserver to move" << endl;
		cout << "\t-n | --node <node_id>\t\tdestination node" << endl;
		cout << endl;
	}

	void OnVersion(const string &argv0) {
		std::cout << VER << std::endl;
	}

	void OnUnrecognize (const std::vector<string> &unrecognized) {
		ForEachI (unrecognized, opt) {
			Warning ("Unrecognized option: %s", opt->c_str());
			std::cerr << "unrecognized option: " << *opt << std::endl;
		}
	}
};

int ISP_MAIN(int argc, char *argv[]) {
	mgr_log::Init(CMD_MIGRATE_SERVER, mgr_log::L_DEBUG);
	Arguments args;
	args.Parse(argc, argv);
	try {
		MigrateServer(args.Server, args.Node);

	} catch (const std::exception&) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
