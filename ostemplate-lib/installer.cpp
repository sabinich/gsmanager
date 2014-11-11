#include "installer.h"
#include <classes.h>
#include <mgr/mgrssh.h>
#include <mgr/mgrerr.h>
#include <mgr/mgrlog.h>
#include "ssh_utils.h"
#include <mgr/mgrfile.h>
#include <mgr/mgrstr.h>
MODULE("installer");

namespace isp_repo
{

namespace
{
string GetEpelRpmUrl (mgr_rpc::SSH &connection)
{
	string osver = ssh_utils::os_info::Version(connection);
	const string osverMaj = str::GetWord (osver, ".");
	Debug("OS release: '%s'", osverMaj.c_str());
	string repo_url = "http://dl.fedoraproject.org/pub/epel/";
	if (osverMaj == "5")
		repo_url += "5/x86_64/epel-release-5-4.noarch.rpm";
	else if (osverMaj == "6")
		repo_url += "6/x86_64/epel-release-6-8.noarch.rpm";
	else
		throw mgr_err::Error("os_release_repo", "epel");
	return repo_url;
}

}

void InstallByRpmUrl (mgr_rpc::SSH &connection, const string & rpmUrl)
{
	if (R_OSFAMILY_REDHAT(connection)) {
		if (connection.Execute("rpm -Uvh " + rpmUrl).Result() != EXIT_SUCCESS) {
			throw mgr_err::Error("install_url", rpmUrl);
		}
	}
}

void InitRepo(mgr_rpc::SSH &connection, const string & remoteTmpDir)
{
	bool is_redhat = R_OSFAMILY_REDHAT(connection);
	if (!is_redhat && !R_OSFAMILY_DEBIAN(connection)) {
		throw mgr_err::Error ("not_implemented");
	}

	if (is_redhat) {
		// add epel
		const string epelFile = mgr_file::ConcatPath(remoteTmpDir, "epel.rpm");
		ssh_utils::file::Remove(connection, epelFile);
		if (connection.Execute("rpm -qa | grep epel-release").Result() == EXIT_SUCCESS) {
			Debug("repository 'epel' installed, removing it");
			connection.Execute("yum -qy remove epel-release").Result();
		}
		const string epelRpmUrl = GetEpelRpmUrl(connection);
		if (connection.Execute("wget " + epelRpmUrl + " -O " + epelFile).Result() != EXIT_SUCCESS)
			throw mgr_err::Error("fetch_repository", epelRpmUrl);
		if (connection.Execute("rpm -Uvh " + epelFile).Result() != EXIT_SUCCESS)
			throw mgr_err::Error("install_repository", "epel");
	}
#ifdef NOEXTERNAL
	string isprepo = is_redhat ? "/etc/yum.repos.d/ispsystem.repo" : "/etc/apt/sources.list.d/ispsystem.list";
	try {
		ssh_utils::file::CopyTo(isprepo, connection, isprepo);
	} catch (...) {
		throw mgr_err::Error("install_repository", isprepo);
	}
#endif
	if (connection.Execute(is_redhat ? "yum -y update" : "apt-get -y update").Result() != EXIT_SUCCESS)
		throw mgr_err::Error(is_redhat ? "yum_update" : "apt_get_update");
}


void InstallPackage(mgr_rpc::SSH &connection, const string &name)
{
	if (R_OSFAMILY_REDHAT(connection)) {
		if (connection.Execute("yum -y install " + name).Result() != EXIT_SUCCESS)
			throw mgr_err::Error("install_" + name);
	} else if (R_OSFAMILY_DEBIAN(connection)) {
		if (connection.Execute("apt-get -yq install " + name).Result() != EXIT_SUCCESS)
			throw mgr_err::Error("install_" + name);
	}
}

}





