#ifndef OSUTILS_H_
#define OSUTILS_H_
#include "ostcommon.h"


#define TEMPLATE_TYPE_RESCUE "rescue"
#define TEMPLATE_TYPE_OS "ostemplate"
#define TEMPLATE_TYPE_DIAG "diag"
#define TEMPLATE_TYPE_SERVERSEARCH "serversearch"
#define OS_INSTALL_RESULT_OK "ok"
#define OS_INSTALL_RESULT_FAILED "failed"

string OsKey2OsName (string osKey, bool showRepo = false);
void GetInstalledOsInfo(StringMap & osinfo, bool skipRestricted = false);
void GetOsInfo(OsInfoMap & info);

#endif

