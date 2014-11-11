#ifndef STAT_UTILS_H
#define STAT_UTILS_H

#endif // STAT_UTILS_H

#include <classes.h>

namespace fts
{

size_t GetList (const string &initialPath, StringVector & fileListOut);
}

namespace StatUtils
{
void InitStatsDB(std::shared_ptr<mgr_db::Cache> dbCacheIn);
std::shared_ptr<mgr_db::Cache> DB();

StringVector SplitKeys(string path);
StringVector SplitPath(string path);

time_t block_offset();
time_t block_left(time_t now, time_t timeBlock);
time_t block_right(time_t now, time_t timeBlock);
}


