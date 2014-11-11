#include "screen.h"
#include <mgr/mgrlog.h>
#include <mgr/mgrproc.h>
#include <api/stdconfig.h>
#include <signal.h>
#include <termios.h> 
MODULE("screen");

ScreenHandler::ScreenHandler(const string & module_name, const string & id, const string & user):
m_id(module_name+"_"+id+"_scr"), m_user(user), m_pid(-1), m_fdm(-1), m_lock(mgr_thread::TextLock<ScreenHandler>::GetLock(m_id))
{
	m_screen_path =  mgr_cf::GetPath("screen");
	IsRunnig();
}

int ScreenHandler::GetPid()
{
	if (m_pid == -1) {
		//mgr_proc::Execute cmd(m_screen_path+" -wipe | grep " + m_id+" | awk '{print $1}'",mgr_proc::Execute::efOut);
		mgr_proc::Execute cmd("ps ax | grep SCREEN | grep " + m_id+" | awk '{print $1}'",mgr_proc::Execute::efOut);
		string result = str::Trim(cmd.Str());
		Debug("Result is %s",result.c_str());
		if (cmd.Result() == 0)
			//m_pid =  str::Int(str::GetWord(result,"."));
			m_pid =  str::Int(result);
		else 
			m_pid = -1;
	}
	return m_pid;
}

bool ScreenHandler::IsRunnig()
{
	bool has_sock = mgr_proc::Execute(m_screen_path+" -list | grep " + m_id).Result() == 0;
/*	if (!has_sock) 
		has_sock = mgr_proc::Execute("kill -CHLD "+str::Str(GetPid())).Result() == 0;*/
	return has_sock;
}

// int ScreenHandler::Open(const string& cmd)
// {
//  	mgr_file::Attrs ttyattrs(0660);
// 	mgr_file::Dir pts("/dev/pts");
// 	while(pts.Read()) {
// 		if (pts.name() == str::Str(str::Int(pts.name())) ) {
// 			Debug("Set Attrs for %s",pts.FullName().c_str());
// 			ttyattrs.Set(pts.FullName());
// 		}
// 	}
// // 	ttyattrs.Set();
// 	
// 	if (IsRunnig()) {
// 		LogError("screen with ");
// 		throw mgr_err::Exists("screen", m_id);
// 	}
// 	
// 	string screencmd = m_screen_path+" -d -m -S "+m_id+" "+cmd;
// 	mgr_proc::Execute screen(screencmd);
// 	screen.SetUser(m_user);
// 	if (!m_workdir.empty())
// 		screen.SetDir(m_workdir);
// 	return screen.Result();
// }

int ScreenHandler::Open(const string& cmd)
{
	AutoLock<mgr_thread::TextLock<ScreenHandler> > lock(m_lock);
	int timeout = 5;
	while (IsRunnig()) {
		mgr_proc::Sleep(1000);
		timeout--;
		if (timeout <= 0)
			throw mgr_err::Exists("screen", m_id);
	}
	
	string screencmd = m_screen_path+" -d -m -S "+m_id+" bash";
	mgr_proc::Execute screen(screencmd);
	screen.SetEnv("TERM","xterm");
	if (!m_workdir.empty())
		screen.SetDir(m_workdir);
	screen.Result();
	ScreenTerminal term(*this);
	term.WaitInputCall("#");
	term.Send("su "+m_user+"\r");
	term.Wait(m_user);
	term.Send(cmd +"\r");

	return 0;
}

int ScreenHandler::SendCommand(const string& cmd)
{
	AutoLock<mgr_thread::TextLock<ScreenHandler> > lock(m_lock);
	string screencmd = m_screen_path+" -p 0 -S "+m_id+" -X eval 'stuff \""+cmd+"\"\\015'";
	mgr_proc::Execute screen(screencmd);
	return screen.Result();
}

void ScreenHandler::SendCtrl_C()
{
	AutoLock<mgr_thread::TextLock<ScreenHandler> > lock(m_lock);
	mgr_proc::Execute (m_screen_path+ " -S " + m_id + " -X stuff $'\003'").Result();
}

void ScreenHandler::Close(const string & cmd, int timeout)
{
	AutoLock<mgr_thread::TextLock<ScreenHandler> > lock(m_lock);
	GetPid();
	int stall = 0;
	if (cmd.empty())
		SendCtrl_C();
	else
		SendCommand(cmd);
	
	while(IsRunnig()) {
		mgr_proc::Sleep(1000);
		stall++;
		if (stall >= timeout) {
			Warning("Screen stalled. Send kill signal to proccess %d", GetPid());
			if (GetPid() > 0) {
				mgr_proc::Kill(GetPid());
				IsRunnig();
			}
			return;
		}
	}
}

void ScreenHandler::Close(int timeout)
{
	AutoLock<mgr_thread::TextLock<ScreenHandler> > lock(m_lock);
	GetPid();
	int stall = 0;
// 	SendCommand("exit");
// 	mgr_proc::Sleep(10);
// 	SendCommand("exit");
	{
		ScreenTerminal term(*this);
		term.Send("exit\r");
		term.WaitInputCall("#");
		term.Send("exit\r");
	}
	while(IsRunnig()) {
		mgr_proc::Sleep(1000);
		stall++;
		if (stall >= timeout) {
			Warning("Screen stalled. Send kill signal to proccess %d", GetPid());
			if (GetPid() > 0) {
				mgr_proc::Kill(GetPid());
				IsRunnig();
			}
			return;
		}
	}
}

string ScreenHandler::GetAttachCmd()
{
	return m_screen_path+" -d -r "+m_id;
}

string ScreenHandler::GetDetachCmd()
{
	return m_screen_path+" -d "+m_id;
}

ScreenTerminal::ScreenTerminal(ScreenHandler& parent):m_parent(parent), m_attach(parent.GetAttachCmd())
{
	m_bufsize = 1024;
	m_buf = (char *)malloc(m_bufsize);
	m_eof = false;
	Attach();
	FD_ZERO(&rfds);
	FD_SET(m_fd, &rfds);
	Debug("m_data[%s]",m_data.c_str());
	tv.tv_sec = 10;
	tv.tv_usec = 0;
}

void ScreenTerminal::Send(const string& data)
{
	if (m_fd) {
		Debug("Send '%s'",data.c_str());
		write(m_fd, data.c_str(), data.length());
		m_eof = false;
		tv.tv_sec = 10;
		tv.tv_usec = 0;
	} else 
		throw mgr_err::Error("screen_terminal","send");
	mgr_proc::Sleep(10);
}

bool ScreenTerminal::Eof()
{
	return m_eof;
}

void ClearColorSymbols(string & str) {
	size_t pos = str.find("\033[");
	while (pos != string::npos) {
		str.erase(pos,2);
		char ch = str[pos]; 
		while ((ch >= '0' && ch <= '9') || ch =='m' || ch == ';') {
			str.erase(pos,1);
			if (ch=='m') break;
			ch = str[pos];
		}
		pos = str.find("\033[");
	}
}

void ScreenTerminal::Read()
{
	string res;
	int sres;
	if ((sres = select(m_fd+1, &rfds, NULL, NULL, &tv))) {
		ssize_t rc = read(m_fd, m_buf, m_bufsize);
		if (rc < 0) {
			m_eof = true;
		}
		if (rc > 0) {
			string tmp(m_buf, rc);
			ClearColorSymbols(tmp);
			tmp = str::Replace(tmp, "\033", "");
			tmp = str::Replace(tmp, "\b", "");
			tmp = str::Replace(tmp, "\v", "");
			tmp = str::Replace(tmp, "\f", "");
			m_data += tmp;
//  			Debug("m_data '%s'", str::Trim(tmp).c_str());
		}
	} else {
		m_eof = true;
	}
}

string ScreenTerminal::ReadLine()
{
	string res;
	if (m_data.find("\n") == string::npos) this->Read();

	if (m_data.find("\n") != string::npos) {
		res = str::GetWord(m_data,"\n");
		return res;
	}
	return "";
}

string ScreenTerminal::ReadWhile(const string& str)
{
	string res;
	while (!this->Eof() && m_data.find(str) == string::npos)
		this->Read();
	size_t pos = m_data.find(str);
	res = m_data.substr(0,pos+str.length());
	m_data.erase(0,pos+str.length());
	return res;
}

void ScreenTerminal::Attach()
{
	string res;
	
	int fdm, fds;
	int rc;

	fdm = posix_openpt(O_RDWR);
	if (fdm < 0)
	{
		LogError("Error %d on posix_openpt()\n", errno);
		throw mgr_err::Error("open_terminal");
	}

	rc = grantpt(fdm);
	if (rc != 0)
	{
		LogError("Error %d on grantpt()\n", errno);
		throw mgr_err::Error("open_terminal");
	}

	rc = unlockpt(fdm);
	if (rc != 0)
	{
		LogError("Error %d on unlockpt()\n", errno);
		throw mgr_err::Error("open_terminal");
	}

	// Open the slave side ot the PTY
	fds = open(ptsname(fdm), O_RDWR);
	
	
 	struct termios slave_orig_term_settings; // Saved terminal settings 
 	struct termios new_term_settings; // Current terminal settings
 	
	rc = tcgetattr(fds, &slave_orig_term_settings); 

  // Set raw mode on the slave side of the PTY
	new_term_settings = slave_orig_term_settings; 
	cfmakeraw(&new_term_settings); 
	tcsetattr(fds, TCSANOW, &new_term_settings); 

	m_attach.SetEnv("TERM","xterm");
	m_attach.RedirectInTo(fds);
	m_attach.RedirectOutTo(fds);
	m_attach.Run();
	close(fds);
	m_fd = fdm;
}

void ScreenTerminal::Detach()
{
	mgr_proc::Sleep(100);
//	select(m_fd+1, NULL, &rfds, NULL, &tv);
 	mgr_proc::Execute(m_parent.GetDetachCmd()).Result();
	if (m_fd > 0)
		close(m_fd);
	m_fd = -1;
}

void ScreenTerminal::Close()
{
	Detach();
}

bool ScreenTerminal::Wait(const string& data)
{
	Debug("Waiting for [%s]",data.c_str());
	while(!Eof()) {
		size_t pos;
		Debug("Wait [%s] check m_data '%s'", data.c_str(), m_data.c_str());
		if ((pos = m_data.find(data)) != string::npos) {
			m_data.erase(0, pos);
			return true;
		} else {
			while(m_data.find("\n") != string::npos)
				str::GetWord(m_data, "\n");
			Read();
		}
	}
	Debug("Waiting timeout '%s'", data.c_str());
	return false;
}

bool ScreenTerminal::WaitInputCall(const string& data)
{
	Debug("Waiting for input call [%s]",data.c_str());
	while(!Eof()) {
		size_t pos;
		Debug("WaitIC [%s] check m_data '%s'",data.c_str(), m_data.c_str());
		if ((pos = m_data.find(data)) != string::npos) {
			m_data.erase(0, pos+1);
			if (m_data.find("\n") == string::npos)
				return true;
		} else {
			while(m_data.find("\n") != string::npos)
				str::GetWord(m_data,"\n");
			Read();
		}
	}
	Debug("WaitingIC timeout '%s'",data.c_str());
	return false;
}

