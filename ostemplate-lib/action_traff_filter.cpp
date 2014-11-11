#include <api/action.h>
#include <api/dbaction.h>
#include <api/module.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrfile.h>

#include "traff_table.h"
#include "stat_utils.h"
#include "traff.h"

using namespace isp_api;

/*
 * Фильтры для игнорирования траффика
 * Фильтры бывают 2 типов: для конкретной VM, либо для всех, либо для конкретной ноды
 * Направление фильтрации - Входящий/исходящий/в-обе-стороны
 * IP-адрес: один или сеть
 *
*/

namespace
{

string GetMessage(const string & lang, const string & module, const string& msg)
{
	auto msgs = isp_api::GetMessages(lang, module);
	return msgs.GetNode("//msg[@name='"+msg+"']");
}

class ActionTraffFilter : public TableIdListAction<TrafficIgnRuleTable>
{
public:
	ActionTraffFilter(mgr_db::Cache &cache)
		: TableIdListAction<TrafficIgnRuleTable> ("traffilter", MinLevel(lvAdmin), cache)
	{}

	// TableIdListAction interface
public:
	void FillObject(Session & ses) const
	{
		ses.NewSelect("object");
		ses.AddChildNode("msg", "*");
		ForEachQuery(StatUtils::DB(), "select name from vm order by name", item) {
			ses.AddChildNode("msg", item->AsString("name"));
		}
		ForEachQuery(StatUtils::DB(), "select name from hostnode order by name", item) {
			ses.AddChildNode("msg", item->AsString("name"));
		}
	}

	void FillDirection(Session & ses) const
	{
		ses.NewSelect("direction");
		ses.AddChildNode("msg", "direction_incoming");
		ses.NodeProp("key", str::Str(TrafficIgnRuleTable::tirdIncoming));
		ses.AddChildNode("msg", "direction_outgowing");
		ses.NodeProp("key", str::Str(TrafficIgnRuleTable::tirdOutgowing));
		ses.AddChildNode("msg", "direction_both");
		ses.NodeProp("key", str::Str(TrafficIgnRuleTable::tirdBoth));
	}

	virtual void TableFormTune(Session &ses, Cursor &table) const
	{
		FillObject(ses);
		FillDirection(ses);
	}
};

}

namespace TraffFilter
{
void Init ()
{
	if (!mgr_cf::FindOption(ConfUseNetflow))
		return;
	StatUtils::DB()->Register<TrafficIgnRuleTable>();
	new ActionTraffFilter(*StatUtils::DB());
}
}

