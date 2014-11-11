#include <api/action.h>
#include <api/dbaction.h>
#include <api/module.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrfile.h>

#define CACHE_XML "var/run/.plist-cache.xml"
#define CACHE_LOCK "var/run/.plist-cache.lock"
#define UPDATE_TIMEOUT 2

using namespace isp_api;

namespace
{

static mgr_thread::SafeLock cplock;

class OneTreadParamlist : public Action
{
public:
	OneTreadParamlist()
		: Action ("otparamlist", MinLevel(lvAdmin))
	{}
	virtual void Execute(Session &ses) const
	{
		mgr_thread::SafeSection safe(cplock);
		ses.xml = InternalCall ("paramlist");
	}
};

}

MODULE_INIT (cparamlist, "")
{
	new OneTreadParamlist;
}

