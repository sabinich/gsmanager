#include <mgr/mgrlog.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrclient.h>
#include <ispbin.h>
#include "cmdarg.h"
#include "../main/defines.h"

#define VER							"0.0.1"

MODULE(CMD_COPY_DIST);

void CopyDist(const string &path, const string &ip, const string &ssh_private_key, const string &ssh_public_key,
              const string &ssh_knownhosts) {
	string full_ssh_private_key = ssh_private_key;
	if (full_ssh_private_key.find(DIRSEP) != 0)
		full_ssh_private_key = mgr_file::ConcatPath(mgr_file::GetCurrentDir(), full_ssh_private_key);
	string full_ssh_public_key = ssh_public_key;
	if (full_ssh_public_key.find(DIRSEP) != 0)
		full_ssh_public_key = mgr_file::ConcatPath(mgr_file::GetCurrentDir(), full_ssh_public_key);
	string full_ssh_knownhosts = ssh_knownhosts;
	if (full_ssh_knownhosts.find(DIRSEP) != 0)
		full_ssh_knownhosts = mgr_file::ConcatPath(mgr_file::GetCurrentDir(), full_ssh_knownhosts);

	string gs_path = mgr_file::GoodPath(path);
	string game_name = str::RGetWord(gs_path, DIRSEP);
	string target_path = mgr_file::ConcatPath(GS_DEFAULT_DISTR_PATH, game_name);
	if (mgr_proc::Execute("cd " + path + " && tar cpzf - * | "
			"ssh -i " + full_ssh_private_key + " -o UserKnownHostsFile=" + full_ssh_knownhosts + " " +
			"root@" + ip + " '(rm -rf " + target_path + "; mkdir -p " + target_path + ") && tar xzpf - -C " + target_path + "'")
			.Run().Result() != 0)
		throw mgr_err::Error("scp_failed");
}

class Arguments : public opts::Args {
public:
	opts::Arg Path;
	opts::Arg Ip;
	opts::Arg SshKey;
	opts::Arg SshPub;
	opts::Arg SshKnownHosts;
	opts::Arg DistUpdate;

	Arguments () :
	    Path("path", 't', this),
	    Ip("ip", 'i', this),
	    SshKey("sshkey", 'p', this),
	    SshPub("sshpub", 'o', this),
	    SshKnownHosts("hosts", 's', this),
		DistUpdate("distupdate", 'd', this)
		{}

	void OnUsage(const string &argv0) {
		using namespace std;
		cout << endl << "Usage: " << mgr_file::Name (argv0) << " [OPTION]" << endl;
		cout << "GSmanager dist copy utility, version " << VER << endl;
		cout << endl << mgr_file::Name (argv0) << endl;
		cout << "\t-t | --path <path>\tpath to copy" << endl;
		cout << "\t-i | --ip <ip>\t\tdestination node ip-address" << endl;
		cout << "\t-p | --sshkey <path>\tpath to ssh private key" << endl;
		cout << "\t-o | --sshpub <path>\tpath to ssh public (open) key" << endl;
		cout << "\t-s | --hosts <path>\tpath to knownhosts file" << endl;
		cout << "\t-s | --distupdate <on|off>\tonly dist update" << endl;
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
	mgr_log::Init(CMD_COPY_DIST);
	Arguments args;
	args.Parse(argc, argv);
	try {
		string path = args.Path;
		CopyDist(path, args.Ip, args.SshKey, args.SshPub, args.SshKnownHosts);
		bool distupdate = (string)args.DistUpdate == "on";
		mgr_client::Local("gsmgr", CMD_COPY_DIST).Query("func=node.gamelist.allow.commit&ip=" + str::url::Encode(args.Ip) + (distupdate?"&distupdate=on":"") +
		                                                "&elid=" + str::RGetWord(path, DIRSEP));
	} catch (const std::exception&) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
