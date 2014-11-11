#include "cmdarg.h"
#include <iostream>
#include <stdexcept>
#include <mgr/mgrerr.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrlog.h>

MODULE ("sbin_utils");

using namespace std;


namespace opts
{


static string getArgName (Arg::Ptr arg)
{
	string result;
	if (arg->Name.empty ())
		result += arg->ShortName;
	else
		result = arg->Name;
	return result;
}

Arg::Arg (const string &name, Args *parent, bool required, bool requireValue)
    : Name (name), ShortName (0), Exists (false)
    , HasOpt (false), Required (required), RequireValue (requireValue)
{
    parent->Register (this);
}
Arg::Arg (const string &name, char shortName, Args *parent, bool required, bool requireValue)
    : Name (name) , ShortName (shortName) , Exists (false)
    , HasOpt (false), Required (required), RequireValue (requireValue)
{
    parent->Register (this);
}
Arg::Arg (char shortName, Args *parent, bool required, bool requireValue)
    : ShortName (shortName) , Exists (false) , HasOpt (false)
    , Required (required), RequireValue (requireValue)
{
    parent->Register (this);
}

int Args::paramType (const string &param) {
    if (param.empty ())
        return -1;
    if (param == "-" || param == "--") return 0;
    string::size_type pos = param.find_first_not_of ('-');
    if (pos > 2) pos = 2;
    return pos;
}
Args::Args()
    : Help ("help", 'h', this, false, false)
    , Version ("version", 'V', this, false, false)
	, IspOpt ('T', this, false, false)
{
}

void Args::Register (Arg::Ptr arg)
{
	Debug ("Register %s (%c)", arg->Name.c_str (), arg->ShortName);
    for (vector <Arg::Ptr>::iterator i = m_Args.begin (); i != m_Args.end (); ++i) {
		if ((*i)->ShortName && (*i)->ShortName == arg->ShortName ) {
			cerr << "\tDublicate argument " << getArgName (*i) << endl;
			throw mgr_err::Error("dublicate").add_param ("name", getArgName (*i));

		}
		if (!(*i)->Name.empty() && (*i)->Name == arg->Name) {
			cerr << "\tDublicate argument " << getArgName (*i) << endl;
			throw mgr_err::Error("dublicate").add_param ("name", getArgName (*i));
        }
    }
    m_Args.push_back (arg);
}

Arg::Ptr Args::Get (const string &name) {
    for (vector <Arg::Ptr>::iterator i = m_Args.begin (); i != m_Args.end (); ++i) {
        if ((*i)->Name == name)
            return *i;
    }
	cerr << "\tArgument missed " << name << endl;
	throw mgr_err::Missed ("argument", name);
}
Arg::Ptr Args::Get (char name) {
    for (vector <Arg::Ptr>::iterator i = m_Args.begin (); i != m_Args.end (); ++i) {
        if ((*i)->ShortName == name)
            return *i;
    }
	cerr << "\tArgument missed " << name << endl;
	throw mgr_err::Missed ("argument", str::Str(name));
}
//Передаю пару...
// Возвращаем сколько нужно пропустить дополнительно
int Args::parseOne (const string & argv0, const string & argv1)
{
    if (argv0.empty ())
        return 0;
    int pType = paramType (argv0);
    if (pType == 0) {
        m_Other.push_back (argv0);
        return 0;
    }
    int skip = 0;
    bool parsed = false;
    if (pType == 1) {
        // Short param
        for (vector <Arg::Ptr>::iterator i = m_Args.begin(); i != m_Args.end(); ++i)  {
            if (!(*i)->ShortName) continue;
            if ((*i)->Exists) continue;
            string::size_type spPos = argv0.find ((*i)->ShortName);
            bool last = spPos == argv0.length () - 1;
            if (spPos != string::npos) {
                (*i)->Exists = true;
                parsed = true;
            }
            if ((*i)->RequireValue) {
                if (last && !argv1.empty ()) {
                    (*i)->Opt = argv1;
                    (*i)->HasOpt = true;
                    skip = 1;
                } else {
                    (*i)->HasOpt = false;
                }
            }
        }
    } else if (pType == 2) {
        // long param
        for (vector <Arg::Ptr>::iterator i = m_Args.begin(); i != m_Args.end(); ++i) {
            if ((*i)->Name.empty()) continue;
            if (argv0 == "--" + (*i)->Name) {
                (*i)->Exists = true;
                parsed = true;
                if ((*i)->RequireValue) {
                    if (!argv1.empty ()) {
                        (*i)->Opt = argv1;
                        (*i)->HasOpt = true;
                        skip = 1;
                    } else {
                        (*i)->HasOpt = false;
                    }
                }
            }
        }
    }
    if (!parsed) {
       m_Unrecognized.push_back (argv0);
    }
    return skip;
}


bool Args::Parse(int argc, char **argv)
{
    int currentArg = 1;
    while (currentArg <= argc - 1) {
        string argv0 = argv[currentArg];
        string argv1 = (currentArg < argc - 1) ? argv[currentArg + 1] : "";
        int skip = parseOne (argv0, argv1);
        currentArg += 1 + skip;
    }
	if (IspOpt.Exists) {
		cout << "(c) ISPsystem.com";
		exit(0);
	}
	if (!m_Unrecognized.empty ()) {
		OnUnrecognize(m_Unrecognized);
		exit (1);
	}
	if (Help.Exists || m_Args.empty ()) {
		OnUsage(argv[0]);
		exit (0);
	}
	if (Version.Exists) {
		OnVersion (argv[0]);
		exit (0);
	}
    // check values
	bool missed = false;
    for (vector <Arg::Ptr>::iterator i = m_Args.begin(); i != m_Args.end(); ++i) {
        Arg::Ptr arg = *i;
        if (arg->Required & !arg->Exists) {
			cerr << "\tMissed argument " << getArgName (arg) << endl;
			missed = true;
        }
        if (arg->Exists && arg->RequireValue && !arg->HasOpt) {
			cerr << "\tArgument " << getArgName (arg) << "require value" << endl;
			missed = true;
        }
    }
	if (missed) {
		OnUsage (argv[0]);
		exit (1);
	}
	return !missed;

}
}
