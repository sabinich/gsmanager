#include "osmgr_internal.h"

#include <mgr/mgrlog.h>
#include <mgr/mgrdate.h>
#include <mgr/mgrxml.h>
#include <mgr/mgrrpc.h>
#include <mgr/mgrproc.h>
#include <api/xmlcache.h>
#include <mgr/mgrstr.h>
#include "ostcommon.h"
#include "osutils.h"
MODULE("osmgr");

#define ConcatPath3(a,b,c) mgr_file::ConcatPath((a),mgr_file::ConcatPath((b), (c)))

string StrMapToStr(const StringMap& data)
{
	string datastr;
	for (auto param = data.begin(); param != data.end(); param++) {
		datastr += param->first+"\t"+param->second+"\n";
	}
	return datastr;
}

void StrToStrMap(string str, StringMap& data)
{
	while(!str.empty()) {
		string key = str::GetWord(str,'\t');
		string val = str::GetWord(str,'\n');
		data[key] = val;
	}
}

void GetRepoList(StringMap & repos)
{
	string configName = "etc/" + isp_api::GetMgrName() + ".conf";
	ConfRef conf = OsRepositoryConfig::Config(configName, false);
	auto osRepoDS = conf.Get<OsRepositoryDataSet>();
	if (osRepoDS->Count() == 0) {
		repos[mgr_cf::GetParam("DefaultRepoName")] = mgr_cf::GetParam("DefaultRepoUrl");
	}
	ForEachRecord(osRepoDS) {
		repos[osRepoDS->Name] = osRepoDS->Location;
	}
}

void UpdateOsInfo()
{
	StringMap repos;
	GetRepoList(repos);
	string tmpOsCachePath = OsCachePath + ".new";
	if (mgr_file::Exists(tmpOsCachePath)) {
		mgr_file::Remove(tmpOsCachePath);
	}
	mgr_file::MkDir(tmpOsCachePath);
	
	ForEachI(repos, repo) {
		if (!mgr_file::Exists(tmpOsCachePath+"/"+repo->first))
			mgr_file::MkDir(tmpOsCachePath+"/"+repo->first);
        try {
			mgr_rpc::FetchFile(repo->second+"/"+RepoOsListFileName, tmpOsCachePath+"/"+repo->first+"/"+RepoOsListFileName+".new");
        } catch (mgr_err::Error &err) {
            Warning ("Failed to fetch '%s'", (repo->second+"/"+RepoOsListFileName).c_str());
            continue;
        }

		StringMap oldOsList;
		StringMap newOsList;
		StrToStrMap(str::Trim(mgr_file::Read(tmpOsCachePath+"/"+repo->first+"/"+RepoOsListFileName+".new")), newOsList);
		if (mgr_file::Exists(tmpOsCachePath+"/"+repo->first+"/"+RepoOsListFileName)) {
			StrToStrMap(str::Trim(mgr_file::Read(tmpOsCachePath+"/"+repo->first+"/"+RepoOsListFileName)), oldOsList);
		}
		
		ForEachI(newOsList, os) {
			try {
				mgr_date::DateTime newosdate(os->second);
				mgr_date::DateTime oldosdate(0);
				if (oldOsList.find(os->first) != oldOsList.end())
					oldosdate = mgr_date::DateTime(oldOsList[os->first]);

				if (newosdate > oldosdate) {//ОСь обновилась
					Debug("OS '%s' has update! Fetching...",os->first.c_str());
					mgr_rpc::FetchFile(repo->second+"/"+os->first+".xml", tmpOsCachePath+"/"+repo->first+"/"+os->first+".xml");
				}
			} catch (mgr_err::Error& e) {
				Warning("UpdateOsInfo failed '%s'", e.what());
			}
		}
		mgr_file::Move(tmpOsCachePath+"/"+repo->first+"/"+RepoOsListFileName+".new", tmpOsCachePath+"/"+repo->first+"/"+RepoOsListFileName);
	}
	mgr_file::Remove (OsCachePath);
	mgr_file::Move (tmpOsCachePath, OsCachePath);
}

class MgrVersion {
public:
	MgrVersion(string version) {
		auto erase_pos = version.find_first_not_of("0123456789.");
		if (erase_pos != string::npos) {
			version.erase(erase_pos);
		}

		m_str_version = version;
		m_version = str::Int(str::GetWord(version, '.')) * 10000 + str::Int(str::GetWord(version, '.')) * 100 + str::Int(str::GetWord(version, '.'));
	}

	operator bool() const { return !!m_version; }
	bool operator > (const MgrVersion &rhs) const {
		return m_version > rhs.m_version;
	}
	string Str() const {
		return m_str_version;
	}
protected:
	int m_version;
	string m_str_version;
};

bool FillOsInfo (mgr_xml:: XmlFile osmeta, OsMetaInfo &info, const bool from_installed_file = true)
{
	bool templateCompatible = false;
	auto supportNodes = osmeta.GetNodes("/doc/support/elem");
	ForEachI(supportNodes, elem) {
		info.Support.push_back(elem->Str());
		if (str::Lower(elem->Str()) == str::Lower(isp_api::GetMgrName())) {
			templateCompatible = true;
			if (!from_installed_file) {
				static const MgrVersion mgr_version(isp_api::ManagerInfo('v').c_str());
				if (mgr_version) {
					MgrVersion need_version(elem->GetProp("version"));
					if (need_version) {
						info.CompatibleVersionString = need_version.Str();
						if (need_version > mgr_version) {
							info.CompatibleVersion = false;
						}
					}
				}
			}
		}
	}
	info.Compatible = templateCompatible;
	info.Type = osmeta.GetNode("/doc/type").Str();
	info.Name = osmeta.GetNode("/doc/osname").Str();
	info.InstallResult = osmeta.GetNode("/doc/install_result").Str();
	auto sshpublickey = osmeta.GetNode("/doc/sshpublickey");
	if (sshpublickey) {
		info.SshPublicKey = (sshpublickey.Str() == "yes");
		info.SshPublicKeyBase64 = (sshpublickey.GetProp("base64") == "yes");
	}



	return !info.Name.empty();

}

bool OsInstalledGood (const OsMetaInfo &info)
{
    return str::Trim(info.InstallResult) == "0" || str::Lower(str::Trim(info.InstallResult)) == "ok";
}

string OsInstallResultMsg (const string & lang, const string & module, const OsMetaInfo &info)
{
	string msg_pfx = "ost_install_";
	string msg_sfx;
	if (OsInstalledGood(info)) {
		msg_sfx = "ok";
	} else {
		msg_sfx = info.InstallResult;
	}
	auto msgs = isp_api::GetMessages(lang, module);
	string msg = msgs.GetNode("//msg[@name='"+msg_pfx+msg_sfx+"']");
	StringMap params;
	params["__name__"] = info.Name;
	params["__repo__"] = info.Repo;
	return str::Replace (msg, params);
}



mgr_xml::Xml LoadTemplateXml (const string & osTemplatePath, const string & osTemplate)
{
	string metaFileName = ConcatPath3(osTemplatePath, osTemplate, OsInfoXmlName);
	return mgr_xml::XmlFile (metaFileName);
}
void SaveTemplateXml (mgr_xml::Xml xml, const string & osTemplatePath, const string & osTemplate)
{
	xml.Save(ConcatPath3(osTemplatePath,osTemplate, OsInfoXmlName), true);
}


void GetInstalledOsInfoByType(StringMap& osinfo, const string& type, bool skipRestricted)
{
	GetInstalledOsInfoByType(osinfo, type, mgr_cf::GetPath("ostemplatedir"), skipRestricted);
}

void GetInstalledOsInfo(StringMap & osinfo, bool skipRestricted)
{
	GetInstalledOsInfo(osinfo, mgr_cf::GetPath("ostemplatedir"), skipRestricted);
}
void GetOsInfo(OsInfoMap & info)
{
	GetOsInfo(info, mgr_cf::GetPath("ostemplatedir"));
}
void InstallOsTemplate(const string &os)
{
	InstallOsTemplate(os, mgr_cf::GetPath("ostemplatedir"));
}
void RemoveOsTemplate(const string & os)
{
	RemoveOsTemplate(os, mgr_cf::GetPath("ostemplatedir"));
}
mgr_xml::Xml LoadTemplateXml (const string & ostPath)
{
	return LoadTemplateXml(mgr_cf::GetPath("ostemplatedir"), ostPath);
}
void SetInstallResult (const string & osTemplate, const string & result)
{
	SetInstallResult(mgr_cf::GetPath("ostemplatedir"), osTemplate, result);
}
void SaveTemplateXml (mgr_xml::Xml xml, const string & ostPath)
{
	SaveTemplateXml(xml, mgr_cf::GetPath("ostemplatedir"), ostPath);
}

void SetInstallResult (const string & osTemplatePath, const string & osTemplate, const string & result)
{
	mgr_xml::Xml osInfoXml = LoadTemplateXml (osTemplatePath, osTemplate);
	osInfoXml.RemoveNodes("/doc/install_result");
	osInfoXml.GetRoot().AppendChild("install_result", result);
	SaveTemplateXml(osInfoXml, osTemplatePath, osTemplate);
}

bool IsTemplateRestricted (const string & templateDir) {
	return mgr_file::Exists(templateDir + "/restrict");
}

void GetInstalledOsInfo(StringMap & osinfo, const string & osTemplatePath, bool skipRestricted)
{
	Debug ("search installed os templates in %s. Skip restricted template: %d", osTemplatePath.c_str(), skipRestricted);
	if (!mgr_file::Exists(osTemplatePath)) return;
	mgr_file::Dir dir(osTemplatePath);
	while(dir.Read()) {
		if (dir.Info().IsDir() && mgr_file::Exists(dir.FullName()+"/"+OsInfoXmlName)) {
			try {
				mgr_xml::XmlFile meta(dir.FullName()+"/"+OsInfoXmlName);
				OsMetaInfo info;
				if (!FillOsInfo (meta, info)) continue;
				if (!OsInstalledGood(info)) continue;
				if (!info.Compatible) continue;
				if (IsTemplateRestricted(dir.FullName()) && skipRestricted) continue;
				osinfo[dir.name()] = info.Name;
			} catch (...) {
				Debug ("Skip bad xml");
				continue;
			}
		}
	}
}

void GetInstalledOsInfoByType(StringMap& osinfo, const string& type, const string& osTemplatePath, bool skipRestricted)
{
	if (!mgr_file::Exists(osTemplatePath)) return;
	mgr_file::Dir dir(osTemplatePath);
	while(dir.Read()) {
		if (dir.Info().IsDir() && mgr_file::Exists(dir.FullName()+"/"+OsInfoXmlName)) {
			mgr_xml::XmlFile meta(dir.FullName()+"/"+OsInfoXmlName);
			OsMetaInfo info;
			if (!FillOsInfo (meta, info)) continue;
			if (!OsInstalledGood(info)) continue;
			if (!info.Compatible) continue;
			if (info.Type != type) continue;
			if (IsTemplateRestricted(dir.FullName()) && skipRestricted) continue;
			osinfo[dir.name()] = info.Name;
		}
	}
}

OsMetaInfo GetOsInfoFromCache(const string& repo, const string& osTemplateName) {
	const string filename = OsCachePath + "/" + repo + "/" + osTemplateName +".xml";
	OsMetaInfo info;
	info.Repo = repo;
	info.CanBeUpdated = false;
	info.Name = osTemplateName;
	try {
		if (mgr_file::Exists(filename)) {
			mgr_xml::XmlFile osmeta(filename);
			FillOsInfo(osmeta, info, false);
		}
	} catch (...) {
		Warning ("Error parsing content for %s", osTemplateName.c_str());
	}
	return info;
}

void GetOsInfo(OsInfoMap& info, const string & osTemplatePath)
{
	STrace();
	StringMap OsList;
	Debug ("check os cache path: %s", OsCachePath.c_str ());
	if (!mgr_file::Exists(OsCachePath)) {
		Debug ("%s does not exists", OsCachePath.c_str ());
		return;
	}
	mgr_file::Dir oscache(OsCachePath);
	while (oscache.Read()) {
		Debug ("found %s", oscache.FullName ().c_str ());
		OsList.clear();
		Debug ("oslist %s", (mgr_file::Exists (oscache.FullName()+"/"+RepoOsListFileName) ? "exists" : "not exists"));
		if (oscache.Info().IsDir() && mgr_file::Exists(oscache.FullName()+"/"+RepoOsListFileName)) {
			StrToStrMap(str::Trim(mgr_file::Read(oscache.FullName()+"/"+RepoOsListFileName)), OsList);

			ForEachI(OsList, os) {
				string metafile;
				try {
					string osid = oscache.name() + RepoSeparator + os->first;
					metafile = mgr_file::ConcatPath (osTemplatePath, osid, OsInfoXmlName);
					OsMetaInfo osMetaInfo = GetOsInfoFromCache(oscache.name(), os->first);
					osMetaInfo.RepoDate = os->second;
					osMetaInfo.Installed = false;
					if (mgr_file::Exists(metafile)) {
						mgr_xml::XmlFile metaxml(metafile);
						osMetaInfo.InstallResult = metaxml.GetNode("/doc/install_result").Str();
						osMetaInfo.Installed = osMetaInfo.InstallResult == OS_INSTALL_RESULT_OK;
					}
					info [osid] = osMetaInfo;
					Debug ("os: %s, installed: %s", osid.c_str (), info[osid].Installed ? "true" : "false");
				} catch (...) {
					// Parse error
					Warning("failed to parse %s", metafile.c_str ());
					continue;
				}
			}
		}
	}
	
	StringMap installedos;
	GetInstalledOsInfo(installedos, osTemplatePath);
	
	ForEachI(installedos, os) {
        Debug ("Installed :%s (%s)", os->first.c_str(), os->second.c_str());
		try {
			mgr_xml::XmlFile osmeta(osTemplatePath+"/"+os->first+"/"+OsInfoXmlName);
			if (info.find(os->first) != info.end()) {

				info[os->first].Installed = true;
				mgr_date::DateTime instdate(0);
				auto node = osmeta.GetRoot().FindNode("date");
				if (node) {
					instdate = mgr_date::DateTime(node.Str());
					info[os->first].InstalledDate = node.Str();
				}
				mgr_date::DateTime repodate(info[os->first].RepoDate);
				if (repodate > instdate) {
					info[os->first].CanBeUpdated = true;
				}
			} else {
				info[os->first].Installed = true;
				info[os->first].CanBeUpdated = false;
				time_t dirUpdateDate = mgr_file::Info(osTemplatePath+"/"+os->first).Time();
				info[os->first].RepoDate = mgr_date::DateTime(dirUpdateDate).AsDateTime();
			}
			info [os->first].AutoupdateOn = mgr_file::Exists(osTemplatePath+"/"+os->first+"/autoupdate");
			info [os->first].AccessAdminOnly = mgr_file::Exists(osTemplatePath+"/"+os->first+"/restrict");
			FillOsInfo(osmeta, info[os->first]);
		} catch (mgr_err::Error& e) {
			Warning("GetOsInfo failed for '%s/%s' '%s'", os->first.c_str(), OsInfoXmlName.c_str(), e.what());
		}
	}
}

string OsKey2OsName (string osKey, bool showRepo)
{
    if (osKey.find (RepoSeparator) == string::npos) {
        return osKey;
    } else {
        string repo = str::GetWord (osKey, RepoSeparator);
        if (showRepo){
            return str::Format ("%s (%s)", osKey.c_str (), repo.c_str ());
        } else {
            return osKey.c_str ();
        }
    }
}

void InstallOsTemplate(string os, const string & osTemplatePath)
{
	Debug ("Install os template '%s' into %s", os.c_str(), osTemplatePath.c_str());
	string osdir = osTemplatePath + "/" + os;
	
	string repo = str::GetWord(os, RepoSeparator);
	StringMap repolist;
	GetRepoList(repolist);
	
	if (repolist.find(repo) == repolist.end())
		throw mgr_err::Missed("no_such_repo",repo);
	
	string fetchurl  = repolist[repo] + "/" + os + ".tar.gz";
	Debug ("Fetch from '%s'", fetchurl.c_str());
	mgr_rpc::FetchFile(fetchurl, OsCachePath+"/"+repo+"/"+os+".tar.gz");
	Debug ("Fetch done");
	if (!mgr_file::Exists(osTemplatePath))
		mgr_file::MkDir(osTemplatePath);
	
	if (!mgr_file::Exists(osdir))
		mgr_file::MkDir(osdir);
	
	mgr_proc::Execute untar("tar xzf "+OsCachePath+"/"+repo+"/"+os+".tar.gz" + " -C " + osdir);
	if (untar.Result() != 0)
		LogError("Failed to extract os image");
	
	mgr_file::CopyFile(OsCachePath+"/"+repo+"/"+os+".xml", osdir+"/"+OsInfoXmlName);
	mgr_xml::XmlFile meta(osdir+"/"+OsInfoXmlName);
	mgr_date::DateTime now(time(0));
	meta.GetRoot().AppendChild("date",now.AsDateTime());
	meta.Save(osdir+"/"+OsInfoXmlName,true);
}

void RemoveOsTemplate(const string & os, const string & osTemplatePath)
{
	string tplFileName = mgr_file::ConcatPath(osTemplatePath, os);
	if (mgr_file::Exists(tplFileName)) {
		Trace ("Removing DIR: %s", tplFileName.c_str());
		mgr_file::Remove(tplFileName);
	}
}

void UpdateOs(const string& os)
{
	
}

StringVector OSTInfo::ExternalUrl()
{
	return m_ExternalUrls;
}

OSTInfo::OSTInfo(const string& name) try :
	mgr_xml::XmlFile(mgr_file::ConcatNPath(3, mgr_cf::GetPath("ostemplatedir").c_str(), name.c_str(), OsInfoXmlName.c_str())),
	SshPublicKey (false),
	SshPublicKeyBase64 (false),
	TempIPv4 (false),
	VirtioDisk (false),
	VirtioNet (false),
	RebootCount (1)
{
	mgr_xml::XmlNode child = GetRoot().FirstChild();
	if (!child) throw mgr_err::Error("metainfo_no_data");
	const string imageDir = mgr_file::ConcatPath(mgr_cf::GetPath("ostemplatedir"), name);
	while(child) {
		const string name = child.Name();
		if (name == "image") {
			Image = mgr_file::ConcatPath(imageDir, child.Str());
		} else if (name == "kernel"){
			Kernel = mgr_file::ConcatPath(imageDir, child.Str());
		} else if (name == "initrd"){
			Initrd = mgr_file::ConcatPath(imageDir, child.Str());
		} else if (name == "kernelcommand"){
			KernelCommand = child.Str();
		} else if (name == "installcfg"){
			InstallCfg = mgr_file::ConcatPath(imageDir, child.Str());
		} else if (name == "installdrive"){
			InstallDrive = mgr_file::ConcatPath(imageDir, child.Str());
		} else if (name == "rebootcount"){
			RebootCount = str::Int(child.Str());
		} else if (name == "nfsshare"){
			NfsShare = mgr_file::ConcatPath(imageDir, child.Str());
		} else if (name == "hddimage"){
			HddImage = mgr_file::ConcatPath(imageDir, child.Str());
		} else if (name == "tempipv4"){
			TempIPv4 = child.Str() == "yes";
		} else if (name == "sharedir"){
			ShareDir = mgr_file::ConcatPath(imageDir, child.Str());
		} else if (name == "illegal_password_characters") {
			IllegalPasswordCharacters = child.Str();
		} else if (name == "up_mem_on_install") {
			UpMemOnInstall = child.Str();
		} else if (name == "sshpublickey") {
			SshPublicKey = child.Str() == "yes";
			SshPublicKeyBase64 = child.GetProp("base64") == "yes";
		} else if (name == "virtiodisk") {
			VirtioDisk = child.Str() == "yes";
		} else if (name == "virtionet") {
			VirtioNet = child.Str() == "yes";
		} else if (name == "loader") {
			DHCPLoader = child.Str();
		} else if (name == "pxelinuxcfg") {
			PXEConf = child.Str();
		} else if (name == "samba") {
			SambaDir = child.Str();
		} else if (name == "sharedir") {
			HttpShareDir = child.Str();
		} else if (name == "osname") {
			OsName = child.Str();
		}
		child = child.Next();
	}

	auto limitNodes = GetNodes("/doc/limit/elem");
	ForEachI(limitNodes, elem) {
		const string name = elem->GetProp("name");
		const string val = elem->Str();
		m_Limit[name] = val;
	}
	auto externals = GetNodes ("/doc/externals/elem/url");
	ForEachI(externals, url) {
		m_ExternalUrls.push_back((*url).Str());
	}

	auto desriptions = GetNodes("/doc/description");
	ForEachI(desriptions, elem) {
		const string lang = elem->GetProp("lang");
		const string val = elem->Str();
		m_Descriptions[lang] = val;
	}

} catch (...) {
	throw mgr_err::Error("metainfo_parse_error");
}

