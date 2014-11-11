/*
 * ostcommon.h
 *
 *  Created on: Jan 28, 2013
 *      Author: boom
 */

#ifndef OSTCOMMON_H_
#define OSTCOMMON_H_

#include <string>
#include <map>

using std::string;

const string OsInfoXmlName = "metainfo.xml";
const string OsCachePath = "var/oscache";
const string RepoOsListFileName = "oslist";
const string RepoInfoFileName = "repo.info";
const string RepoInfoContent = "ISPsystem OSTemplate";
const string RepoSeparator = "__";

struct OsMetaInfo {
	string Name;
	string Repo;
	string Type;
	StringList Support;
	string InstalledDate;
	string RepoDate;
	string Descr;
	bool Installed;
	bool CanBeUpdated;
	bool Compatible;
	bool CompatibleVersion;
	bool AutoupdateOn;
	bool AccessAdminOnly;
	bool SshPublicKey;
	bool SshPublicKeyBase64;
	string CompatibleVersionString;
	string InstallResult;
	OsMetaInfo() :
		CompatibleVersion(true),
		AutoupdateOn (false),
		AccessAdminOnly (false),
		SshPublicKey(false),
		SshPublicKeyBase64(false)
	{ }
};

class OSTInfo : public mgr_xml::XmlFile {
protected:
	StringMap m_Limit;
	StringVector m_ExternalUrls;
	StringMap m_Descriptions;
public:
	OSTInfo(const string& name);
	bool SshPublicKey;
	bool SshPublicKeyBase64;
	bool TempIPv4;
	bool VirtioDisk;
	bool VirtioNet;
	int RebootCount;
	string HddImage;
	string IllegalPasswordCharacters;
	string Image;
	string Initrd;
	string InstallCfg;
	string InstallDrive;
	string Kernel;
	string KernelCommand;
	string NfsShare;
	string ShareDir;
	string UpMemOnInstall;
	string DHCPLoader;
	string PXEConf;
	string HttpShareDir;
	string SambaDir;
	string Url;
	string OsName;
public:
	bool HasLimit(const string& name) const {
		return m_Limit.find(name) != m_Limit.end();
	}
	string Limit(const string& name) const
	try {
		return m_Limit.at(name);
	} catch (...) {
		return "";
	}
	bool HasDescription(const string& lang) const {
		return m_Descriptions.find(lang) != m_Descriptions.end();
	}
	string Description(const string& lang) const
	try {
		return m_Descriptions.at(lang);
	} catch (...) {
		return "";
	}
	StringVector ExternalUrl();
};


typedef std::map<string, OsMetaInfo> OsInfoMap;
#endif /* OSTCOMMON_H_ */
