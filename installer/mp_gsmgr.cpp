#include <installer/pkg.h>
#include <installer/pkgmgr.h>
#include <mgr/mgrclient.h>
#include <mgr/mgrdb.h>
#include <mgr/mgrenv.h>
#include <mgr/mgrerr.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrproc.h>
#include <api/vault.h>

#define PKGODBC "mpdb__pkgdbodbc"
#define PKGMYSQL "mpdb__pkgdbmysql"
#define VAULT_GSMGR "mgr.gsmgr"

MODULE("pkggsmgr");

namespace mgr_pkg {
class GSMgrMetaPackage : public MgrMetaPackage {
public:
	GSMgrMetaPackage(const string &shortname, const string &fullname): MgrMetaPackage(shortname, fullname) {}
	virtual bool NeedRequest(MpSession &ses) const { return false; }

	virtual void CheckConflicts(MpSession &ses) const {
		MgrMetaPackage::CheckConflicts(ses);
		const string arch = mgr_env::GetOsArch();
		if (arch != "x86_64" && arch != "amd64")
			throw mgr_err::Error("pkg_arch", m_fullname, "x86_64");

		ses.AppendCallStack(PKGODBC);
		ses.AppendCallStack(PKGMYSQL);
	}

	virtual void Configure(MpSession &ses) const {
		const string mgrdbuser = "gsmgr";
		const string host = "localhost";
		const string dbname = "gsmgr";
		isp_api::vault::Props mgrvault(VAULT_GSMGR);
		string pwd = mgrvault.Get("dbpassword");
		if (pwd.empty())
			pwd = str::hex::Encode(str::Random(16));
		mgr_db::ConnectionParams cparams;
		cparams.type = "mysql";
		cparams.db = "mysql";
		cparams.host = host;
		cparams.user = "root";
		cparams.password = isp_api::vault::Props(VAULT_MYSQL).Get("rootpassword");
		mgr_db::ConnectionPtr dbconn = Connect(cparams);
		mgr_db::QueryPtr q = dbconn->Query("select count(*) from user where user.user=" + dbconn->EscapeValue(mgrdbuser) + " and host=" + dbconn->EscapeValue(host) + "");
		if (q->AsInt(0) > 0) {
			q = dbconn->Query("select count(*) from user where user.user=" + dbconn->EscapeValue(mgrdbuser) + " and host=" + dbconn->EscapeValue(host) + " and password=PASSWORD(" + dbconn->EscapeValue(pwd) + ")");
			if (q->AsInt(0) == 0) {
				Debug("user '%s' exists, resetting password", mgrdbuser.c_str());
				dbconn->Query("set password for " + dbconn->EscapeValue(mgrdbuser) + "@" + dbconn->EscapeValue(host) + " = PASSWORD(" + dbconn->EscapeValue(pwd) + ")");
			}
		} else {
			dbconn->Query("create user " + dbconn->EscapeValue(mgrdbuser) + "@" + dbconn->EscapeValue(host) + " identified by " + dbconn->EscapeValue(pwd));
		}

		q = dbconn->Query("show databases like " + dbconn->EscapeValue(dbname));
		if (q->Eof()) {
			LogNote("creating database named '%s' for GSmanager", dbname.c_str());
			dbconn->Query("create database " + dbname + " default character set utf8");
		} else
			Debug("database named '%s' already exists", dbname.c_str());

		dbconn->Query("grant all privileges on " + dbname + ".* to " + dbconn->EscapeValue(mgrdbuser) + "@" + dbconn->EscapeValue(host));
		dbconn->Query("flush privileges");

		mgrvault.Set("dbhost", host);
		mgrvault.Set("dbuser", mgrdbuser);
		mgrvault.Set("dbpassword", pwd);
		mgrvault.Set("dbname", dbname);

		SetMgrParam(ses, "DBHost", host);
		SetMgrParam(ses, "DBUser", mgrdbuser);
		SetMgrParam(ses, "DBPassword", pwd);
		SetMgrParam(ses, "DBName", dbname);
		MgrMetaPackage::Configure(ses);
	}
};
} // end of mgr_pkg namespace

MGRPACKAGE(GSMgrMetaPackage, "gsmgr", "GSmanager");
