#include <api/module.h>
#include <mgr/mgrlog.h>
#include <mgr/mgruser.h>
#include <mgr/mgrscheduler.h>
#include <api/action.h>
#include <api/stdconfig.h>
#include "../main/defines.h"
#include "gmmini.h"
#include <api/auth_method.h>
#include "gamemodule.h"

const string vsftpd_conf_path= "/etc/vsftpd.conf";
const string vsftpd_temp_conf_path= "var/vsftpd_temp.conf";
MODULE("gsmini_ftp");
using namespace isp_api;


class aConfigFtp : public FormAction {
public:
    aConfigFtp():FormAction("ftpconf", MinLevel(lvUser)){}
    virtual void Get(Session& ses, const string& elid) const
    {
		ServerInstanceInfo srv(ses);
		string passwd = srv.GetParam("ftppasswd");
		if (!passwd.empty()) {
			ses.NewNode("turn_on_ftp","on");
			ses.NewNode("agree","on");
		}
		ses.NewNode("user", srv.UserName);
		ses.NewNode("address","ftp://"+srv.GetIp()+"/");
		ses.NewNode("passwd",passwd);
	}
    virtual void Set(Session& ses, const string& elid) const
    {
		ServerInstanceInfo srv(ses);
		string command = "usermod -p $FTPPASS "+ses.auth.name();
		mgr_proc::Execute usermod(command);
		string passwd = ses.Param("passwd");
		if (ses.Checked("turn_on_ftp") && ses.Checked("agree")) {
			usermod.SetEnv("FTPPASS", str::Crypt(passwd));
			if (usermod.Result() != 0)
				throw mgr_err::Error("ftpconf");
			srv.SetParam("ftppasswd", passwd);
		} else {
			srv.SetParam("ftppasswd", "");
			usermod.SetEnv("FTPPASS", str::Crypt(str::hex::Encode(str::Random(10))));
			usermod.Result();
		}
	}
};

MODULE_INIT(gsmini_ftp,"gsmini")
{
	Debug("Loading GSmini FTP module...");
	if (!mgr_file::Exists(vsftpd_conf_path)) {
		Warning("Seems like VSFTPD not installed. Can`t init FTP module.");
		return;
	}
	string vsftpdconf =  mgr_file::Read(vsftpd_conf_path);
	if (vsftpdconf.find("#GSmanager vsftpd_config") != 0) {
		mgr_file::Move(vsftpd_conf_path,vsftpd_conf_path+".bak");
		mgr_file::Copy(vsftpd_temp_conf_path, vsftpd_conf_path);
		if (mgr_proc::Execute("service vsftpd restart").Result() != 0) {
			Warning("Failed to restart VSFTPD!Can`t init FTP module.");
			return;
		}
	} else {
		string res = mgr_proc::Execute("service vsftpd status").Str();
		if (res.find("start/running") == string::npos) {
			if (mgr_proc::Execute("service vsftpd start").Result() != 0) {
				Warning("Failed to start VSFTPD!Can`t init FTP module.");
				return;
			}
		}
	}
	new aConfigFtp();
}