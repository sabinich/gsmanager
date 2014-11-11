#include "traff_table.h"

TrafficIgnRuleTable::TrafficIgnRuleTable()
	: mgr_db::IdTable("traffignore")
	, Object (this, "object", 64) //Имена объектов, для всех = '*'
	, Direction (this, "direction")
	, IpAddress (this, "ip", 64)
{

}
