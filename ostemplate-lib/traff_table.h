#ifndef TRAFF_TABLE_H
#define TRAFF_TABLE_H

#include <mgr/mgrdb.h>
#include <mgr/mgrdb_struct.h>

#define DIRECTION_UNKNOWN 0x00
#define DIRECTION_INCOMING 0x01
#define DIRECTION_OUTGOWING 0x02
#define DIRECTION_BOTH 0x03

class TrafficIgnRuleTable : public mgr_db::IdTable
{
public:
	enum PacketDirection {tirdIncoming = DIRECTION_INCOMING,
						  tirdOutgowing = DIRECTION_OUTGOWING,
						  tirdBoth = DIRECTION_BOTH,
						  tirdUnknown = DIRECTION_UNKNOWN};
	mgr_db::StringField Object;
	mgr_db::IntField Direction;
	mgr_db::StringField IpAddress;
	TrafficIgnRuleTable();
};

#endif // TRAFF_TABLE_H
