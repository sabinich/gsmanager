#include "dbobject.h"
#include <api/module.h>
#include <mgr/mgrlog.h>
#include <api/dbaction.h>
#include <mgr/mgrcrypto.h>
#include <sys/stat.h>
#include <mgr/mgrhash.h>
#include "defines.h"
MODULE("gsmgr");
using namespace isp_api;


class aAfterInstall : public Event {
public:
	aAfterInstall() :
		Event("afterinstall", "gsmgrafter") {

	}
protected:
	void AfterExecute(Session &ses) const {
		//Сгенерить shh ключ

		if (!mgr_file::Exists(mgr_cf::GetParam(ConfsshPublicKey)) || !mgr_file::Exists(mgr_cf::GetParam(ConfsshPrivatKey))) {
			Trace("generate ssh keys");
			mgr_crypto::RsaPrivateKey key(2048);
			const string public_key = mgr_crypto::ssh_public::Encode(key);
			const string private_key = mgr_crypto::pem_private::Encode(key);

			mgr_file::Write(mgr_cf::GetParam(ConfsshPublicKey), public_key);
			mgr_file::Write(mgr_cf::GetParam(ConfsshPrivatKey), private_key);
			chmod(mgr_cf::GetParam(ConfsshPrivatKey).c_str(), 0600);
		}

		if (!isp_api::UserExists("admin")) {
			string rnd_passw = str::hex::Encode(str::Random(10));
			InternalCall(ses, "user.edit", "sok=ok&level=" + str::Str(lvAdmin) + "&"
						 "name=admin&"
						 "passwd=" + str::url::Encode(rnd_passw));
		}
	}

};

MODULE_INIT(install, "gsmgr") {
	STrace();
	new aAfterInstall();
}

