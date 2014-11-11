#ifndef CMDARG_H
#define CMDARG_H

#include <string>
#include <vector>

namespace opts
{

class Args;

struct Arg
{
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
		std::copy (m_Other.begin (), m_Other.end (), back_inserter (other));
	}

	template <class O>
	void GetUnrecognized (O & other)
	{
		std::copy (m_Unrecognized.begin (), m_Unrecognized.end (), back_inserter (other));
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

}

#endif // CMDARG_H
