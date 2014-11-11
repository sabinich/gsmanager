#ifndef CMDARG_H
#define CMDARG_H

#include <string>
#include <vector>
#include <algorithm>
#include <iterator>
#include <mgr/mgrtest.h>

namespace opts
{

class Args;

class Arg
{
public:
	typedef Arg * Ptr;
	std::string Name;
	char ShortName;
	bool Exists;
	std::string Opt;
	bool HasOpt;
	bool Required;
	bool RequireValue;
	Arg (const std::string &name, Args * parent, bool required=true,  bool requireValue=true);
	Arg (const std::string &name, char shortName, Args * parent, bool required=true, bool requireValue=true);
	Arg (char shortName, Args * parent, bool required=true, bool requireValue=true);
	inline operator std::string() const { return Opt; }
	inline std::string AsString() const { return Opt; }
	string OnUsage() const;
	 /*
	  * Позволяет задать необходимость наличия значения у аргумента, в зависимости от наличия
	  *другого аргумента с определенным значением.
	  */
	void Depends(Ptr depends, const string& value);
	void SetValidator(test::Valid validator);
protected:
	std::vector<std::pair<Ptr, string> > m_depends;
	test::Valid m_validator;

	friend class Args;
};

class Args
{
private:
	std::vector <Arg::Ptr> m_Args;
	std::vector<std::string> m_Other;
	std::vector<std::string> m_Unrecognized;
public:
	Arg Help;
	Arg Version;
	Arg IspOpt;
	Args();
	void Register (Arg::Ptr arg);
	Arg::Ptr Get (const std::string &name);
	Arg::Ptr Get (char name);
	bool Parse (int argc, char **argv);
	template <class O>
	void GetOther (O & other)
	{
		std::copy (m_Other.begin (), m_Other.end (), std::back_inserter (other));
	}

	template <class O>
	void GetUnrecognized (O & other)
	{
		std::copy (m_Unrecognized.begin (), m_Unrecognized.end (), std::back_inserter (other));
	}
	virtual ~Args() {}
protected:
	virtual void OnUsage (const std::string &argv0) {}
	virtual void OnVersion (const std::string &argv0) {}
	virtual void OnUnrecognize (const std::vector<std::string> & unrecognized) {}

private:
	int paramType (const std::string &param);
	int parseOne (const std::string & argv0, const std::string & argv1);

};

void Dump (int argc, char **argv) ;

}

#endif // CMDARG_H
