#include <classes.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrerr.h>
#include <mgr/mgrdb_struct.h>
#include "stat_raw.h"

#undef __USE_FILE_OFFSET64 
#include <fts.h>

MODULE("stat");
using namespace std;
namespace fts
{

size_t GetList (const string &initialPath, StringVector & fileListOut)
{
	char * pathArg[2];
	pathArg[0] = strdup(initialPath.c_str());
	pathArg[1] = nullptr;
	FTS *tree = fts_open(pathArg, FTS_NOCHDIR, 0);
	if (!tree) {
		free (pathArg[0]);
		throw mgr_err::Error("fts_open").add_param("path", initialPath);
	}
	size_t count = 0;
	FTSENT *node;
	while ((node = fts_read(tree))) {
		if (node->fts_info & FTS_F) {
			Debug ("add file %s", node->fts_path);
			fileListOut.push_back(node->fts_path);
			++ count;
		}
	}
	if (fts_close(tree)) {
		free (pathArg[0]);
		throw mgr_err::Error("fts_close").add_param("path", initialPath);
	}
	free (pathArg[0]);
	return count;
}

}

namespace StatUtils
{

std::shared_ptr<mgr_db::Cache> __dbCache;

void InitStatsDB(std::shared_ptr<mgr_db::Cache> dbCacheIn)
{
		__dbCache = dbCacheIn;
//		__dbCache->Register<Stat::CountersTable>();
}
std::shared_ptr<mgr_db::Cache> DB()
{
	if (!__dbCache)
		throw mgr_err::Error ("db_doesnt_init");
	return __dbCache;
}


StringVector SplitKeys(string path)
{
	StringVector result;
	while (!path.empty()) {
		string elem = str::GetWord(path, "/");
		if (elem.empty()) continue;
		if (elem.find(STAT_KEYPREFIX) == 0) {
			result.push_back(elem.substr(1, elem.length() - 1));
		}
	}
	return result;
}

StringVector SplitPath(string path)
{
	StringVector result;
	while (!path.empty()) {
		string elem = str::GetWord(path, "/");
		if (elem.empty()) continue;
		result.push_back(elem);
	}
	return result;
}

time_t block_offset()
{
	struct tm offset_tm;
	time_t offset_time=time(0);
	localtime_r (&offset_time, &offset_tm);
	return offset_tm.tm_gmtoff;
}

time_t block_left(time_t now, time_t timeBlock)
{
	time_t x = (now + block_offset()) / timeBlock;
	return x * timeBlock - block_offset();
}

time_t block_right(time_t now, time_t timeBlock)
{
	time_t x = (now + block_offset()) / timeBlock;
	return (x + 1) * timeBlock - 1 - block_offset();
}


}

