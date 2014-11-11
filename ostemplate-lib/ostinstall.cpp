#include <mgr/mgrlog.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrclient.h>
#include <mgr/mgrenv.h>
#include <ispbin.h>
#include <string.h>
#include <iostream>
#include <mgr/mgrstr.h>
#include <mgr/mgrrpc.h>
#include <mgr/mgrdate.h>
#include "ostcommon.h"
#include "objectlock.h"
MODULE("osmgr");

void PrintExecutonString(int argc, char **argv)
{
	string result;
	for (int i = 0; i < argc; ++i) {
		if (!result.empty()) {
			result.append(" ");
		}
		result.append(argv[i]);
	}
	Debug ("Exec : '%s'", result.c_str());
}
namespace
{
void GetRepoList(const string & mgrName, StringMap & reposOut)
{
	mgr_client::Local repoQuery(mgrName, "ost_install");

	auto repoListResult = repoQuery.Query("out=xml&func=repo");
	ForEachI (repoListResult.elems(), elemNode) {
		string repoName = elemNode->FindNode("name").Str();
		string repoLocation = elemNode->FindNode("location").Str();
		Debug ("Add repo %s (%s)", repoName.c_str(), repoLocation.c_str());
		reposOut[elemNode->FindNode("name").Str()] = elemNode->FindNode("location").Str();
	}
}

void SplitOslist(string str, StringMap& data)
{
	while(!str.empty()) {
		string key = str::GetWord(str,'\t');
		string val = str::GetWord(str,'\n');
		data[key] = val;
	}
}


string GetUpdateTime (const string &repoName, const string &osName)
{
	string fileName = mgr_file::ConcatNPath(3, OsCachePath.c_str(), repoName.c_str(), RepoOsListFileName.c_str());
	StringMap osList;
	SplitOslist(str::Trim(mgr_file::Read(fileName)), osList);
	return osList[osName];
}

/*
<doc>
	<osname>Debian-6.0-x86_64</osname>
	<support>
		<elem>VEmgr</elem>
	</support>
	<ignore-content>yes</ignore-content>
	<externals>
		<elem>
			<url>http://download.openvz.org/template/precreated/debian-6.0-x86_64.tar.gz</url>
		</elem>
	<externals>
</doc>
 */
//                                             ISPsystem_OSNAME     /nfsshare
void InstallOsTemplate(StringMap & repolist, string ostemplateId, const string & ostemplatePath, const string & tmpDirLocation)
{
	Debug ("Install os template '%s' into %s", ostemplateId.c_str(), ostemplatePath.c_str());
	string targetOsTemplateDir = mgr_file::ConcatPath(ostemplatePath, ostemplateId); // /nfsshare/ISPsystem__OSNAME
	string repoName = str::GetWord(ostemplateId, RepoSeparator);
	if (repolist.find(repoName) == repolist.end()) {
		Debug("Local repo. Exit.");
		return;
	}
	string ostemplateName = ostemplateId;

	string ostemplateUpdateTime = GetUpdateTime(repoName, ostemplateName);
	string tplContentUrl    = repolist[repoName] + "/" + ostemplateName + ".tar.gz";
	mgr_file::TmpDir tmpDir(mgr_file::ConcatPath(tmpDirLocation, "ost"));
	string tplContentTmpTarget = mgr_file::ConcatPath(tmpDir, ostemplateName) + ".tar.gz";
	string metainfoFileSrc  = mgr_file::ConcatNPath(3, OsCachePath.c_str(), repoName.c_str(), ostemplateName.c_str()) + ".xml";
	string metainfoFileDest = mgr_file::ConcatPath (targetOsTemplateDir, OsInfoXmlName);

	if (!mgr_file::Exists(ostemplatePath))
		mgr_file::MkDir(ostemplatePath);

	if (!mgr_file::Exists(targetOsTemplateDir))
		mgr_file::MkDir(targetOsTemplateDir);

	mgr_file::CopyFile(metainfoFileSrc, metainfoFileDest);
	mgr_xml::XmlFile metadata(metainfoFileDest);
	bool ignoreContent = metadata.GetNode("/doc/ignore-content").Str() == "yes";
	if (!ignoreContent){
		if (mgr_rpc::FetchFile(tplContentUrl, tplContentTmpTarget)) {
			mgr_proc::Execute untar("tar xzf " + tplContentTmpTarget + " -C " + targetOsTemplateDir);
			if (untar.Result() != 0) {
				LogError("Failed to extract os image");
				throw mgr_err::Error ("tar_fail", "", tplContentTmpTarget);
			}
		} else {
			throw mgr_err::Error ("fetch fail", "", tplContentUrl);
		}
	}
	mgr_xml::XPath externalElems = metadata.GetNodes("//externals/elem");
	ForEachI (externalElems, elem) {
		string elemUrl = elem->FindNode("url").Str();
		if (elemUrl.empty()) continue;
		if (!mgr_rpc::FetchFile(elemUrl, mgr_file::ConcatPath(targetOsTemplateDir, mgr_file::Name(elemUrl)))) {
			throw mgr_err::Error ("fetch fail", "", elemUrl);
		}
	}
	metadata.GetRoot().AppendChild("date", ostemplateUpdateTime);
	metadata.Save(metainfoFileDest);
}
}

bool WaitForLock (mgr_thread::ObjectLock<int> & lock, int timeoutSec=5)
{
	while (timeoutSec --) {
		if (lock.TryLock ())
			return true;
		Debug ("Wait for lock...");
		mgr_proc::Sleep (1000);
	}
	return false;
}

int ISP_MAIN(int argc, char *argv[]) {
	mgr_log::Init("ost_install", mgr_log::L_DEBUG);
	PrintExecutonString (argc, argv);
	if (argc < 4) {
		Warning ("usage: %s manager_name target_path template_name [tmp dir]", argv[0]);
		std::cerr << "usage: " << argv[0] << "manager_name target_path template_name [tmp dir]" << std::endl;
		return 1;
	}
	string elid = argv[1];
	string templatePath = argv[2];
	string mgrName = argv[3];
	string tmpDir = (argc > 4) ? string(argv[4]) : mgr_file::ConcatPath(templatePath, "tmp");
	string ostemplateId = elid;
	string repoName = str::GetWord(ostemplateId, RepoSeparator);
	try {
		StringMap repolist;
		{
			mgr_thread::ObjectLock<int> lock("repo_" + repoName);
			if (!WaitForLock (lock, 10)) {
				throw mgr_err::Error("repo_locked").add_param("repo", repoName);
			}
			GetRepoList(mgrName, repolist);
		}
		if (argc > 4) {
			tmpDir = argv[4];
		}
		if (!mgr_file::Exists(tmpDir)) {
			mgr_file::MkDir(tmpDir);
		}
		InstallOsTemplate (repolist, elid, templatePath, tmpDir);
		mgr_client::Local osmgr_done(mgrName, "ostinstall");
		osmgr_done.Query("out=xml&func=osmgr.done&elid=" + str::url::Encode(elid) + "&result=ok");
		osmgr_done.Query("out=xml&func=osmgr.fetch");
		if (!mgr_env::GetEnv("MGR_LT_PID").empty())
			osmgr_done.Query("out=xml&func=longtask.finish&elid=" + str::url::Encode(mgr_env::GetEnv("MGR_LT_PID")) + "&status=ok");
		osmgr_done.Query("func=osmgr.afterinstall");
	} catch (std::exception &err) {
		mgr_client::Local osmgr_done(mgrName, "ostinstall");
		osmgr_done.Query("out=xml&func=osmgr.done&elid=" + str::url::Encode(elid) + "&result=failed");
		osmgr_done.Query("out=xml&func=osmgr.fetch");
		if (!mgr_env::GetEnv("MGR_LT_PID").empty())
			osmgr_done.Query("out=xml&func=longtask.finish&elid=" + str::url::Encode(mgr_env::GetEnv("MGR_LT_PID")) + "&status=ERR");
		throw;
	}

	return 0;

}
