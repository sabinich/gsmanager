#include <ispbin.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrfile.h>
#include <mgr/mgrssh.h>
#include "../ostemplate-lib/cmdarg.h"
#include "sbin_utils.h"
#include <iostream>

#define BINARY_NAME "ostsync"
#define BINARY_VER "1.0"

MODULE ("ostsync");


using namespace std;
class Arguments : public opts::Args
{
public:
	opts::Arg Ip;
	opts::Arg Key;
	opts::Arg Dir;

	Arguments()
		: Ip ("ip", this)
		, Key ("key", this)
		, Dir ("dir", this)
	{}

	// Args interface
protected:
	virtual void OnUsage(const string &argv0)
	{
		cout << endl << "Usage: " << mgr_file::Name(argv0) << " --ip <IP-address> --key <private-key> --dir <ostemplate-dir>" << endl;
		cout << "OS template cluster syncronization" << endl;
	}

	virtual void OnVersion(const string &argv0)
	{
		cout << BINARY_VER << endl;
	}
	void OnUnrecognize (const vector<string> &unrecognized) {
		ForEachI (unrecognized, opt) {
			Warning ("Unrecognized option: %s", opt->c_str());
			std::cerr << "unrecognized option: " << *opt << endl;
		}
	}
};

int ISP_MAIN(int argc, char ** argv)
{
	sbin::Init("ostsync");
	opts::Dump(argc, argv);
	Arguments args;
	args.Parse(argc, argv);
	string remoteShellCmd = "ssh -l root -i " + args.Key.Opt;
	// unmount nfs first
	mgr_rpc::SSHkey ssh(args.Ip, "root", args.Key);
	ssh.Execute("umount -fl " + mgr_proc::Escape(args.Dir)).Result();
	string rsyncCmd = str::Format("rsync -avc -e '%s' --delete %s/ %s:%s/",
								  remoteShellCmd.c_str(),
								  args.Dir.Opt.c_str(),
								  args.Ip.Opt.c_str(),
								  args.Dir.Opt.c_str()
								  );
	auto exec = mgr_proc::Execute(rsyncCmd);
	string execOutput = exec.Str();
	LogInfo ("SY: %s", execOutput.c_str());
	return exec.Result();
}


