#include <windows.h>
#include <iostream>
#include <memory>
#include <mgr/mgrstr.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrfile.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrclient.h>
#include <mgr/mgrnet.h>
#include <api/action.h>
#include <api/autotask.h>
#include <api/vault.h>
#include <installer/pkg.h>
#include <tchar.h>
#include "mini/instance.h"
#include <psapi.h>
#include <WinCred.h>

#include "mini/win_common.h"
//определение всего, что нужно для Windows
#include <winlibcntl.h>
USECORELIB("libmgr")
USECORELIB("ispapi")
USECORELIB("parser")
USELIB("Ws2_32")
USELIB("Shlwapi")
USELIB("Psapi")

MODULE("win_service");

#define	PREFIX	L"ISPsystem GSmanager "
#define MAX_START_STOP_DELAY 5000 //milisec

static SERVICE_STATUS serviceStatus;
static SERVICE_STATUS_HANDLE serviceHandle;
static _TCHAR* serviceName;
static _TCHAR* serviceBinary;

static void WINAPI ServiceMain(int argc, _TCHAR* argv[]);
static void WINAPI ServiceControl(DWORD dwControlCode);
static int DoServiceControl(_TCHAR **argv);

static const char *USAGE = "Usage: core <service name> [start|stop|restart|install|remove]\n"
	"start, stop, restart, install, remove - windows service commands\n\n"
	"It can be started without any command. In this case it will be run in debug mode\n";



#include <comdef.h>
#define MAX_NAME 256



void RunAsUser(const str::u16string & cmd, const str::u16string & dir, const str::u16string & user, const str::u16string & pass) {
PROCESS_INFORMATION m_procinfo;

	m_procinfo.hProcess = INVALID_HANDLE_VALUE;
	m_procinfo.hThread = INVALID_HANDLE_VALUE;

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = true;
	sa.lpSecurityDescriptor = nullptr;

	STARTUPINFO si;
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = 0;		
	si.hStdInput = INVALID_HANDLE_VALUE;
	si.hStdOutput = INVALID_HANDLE_VALUE;
	si.hStdError = INVALID_HANDLE_VALUE;

	BOOL res = FALSE;



		/*HANDLE user_token;
		BOOL res_a = LogonUser(user.c_str(),
			L".",
			(LPCWSTR) pass.c_str(),
			LOGON32_LOGON_INTERACTIVE,
			LOGON32_PROVIDER_DEFAULT,
			&user_token);

		if (res_a == FALSE)
			throw mgr_err::System("LogonUser");

		res = CreateProcessAsUser(user_token,
								nullptr,
								(LPWSTR) cmd.c_str(),
								nullptr,
								nullptr,
								TRUE, //FIXME надо ли наследовать дескрипторы пользователю ?
								CREATE_UNICODE_ENVIRONMENT,
								nullptr,
								(LPCWSTR) dir.c_str(),
								&si,
								&m_procinfo);*/


	res = CreateProcessWithLogonW(		(LPCWSTR) user.c_str(),
										nullptr,
										(LPCWSTR) pass.c_str(),
										LOGON_WITH_PROFILE,
										nullptr,
										(LPWSTR) cmd.c_str(),
										CREATE_UNICODE_ENVIRONMENT|CREATE_NEW_CONSOLE,
										nullptr,
										(LPCWSTR) dir.c_str(),
										&si,
										&m_procinfo
								);

	if (!res)
		throw mgr_err::System("CreateProccess");

	CloseHandle(m_procinfo.hThread);


	LogExtInfo("Start pid %d",  m_procinfo.dwProcessId);

	
	DWORD code = 0;
	do {
		GetExitCodeProcess(m_procinfo.hProcess, &code);
	} while (code == STILL_ACTIVE);

	CloseHandle(m_procinfo.hProcess);
}


string GetLogonFromToken (ResHandle hToken) 
{
	DWORD dwSize = MAX_NAME;
	BOOL bSuccess = FALSE;
	DWORD dwLength = 0;
	string strUser = "";
	PTOKEN_USER ptu = NULL;

	if (!GetTokenInformation(
		hToken,         // handle to the access token
		TokenUser,    // get information about the token's groups 
		NULL,   // pointer to PTOKEN_USER buffer
		0,              // size of buffer
		&dwLength       // receives required buffer size
		)) 
	{
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			return "";		
		}
	}

	std::vector<BYTE> data(dwLength);
	if (!GetTokenInformation(
		hToken,         // handle to the access token
		TokenUser,    // get information about the token's groups 
		(LPVOID) data.data(),   // pointer to PTOKEN_USER buffer
		dwLength,       // size of buffer
		&dwLength       // receives required buffer size
		)) 
	{
		return "";
	}
	
	return mgr_user::WinSid(((PTOKEN_USER)data.data())->User.Sid).name();
}

string GetUserFromProcess(const DWORD procId)
{
	ResHandle hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, procId); 
	if (!hProcess)
		return "";
	

	HANDLE h;
	if (!OpenProcessToken(hProcess, TOKEN_QUERY, (PHANDLE)&h))
	{
		return "";
	}
	ResHandle hToken(h);
	return GetLogonFromToken(hToken);	
}


void KillAllProcessOfUser(const string & username) 
{

	DWORD aProcesses[65000], cbNeeded, cProcesses;
	if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded )) return;
	cProcesses = cbNeeded / sizeof(DWORD);
	for (unsigned int i = 0; i < cProcesses; i++ ) {
		if(aProcesses[i] == 0)
			continue;
		ResHandle proc = ::OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, aProcesses[i]);
		if (!proc)
			continue;
		HMODULE hMod;
		DWORD cbNeeded;
		std::vector<wchar_t> buf(MAX_PATH);
		if (::EnumProcessModules(proc, &hMod, sizeof(hMod), &cbNeeded)) {
			GetModuleBaseName(proc, hMod, buf.data(), (DWORD)buf.size()/sizeof(wchar_t));
			string p_name = str::u16string(buf.data());
			p_name = str::Trim(str::Lower(p_name));
			try {
				if (GetUserFromProcess(aProcesses[i]) == username) 
					mgr_proc::Kill(aProcesses[i]);
			} catch (...) {
			}
		}
	}
}

void MyCredSet(const str::u16string &name, const str::u16string &value) {
	CREDENTIALW cred = {0};
	cred.Type = CRED_TYPE_GENERIC_CERTIFICATE;
	cred.Persist = CRED_PERSIST_ENTERPRISE;
	cred.TargetName = (PWSTR)name.c_str();
	cred.UserName = (PWSTR)name.c_str();
	cred.CredentialBlob = (PBYTE)value.c_str();
	cred.CredentialBlobSize = (DWORD)(sizeof(WCHAR) * value.size());
	if (!::CredWrite(&cred, 0))
		throw mgr_err::Error("cred_write");
}

bool CheckCred(const str::u16string &name, int type) {
	PCREDENTIALW cred = nullptr;
	str::u16string res;
	if(::CredRead(name.c_str(), type, 0, &cred)) {
		res = str::u16string((WCHAR*)cred->CredentialBlob, cred->CredentialBlobSize / sizeof(WCHAR));

		Debug("blobsize %ld", cred->CredentialBlobSize);
		Debug("attrcnt %ld", cred->AttributeCount);
		if (cred->Comment) Debug("Comments %s", string(str::u16string(cred->Comment)).c_str());
		Debug("Flags %ld", cred->Flags);
		if (cred->TargetAlias) Debug("TAlias %s", string(str::u16string(cred->TargetAlias)).c_str());
		if (cred->TargetName) Debug("TName %s", string(str::u16string(cred->TargetName)).c_str());
		if (cred->UserName) Debug("UName %s", string(str::u16string(cred->UserName)).c_str());

		CredFree(cred);
		return true;
	} 
	Debug("Err %ld", GetLastError());
	return false;
}


class CommonGameServerService {
protected:
	ServerInstanceInfo m_srv;
	std::shared_ptr<mgr_proc::Execute> m_exec;
public:
	CommonGameServerService(const ServerInstanceInfo & inst) : m_srv(inst)
	{
	}
	virtual void Init()
	{		
		Debug("Init Service");
		string credname = mgr_net::GetHostName()+"\\"+m_srv.UserName;

		str::u16string cred_name(credname);
		//::CredDelete(cred_name.c_str(), CRED_TYPE_GENERIC_CERTIFICATE, 0);
		//::CredDelete(cred_name.c_str(), CRED_TYPE_DOMAIN_VISIBLE_PASSWORD, 0);
		
		for (int i = 0; i < 8;i++) {
		if (CheckCred(credname,i))
				Debug("Has Cred %x",i);

		}



		/*m_exec.reset(new mgr_proc::Execute(""+mgr_proc::Escape(m_srv.GetParam("exe_path"))+" "+m_srv.GetParam("exec_args")));
		m_exec->SetDir(m_srv.GetParam("work_dir"));
		//m_exec->SetEnv("ARGS_PARAM",);
		Debug("WorkDir '%s'",m_srv.GetParam("work_dir").c_str());
		Debug("Execute '%s' '%s'",m_srv.GetParam("exe_path").c_str(),m_srv.GetParam("exec_args").c_str());
		string pass = isp_api::vault::GetPassword(m_srv.UserName);
		Debug("user [%s] pass[%s]", m_srv.UserName.c_str(), pass.c_str());
		m_exec->SetUser(m_srv.UserName, pass);	*/


		//m_exec.reset(new mgr_proc::Execute("runas /savecred /user:"+m_srv.UserName+" \""+mgr_proc::Escape(m_srv.GetParam("exe_path"))+" "+m_srv.GetParam("exec_args")+"\""));
		m_exec.reset(new mgr_proc::Execute("runas /savecred /user:"+m_srv.UserName+" \"ping -t 8.8.8.8\""));
		m_exec->SetDir(m_srv.GetParam("work_dir"));
		//m_exec->SetEnv("ARGS_PARAM",);
		Debug("WorkDir '%s'",m_srv.GetParam("work_dir").c_str());
		Debug("Execute '%s' '%s'",m_srv.GetParam("exe_path").c_str(),m_srv.GetParam("exec_args").c_str());
		string pass = isp_api::vault::GetPassword(m_srv.UserName);
		
		Debug("Set cred '%s' to '%s'",credname.c_str(),pass.c_str());
		//MyCredSet(mgr_net::GetHostName()+"\\"+m_srv.UserName, L"ca9902b9721sd@sd1e18b1fdab0");
		Debug("user [%s] pass[%s]", m_srv.UserName.c_str(), pass.c_str());
		

	}
	virtual void Run()
	{//
		//Jun 10 09:01:20 [33568:1] win_service DEBUG win_service.cpp:237 user [arma3_4] pass[ca9902b9721sd@sd1e18b1fdab0]
		Debug("Run service");
	//	string res = m_exec->Str();
		/*Debug("WorkDir '%s'",m_srv.GetParam("work_dir").c_str());
		Debug("Execute '%s' '%s'",m_srv.GetParam("exe_path").c_str(),m_srv.GetParam("exec_args").c_str());
		Debug("user [%s] pass[%s]", m_srv.UserName.c_str(), isp_api::vault::GetPassword(m_srv.UserName).c_str());
		RunAsUser(m_srv.GetParam("exe_path")+" "+m_srv.GetParam("exec_args"), m_srv.GetParam("work_dir"), m_srv.UserName, "ca9902b9721sd@sd1e18b1fdab0");*/
		//RunAsUser("ping.exe -t 8.8.8.8", m_srv.GetParam("work_dir"), m_srv.UserName, isp_api::vault::GetPassword(m_srv.UserName));


//		Debug("Service stoped with '%s'",res.c_str());
	}
	virtual void Stop()
	{
		Debug("Stop Service");
		KillAllProcessOfUser(m_srv.UserName);
	//	m_exec->Terminate();
	}
};

//Jun 09 12:05:11 [17568:1] win_service DEBUG win_service.cpp:148 user [arma3_4] pass[ca9902b9721sd@sd1e18b1fdab0]//Jun 09 12:05:11 [17568:1] proc EXTINFO arg: 'arma3server.exe -port=33000 -ip=94.250.249.113 -config=server.cfg -world=empty'


static std::shared_ptr<CommonGameServerService> servicePtr;

static BOOL WINAPI SignalHandler(DWORD dwCtrlType) {
	Trace("signal %d", dwCtrlType);
	servicePtr->Stop();
	return TRUE;
}

extern "C" __declspec(dllexport) int core_main(int argc, _TCHAR* argv[]);

int _tmain(int argc, _TCHAR* argv[]) {
	
	SERVICE_TABLE_ENTRY DispatcherTable[2];
	if (argc < 2) {
		//FUCK!
	}
	serviceName = argv[1];
	serviceBinary = argv[0];	
	servicePtr.reset(new CommonGameServerService(ServerInstanceInfo(str::u16string(serviceName))));
	DispatcherTable[0].lpServiceName = serviceName;
	DispatcherTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION) ServiceMain;
	DispatcherTable[1].lpServiceName = 0;
	DispatcherTable[1].lpServiceProc = 0;
	
	SetCurrentDirectory(str::u16string(mgr_file::Path(str::u16string(serviceBinary))).c_str());
	if (!StartServiceCtrlDispatcher(DispatcherTable)) {
		if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
			try {
				if (argc == 3)
					return DoServiceControl(argv);

				string manager = isp_api::GetMgrName();
				if (mgr_file::Exists(mgr_file::Name(str::u16string(argv[0])))) {					
					servicePtr->Init();
					servicePtr->Run();
					return 0;
				} else {
					std::cout << USAGE << std::endl;
				}
			} catch (const std::exception& e) {
				std::cerr << e.what() << std::endl;
				return EXIT_FAILURE;
			}
		}
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

void WINAPI ServiceMain(int argc, _TCHAR* argv[]) {
	
	serviceHandle = RegisterServiceCtrlHandler(nullptr, ServiceControl);
	if (!serviceHandle) return;

	try {
		
		serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		serviceStatus.dwCurrentState = SERVICE_START_PENDING;
		serviceStatus.dwControlsAccepted = 0;
		serviceStatus.dwWin32ExitCode = NO_ERROR;
		serviceStatus.dwServiceSpecificExitCode = NO_ERROR;
		serviceStatus.dwCheckPoint = 0;
		serviceStatus.dwWaitHint = MAX_START_STOP_DELAY;

		SetServiceStatus(serviceHandle, &serviceStatus);

		isp_api::SetMgrName(str::u16string(argc < 1 ? L"core" : argv[0]));
		string manager = isp_api::GetMgrName();
		mgr_proc::SingleInstance pid(manager);

		mgr_log::Init(manager);
		servicePtr->Init();

		serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
		serviceStatus.dwCurrentState = SERVICE_RUNNING;
		serviceStatus.dwWaitHint = 0;
		SetServiceStatus(serviceHandle, &serviceStatus);

		servicePtr->Run();

		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		serviceStatus.dwControlsAccepted = 0;	
		serviceStatus.dwWaitHint = 0;
	} catch (...) {
		serviceStatus.dwWaitHint = 0;
		serviceStatus.dwControlsAccepted = 0;		
		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
		serviceStatus.dwServiceSpecificExitCode = EXIT_FAILURE;
	}

	SetServiceStatus(serviceHandle, &serviceStatus);
}

void WINAPI ServiceControl(DWORD dwControlCode) {
	if (dwControlCode == SERVICE_CONTROL_STOP || dwControlCode == SERVICE_CONTROL_SHUTDOWN) {		
		serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		serviceStatus.dwWaitHint = MAX_START_STOP_DELAY;
		SetServiceStatus(serviceHandle, &serviceStatus);
		servicePtr->Stop();
	}
}

class ServiceHandle {
public:
	ServiceHandle(SC_HANDLE handle): m_handle(handle) {
		if (!handle)
			throw std::runtime_error("Failed to create service handle");
	}
	~ServiceHandle() { ::CloseServiceHandle(m_handle); }
	operator SC_HANDLE() { return m_handle; }
private:
	SC_HANDLE m_handle;
};

int DoServiceControl(_TCHAR **argv) {
	try {
		if (lstrcmpi(argv[2], L"start") == 0) {
			if (!mgr_proc::sc::Start(serviceName))
				throw std::runtime_error("Failed to start service");

		} else if (lstrcmpi(argv[2], L"stop") == 0) {
			if (!mgr_proc::sc::Stop(serviceName, &serviceStatus))
				throw std::runtime_error("Failed to stop service");

		} else if (lstrcmpi(argv[2], L"restart") == 0) {
			if (!mgr_proc::sc::Restart(serviceName, &serviceStatus))
				throw std::runtime_error("Failed to restart service");

		} else if (lstrcmpi(argv[2], L"install") == 0) {
			if (wcschr(argv[0], ':') == 0 || !mgr_file::Exists(str::u16string(argv[0])))
				throw std::runtime_error("You should give full binary path");
			str::u16string dname(PREFIX);
			dname += serviceName;
			str::u16string cmd(argv[0]);
			cmd.push_back(' ');
			cmd += serviceName;
			const string mgrname = str::u16string(argv[1]);

			if (!mgr_proc::sc::Create(serviceName, dname.c_str(), cmd.c_str(), true))
				throw std::runtime_error("Failed to register service");

			if (!mgr_proc::sc::Start(serviceName))
				throw std::runtime_error("Failed to start service");

		} else if (lstrcmpi(argv[2], L"remove") == 0 ) {
			const string mgrname = str::u16string(argv[1]);

			if (mgrname == "core") {
				//remove autoupdate user
				try {
					isp_api::task::RemoveUser();
				} catch(...) {
					//×òî òóò ïîäåëàòü?
				}
			}

			if (!mgr_proc::sc::Remove(argv[1], &serviceStatus))
				throw std::runtime_error("Failed to delete service");

			if (mgrname != "core") {
				mgr_client::Local local("core", "core");
				local.Query("func=mgr.delete&elid=?", mgrname);
			}
		} else {
			std::cerr << "Unknown command" << std::endl << USAGE;
			return EXIT_FAILURE;
		}

		std::cout << string(str::u16string(argv[2])) << " complete\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& e) {
		std::cerr << e.what() << ". Error: " << str::Str(::GetLastError()) << std::endl;
		return EXIT_FAILURE;
	}
}
