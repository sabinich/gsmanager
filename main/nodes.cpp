#include <mgr/mgrlog.h>
#include <mgr/mgrproc.h>
#include <api/action.h>
#include <api/dbaction.h>
#include <api/module.h>
#include <api/longtask.h>
#include "dbobject.h"
#include "nodes.h"
#include "defines.h"
#include "gmmain.h"

MODULE("gsmgr");

Node::Node (const string &ip, const string &name)
	: m_ip (ip)
	, m_name (name)
	, m_id (-1)
	, m_miniPort (1500)
	, m_isLocal (false)
{ }

Node::Node (int hostID)
	: m_id (hostID)
{
	auto cursor = db->Get<NodeTable>(str::Str(m_id));
	Init(cursor);
}

Node::Node (NodeTable::Cursor &cursor) {
	Init(cursor);
}

void Node::Init(NodeTable::Cursor &cursor)
{
	m_id = cursor->Id;
	m_ip = cursor->Ip;
	m_name = cursor->Name;
	m_password = cursor->MiniPassword;
	m_isLocal = cursor->LocalNode;
	m_miniPort = cursor->MiniMgrPort;
	//m_use_https = cursor->Type == "windows";
	m_use_https = true;
}

mgr_rpc::SSH Node::AssertSSHConnection ()
{
	mgr_rpc::SSHkey checker(m_ip, "root", mgr_cf::GetParam(ConfsshPrivatKey));
	checker.EnableVerifyHostKey(mgr_cf::GetParam(ConfsshKnownHosts), mgr_rpc::hkaReplace);
	checker.Execute("echo");
	if (checker.Result()) {
		throw mgr_err::Error ("node", "connection", "failed");
	}
	return checker;
}

mgr_rpc::SSH Node::ssh() {
	mgr_rpc::SSHkey ssh_ptr(m_ip, "root", mgr_cf::GetParam(ConfsshPrivatKey));
	ssh_ptr.Connect ();
	return ssh_ptr;
}

bool Node::IsLocal() {
	mgr_file::Tmp tmpFile ("/tmp/check-local");
	m_isLocal =  ssh().Execute("ls " + tmpFile.Str()).Result() == 0;
	return m_isLocal;
}

mgr_client::Result Node::MiniQuery(const string& query, int timeout) {
	STrace();
	if (!m_mini_client) {
		if (m_isLocal) {
			m_mini_client.reset(new mgr_client::Local("gsmini", "gsmgr"));
		} else {
			string proto = m_use_https?"https":"http";
			m_mini_client.reset(new mgr_client::Remote(str::Format("%s://%s:%d/gsmini", proto.c_str(), m_ip.c_str(), m_miniPort)));
		}
	}
	if (m_password.empty())
		throw mgr_err::Missed("minipassword");
	const string auth = "authinfo=admin:" + str::url::Encode(m_password);

	if (m_isLocal) {
		Debug ("Local query");
		return m_mini_client->Query(query);
	} else {
		string proto = m_use_https?"https":"http";
		Debug ("Remote query to %s://%s:%d/gsmini", proto.c_str(), m_ip.c_str(), m_miniPort);
		mgr_client::Remote* remote = dynamic_cast<mgr_client::Remote*>(m_mini_client.get());
		if (remote) {
			if (timeout)
				remote->SetTimeout(timeout);
			return remote->Query(auth + "&" + query);
		} else
			throw mgr_err::Error("cast");
	}

}

static bool checkConnection (mgr_rpc::SSH & ssh)
try {
	ssh.Connect ();
	return true;
} catch (...) {
	return false;
}

bool InitSshConnection(const string &ip, const string &passwd, const string &user = "root", int port = 22)
{
	mgr_rpc::SSHkey sshKey(ip, user, mgr_cf::GetParam(ConfsshPrivatKey), "", port);
	sshKey.EnableVerifyHostKey(mgr_cf::GetParam(ConfsshKnownHosts), mgr_rpc::hkaReplace);
	Trace ("Try co connect using key %s. login: %s, passwd: *, port: %d",
		   mgr_cf::GetParam(ConfsshPrivatKey).c_str (),
		   user.c_str (),
		   port);
	if (checkConnection (sshKey)) {
		Trace ("OK");
		return true;
	}
	Trace ("try to connect using password");
	mgr_rpc::SSHpass sshPass(ip, user, passwd, port);
	sshPass.EnableVerifyHostKey(mgr_cf::GetParam(ConfsshKnownHosts), mgr_rpc::hkaReplace);

	if (checkConnection (sshPass)) {
		//write key...
		Trace("Connect OK, write key");
		const string pubKey = mgr_file::ReadE (mgr_cf::GetParam (ConfsshPublicKey));
		Debug ("public key: \n%s\n", pubKey.c_str ());
		sshPass.Execute ("test -d /root/.ssh || mkdir /root/.ssh").Result ();
		sshPass.Execute ("echo >> /root/.ssh/authorized_keys").Result();
		sshPass.Execute ("cat >> /root/.ssh/authorized_keys").In (pubKey).Result ();
		sshPass.Execute ("echo >> /root/.ssh/authorized_keys").Result ();
		return true;
	}
	Trace ("Fail");
	return false;
}

void InstallGSMini(mgr_rpc::SSH& ssh, const string& mgrdir) {
	if (ssh.Execute(mgr_file::ConcatPath(mgrdir, "/bin/core") + " gsmini install").Result() != 0)
		throw mgr_err::Error("install_gsmini_failed");
}

void CopySelf(mgr_rpc::SSH& ssh, const string& mgrdir, const string& ip) {
	ssh.Execute("killall ihttpd").Result();
	ssh.Execute("killall core").Result();
	ssh.Execute("mkdir -p " + mgrdir).Result();
	StringVector files;
	files.push_back("bin");
	files.push_back("addon");
	files.push_back("cgi");
	//files.push_back("etc/common.conf");
	files.push_back("etc/core.conf");
	files.push_back("etc/gtaver.conf");
	files.push_back("etc/debug.conf");
	files.push_back("etc/manager.crt");
	files.push_back("etc/manager.key");
	files.push_back(mgr_cf::GetParam(ConfsshPrivatKey));
	files.push_back(mgr_cf::GetParam(ConfsshPublicKey));
	files.push_back(mgr_cf::GetParam(ConfsshKnownHosts));
	files.push_back("etc/ihttpd_errpage");
	files.push_back("etc/scripts");
	files.push_back("etc/xml");
	//files.push_back("etc/xml/core*");
	//files.push_back("etc/xml/gsmini*");
	//files.push_back("etc/xml/vemini_msg_ru.xml");
	files.push_back("external");
	files.push_back("libexec");
	files.push_back("sbin");
	files.push_back("skins");
	files.push_back("xmlschema");
	mgr_file::Dir d(mgr_file::ConcatPath(mgrdir, "lib"));
	while (d.Read()) {
		if (d.name() != "gsmgr.so")
			files.push_back("lib/" + d.name());
	}
	string items_to_copy = str::Join(files, " ");
	string ssh_key = mgr_cf::GetParam(ConfsshPrivatKey);
	if (!ssh_key.empty() && ssh_key[0] != '/')
		ssh_key = mgr_file::ConcatPath(mgr_file::GetCurrentDir(), ssh_key);
	string remove_cmd = "cd " + mgrdir + "; rm -rf " + items_to_copy;
	if (ssh.Execute(remove_cmd).Result() != 0)
		throw mgr_err::Error("remote_rm_failed");
	if (mgr_proc::Execute("tar cpzf - " + items_to_copy + " | "
			"ssh -i " + ssh_key + " -o UserKnownHostsFile=" + mgr_cf::GetParam(ConfsshKnownHosts) + " " +
			"root@" + ip + " 'tar xzpf - -C " + mgrdir + "'")
			.Run().Result() != 0)
		throw mgr_err::Error("scp_failed");
	if (ssh.Execute(mgrdir + "/sbin/ihttpd").Result() != 0)
		throw mgr_err::Error("remote_ihttpd_failed");
}

string GeneratePassword(mgr_rpc::SSH& ssh, const string& mgr_dir) {
	string pass = str::hex::Encode(str::Random(PASSWDLEN));
	string tmp = str::base64::Encode(str::Crypt(pass));
	ssh.Execute("cd " + mgr_dir + "; echo " ConfMiniPasswordParam " " + tmp + " > ./etc/gsmini.conf").Result();
	return pass;
}

using namespace isp_api;

void InstallPackage(Session& ses, mgr_rpc::SSH& ssh, const string& package, const string& mgrdir, const string& command) {
	Trace ("Install %s...", package.c_str());
	ses.ProgressSetComment("install_" + package);
	auto res =  ssh.Execute("cd " + mgrdir + "; " + command);
	Debug("pkg out = '%s'", ssh.Str().c_str());
	if (res.Result() != 0)
		throw mgr_err::Error("install_failed", package, package);

	ses.ProgressAddDone();
}

class aNode : public TableIdListAction<NodeTable>
{
private:
	static mgr_thread::SafeLock m_node_lock;

	/**
	 * @brief InstallCommit используется для подтверждения установки дистрибутива игры на ноду
	 * @param elid - имя игры
	 * @param node_id - id ноды
	 */
	void InstallCommit(const string& elid, const string& node_id) const {
		STrace();
		Debug("InstallCommit game='%s', node_id='%s'", elid.c_str(), node_id.c_str());
		auto installedGamesTable = db->Get<AllowedGamesTable>();
		if (!installedGamesTable->DbFind("node=" + db->EscapeValue(node_id) + " and gamename=" + db->EscapeValue(elid))) {
			installedGamesTable->New();
			installedGamesTable->Node = str::Int(node_id);
			installedGamesTable->GameName = elid;
			installedGamesTable->Post();
		}
	}

	friend class aNodeGameList;
public:
	aNode()
		: TableIdListAction<NodeTable> ("node", MinLevel (lvAdmin), *db.get()),
			AddWinNode(this),
			NodeQuery(this),
			GameList(this)
	{
	}

	void TableFormTune(Session &ses, Cursor &table) const {
		ses.NewNode("sshkey", mgr_file::ReadE(mgr_cf::GetParam(ConfsshPublicKey)));
	}

	void TableSet(Session& ses, Cursor &table) const
	{
		mgr_thread::SafeSection ss(m_node_lock);
		if (table->IsNew ()) {
			//новая нода
			Node host(table->Ip, table->Name);
			table->Status = true;
#define NodeProgressSteps 7

			ses.ProgressAddSteps(NodeProgressSteps);
			ses.ProgressSetComment("checkconn");
			Trace("check connection...");
			if (!InitSshConnection(table->Ip, ses.Param("passwd"))) {
				throw mgr_rpc::ConnectErr(table->Ip);
			}
			ses.ProgressAddDone();

			auto ssh = host.AssertSSHConnection();
			ses.ProgressSetComment("check_platform");
			string platform = ssh.Execute("arch").Str();
			Debug("platform '%s'", platform.c_str());
			if (platform.find("64") == string::npos)
				throw mgr_err::Error("not_64_platform");
			ses.ProgressAddDone();
			//ses.ProgressSetComment("disable_selinux");
			//DisableSELinux(host);
			//ses.ProgressAddDone();

			ses.ProgressSetComment("copyself");
			string mgrdir = mgr_file::GetCurrentDir();
			Trace ("Check local...");
			if (!host.IsLocal()) {
				Trace ("Copy self...");
				CopySelf(ssh, mgrdir, table->Ip);
				table->LocalNode = false;
			} else {
				Trace ("Local node.");
				table->LocalNode = true;
			}
			table->MiniMgrPort = DefaultMinimgrPort;
			InstallGSMini(ssh, mgrdir);
			ses.ProgressAddDone();

			ses.ProgressSetComment("passwd_gen");
			//генерация пароля для ноды
			table->MiniPassword = GeneratePassword(ssh, mgrdir);
			host.SetMiniPassword(table->MiniPassword);
			ses.ProgressAddDone();

			ses.ProgressSetComment("ihttpd_start");
			if (ssh.Execute("cd " + mgrdir + "; sh ./etc/scripts/ihttpd_start.sh " + mgrdir).Result() != EXIT_SUCCESS)
				throw mgr_err::Error("ihttpd_start");
			ses.ProgressAddDone();

			ses.ProgressSetComment("check_gsmini");
			host.MiniQuery("func=actionlist");
			ses.ProgressAddDone();

			InstallPackage(ses, ssh, "screen", mgrdir, "sbin/pkgctl install screen");
			InstallPackage(ses, ssh, "ia32-libs", mgrdir, "sbin/pkgctl install ia32-libs");
			InstallPackage(ses, ssh, "smem", mgrdir, "sbin/pkgctl install smem");
		}
	}

	virtual void TableBeforeDelete(Session& ses, Cursor &table) const {
		mgr_thread::SafeSection ss(m_node_lock);
	}
	
	class aAddWinNode : public FormAction {
	public:
		aAddWinNode(aNode * parent): FormAction("winnode",MinLevel (lvAdmin),parent){};
		
		virtual void Get(Session& ses, const string& elid) const
		{
		}
		
		virtual void Set(Session& ses, const string& elid) const
		{
			auto cNode = db->Get<NodeTable>();
			cNode->New();
			cNode->Name = ses.Param("name");
			cNode->Ip = ses.Param("ip");
			cNode->Type = "windows";
			cNode->MiniMgrPort = str::Int(ses.Param("minimgrport"));
			cNode->MiniPassword = ses.Param("passwd");
			cNode->Status = true;
			cNode->Post();
			Node node(cNode);
			node.MiniQuery("func=serversstate&out=xml");
		}
	} AddWinNode;
	
	class aNodeQuery : public Action {
	public:
		aNodeQuery(aNode * parent): Action("query",MinLevel (lvAdmin),parent){};
		virtual void Execute(Session& ses) const
		{
			auto cNode = db->Get<NodeTable>(ses.Param("elid"));
			string query = str::base64::Decode(ses.Param("query"));
			Node node(cNode);
			ses.xml = node.MiniQuery(query+"&out=xml").xml;
		}
	} NodeQuery;

	class aNodeGameList : public ListAction {
	public:
		aNodeGameList(Action *parent) :
			ListAction("gamelist", MinLevel (lvAdmin), parent),
			NodeAllowGame (this),
			NodeDenyGame (this){}

		void List(Session &ses) const {
			//all games
			string nodeId = ses.Param("elid");
			StringMap games = GameModules::Get();
			ForEachI (games, module) {
				lt::InfoList tasks = lt::GetQueueTasks("COPYDISTR");
				auto last_task = tasks.begin();
				bool fnd = false;
				for (auto it = tasks.begin(); it != tasks.end(); ++it) {
					if (it->elid() == module->first && last_task->id() < it->id()) {
						last_task = it;
						fnd = true;
					}
				}
				bool installing = fnd && (last_task->status() == lt::Info::sRun ||
				                          last_task->status() == lt::Info::sPre ||
				                          last_task->status() == lt::Info::sQueue);
				bool error = fnd && last_task->status() == lt::Info::sErr;
// 				if (fnd && last_task->status() == lt::Info::sOk) {
// 					reinterpret_cast<const aNode*>(parent())->InstallCommit(*module, nodeId);
// 				}
				ses.NewElem();
				ses.AddChildNode("name", module->first);
				if (error) {
					ses.AddChildNode("error", "on");
				} else if (installing) {
					ses.AddChildNode("installing", "on");
				} else {
					auto allowedGamesTable = db->Get<AllowedGamesTable>();
					bool allowed = allowedGamesTable->DbFind ("node=" + nodeId + " AND gamename=" + db->EscapeValue(module->first));
					if (allowed) {
						string installedCount = db->Query("SELECT COUNT(*) FROM server WHERE type=" + db->EscapeValue(module->first) + " AND node = " + nodeId)->Str();
						ses.AddChildNode("allowed", "on");
						ses.AddChildNode("icount", installedCount);
					} else {
						ses.AddChildNode("allowed", "off");
					}
				}
			}
			ses.NewNode("plid", nodeId);
		}

		class aNodeAllowGame : public GroupAction {
		public:
			aNodeAllowGame (Action *parent) : GroupAction ("allow", MinLevel(lvAdmin), parent), m_install_commit(this) {}
			virtual void ProcessOne(Session &ses, const string &elid) const {
				string path = mgr_file::ConcatPath(mgr_cf::GetPath(ConfGameInstallDir), elid);
				string nodeId = ses.Param("plid");
				auto cNode = db->Get<NodeTable>(nodeId);
				if (cNode->Type=="windows") {
					reinterpret_cast<const aNode*>(parent()->parent())->InstallCommit(elid, nodeId);
					return;
				}
				
				if (!mgr_file::Exists(path))
					throw mgr_err::Missed("path", path);


				Node node(str::Int(nodeId));
				StringVector params;
				params.push_back("--path");
				params.push_back(path);
				params.push_back("--ip");
				params.push_back(node.Ip());
				params.push_back("--sshkey");
				params.push_back(mgr_cf::GetParam(ConfsshPrivatKey));
				params.push_back("--sshpub");
				params.push_back(mgr_cf::GetParam(ConfsshPublicKey));
				params.push_back("--hosts");
				params.push_back(mgr_cf::GetParam(ConfsshKnownHosts));
				params.push_back("--distupdate");
				if (ses.Checked("distupdate"))
					params.push_back("on");
				else
					params.push_back("off");

				lt::LongTask copy_longtask("sbin/" CMD_COPY_DIST, elid, "COPYDISTR");
				copy_longtask.SetParams(params);
				new mgr_job::RunLongTask(copy_longtask);

//				node.MiniQuery("func=" + elid + ".installdist");
			}
		private:
			class aNodeInstallCommit : public Action {
			public:
				aNodeInstallCommit(const Action* parent) : Action("commit", MinLevel(lvAdmin), parent) {}
				void Execute(Session &ses) const {
					auto node_table = db->Get<NodeTable>();
					if (!node_table->DbFind("ip=" + db->EscapeValue(ses.Param("ip"))))
						throw mgr_err::Missed("node", ses.Param("ip"));
					if (!ses.Checked("distupdate"))
						reinterpret_cast<const aNode*>(parent()->parent())->InstallCommit(ses.Param("elid"), node_table->Id);
				}
			} m_install_commit;
		} NodeAllowGame;
		
		class aNodeDenyGame : public GroupAction {
		public:
			aNodeDenyGame (Action *parent) : GroupAction ("deny", MinLevel(lvAdmin), parent) {}
			virtual void ProcessOne(Session &ses, const string &elid) const {
				auto cAllow = db->Get<AllowedGamesTable>();
				if (cAllow->DbFind("node='"+ses.Param("plid")+"' AND gamename='"+elid+"'"))
					cAllow->Delete();
			}
		}NodeDenyGame;

	} GameList;

};


class aRefillNode : public Action
{
public:
	aRefillNode () : Action ("node.refill", mgr_access::MinLevel(mgr_access::lvAdmin))
	{}
	void Execute(Session &ses) const
	{
		string id = ses.Param("elid");
		Debug ("id='%s'", id.c_str());
		auto nodeTable = db->Get <NodeTable>(id);
		Node host(nodeTable->Ip, nodeTable->Name);
		if (!host.IsLocal()) {
			Trace ("Copy self...");
			mgr_rpc::SSH ssh = host.ssh();
			CopySelf(ssh, mgr_file::GetCurrentDir(), nodeTable->Ip);
			ssh.Execute("killall core");
		}
	}

};

class aNodeDistUpdate : public GroupAction
{
public:
	aNodeDistUpdate () : GroupAction ("node.updatedist", mgr_access::MinLevel(mgr_access::lvAdmin))
	{}
    virtual void ProcessOne(Session& ses, const string& elid) const
	{
		string nodeid = ses.Param("elid");
		ForEachQuery(db,"SELECT gamename FROM allowedgames WHERE node="+nodeid,game) {
			string dist = game->Str();
			Debug("InternalCall %s",((string)("ndoe.gamelist.allow.one&elid="+dist+"&plid="+nodeid+"&distupdate=on")).c_str());
			InternalCall(ses,"node.gamelist.allow","elid="+dist+"&plid="+nodeid+"&distupdate=on");
		}
	}
};


mgr_thread::SafeLock aNode::m_node_lock;

MODULE_INIT(nodes, "gsmgr")
{
	Debug("Init Nodes module");
	new aNode();
	new aRefillNode();
	new aNodeDistUpdate();
}
