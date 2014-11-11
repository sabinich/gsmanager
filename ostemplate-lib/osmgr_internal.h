/*
 * osmgr_internal.h
 *
 *  Created on: 10.06.2013
 *      Author: boom
 */

#ifndef OSMGR_INTERNAL_H_
#define OSMGR_INTERNAL_H_

#include <api/stdconfig.h>
#include <parser/conf_cache.h>
#include "ostcommon.h"

bool OsInstalledGood (const OsMetaInfo &info);
mgr_xml::Xml LoadTemplateXml (const string & osTemplatePath, const string & ostPath);
mgr_xml::Xml LoadTemplateXml (const string & ostPath);
OsMetaInfo GetOsInfoFromCache(const string& repo, const string& osTemplateName);
string OsInstallResultMsg (const string & lang, const string & module, const OsMetaInfo &info);
//string OsKey2OsName (string osKey, bool showRepo = false);
//void GetInstalledOsInfo(StringMap & osinfo, bool skipRestricted = false);
void GetInstalledOsInfo(StringMap & osinfo, const string & osTemplatePath, bool skipRestricted = false);
void GetInstalledOsInfoByType(StringMap & osinfo, const string & type, const string & osTemplatePath, bool skipRestricted = false);
void GetInstalledOsInfoByType(StringMap & osinfo, const string& type, bool skipRestricted = false);
void GetOsInfo(OsInfoMap & info, const string & osTemplatePath);
void GetOsInfo(OsInfoMap & info);
void InstallOsTemplate(string os, const string & osTemplatePath);
void RemoveOsTemplate(const string & os, const string & osTemplatePath);
void RemoveOsTemplate(const string & os);
void SaveTemplateXml (mgr_xml::Xml xml, const string & osTemplatePath, const string & ostPath);
void SaveTemplateXml (mgr_xml::Xml xml, const string & ostPath);
void SetInstallResult (const string & osTemplate, const string & result);
void SetInstallResult (const string & osTemplatePath, const string & osTemplate, const string & result);
void UpdateOs(const string & os);
void UpdateOsInfo();

class OsRepositoryDataSet :public LineDataSet
{
private:
	LINEPREFIX("Repo");
public:
	dd::Field Name;
	dd::Field Location;
	OsRepositoryDataSet(): Name (this, "name"), Location(this,"location") {};
};

class OsRepositoryConfig : public MgrCachedConfig<OsRepositoryConfig> {
public:
	OsRepositoryConfig(): MgrCachedConfig<OsRepositoryConfig>(true, mgr_cf::MgrConf::Instance().Name())
	{
		Add<OsRepositoryDataSet>();
	}
	static ConfRef GetConf() {
		string configName = "etc/" + isp_api::GetMgrName() + ".conf";
		return OsRepositoryConfig::Config(configName, false);
	}
};

#endif /* OSMGR_INTERNAL_H_ */
