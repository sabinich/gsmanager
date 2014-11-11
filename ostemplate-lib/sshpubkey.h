#ifndef SSHPUBKEY_H
#define SSHPUBKEY_H
#include <mgr/mgrdb.h>
#include <mgr/mgrdb_struct.h>

//публичные ключи на ssh

namespace test {
bool SshAuthorizeKey(string& data);
}

void InitSshPubKeyModule(std::shared_ptr<mgr_db::Cache> db, const string& OwnerTableName = "user", bool ShareAdminsKeys = true);

class SshPubKeyTable : public mgr_db::Table
{
	friend class aSshPubKey;
public:
	SshPubKeyTable();
	mgr_db::TextField PubKey;
	mgr_db::ReferenceField Owner;

	static void SetOwnerTable(const string& OwnerTableName) {m_OwnerTableName = OwnerTableName;}
	bool CheckAccess(int level, const string &id) const;
private:
	static string m_OwnerTableName;

};

#endif // SSHPUBKEY_H
