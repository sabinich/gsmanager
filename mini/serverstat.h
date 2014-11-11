#ifndef SERVERSTAT_H
#define SERVERSTAT_H
#include <api/action.h>
#include "gamemodule.h"
void AddServerStat(time_t now, ServerInstanceInfo & srv, int players, int mem);
void GetDayStat(ServerInstanceInfo & srv, isp_api::Session & ses);
void GetMonthStat(ServerInstanceInfo & srv, isp_api::Session & ses);
#endif