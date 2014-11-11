#include "teamspeak.h"
#include <api/problems.h>
#include <api/stdconfig.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrjob.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include "defines.h"
#include "dbobject.h"
#ifdef Linux
#include <wait.h>
#endif

MODULE("gs_teamspeak_handler");


/*
 * 
 * "space" \s
 * 
 * login serveradmin $pass
 * 
 * 
 * servercreate virtualserver_name=$servername virtualserver_port=$port virtualserver_maxclients=$ports
 *  >> sid=2 token=BtY3VmkeMNZ9iYzBN+XjBMSjP+RCy5BLckweXkFW virtualserver_port=33550
 * serverstop sid=$id
 * 
 * serverstart sid=%id
 * 
 * serverdelete sid=%id
 * 
 * 
 * error id=0 msg=ok
 * 
 * 
 * 
 */


string TerminalBase::GetString()
{
	string data = this->RecvData();
	if (Lines.size() > 0) {
		data = Lines.front();
		Lines.pop_front();
	} else {
		size_t pos;
		while ((pos = data.find_first_of("\r\n")) == string::npos) {
			data += this->RecvData();
		}
		string tmp;
		while ((pos = data.find_first_of("\r\n")) != string::npos) {
			tmp = data.substr(0, pos);
			data.erase(0, pos);
			while (!data.empty() && (data[0] == '\r' || data[0] == '\n')) {
				data.erase(0, 1);
			}
			Lines.push_back(tmp);
		}
		data = Lines.front();
		Lines.pop_front();
	}
	return data;
}

void TerminalBase::Send(const string& data)
{
	Trace("Send [%s]", data.c_str());
	this->SendData(data);
}

bool TerminalBase::WaitFor(const string& data, int timeout)
{
	int tpass = 0;
	string tmp = GetString();
	while(tmp.find(data) == string::npos) {
		mgr_proc::Sleep(1000);
		tmp = GetString();
		tpass+=1;
		if (tpass >= timeout) return false;
	}
	return true;
}

SystemInOutConnection::SystemInOutConnection(bool useError):UseError(useError), Opened(false), CPos(0)  {
	
};

SystemInOutConnection::~SystemInOutConnection() {
	Close();
}

void Split(string str, StringVector & cont, const string & delimiter)
{
	string word;
	size_t pos;
	int dsize;
	while (!str.empty()) {
		pos = str.find("\r\n");
		dsize = 2;
		if (pos == string::npos) {
			pos = str.find_first_of("\r\n");
			dsize = 1;
		}
		if (pos == string::npos)
			dsize = 0;
		word = str.substr(0,pos);
		str.erase(0,pos+dsize);
		cont.push_back(word);
	}
}

void SystemInOutConnection::Open(const string & command, const string & workDir) {
		Command = command;
		
	if (Opened)
		throw mgr_err::Error("Connection already opened!");
	if (pipe(in_fd) == -1 || pipe(out_fd) == -1 || pipe(err_fd) == -1)
		throw mgr_err::Error("can't create pipe for system execute");

	//Debug("in [%d:%d] out [%d:%d] err [%d:%d]", in_fd[0], in_fd[1], out_fd[0], out_fd[1], err_fd[0], err_fd[1]);

	switch (pid = fork()) {
	case -1:
		close(in_fd[0]);
		close(in_fd[1]);
		close(out_fd[0]);
		close(out_fd[1]);
		close(err_fd[0]);
		close(err_fd[1]);
		throw mgr_err::Error("Faield to fork");
	case 0:
		setpgid(0, 0);

		if (!workDir.empty())
			chdir(workDir.c_str());

		// close all descriptors
		int NoFile = getdtablesize();
		for (int i = 0; i < NoFile; i++)
			if (i != in_fd[0] && i != out_fd[1] && i != err_fd[1])
				close(i);

		// pipe input
//				close(in_fd[1]);
		if (in_fd[0] != STDIN_FILENO) {
			dup2(in_fd[0], STDIN_FILENO);
			close(in_fd[0]);
		}

		// pipe out
//				close(out_fd[0]);
		dup2(out_fd[1], STDOUT_FILENO);
		close(out_fd[1]);

		// pipe stderr
//				close(err_fd[0]);
		dup2(err_fd[1], STDERR_FILENO);
		close(err_fd[1]);

		// XXX need replace to ecelv
		execl("/bin/sh", "/bin/sh", "-c", command.c_str(), (char *)0);
		exit(1);
		//throw InternalError("can't exec '"+cmd+"' "+strerror(errno));
	}
	close(err_fd[1]);
	close(out_fd[1]);  // out_fd[0] will use for reading data
	close(in_fd[0]); // in_fd[1] will use for writing data

	string err;

	fcntl(out_fd[0], F_SETFL, fcntl(out_fd[0], F_GETFL) | O_NONBLOCK);
	fcntl(err_fd[0], F_SETFL, fcntl(err_fd[0], F_GETFL) | O_NONBLOCK);
	
	Opened = true;

}

void SystemInOutConnection::Close()
{
	if (!Opened) return;
	close(in_fd[1]);
	close(out_fd[0]);
	close(err_fd[0]);
	
	if (pid > 0) {
		int error, status;
		kill(pid, 15);
		if (waitpid(pid, &status, 0) == -1) {
			error = errno;
			Debug("Nothing to wait");
		} else {
			error = WEXITSTATUS(status);
		}

		LogExtInfo( "End execution of (%s) return=%d %s", Command.c_str(), WIFEXITED(status) ? error : WTERMSIG(status),
			WIFEXITED(status) ? "exited" : WIFSIGNALED(status) ? "killed" : "unknown");
	}
	Opened = false;
}

string SystemInOutConnection::Read()
{
	ReadError();
	char buf[1024];
	ssize_t len = read(UseError?err_fd[0]:out_fd[0], buf, 1023);
	if (len <= 0) return "";
	buf[len] = 0;
	string tmp(buf);
	Debug("Read [%s]", tmp.c_str());
	return tmp;
}

string SystemInOutConnection::ReadError()
{
	char buf[1024];
	ssize_t len = read(err_fd[0], buf, 1023);
	if (len <= 0) return "";
	buf[len]=0;
	string tmp(buf);
	LogError("Telnet:[%d][%s]", len, tmp.c_str());
	throw mgr_err::Error("telnet_error",tmp,tmp);
	return tmp;
}

void SystemInOutConnection::Send(const string& str) {
	string tmp = str::Replace(str,"\\r","\r");
	tmp = str::Replace(tmp,"\\n","\n");
	if (!tmp.empty()) {
		size_t cnt = write(in_fd[1], tmp.data(), tmp.size());
		Debug("Send %d bytes",cnt);
	}
}

string SystemInOutConnection::GetString(void)
{
	int timeout = 100;
	if (CPos < log.size()) {
		string tmp = log[CPos++];
		Debug("return string [%s]",tmp.c_str());
		return tmp;
	} else {
		string tmp = "";
		while(tmp.find("\n") == string::npos) {
			tmp += this->Read();
			usleep(10000);
			timeout--;
			if (timeout == 0) break;
		}

		if (!tmp.empty()) {
			Split(tmp,log,"\n");
			string tmp2 = log[CPos++];
			Debug("return string [%s]",tmp2.c_str());
			return tmp2;
		}
	}
	Debug("return string []");
	return "";
}

void SystemInOutConnection::Wait(const string & str, int timeout)
{
	Trace("Wait for '%s'",str.c_str());
	while (GetString().find(str) == string::npos) {
		timeout--;
		if (timeout == 0) throw mgr_err::Error("wait_timeout", Command);
		continue;
	}
}

string SystemInOutConnection::Wait(const StringList& strs, int timeout){
	while (1) {
		string tmp = GetString();
		ForEachI(strs, i) {
			if (tmp.find(*i) != string::npos)
				return *i;
		}
		timeout--;
		if (timeout == 0) throw mgr_err::Error("wait_timeout", Command);
	}
}

string TelnetTerminal::RecvData()
{
	return TelnetSession.Read();
}

void TelnetTerminal::SendData(const string& data)
{
	TelnetSession.Send(data);
}

string TelnetTerminal::GetString()
{
	string tmp = TelnetSession.GetString();
	Debug("Telnet get str '%s'",tmp.c_str());
	return tmp; 
}

void TelnetTerminal::Wait(const string& str, int timeout)
{
	TelnetSession.Wait(str, timeout);
}

string TelnetTerminal::Wait(const StringList& strs, int timeout)
{
	return TelnetSession.Wait(strs, timeout);
}

void TelnetTerminal::Close()
{
	mgr_proc::Sleep(100);
	TelnetSession.Close();
}

void TelnetTerminal::Open(const string& server, int port)
{
	TelnetSession.Open("telnet "+server+" "+str::Str(port),"");
}

TelnetTerminal::~TelnetTerminal()
{
	Close();
}

TeamSpeakCommandResult TeamSpeakNode::Init(TelnetTerminal& telnet)
{
	telnet.Open(Ip, Port);
	telnet.Wait("Welcome to the TeamSpeak 3",10);
	auto res = SendComand("login serveradmin " + EscapeTelnetString(Password), telnet);
	if (!res) throw mgr_err::Error("teamspeak","login", res.Result["msg"]);
	return res;
}


TeamSpeakCommandResult TeamSpeakNode::GetServerList()
{
	TelnetTerminal telnet;
	Init(telnet);
	auto res = SendComand("serverlist" , telnet);
	telnet.Close();
	return res;
}

void TeamSpeakNode::GetServerStat()
{
	TelnetTerminal telnet;
	Init(telnet);
	auto res = SendComand("serverlist" , telnet);
	telnet.Close();
	ForEachI(res.Data, server) {
		string id = (*server)["virtualserver_id"];
		string status = (*server)["virtualserver_status"];
		string name = (*server)["virtualserver_name"];
		int clients = str::Int((*server)["virtualserver_clientsonline"]);
		auto aServer = db->Get<TeamSpeakServerTable>();
		if (aServer->DbFind("sid="+db->EscapeValue(id))) {
			aServer->OnlinePlayers = clients;
			aServer->Active = status == "online";
			aServer->Name = name;
			aServer->Post();
		}
	}
}

StringMap TeamSpeakNode::ParseLine(const string& line)
{
	StringMap result;
	StringList params;
	str::Split(line, params, " ");
	ForEachI(params, p) {
		string tmp = UnEscapeTelnetString(*p);
		string name = str::GetWord(tmp,'=');
		string val = tmp;
		result[name] = val;
	}
	return result;
}

TeamSpeakCommandResult TeamSpeakNode::CreateServer(const string& name, int slots)
{
	TelnetTerminal telnet;
	Init(telnet);
	auto res = SendComand("servercreate virtualserver_name=" + EscapeTelnetString(name) + " virtualserver_maxclients=" + str::Str(slots) , telnet);
	telnet.Close();
	return res;
}

TeamSpeakCommandResult TeamSpeakNode::SetServer(int id, const string& name, int slots)
{
	TelnetTerminal telnet;
	Init(telnet);
	auto res = SendComand("use sid=" + str::Str(id) , telnet);
	res = SendComand("serveredit virtualserver_name=" + EscapeTelnetString(name) + " virtualserver_maxclients=" + str::Str(slots) , telnet);
	telnet.Close();
	return res;
}

TeamSpeakCommandResult TeamSpeakNode::DeleteServer(int id)
{
	TelnetTerminal telnet;
	Init(telnet);
	auto res = SendComand("serverstop sid=" + str::Str(id) , telnet);
	mgr_proc::Sleep(100);
	res = SendComand("serverdelete sid=" + str::Str(id) , telnet);
	telnet.Close();
	return res;
}

TeamSpeakCommandResult TeamSpeakNode::StartServer(int id)
{
	TelnetTerminal telnet;
	Init(telnet);
	auto res = SendComand("serverstop sid=" + str::Str(id), telnet);
	mgr_proc::Sleep(100);
	res = SendComand("serverstart sid=" + str::Str(id), telnet);
	telnet.Close();
	return res;

}

TeamSpeakCommandResult TeamSpeakNode::StopServer(int id)
{
	TelnetTerminal telnet;
	Init(telnet);
	auto res = SendComand("serverstop sid=" + str::Str(id) , telnet);
	telnet.Close();
	return res;
}

TeamSpeakNode::TeamSpeakNode()
{
	Ip = mgr_cf::GetParam(TeamSpeakNodeIp);
	Port = str::Int(mgr_cf::GetParam(TeamSpeakNodePort));
	Password = mgr_cf::GetParam(TeamSpeakNodePassword);
}

TeamSpeakCommandResult TeamSpeakNode::SendComand(const string & cmd, TelnetTerminal& telnet)
{
	int timeout = 100;
	TeamSpeakCommandResult res;
	telnet.Send(cmd+"\n");
	telnet.GetString();
	string line = str::Trim(telnet.GetString());
	
	while (timeout) {
		if (line.empty()) {
			mgr_proc::Sleep(100);
			timeout--;
		} else {
			if (line.find("error") == 0) {
				res.Result = ParseLine(line);
				return res;
			} else {
// 				StringMap tmp = ParseLine(line);
// 				res.Data.push_back(tmp);
				ParseData(line, res);
			}
		}
		line = str::Trim(telnet.GetString());
	}
	Warning("TEAMSPEAK TIMEOUT");
	return res;
}

void TeamSpeakNode::ParseData(const string & data, TeamSpeakCommandResult & res)
{
	StringList input;
	str::Split(data,input,"|");
	ForEachI(input,in) {
		StringMap tmp = ParseLine(*in);
		res.Data.push_back(tmp);
	}
}


string TeamSpeakNode::EscapeTelnetString(string data)
{
	StringMap repmap;
	repmap["\\"]="\\\\";
	repmap["/"]="\\/";
	repmap[" "]="\\s";
	repmap["|"]="\\p";
	repmap["\t"]="\\t";
	data = str::Replace(data, repmap);	
	return data;
}

string TeamSpeakNode::UnEscapeTelnetString(string data)
{
	StringMap repmap;
	repmap["\\\\"]="\\";
	repmap["\\/"]="/";
	repmap["\\s"]=" ";
	repmap["\\p"]="|";
	repmap["\\t"]="\t";
	data = str::Replace(data, repmap);
	return data;
}


