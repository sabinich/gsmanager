#ifndef TEAMSPEAK_H
#define TEAMSPEAK_H

#include <mgr/mgrfile.h>
#include <api/module.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrssh.h>
#include <mgr/mgrproc.h>

class TerminalBase {
private:
	string LeftData;
protected:
	StringList Lines;
	virtual void SendData(const string& data)=0;
	virtual string RecvData()=0;
public:
	TerminalBase(){};
	virtual void Send(const string& data);
	virtual string GetString();
	virtual bool WaitFor(const string& data, int timeout = 0);
};

class SystemInOutConnection {
private:
	pid_t pid;
	int out_fd[2];
	int in_fd[2];
	int err_fd[2];
	string Command;
	bool UseError;
	bool Opened;
public:
	StringVector log;
	size_t CPos;
	SystemInOutConnection(bool useError = false);
	void Open(const string & command, const string & workDir);
	string GetString();
	void Wait(const string& str, int timeout = -1);
	string Wait(const StringList& strs, int timeout = -1);
	void Send(const string& str);
	void Close();
	string Read();
	string ReadError();
	virtual ~SystemInOutConnection();
};

class TelnetTerminal: public TerminalBase {
private:
	SystemInOutConnection TelnetSession;
protected:
	string RecvData();
	void SendData(const string& data);
public:
	void Open(const string& server, int port = 23);
	void Close();
	string GetString();
	void Wait(const string& str, int timeout = -1);
	string Wait(const StringList& strs, int timeout = -1);
	~TelnetTerminal();
};

class TeamSpeakServer;

typedef std::list<TeamSpeakServer> TeamSpeakServerList;

struct TeamSpeakCommandResult {
	std::list<StringMap> Data;
	StringMap Result;
	operator bool() {
		return Result["id"] == "0";
	}
};

class TeamSpeakNode {

public:
	string Ip;
	int Port;
	string Password;
	string LastError;
	TeamSpeakNode();
	TeamSpeakCommandResult GetServerList();
	void GetServerStat();
	
	TeamSpeakCommandResult CreateServer(const string & name, int slots);
	TeamSpeakCommandResult SetServer(int id, const string & name, int slots);
	TeamSpeakCommandResult DeleteServer(int id);
	TeamSpeakCommandResult StartServer(int id);
	TeamSpeakCommandResult StopServer(int id);
	
	TeamSpeakCommandResult SendComand(const string & cmd, TelnetTerminal & telnet);
private:
	void ParseData(const string & data, TeamSpeakCommandResult & res);
	StringMap ParseLine(const string & line);
	TeamSpeakCommandResult Init(TelnetTerminal & telnet);
	string EscapeTelnetString(string data);
	string UnEscapeTelnetString(string data);
};


#endif