#ifndef RPMINSTALLER_H
#define RPMINSTALLER_H

#include <mgr/mgrssh.h>
#include <mgr/mgrenv.h>


namespace isp_repo
{

void InitRepo(mgr_rpc::SSH & connection, const string &remoteTmpDir="/tmp");
void InstallPackage (mgr_rpc::SSH & connection, const string & name);
void InstallByRpmUrl (mgr_rpc::SSH &connection, const string & rpmUrl);

}

#endif // RPMINSTALLER_H
