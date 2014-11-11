#include "sshpubkey.h"
#include <api/dbaction.h>
using namespace isp_api;

namespace test {
bool SshAuthorizeKey(string& data) {
	if (data.empty())
		return true;
	string copy_data = data;
	if (copy_data.find('\n') != string::npos || copy_data.find("ssh-") == string::npos)
		return false;
	str::GetWord(copy_data, "ssh-");
	str::GetWord(copy_data, ' ');
	string base64key = str::GetWord(copy_data, ' ');

	static const char * base64_chars_ssh =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/="; //тут еще равно, хз для чего оно в ssh ключах в конце

		if (base64key.find_first_not_of(base64_chars_ssh) != string::npos)
		return false;
	return true;
}
}

SshPubKeyTable::SshPubKeyTable() :
	mgr_db::Table("sshpubkey", 64),
	PubKey(this, "pubkey"),
	Owner(this, "owner", m_OwnerTableName, mgr_db::rtCascade)
{

}

bool SshPubKeyTable::CheckAccess(int level, const string &id) const {
	if (level < mgr_access::lvAdmin && Owner.AsString() != id)
		return false;
	return true;
}

string SshPubKeyTable::m_OwnerTableName;

class aSshPubKey : public TableIdListAction<SshPubKeyTable>
{
public:
	aSshPubKey(std::shared_ptr<mgr_db::Cache> _db, bool share_keys)
		:  TableIdListAction<SshPubKeyTable> ("sshpubkey", MinLevel (lvUser), *_db.get())
		, m_db(_db), ShareAdminsKeys(share_keys)
	{

	}

	void TableSet(Session& ses, Cursor &table) const
	{
		table->Owner = str::Int(ses.auth.ext("id"));
		string pubkey = table->PubKey;
	}

	void List(Session &ses) const
	{
		if(ses.auth.level() > lvUser && ShareAdminsKeys)
			DbList(ses, m_db->Query("SELECT k.id, k.name FROM sshpubkey k "
									"left join "+SshPubKeyTable::m_OwnerTableName+" u on u.id=k.owner "
									"where u.level>" + str::Str(lvUser)));
		else
			DbList(ses, m_db->Query("SELECT * FROM sshpubkey where owner=" + ses.auth.ext("id")));
	}

private:
	std::shared_ptr<mgr_db::Cache> m_db;
	bool ShareAdminsKeys;
};

class CheckSshPubKey : public CheckValue {
public:
	CheckSshPubKey() : CheckValue("sshpubkey") {}
	bool Check(string &value, const string &) const {
		return test::SshAuthorizeKey(value);
	}
};

void InitSshPubKeyModule(std::shared_ptr<mgr_db::Cache> db, const string& OwnerTableName, bool ShareAdminsKeys) {
	SshPubKeyTable::SetOwnerTable(OwnerTableName);
	db->Register<SshPubKeyTable>();
	new aSshPubKey(db,ShareAdminsKeys);
	new CheckSshPubKey();
}



