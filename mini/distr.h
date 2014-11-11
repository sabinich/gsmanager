#ifndef DSITR_H
#define DSITR_H
#include <mgr/mgrstr.h>
struct DistInfo {
	string Path;
	string Name;
	string Info;
	string Game;
	bool IsDefault;
};
typedef std::list<DistInfo> DistInfoList;
void GetDistInfoList(DistInfoList& res, const string& game="");
DistInfo GetDistInfo(const string& game, const string& dist);
#endif