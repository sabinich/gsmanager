#ifndef SCREEN_H
#define SCREEN_H
#include <mgr/mgrstr.h>
#include <mgr/mgrproc.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "../ostemplate-lib/textlock.h"

template<class Type>
class AutoLock
{
	Type & Target;
public:
	AutoLock(Type & object):Target(object) {
		Target.Lock();
	};
	virtual ~AutoLock() {
		Target.Unlock();
	};
};

class ScreenHandler {
friend class ScreenTerminal;
private:
	string m_id;
	string m_workdir;
	string m_screen_path;
	string m_user;
	int m_pid;
	int m_fdm;
	int GetPid();
	string GetAttachCmd();
	string GetDetachCmd();
	string GetUser(){return m_user;}
	mgr_thread::TextLock<ScreenHandler> m_lock;
public:
	//module_name - Имя модуля, например "minecraft" id - идентификатор экземпляра сервера
	ScreenHandler(const string & module_name, const string & id, const string & user = "root");
	//Установить рабочую диру, если требуется
	void SetWorkDir(const string & dir){m_workdir=dir;}
	//Открыть сессию, cmd - комманда которая будет запущена в screen
	int Open(const string & cmd);
	//Отправить комманду в screen
	int SendCommand(const string & cmd);
	//Отправить Ctrl+C
	void SendCtrl_C();
	//Закрыть сессию screeen. Если cmd пустая то будет послан Ctrl+C иначе отправлена конкретная комманда. Далее метод будет ожиидать фактического закрытия данного скрина. 
	//Если в течение timeout не будет завершён процесс то ему будет отправлен сигнал KILL.
	void Close(const string & cmd="", int timeout=5);
	void Close(int timeout);
	//Возвращает true если Screen с таким  идентификатором запущен
	bool IsRunnig();
// 	static void GetList(ScreenInfoList & list);
};

class ScreenTerminal {
private:
	int m_fd;
	fd_set rfds;
    struct timeval tv;
	string m_data;
	bool m_eof;
	ScreenHandler & m_parent;
	void Attach();
	void Detach();
	mgr_proc::Execute m_attach;
	char * m_buf;
	int m_bufsize;
public:
	bool Eof();
	ScreenTerminal(ScreenHandler & parent);
	string ReadLine();
	string ReadWhile(const string & str);
	void Read();
	bool Wait(const string & data);
	bool WaitInputCall(const string & data);
	void Send(const string & data);
	void Close();
	~ScreenTerminal() {
		free(m_buf);
		Close();
	}
};

#endif