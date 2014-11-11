#ifndef __WIN_COMMON_H__
#define __WIN_COMMON_H__

#include <mgr/mgrstr.h>
#include <mgr/mgrfile.h>

#include <KnownFolders.h>
#include <ShTypes.h>

#define ISP_BASE "GSmanagerUsers"

#define ISP_USERS_GROUP "GSmgrUsers"
#define ISP_USERS_COMMENT "Group for ISPsystem GSmanager users"
#define ISP_USER_COMMENT_PREFIX "ISPsystem GSmanager user "
#define ISP_USER_GROUP_PREFIX "GSmgr_"
#define ISP_USER_GROUP_COMMENT_PREFIX "Group for ISPsystem ISPmanager user "
#define ISP_USERS_HOME "GSmgrUsers"

#define ISP_FTP_ROOT "GSmgrFTP"
#define ISP_FTPUSERS_HOME "GSmgrFTP\\LocalUser"
#define ISP_FTPUSERS_GROUP "HSmgrFTPUsers"
#define ISP_FTPUSERS_COMMENT "Group for ISPsystem GSmanager FTP users"
#define ISP_FTPUSER_COMMENT_PREFIX "ISPsystem GSmanager FTP user for user "

namespace sys_path {
	string GetSpecialPath(KNOWNFOLDERID kfid);
	string PathNice(const string& path);
	string GetVolumePath(const string& path);

/**
 * @brief Возвращает базовую директорию для системных пользователей (см. ниже представление структуры директорий ISPmanager)
 */
	string Users();

/**
 * @brief Возвращает базовую директорию для всех системных пользователей (в т.ч. ftp) ISPmanager (Users\ISP_BASE)
 */
	string UsersBase();
/**
 * @brief Возвращает базовую директорию для системных пользователей ISPmanager (Users\ISP_BASE\ISP_USERS_HOME)
 */
	string UsersRoot();
/**
 * @brief Возвращает базовую директорию для FTP, используемую ISPmanager (Users\ISP_BASE\ISP_FTP_ROOT)
 */
	 string FTPRoot();
/**
 * @brief Возвращает базовую директорию для FTP-пользователей, используемую ISPmanager (Users\ISP_BASE\ISP_FTPUSERS_HOME)
 */
	 string FTPUsersRoot();
/**
 * @brief Возвращает защищённую директорию пользователя ISPmanager (Users\ISP_BASE\ISP_USERS_HOME\[Username])
 */
	 string UserHomeSecurator(const string &name);
/**
 * @brief Возвращает домашнюю директорию пользователя ISPmanager (Users\ISP_BASE\ISP_USERS_HOME\[Username]\instance)
 */
	 string ServerInstanceDir(const string &name);
/**
 * @brief Возвращает домашнюю директорию FTP-пользователя ISPmanager (Users\ISP_BASE\ISP_FTPUSERS_HOME\[Username])
 */
	 string FTPUserHome(const string &name);

	 void CheckUsersBase();
	 void CheckUsersRoot();
	 void CheckFTPRoot();
	 void CheckFTPUsersRoot();

	 string System32();
	 string IIS_home();

	 bool MakeJunction(const string& target, const string& junkpath, mgr_file::Attrs* attrs = nullptr);
	 bool RemoveJunction(const string& junkpath);

	//WinBuiltinAdministratorsSid
	//WinNetworkServiceSid
	//WinLocalServiceSid
	//WinLocalSystemSid
	//все добавляются с доступом FILE_ALL_ACCESS через Protected рекурсивно
	 void AssignDefaultRights(const string& path);

	//WinIUserSid			FILE_GENERIC_READ | FILE_GENERIC_EXECUTE
	//WinBuiltinIUsersSid	FILE_GENERIC_READ | FILE_GENERIC_EXECUTE
	//все через Protected рекурсивно
	 void AssignWebRights(const string& path);
};

void FirewallOpenLocalPort(const string & name, const string & ip, int port, const string & protocol);
void RemoveFirewallRule(const string & name);

namespace sys_app {
	 string IIS_appcmd();
}

//Структура директорий ISPmanager представлена таки образом:
// [Директория системных пользователей Windows] - определена настройкой операционной системы
//     [ISPmanagerUsers] - директория всех системных пользователей ISPmanager
//          [ISPmgrUsers] - директория системных пользователей ISPmanager
//              [User1] - защищённый каталог пользователя User1
//				    [data] - домашняя директория пользователя User1
//              [User2] - защищённый каталог пользователя User2
//				    [data] - домашняя директория пользователя User2
//				. . .
//              [UserN] - защищённый каталог пользователя UserN
//				    [data] - домашняя директория пользователя UserN
//          [ISPmgrFTP] - директория системных FTP-пользователей ISPmanager
//				[FtpUser1] - домашняя директория FTP-пользователя FtpUser1, соединена (junction) с какой-либо директорией системного пользователя
//				[FtpUser2] - домашняя директория FTP-пользователя FtpUser2, соединена (junction) с какой-либо директорией системного пользователя
//				. . .
//				[FtpUserN] - домашняя директория FTP-пользователя FtpUserN, соединена (junction) с какой-либо директорией системного пользователя

#endif //__WIN_COMMON_H__