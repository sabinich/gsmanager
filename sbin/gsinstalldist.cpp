#include <mgr/mgrlog.h>
#include <mgr/mgrjob.h>
#include <mgr/mgrfile.h>
#include <mgr/mgrxml.h>
#include <mgr/mgrrpc.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrdate.h>
#include <mgr/mgrclient.h>
#include <ispbin.h>
#include <algorithm>
#include <vector>
#include "cmdarg.h"
#include "../main/defines.h"

#define VER							"0.0.1"

MODULE (CMD_INSTALL_DIST);

#define INIT_LOG(path)			Log::Init(path)
#define LOG_ERROR(file)			Log::Error(file)
#define LOG_ERROR_M(file, msg)	Log::Error(file, msg)
#define LOG_OK(file)			Log::Ok(file)
#define LOG_OK_M(file, msg)		Log::Ok(file, msg)

class Log {
public:
	Log() {}

	static void Ok(const string& filename, const string& message = "") {
		SINGLETON(Log).AddNode(filename, "Ok", message);
	}
	static void Error(const string& filename, const string& message = "") {
		SINGLETON(Log).AddNode(filename, "error", message);
	}
	static void Init(const string& initial_path) {
		SINGLETON(Log).m_filename = mgr_file::ConcatPath(initial_path, "install.xml");
	}
private:
	Log(const Log&);
	void operator=(const Log&);

	void AddNode(const string& filename, const string& status, const string& message = "") {
		if (m_filename.empty())
			throw mgr_err::Error("log_not_initialized");
		auto node = m_xml.GetRoot().AppendChild("elem").SetProp("name", filename)
		        .SetProp("date", mgr_date::DateTime());
		node.AppendChild("status", status);
		if (!message.empty())
			node.AppendChild("msg", message);
		m_xml.Save(m_filename, true);
	}

	mgr_xml::Xml m_xml;
	string m_filename;
};

class GameInfo {
public:
	GameInfo(const string& xml_filename, const string& gs_path) {
		mgr_xml::XmlFile xml_game(xml_filename);
		//main info
		auto node = xml_game.GetNode("/game");
		if (!node)
			throw mgr_err::Error("gs_bad_xml", "", xml_filename);
		Name = node.GetProp("name");
		if (Name.empty())
			throw mgr_err::Error("gs_bad_xml", "gamename", xml_filename);
//		string name = mgr_file::Name(xml_filename);
		Path = gs_path;//mgr_file::ConcatPath(gs_path, str::GetWord(name, '.'));
		//versions
		auto versions = xml_game.GetNodes("/game/version");
		ForEachI(versions, ver) {
			Version version;
			version.Name = ver->GetProp("name");
			if (version.Name.empty())
				throw mgr_err::Error("gs_bad_xml", "vername", xml_filename).add_param("game", Name);
			node = ver->FindNode("url");
			version.Url = node.Str();
			if (version.Url.empty())
				throw mgr_err::Error("gs_bad_xml", "url", xml_filename).add_param("game", Name);
			node = ver->FindNode("info");
			version.Info = node.Str();

			string ver_path = mgr_file::ConcatPath(Path, version.Name);
			version.Path = mgr_file::ConcatPath(ver_path, "dist");
			//plugins
			auto plugins = xml_game.GetNodes("/game/version[@name='" + version.Name + "']/plugin");
			ForEachI(plugins, plug) {
				Plugin pl;
				pl.Name = plug->GetProp("name");
				if (pl.Name.empty())
					throw mgr_err::Error("gs_bad_xml", "pluginname", xml_filename)
				        .add_param("game", Name).add_param("ver", version.Name);
				node = plug->FindNode("url");
				pl.Url = node.Str();
				if (pl.Url.empty()) {
					LOG_ERROR_M(mgr_file::Name(xml_filename), str::Format("empty plugin '%s' url", pl.Name.c_str()));
					continue;
				}
				node = plug->FindNode("info");
				pl.Info = node.Str();

				pl.Path = mgr_file::ConcatNPath(3, ver_path.c_str(), "plugins", pl.Name.c_str());

				version.Plugins.push_back(pl);
			}
			Versions.push_back(version);
		}
	}

	string Name;
	string Path;

	class ItemInfo {
	public:
		string Name;
		string Url;
		string Info;
		string Path;
	};

	class Plugin : public ItemInfo {
	};

	class Version : public ItemInfo {
	public:
		std::vector<Plugin> Plugins;
	};

	std::vector<Version> Versions;
};

bool Convert(const string& filename) {
	//must be installed 7z, unrar-free, tar, file
	//supported archive types: tar, 7z, rar, zip, gzip, bzip2
	StringList arch_list;
	arch_list.push_back("tar");
	arch_list.push_back("7z");
	arch_list.push_back("rar");
	arch_list.push_back("zip");
	arch_list.push_back("gzip");
	arch_list.push_back("bzip2");

	string source_name = mgr_file::Name(filename);
	string files_to_pack = source_name;
	string source_path = mgr_file::Path(filename);
	string arch_type = str::Lower(str::RGetWord(source_name, '.'));
	string target_filename = mgr_file::ConcatPath(source_path, "dist.tgz");
	string chdir_cmd = "cd " + mgr_proc::Escape(source_path) + " && ";

	if (arch_type == "gz") {
		string tmp = source_name;
		if (str::RGetWord(tmp, '.') == "tar") {//tar.gz archive
			arch_type = "tgz";
		}
	}
	if (arch_type == "tgz") {
		mgr_file::Move(filename, target_filename);
		return true;
	}
	auto it = find(arch_list.begin(), arch_list.end(), arch_type);
	if (it == arch_list.end())
		arch_type = "";

	if (!arch_type.empty()) {
		string tmp_dir = mgr_file::Tmp("/tmp/" + source_name).Str();
		chdir_cmd = "cd " + tmp_dir + " && ";
		files_to_pack = "* .*";

		string unpack_cmd;
		if (arch_type == "rar")
			unpack_cmd = str::Format("unrar-free -x %s %s", filename.c_str(), tmp_dir.c_str());
		else if (arch_type == "tar")
			unpack_cmd = str::Format("tar xf %s -C %s", filename.c_str(), tmp_dir.c_str());
		else if (arch_type == "7z" || arch_type == "zip" || arch_type == "gzip" || arch_type == "bzip2")
			unpack_cmd = str::Format("7z e %s -o%s", filename.c_str(), tmp_dir.c_str());

		int rs = 0;
		if (!unpack_cmd.empty())
			rs = mgr_proc::Execute(unpack_cmd).Run().Result();
		if (rs != 0) {
			LOG_ERROR_M(filename, str::Format("Comand '%s' exits with code '%i'", unpack_cmd.c_str(), rs));
			return false;
		}
	}

	if (mgr_proc::Execute(chdir_cmd + "tar cpzf " + mgr_proc::Escape(target_filename) + " " +
	                      mgr_proc::Escape(files_to_pack)).Run().Result() == 0) {
		mgr_file::RemoveFile(filename);
	} else {
		LOG_ERROR_M(filename, str::Format("Pack to 'tgz' failed"));
		return false;
	}

	return true;
}

void Fetch(const string& url, const string& path) {
	mgr_file::Remove(path);
	mgr_file::MkDir(path);
	string filename = mgr_file::ConcatPath(path, mgr_file::Name(url));
	bool ok = mgr_rpc::FetchFile(url, filename);
	if (!ok)
		LOG_ERROR_M(filename, "FetchFile failed");
	else
		ok &= Convert(filename);
	if (ok)
		LOG_OK(filename);
	else
		mgr_file::RemoveFile(filename);
}

void CreateInfo(const string& info, const string& path) {
	string filename = mgr_file::ConcatPath(path, "info");
	mgr_file::Write(filename, info);
}

void InstallDist(const string& xml) {
	Trace();
	string path;
	auto plist = mgr_client::Local("gsmgr", CMD_INSTALL_DIST).Query("func=pathlist").xml;
	auto node = plist.GetNode("/doc/elem/" ConfGameInstallDir);
	path = node.Str();
	Debug("game_install_dir='%s'", path.c_str());

	auto xml_filename = mgr_file::ConcatNPath(3, mgr_file::GetCurrentDir().c_str(), "var/gsdist/", xml.c_str());
	string name = mgr_file::Name(xml_filename);
	string full_path = mgr_file::ConcatPath(path, str::GetWord(name, '.'));
	INIT_LOG(full_path);
	GameInfo info(xml_filename, full_path);
	Debug("name='%s'", info.Name.c_str());
	Debug("dir='%s'", info.Path.c_str());

	new mgr_job::RemoveFile(info.Path);

	ForEachI(info.Versions, version) {
		Debug("ver='%s'", version->Name.c_str());
		Debug("dist_path='%s'", version->Path.c_str());
		Debug("url='%s'", version->Url.c_str());
		Debug("info='%s'", version->Info.c_str());
		Fetch(version->Url, version->Path);
		CreateInfo(version->Info, version->Path);
		ForEachI(version->Plugins, plugin) {
			Debug("\tplugin='%s'", plugin->Name.c_str());
			Debug("\tdist_path='%s'", plugin->Path.c_str());
			Debug("\turl='%s'", plugin->Url.c_str());
			Debug("\tinfo='%s'", plugin->Info.c_str());
			Fetch(plugin->Url, plugin->Path);
			CreateInfo(plugin->Info, plugin->Path);
		}
	}
}


class Arguments : public opts::Args {
public:
	opts::Arg XmlName;

	Arguments () :
	    XmlName("xml", 'x', this) {}

	void OnUsage(const string &argv0) {
		using namespace std;
		cout << endl << "Usage: " << mgr_file::Name (argv0) << " [OPTION]" << endl;
		cout << "GSmanager dist install utility, version " << VER << endl;
		cout << endl << mgr_file::Name (argv0) << endl;
		cout << "\t-x|--xml the name of game xml" << endl;
		cout << endl;
	}

	void OnVersion(const string &argv0) {
		std::cout << VER << std::endl;
	}

	void OnUnrecognize (const std::vector<string> &unrecognized) {
		ForEachI (unrecognized, opt) {
			Warning ("Unrecognized option: %s", opt->c_str());
			std::cerr << "unrecognized option: " << *opt << std::endl;
		}
	}
};

void CommitInstall() {
	mgr_client::Local("gsmgr", CMD_INSTALL_DIST).Query("func=gs.installdist.commit");
}

int ISP_MAIN(int argc, char *argv[]) {
	mgr_log::Init(CMD_INSTALL_DIST);
	Arguments args;
	args.Parse(argc, argv);
	try {
		InstallDist(args.XmlName);
		mgr_job::Commit();
	} catch (const std::exception&) {
		mgr_job::Rollback();
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
