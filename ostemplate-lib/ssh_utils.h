#ifndef SSH_UTILS_H
#define SSH_UTILS_H

#include <classes.h>
#include <mgr/mgrssh.h>
#include <mgr/mgrenv.h>

#define R_OSFAMILY_REDHAT(connection) ((ssh_utils::os_info::Type((connection)) & BASE_OSFAMILY_REDHAT) == BASE_OSFAMILY_REDHAT)
#define R_OSFAMILY_DEBIAN(connection) ((ssh_utils::os_info::Type((connection)) & BASE_OSFAMILY_DEBIAN) == BASE_OSFAMILY_DEBIAN)


namespace ssh_utils
{

namespace os_info
{

mgr_env::OsTypeEnum Type(mgr_rpc::SSH & connection);
string Version(mgr_rpc::SSH &connection);
string Arch(mgr_rpc::SSH & connection);
string GetOs(mgr_rpc::SSH &connection);

} //namespace os_info
namespace file
{

class DownloadWatcher
{
public:
	DownloadWatcher(size_t streamLength);

	virtual void OnGetData(char *buf, size_t len);
	virtual bool Finished();
	virtual ~DownloadWatcher();
private:
	size_t m_ReadenTotal;
	size_t m_ReadenTotalPreviousPercent;
	size_t m_Length;

};

string Read (mgr_rpc::SSH & connection, const string & fileName);
void Write (mgr_rpc::SSH & connection, const string & fileName, const string & data);
bool Exists (mgr_rpc::SSH & connection, const string & fileName, string type = "e");
void MkDir (mgr_rpc::SSH & connection, const string & dirName);
void Remove (mgr_rpc::SSH & connection, const string & fileName);
size_t Size(mgr_rpc::SSH & connection, const string & fileName);
void CopyTo (const string & fileName, mgr_rpc::SSH & connection, const string & remoteFileName);

}//namespace file


} // namespace ssh_utils


#endif // SSH_UTILS_H
