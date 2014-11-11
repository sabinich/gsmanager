#include <api/dbaction.h>
#include "dbobject.h"
#include <api/module.h>
#include <mgr/mgrlog.h>
MODULE("gsmgr");

using namespace isp_api;

using namespace mgr_log;

// --- User management
class aUser : public TableIdListAction<UserTable> {
public:
	aUser()
		: TableIdListAction<UserTable>("user", MinLevel(lvAdmin), *db.get())
		, UserEnable(this)
		, UserDisable(this)

	{
	}

	void TableFormTune(Session &ses, Cursor &table) const {
		if (ses.Param("elid").empty()) {
			ses.NewSelect("level");
			ses.AddChildNode("msg", "user");
			ses.NodeProp("key", str::Str(lvUser));
			ses.AddChildNode("msg", "admin");
			ses.NodeProp("key", str::Str(lvAdmin));
		} else {
			ses.xml.RemoveNodes("//field[@name='level']");
		}
	}

	void TableSet(Session &ses, Cursor &table) const
	{
		if(table->IsNew() || table->Name.OldValue() != table->Name.AsString()) {
			if (isp_api::UserExists(table->Name))
				throw mgr_err::Exists("user", table->Name);
		}
		if(table->IsNew()) {
			table->Level = str::Int(ses.Param("level"));
			table->Enabled = true;
		}
		if (!ses.Param("passwd").empty())
			table->Password = str::Crypt(ses.Param("passwd"));
	}

	virtual void TableBeforeDelete(Session& ses, Cursor &table) const {
		DropAuthen(table->Name);
	}

	class aUserEnable: public GroupAction {
		public:
			aUserEnable(aUser *parent):GroupAction("enable", MinLevel(lvAdmin), parent) {}
			void ProcessOne(Session & ses, const string &elid) const
			{
				auto table = db->Get<UserTable>();
				table->Assert(elid);
				ForEachQuery(db, "SELECT id FROM server WHERE user="+table->Id, res) {
					InternalCall(ses,"server.enable","sok=ok&elid="+res->AsString("id"));
					InternalCall(ses,"server.resume","sok=ok&elid="+res->AsString("id"));
				}
				table->Enabled = true;
				table->Post();
			}
		} UserEnable;

		class aUserDisable: public GroupAction {
		public:
			aUserDisable(aUser *parent):GroupAction("disable", MinLevel(lvAdmin), parent) {}
			void ProcessOne(Session & ses, const string &elid) const
			{
				auto table = db->Get<UserTable>();
				table->Assert(elid);
				if (table->Name == ses.auth.name())
					throw mgr_err::Error("disable_self");
				DropAuthen(table->Name);
				ForEachQuery(db, "SELECT id FROM server WHERE user="+table->Id, res) {
					InternalCall(ses,"server.disable","sok=ok&elid="+res->AsString("id"));
				}
				table->Enabled = false;
				table->Post();
			}
		} UserDisable;
};

class aUserSu : public Action {
public:
	aUserSu(): Action("user.su", MinLevel(lvAdmin)) {}

	void Execute(Session &ses) const {
		auto user = db->Get<UserTable>(ses.Param("elid"));
		ses.xml = InternalCall(ses, "su", "elid=" + str::url::Encode(user->Name));
	}
};


MODULE_INIT(user, "gsmgr")
{
	STrace();
	new aUser;
	new aUserSu;
}
