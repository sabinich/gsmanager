#ifndef GMMAIN_H
#define GMMAIN_H

#include <mgr/mgrdb.h>
#include <mgr/mgrdb_struct.h>
#include <api/dbaction.h>
#include <api/action.h>
#include "dbobject.h"
#include "nodes.h"
#include "gsutils.h"

class GameModules {
private:
	static StringMap m_game_modules;
public:
	static void AddGame(const string & name, const string & fullname) {
		m_game_modules[name] = fullname;
	}
	static StringMap Get() {
		return m_game_modules;
	}
};

#endif
