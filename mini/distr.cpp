#include "distr.h"
#include <mgr/mgrfile.h>
#include "../main/defines.h"
#include <api/stdconfig.h>

void GetDistInfoList(DistInfoList& res, const string& game)
{
	mgr_file::Dir distr(mgr_cf::GetPath("dist_path"));
	while(distr.Read()) {
		if (distr.Info().IsDir()) {
			string gamename = distr.name();
			if (!game.empty() && gamename != game) continue;
			mgr_file::Dir gamedist(distr.FullName());
			while(gamedist.Read()) {
				if (gamedist.Info().IsDir()) {
					DistInfo info;
					info.Game = gamename;
					info.Name = gamedist.name();
#ifdef WIN32
					info.Path = mgr_file::ConcatPath(gamedist.FullName(), "/dist/dist.zip");
#else
					info.Path = mgr_file::ConcatPath(gamedist.FullName(), "/dist/dist.tgz");
#endif
					string infofile = mgr_file::ConcatPath(gamedist.FullName(), "/dist/info");
					string defaultfile = mgr_file::ConcatPath(gamedist.FullName(), "/dist/default");
					if (mgr_file::Exists(infofile)) {
						info.Info = mgr_file::Read(infofile);
					}
					info.IsDefault = mgr_file::Exists(defaultfile);
					res.push_back(info);
				}
			}
		}
	}
}

DistInfo GetDistInfo(const string& game, const string& dist)
{
	DistInfo res;
	res.Game = game;
	res.Name = dist;
	string distpath = mgr_file::ConcatPath(mgr_cf::GetPath("dist_path"), game, dist);
	if (mgr_file::Exists(distpath)) {
#ifdef WIN32
		string tarpath = mgr_file::ConcatPath(distpath, "/dist/dist.zip");
#else
		string tarpath = mgr_file::ConcatPath(distpath, "/dist/dist.tgz");
#endif
		string infopath = mgr_file::ConcatPath(distpath, "/dist/info");
		if (mgr_file::Exists(infopath)) {
			res.Info = mgr_file::Read(infopath);
		}
		res.Path = tarpath;
		return res;
	} else throw mgr_err::Error("unknown_dist","Game '"+ game+"' dist '"+dist+"'");
}
