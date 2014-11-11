#include <mgr/mgrstr.h>
#include <mgr/mgrnet.h>
#include <api/auth_method.h>
#include <api/module.h>
#include <fstream>
#include <sstream>
#include <arpa/inet.h>
#include <api/stdconfig.h>
#include <api/longtask.h>
#include <api/xmlcache.h>
#include "osmgr_internal.h"
#include <api/wizard.h>
#include <mgr/mgrrpc.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrscheduler.h>
#include <mgr/mgrstr.h>
#include "objectlock.h"
#define OST_INSTALL_QUEUE_NAME "OST_INSTALL"

MODULE("osmgr");

namespace {
using namespace isp_api;

class CheckRepoName : public CheckValue {
public:
    CheckRepoName() : CheckValue("reponame") {}
    bool Check(string &value, const string &) const {
        value = str::Trim(value);
        if (value.empty())
            return true;
        if ((value[0] >= '0' && value[0] <= '9') || value[0] == '-')
            return false;
        return value.find_first_not_of("qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890-") == string::npos;
    }
};

void GetInstallingTemplates (StringSet &templatesOut)
{
    lt::InfoList tasks = lt::GetQueueTasks(OST_INSTALL_QUEUE_NAME);
    ForEachI (tasks, task) {
        if (task->status() == lt::Info::sPre || task->status() == lt::Info::sRun  || task->status() == lt::Info::sQueue){
            templatesOut.insert(task->elid());
        }
    }
}

void StartInstallTask (const string & elid)
{
	lt::LongTask osTemplateInstallLT("sbin/ost_install", elid, OST_INSTALL_QUEUE_NAME);
	StringVector params;
	params.push_back(elid);
	params.push_back(mgr_cf::GetPath("ostemplatedir"));
	params.push_back(GetMgrName());
	string tmpPath = mgr_cf::GetPath("temppath");
	if (!tmpPath.empty())
		params.push_back(tmpPath);

	osTemplateInstallLT.SetParams(params);
	new mgr_job::RunLongTask(osTemplateInstallLT);
}

class ActionAfterInstallTemplate : public Action
{
public:
	ActionAfterInstallTemplate()
		: Action ("osmgr.afterinstall", MinLevel (lvAdmin))
	{}
private:
	virtual void Execute(Session &ses) const
	{
		Debug ("dummy action 'asmgr.afterinstall'");
	}
};

class ChangeParameterAction{
public:
	ChangeParameterAction (Action *parent, const string parameterName)
		: m_ParentAction (parent)
		, m_ParamName (parameterName)
		, m_Toggle (this)
		, m_On (this)
		, m_Off (this)
	{}

	class Toggle : public GroupAction
	{
	public:
		Toggle (ChangeParameterAction * parent) :
			GroupAction (parent->m_ParamName + ".toggle", MinLevel (lvAdmin), parent->m_ParentAction),
			m_Parent (parent)
		{}
		void ProcessOne(Session & ses, const string &elid) const
		{
			Debug("elid=%s", elid.c_str());
			string paramFile = mgr_cf::GetPath("ostemplatedir")+"/"+elid +"/" + m_Parent->m_ParamName;
			if (!mgr_file::Exists(mgr_cf::GetPath("ostemplatedir")+"/"+elid)) {
				Debug ("template %s not installed", elid.c_str());
				return;
			}
			Debug ("param file: %s", paramFile.c_str());
			if (mgr_file::Exists(paramFile)){
				mgr_file::RemoveFile(paramFile);
			} else {
				mgr_file::Write(paramFile, "");
			}
		}
	private:
		ChangeParameterAction * m_Parent;
	};

	class On : public GroupAction
	{
	public:
		On (ChangeParameterAction * parent) :
			GroupAction (parent->m_ParamName + ".on", MinLevel (lvAdmin), parent->m_ParentAction),
			m_Parent (parent)
		{}
		void ProcessOne(Session & ses, const string &elid) const
		{
			Debug("elid=%s", elid.c_str());
			string paramFile = mgr_cf::GetPath("ostemplatedir")+"/"+elid +"/" + m_Parent->m_ParamName;
			if (!mgr_file::Exists(mgr_cf::GetPath("ostemplatedir")+"/"+elid)) {
				Debug ("template %s not installed", elid.c_str());
				return;
			}
			Debug ("param file: %s", paramFile.c_str());
			if (!mgr_file::Exists(paramFile)){
				mgr_file::Write(paramFile, "");
			}
		}
	private:
		ChangeParameterAction * m_Parent;
	};
	class Off : public GroupAction
	{
	public:
		Off (ChangeParameterAction * parent) :
			GroupAction (parent->m_ParamName + ".off", MinLevel (lvAdmin), parent->m_ParentAction),
			m_Parent (parent)
		{}
		void ProcessOne(Session & ses, const string &elid) const
		{
			Debug("elid=%s", elid.c_str());
			string paramFile = mgr_cf::GetPath("ostemplatedir")+"/"+elid +"/" + m_Parent->m_ParamName;
			Debug ("restrict file: %s", paramFile.c_str());
			if (mgr_file::Exists(paramFile)){
				mgr_file::RemoveFile(paramFile);
			}
		}
	private:
		ChangeParameterAction * m_Parent;
	};



private:
	Action * m_ParentAction;
	string m_ParamName;
	Toggle m_Toggle;
	On m_On;
	Off m_Off;
};



class aOsMgr : public StdListAction {
private:
    ChangeParameterAction m_AutoUpdate;
    ChangeParameterAction m_UserRestrict;
public:
	void AddRepoTag (Session &ses, const OsMetaInfo &info) const
    {
        if (info.Repo.empty()) {
            //mgr_xml::Xml messages = isp_api::GetMessages(ses.conn.lang(), "osmgr");
            //string localRepoMsg = messages.GetNode("//msg[@name='local_repo']");
            ses.AddChildNode("repo", "local_repository");
        } else {
            ses.AddChildNode("repo", info.Repo);
        }
    }
    void List(Session& ses) const
    {
        if (isp_api::GetMgrName() == "vmmgr") {
            ses.xml.RemoveNodes("//col[@name='type']");
        }
		string tmpPath = mgr_cf::GetPath("temppath");
		Debug ("tmppath: %s", tmpPath.c_str ());
        ses.xml.RemoveNodes("//col[@name='support']");
        OsInfoMap osinfo;
        GetOsInfo(osinfo);
        StringSet installingTemplates;
        GetInstallingTemplates(installingTemplates);
        lt::InfoList tasks = lt::GetQueueTasks (OST_INSTALL_QUEUE_NAME);
        ForEachI(osinfo, os) {
            if (!os->second.Compatible)
                continue;
            bool installTaskRunning = false;
            ForEachI(tasks, task) {
                if (task->elid() == os->first && task->IsAlive()) {
                    installTaskRunning = true;
                    break;
                } 
            }
            ses.NewElem();
            ses.AddChildNode("id", os->first);
            ses.AddChildNode("name", os->second.Name);
            AddRepoTag(ses, os->second);
			ses.AddChildNode("date",os->second.RepoDate);
            ses.AddChildNode("type",os->second.Type);
			Debug ("installTaskRunning: %d, install result: '%s'", installTaskRunning, os->second.InstallResult.c_str());
			if (!installTaskRunning) {
				if (os->second.Installed) {
					if (!OsInstalledGood(os->second)) {
						ses.AddChildNode ("install_error");
					}
					if (os->second.AutoupdateOn) {
						ses.AddChildNode("autoupdate", "on");
					} else {
						ses.AddChildNode("autoupdate", "off");
					}

					if (os->second.AccessAdminOnly) {
						ses.AddChildNode("restrict", "on");
					} else {
						ses.AddChildNode("restrict", "off");
					}
				}
				ses.AddChildNode ("installed", os->second.InstallResult);
			}
            if (os->second.CanBeUpdated)
                ses.AddChildNode("updateable");
			if (installingTemplates.find(os->first) != installingTemplates.end() || installTaskRunning)
                ses.AddChildNode("installing", "on");
			else
				ses.AddChildNode("installing", "off");
			if (!os->second.CompatibleVersion)
				ses.AddChildNode("need_version", os->second.CompatibleVersionString);
        }

    }
    void Del(Session& ses, const string& elid) const
    {
        RemoveOsTemplate(elid);
    }

    void Get(Session & ses, const string & elid) const
    {
    	OsInfoMap osinfo;
    	GetOsInfo(osinfo);
		ses.NewNode("autoupdate", osinfo[elid].AutoupdateOn ? "on":"");
		ses.NewNode("restrict", osinfo[elid].AccessAdminOnly ? "on":"");
		ses.NewNode("name", osinfo[elid].Name);
    }

    void Set(Session &ses, const string & elid) const
    {
    	STrace();
    	string action = ses.Checked("autoupdate") ? "osmgr.autoupdate.on" : "osmgr.autoupdate.off";
    	InternalCall(action, "elid=" + elid);
    	action = ses.Checked("restrict") ? "osmgr.restrict.on" : "osmgr.restrict.off";
    	InternalCall(action, "elid=" + elid);
    	STrace();
    }

    class aUpdateAll : public Action
    {
    public:
    	aUpdateAll (aOsMgr *parent) : Action ("updateall", MinLevel (lvAdmin), parent) {}
    	void Execute (Session &ses) const
    	{
    		OsInfoMap osinfo;
    		GetOsInfo(osinfo);
//    		StringSet installingTemplates;
//    		GetInstallingTemplates(installingTemplates);
    		lt::InfoList tasks = lt::GetQueueTasks (OST_INSTALL_QUEUE_NAME);
    		ForEachI(osinfo, os) {
    			if (!os->second.Compatible)
    				continue;
    			bool installTaskRunning = false;
    			ForEachI(tasks, task) {
    				if (task->elid() == os->first && task->IsAlive()) {
    					installTaskRunning = true;
    					break;
    				}
    			}
    			Debug ("ostemplate: %s. Installed: %d, install task running: %d, template can be updated: %d, autoupdate enabed: %d",
    					os->first.c_str(),
    					os->second.Installed,
    					installTaskRunning,
    					os->second.CanBeUpdated,
    					os->second.AutoupdateOn );
    			bool needToUpdate = os->second.Installed
    							 && !installTaskRunning
    							 && os->second.CanBeUpdated
    							 && os->second.AutoupdateOn;
    			if (needToUpdate) {
    				StartInstallTask (os->first);
    			}
    		}
    	}
    } UpdateAll;

    class aInstallOSTemplate: public GroupAction {
    public:
        aInstallOSTemplate(aOsMgr *parent):GroupAction("install", MinLevel(lvAdmin), parent) {}
        void ProcessOne(Session & ses, const string &elid) const
        {
			string osTemplateName = elid;
			Debug("elid=%s", elid.c_str());
			const string repo = str::GetWord(osTemplateName, RepoSeparator);
			ConfRef conf = OsRepositoryConfig::GetConf();
			// Check repo exists
			conf.Get<OsRepositoryDataSet>(repo);

        	auto info = GetOsInfoFromCache(repo, osTemplateName);
			if (!info.CompatibleVersion)
				throw mgr_err::Error("osmgr_need_version", "mgr", info.CompatibleVersionString);
			StartInstallTask (elid);
        }
    } InstallOSTemplate;

    class aDone : public Action {
    public:
        aDone (aOsMgr *parent) : Action ("done", MinLevel (lvAdmin), parent) {}
        void Execute (Session &ses) const
        {
            string result = ses.Param ("result");
            string elid = ses.Param("elid");
            SetInstallResult (mgr_cf::GetPath("ostemplatedir"), elid, result);
        }
    } Done;

    class aFetchOSList: public Action {
    public:
        aFetchOSList(aOsMgr *parent):Action("fetch", MinLevel(lvAdmin), parent) {}
        void Execute(Session& ses) const
        {
            UpdateOsInfo();
            ses.Ok(mgr_session::BaseSession::okNone);
        }
    }FetchOSList;

    aOsMgr()
    	: StdListAction("osmgr", MinLevel(lvAdmin))
    	, m_AutoUpdate (this, "autoupdate")
    	, m_UserRestrict (this, "restrict")
    	, UpdateAll (this)
    	, InstallOSTemplate(this)
    	, Done(this)
    	, FetchOSList(this) {}
};

class aRepo : public StdListAction
{
public:
    aRepo()
        : StdListAction ("repo", MinLevel (lvAdmin))
    {}

    void AssertLocationExists (Session &ses) const
    {
        ConfRef conf = OsRepositoryConfig::GetConf();
        auto osRepoDS = conf.Get<OsRepositoryDataSet>();
        string newLocation = str::Trim (ses.Param ("location"));
        Debug ("new location: '%s'", newLocation.c_str ());
        ForEachRecord (osRepoDS) {
            Debug ("check '%s'", osRepoDS->Location.c_str());
            if (newLocation == str::Trim (osRepoDS->Location)) {
                Debug ("Location exists!");
                throw mgr_err::Exists ("location", osRepoDS->Location);
            }
        }
    }
    void AssertTemplateCount (const string &osRepoName) const
    {
        OsInfoMap info;
        GetOsInfo(info);
        size_t template_count = 0;
        ForEachI(info, e) {
            if (e->second.Repo == osRepoName)
                ++template_count;
        }

        if (template_count == 0)
            throw mgr_err::Error("no_template");
    }

    void AssertLocationGood(Session &ses) const
    {
        STrace();
        try {
            std::stringstream result;
            mgr_rpc::HttpQuery hq;
            if (!hq.Get(ses.Param ("location")+"/"+RepoOsListFileName, result).good())
				throw mgr_err::Error("osmgr_bad_location");
            Debug ("oslist: \n%s", result.str ().c_str ());
        } catch (mgr_err::Error &err) {
            Warning ("Failed to fetch '%s'", (ses.Param ("location")+"/"+RepoOsListFileName).c_str());
            throw mgr_err::Error ("osmgr_bad_location").add_param ("location", ses.Param ("location")+"/"+RepoOsListFileName);
        }

		try {
			std::stringstream result;
			mgr_rpc::HttpQuery hq;
			hq.Get(ses.Param ("location")+"/" + RepoInfoFileName, result);
			const string content = result.str();
			Debug("repo.info:\n%s", content.c_str());
			if (content.find(RepoInfoContent) != 0)
				throw mgr_err::Error("osmgr_bad_repoinfo");
		} catch (mgr_err::Error &err) {
			Warning ("Failed to fetch or parse '%s'", (ses.Param ("location")+"/"+RepoInfoFileName).c_str());
			throw mgr_err::Error ("osmgr_bad_repoinfo").add_param("filename", RepoInfoFileName).add_param("content", RepoInfoContent);
		}
        Debug ("All ok");
    }

    virtual void Get(Session &ses, const string &elid) const
    {
        if (elid.empty()) {
            return;
        }
        ConfRef conf = OsRepositoryConfig::GetConf();
        auto osRepoDS = conf.Get<OsRepositoryDataSet>(elid);
		ses.NewNode("title", elid);
        ses.NewNode("location", osRepoDS->Location);
    }
    virtual void Set(Session &ses, const string &elid) const
    {
        ConfRef conf = OsRepositoryConfig::GetConf();
        auto osRepoDS = conf.Get<OsRepositoryDataSet>(elid);
        if (ses.Param ("location") != osRepoDS->Location.Str()) {
            AssertLocationExists (ses);
            AssertLocationGood (ses);
        }
        //osRepoDS->Name = ses.Param("name");
        osRepoDS->Location = ses.Param("location");
        osRepoDS->Post();
        UpdateOsInfo();
        AssertTemplateCount(osRepoDS->Name);
    }
    virtual void New(Session &ses) const
    {
        ConfRef conf = OsRepositoryConfig::GetConf();
        auto osRepoDS = conf.Get<OsRepositoryDataSet>();
        if (osRepoDS->Find(ses.Param ("name"))) {
            throw mgr_err::Exists("name", ses.Param ("name"));
        }
        AssertLocationExists (ses);
        AssertLocationGood (ses);
        osRepoDS->New();
        osRepoDS->Name = ses.Param("name");
        osRepoDS->Location = ses.Param("location");
        osRepoDS->Post();
        UpdateOsInfo();
        AssertTemplateCount(osRepoDS->Name);

    }
    virtual void Del(Session &ses, const string &elid) const
    {
    	mgr_thread::ObjectLock<int> lock("repo_" + elid);
    	if (lock.TryLock()) {
			ConfRef conf = OsRepositoryConfig::GetConf();
			auto osRepoDS = conf.Get<OsRepositoryDataSet>();
			if (!osRepoDS->Find (elid)) {
				return;
			}
			osRepoDS->Delete();
			osRepoDS->Post();
			UpdateOsInfo();
    	} else {
    		throw mgr_err::Error("repo_locked").add_param("repo", elid);
    	}
    }
    virtual void List(Session &ses) const
    {
        ConfRef conf = OsRepositoryConfig::GetConf();
        auto osRepoDS = conf.Get<OsRepositoryDataSet>();
        ForEachRecord(osRepoDS) {
            ses.NewElem();
            ses.AddChildNode("name", osRepoDS->Name);
            ses.AddChildNode("location", osRepoDS->Location);
            ses.AddChildNode("default", "no");
        }
    }
};


class aInstallTemplate : public FormAction {
public:
	aInstallTemplate()
        : FormAction("osmgr.installtemplate", mgr_access::AccessAdmin) {
    }

protected:

    void CheckRepo () const
    {
        ConfRef conf = OsRepositoryConfig::GetConf();
        auto osRepoDS = conf.Get<OsRepositoryDataSet>();
        size_t repo_count = 0;
        ForEachRecord(osRepoDS) {
            ++repo_count;
        }
        if (repo_count == 0) {
            Debug ("setup repo");
            NotConfigured("repo_edit", "repo.edit");

        }
    }
protected:
    virtual void Get(Session &ses, const string &elid) const
    {
        STrace();
        CheckRepo();
        OsInfoMap info;
        GetOsInfo(info);
        ConfRef conf = OsRepositoryConfig::GetConf();
        auto osRepoDS = conf.Get<OsRepositoryDataSet>();

        ses.NewSelect("repo");
        ForEachRecord(osRepoDS) {
            ses.AddChildNode("val", osRepoDS->Name);
        }

        ses.NewSelect("template");
        ForEachI(info, e) {
            if (!e->second.Compatible)
                continue;
            ses.AddChildNode("val", e->second.Name);
            ses.NodeProp("key", e->first);
            ses.NodeProp("depend", e->second.Repo);
        }
    }
    virtual void New(Session &ses) const
    {
        if(ses.Param("template").empty())
            throw mgr_err::Value("template");
        int res = mgr_proc::Execute("sbin/ost_install " +
                                    mgr_proc::Escape(ses.Param("template")) + " " +
                                    mgr_cf::GetPath("ostemplatedir") + " " +
                                    GetMgrName() + " " +
                                    mgr_cf::GetPath("temppath")).Run().Result();
        if (res != 0)
            throw mgr_err::Error("install_failed");
    }
};

MODULE_INIT(OSMgr,"")
{

    Debug("Init OSmgr");
    //		mgr_cf::AddParam("DefaultRepoUrl", "http://master.download.ispsystem.com/OSTemplate");
    //		mgr_cf::AddParam("DefaultRepoName", "ISPsystem");
    new CheckRepoName();
    new aOsMgr();
    new aRepo();

//    new aCreateRepo();
    new aInstallTemplate();
	new ActionAfterInstallTemplate;
//	AddCron Task for os update;
	string command = mgr_file::ConcatPath(mgr_file::GetCurrentDir(), "sbin/mgrctl -m "+GetMgrName()+" osmgr.");
	mgr_scheduler::Scheduler schdlr("root");
	if (!schdlr.LocateByCommand(command+"fetch")) {
		schdlr.CreateTask(command+"fetch");
		schdlr.SetMinute("2");
		schdlr.SetHour("5");
		schdlr.Post();
	}
	if (!schdlr.LocateByCommand(command+"updateall")) {
		schdlr.CreateTask(command+"updateall");
		schdlr.SetMinute("2");
		schdlr.SetHour("5");
		schdlr.Post();
	}

}
}
