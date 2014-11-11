#include "win_common.h"
#include <api/vault.h>
#include <Shlwapi.h>
#include <Shlobj.h>
#include <WinIoCtl.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrlog.h>

MODULE("gsmini");

namespace {
#ifndef IO_REPARSE_TAG_VALID_VALUES
#define IO_REPARSE_TAG_VALID_VALUES 0xF000FFFF
#endif

#ifndef IsReparseTagValid
#define IsReparseTagValid(_tag) (!((_tag)&~IO_REPARSE_TAG_VALID_VALUES)&&((_tag)>IO_REPARSE_TAG_RESERVED_RANGE))
#endif

#ifndef REPARSE_DATA_BUFFER_HEADER_SIZE
	typedef struct _REPARSE_DATA_BUFFER {
		ULONG ReparseTag;
		USHORT ReparseDataLength;
		USHORT Reserved;
		union {
			struct {
				USHORT SubstituteNameOffset;
				USHORT SubstituteNameLength;
				USHORT PrintNameOffset;
				USHORT PrintNameLength;
				ULONG Flags;
				WCHAR PathBuffer[1];
			} SymbolicLinkReparseBuffer;
			struct {
				USHORT SubstituteNameOffset;
				USHORT SubstituteNameLength;
				USHORT PrintNameOffset;
				USHORT PrintNameLength;
				WCHAR PathBuffer[1];
			} MountPointReparseBuffer;
			struct {
				UCHAR DataBuffer[1];
			} GenericReparseBuffer;
		};
	} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

#define IO_REPARSE_TAG_SYMLINK (0xA000000CL)
#define REPARSE_DATA_BUFFER_HEADER_SIZE FIELD_OFFSET(REPARSE_DATA_BUFFER,GenericReparseBuffer)
#endif

inline DWORD get_attributes(PCWSTR path) {
	return ::GetFileAttributes(path);
}

inline bool set_attributes(PCWSTR path, DWORD attr) {
	return ::SetFileAttributes(path, attr) != 0;
}

HANDLE OpenLinkHandle(PCWSTR path, ACCESS_MASK acc = 0, DWORD create = OPEN_EXISTING) {
	return ::CreateFile(path, acc, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr, create,
		FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
		nullptr);
}

class REPARSE_BUF {
public:
	bool get(PCWSTR path) {
		DWORD attr = get_attributes(path);
		if ((attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
			ResHandle hLink(OpenLinkHandle(path));
			if (hLink) {
				DWORD dwBytesReturned;
				return ::DeviceIoControl(hLink, FSCTL_GET_REPARSE_POINT, nullptr, 0, buf, (DWORD)size(), &dwBytesReturned, nullptr) &&
					IsReparseTagValid(rdb.ReparseTag);
			}
		}
		return false;
	}

	bool set(PCWSTR path) const {
		bool ret = false;
		if (IsReparseTagValid(rdb.ReparseTag)) {
			DWORD attr = get_attributes(path);
			if (attr != INVALID_FILE_ATTRIBUTES) {
				if (attr & FILE_ATTRIBUTE_READONLY) {
					set_attributes(path, attr & ~FILE_ATTRIBUTE_READONLY);
				}
				mgr_win_perms::ProcessPrivilege CreateSymlinkPrivilege(SE_CREATE_SYMBOLIC_LINK_NAME);
				ResHandle hLink(OpenLinkHandle(path, GENERIC_WRITE));
				if (hLink) {
					DWORD dwBytesReturned;
					ret = ::DeviceIoControl(hLink, FSCTL_SET_REPARSE_POINT,(PVOID)&rdb, rdb.ReparseDataLength + REPARSE_DATA_BUFFER_HEADER_SIZE,
						nullptr, 0, &dwBytesReturned, nullptr) != 0;
				}
				if (attr & FILE_ATTRIBUTE_READONLY) {
					set_attributes(path, attr);
				}
			}
		}
		return ret;
	}

	bool fill(PCWSTR PrintName, size_t PrintNameLength, PCWSTR SubstituteName, size_t SubstituteNameLength) {
		bool Result = false;
		rdb.Reserved = 0;

		switch (rdb.ReparseTag) {
		case IO_REPARSE_TAG_MOUNT_POINT:
			rdb.MountPointReparseBuffer.SubstituteNameOffset=0;
			rdb.MountPointReparseBuffer.SubstituteNameLength=static_cast<WORD>(SubstituteNameLength*sizeof(wchar_t));
			rdb.MountPointReparseBuffer.PrintNameOffset=rdb.MountPointReparseBuffer.SubstituteNameLength+2;
			rdb.MountPointReparseBuffer.PrintNameLength=static_cast<WORD>(PrintNameLength*sizeof(wchar_t));
			rdb.ReparseDataLength=FIELD_OFFSET(REPARSE_DATA_BUFFER,MountPointReparseBuffer.PathBuffer)+rdb.MountPointReparseBuffer.PrintNameOffset+rdb.MountPointReparseBuffer.PrintNameLength+1*sizeof(wchar_t)-REPARSE_DATA_BUFFER_HEADER_SIZE;

			if (rdb.ReparseDataLength+REPARSE_DATA_BUFFER_HEADER_SIZE<=static_cast<USHORT>(MAXIMUM_REPARSE_DATA_BUFFER_SIZE/sizeof(wchar_t))) {
				wmemcpy(&rdb.MountPointReparseBuffer.PathBuffer[rdb.MountPointReparseBuffer.SubstituteNameOffset/sizeof(wchar_t)],SubstituteName,SubstituteNameLength+1);
				wmemcpy(&rdb.MountPointReparseBuffer.PathBuffer[rdb.MountPointReparseBuffer.PrintNameOffset/sizeof(wchar_t)],PrintName,PrintNameLength+1);
				Result=true;
			}

			break;
		case IO_REPARSE_TAG_SYMLINK:
			rdb.SymbolicLinkReparseBuffer.PrintNameOffset=0;
			rdb.SymbolicLinkReparseBuffer.PrintNameLength=static_cast<WORD>(PrintNameLength*sizeof(wchar_t));
			rdb.SymbolicLinkReparseBuffer.SubstituteNameOffset=rdb.MountPointReparseBuffer.PrintNameLength;
			rdb.SymbolicLinkReparseBuffer.SubstituteNameLength=static_cast<WORD>(SubstituteNameLength*sizeof(wchar_t));
			rdb.ReparseDataLength=FIELD_OFFSET(REPARSE_DATA_BUFFER,SymbolicLinkReparseBuffer.PathBuffer)+rdb.SymbolicLinkReparseBuffer.SubstituteNameOffset+rdb.SymbolicLinkReparseBuffer.SubstituteNameLength-REPARSE_DATA_BUFFER_HEADER_SIZE;

			if (rdb.ReparseDataLength+REPARSE_DATA_BUFFER_HEADER_SIZE<=static_cast<USHORT>(MAXIMUM_REPARSE_DATA_BUFFER_SIZE/sizeof(wchar_t))) {
				wmemcpy(&rdb.SymbolicLinkReparseBuffer.PathBuffer[rdb.SymbolicLinkReparseBuffer.SubstituteNameOffset/sizeof(wchar_t)],SubstituteName,SubstituteNameLength);
				wmemcpy(&rdb.SymbolicLinkReparseBuffer.PathBuffer[rdb.SymbolicLinkReparseBuffer.PrintNameOffset/sizeof(wchar_t)],PrintName,PrintNameLength);
				Result=true;
			}
			break;
		}
		return Result;
	}

	size_t size() const {
		return sizeof(buf);
	}

	PREPARSE_DATA_BUFFER operator->() const {
		return (PREPARSE_DATA_BUFFER)&rdb;
	}
private:
	union {
		BYTE buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
		REPARSE_DATA_BUFFER	rdb;
	};
};

bool is_junction(PCWSTR path) {
	REPARSE_BUF rdb;
	return rdb.get(path) && rdb->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT;
}

bool delete_reparse_point(PCWSTR path) {
	bool ret = false;
	DWORD attr = get_attributes(path);
	if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
		if (attr & FILE_ATTRIBUTE_READONLY) {
			set_attributes(path, attr & ~FILE_ATTRIBUTE_READONLY);
		}
		REPARSE_BUF rdb;
		if (rdb.get(path)) {
			mgr_win_perms::ProcessPrivilege CreateSymlinkPrivilege(SE_CREATE_SYMBOLIC_LINK_NAME);
			ResHandle hLink(OpenLinkHandle(path, GENERIC_WRITE));
			if (hLink) {
				REPARSE_GUID_DATA_BUFFER rgdb = {0};
				rgdb.ReparseTag = rdb->ReparseTag;
				DWORD dwBytesReturned;
				ret = ::DeviceIoControl(hLink, FSCTL_DELETE_REPARSE_POINT, &rgdb,
					REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, nullptr, 0, &dwBytesReturned, 0) != 0;
			}
		}
		if (attr & FILE_ATTRIBUTE_READONLY) {
			set_attributes(path, attr);
		}
	}
	return ret;
}
} //end of private namespace

namespace sys_path {

string GetSpecialPath(KNOWNFOLDERID kfid) {
	PWSTR res = nullptr;
	string result;
	if (SHGetKnownFolderPath(kfid, 0, nullptr, &res) == S_OK) {
		result = str::u16string(res);
	}
	if (res)
		::CoTaskMemFree(res);
	return result;
}

string PathNice(const string& path) {
	WCHAR ret[MAX_PATH];
	str::u16string expanded = ::ExpandEnvironmentStrings(str::u16string(path).c_str(), ret, sizeof(ret)) ? str::u16string(ret) : str::u16string();
	WCHAR retd[MAX_PATH];
	return ::PathCanonicalize(retd, expanded.c_str()) ? str::u16string(retd) : str::u16string();
}

string GetVolumePath(const string& path) {
	std::vector<wchar_t> buf(MAX_PATH, L'\0');
	if (GetVolumePathName(str::u16string(path).c_str(), buf.data(), MAX_PATH))
		return str::u16string(buf.data());
	return path;
}

string Users() {
	string ret = PathNice("%PUBLIC%\\..\\");
	return (ret.empty() || (ret == "\\")) ? PathNice("%ALLUSERSPROFILE%\\..\\") : ret;
}

string UsersBase() {
	return mgr_file::ConcatPath(Users(), ISP_BASE);
}

string UsersRoot() {
	return mgr_file::ConcatPath(UsersBase(), ISP_USERS_HOME);
}

string FTPRoot() {
	return mgr_file::ConcatPath(UsersBase(), ISP_FTP_ROOT);
}

string FTPUsersRoot() {
	return mgr_file::ConcatPath(UsersBase(), ISP_FTPUSERS_HOME);
}

string UserHomeSecurator(const string &name) {
	return mgr_file::ConcatPath(UsersRoot(), name);
}

string ServerInstanceDir(const string &name) {
	return mgr_file::ConcatPath(UserHomeSecurator(name), "instance");
}

string FTPUserHome(const string &name) {
	return mgr_file::ConcatPath(FTPUsersRoot(), name);
}

void AssignDefaultRights(const string& path) {
	if (!mgr_file::Exists(path))
		return;
	mgr_file::Attrs attr;
	attr.UseProtectedSet();
	attr.SetAccess(mgr_user::WinSid(WinBuiltinAdministratorsSid).name(), FILE_ALL_ACCESS);
	attr.SetAccess(mgr_user::WinSid(WinNetworkServiceSid).name(), FILE_ALL_ACCESS);
	attr.SetAccess(mgr_user::WinSid(WinLocalServiceSid).name(), FILE_ALL_ACCESS);
	attr.SetAccess(mgr_user::WinSid(WinLocalSystemSid).name(), FILE_ALL_ACCESS);

	//attr.SetAccess(mgr_user::WinSid(WinBuiltinUsersSid).name(), ACCESS_NONE); не нужно, так как используем ProtectedSet

	attr.Set(path, true);
}

void AssignWebRights(const string& path) {
	if (!mgr_file::Exists(path))
		return;
	mgr_file::Attrs attr;
	attr.SetAccess(mgr_user::WinSid(WinIUserSid).name(), FILE_GENERIC_READ | FILE_GENERIC_EXECUTE);
	attr.SetAccess(mgr_user::WinSid(WinBuiltinIUsersSid).name(), FILE_GENERIC_READ | FILE_GENERIC_EXECUTE);

	attr.Set(path, true);
}

void CheckUsersBase() {
	string path = UsersBase();
	if (mgr_file::Exists(path))
		return;

	mgr_file::MkDir(path);
	AssignDefaultRights(path);
}

void CheckUsersRoot() {
	CheckUsersBase();
	string path = UsersRoot();
	if (mgr_file::Exists(path))
		return;
	mgr_file::MkDir(path);
	AssignDefaultRights(path);
}

void CheckFTPRoot() {
	CheckUsersBase();
	string path = FTPRoot();
	if (mgr_file::Exists(path))
		return;
	mgr_file::MkDir(path);
	AssignDefaultRights(path);
}

void CheckFTPUsersRoot() {
	CheckFTPRoot();
	string path = FTPUsersRoot();
	if (mgr_file::Exists(path))
		return;
	mgr_file::MkDir(path);
	AssignDefaultRights(path);
}

string System32() {
	return GetSpecialPath(FOLDERID_System);
}

string IIS_home() {
	return mgr_file::ConcatPath(System32(), "inetsrv");
}

bool MakeJunction(const string& target, const string& junkpath, mgr_file::Attrs* attrs) {
	if (mgr_file::Exists(junkpath))
		return false;
	if (!mgr_file::Exists(target))
		return false;
	mgr_file::Info nfo(target);
	if (!nfo.IsDir())
		return false;
	mgr_file::MkDir(junkpath, attrs);
	str::u16string tgt(target);
	str::u16string junk(junkpath);
	ResHandle hLink(OpenLinkHandle(str::u16string(junkpath).c_str(), GENERIC_WRITE));
	if ((bool)hLink) {
		str::u16string SubstituteName("\\??\\" + target);
		REPARSE_BUF rdb;
		rdb->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
		rdb.fill(tgt.c_str(), tgt.size(), SubstituteName.c_str(), SubstituteName.size());
		if (rdb.set(junk.c_str())) {
			return true;
		}
	}
	mgr_file::Remove(junkpath);
	return false;
}

bool RemoveJunction(const string& junkpath) {
	str::u16string path(junkpath);
	bool ret = delete_reparse_point(path.c_str());
	if (!ret)
		return false;
	if (!mgr_file::Info(junkpath).IsDir())
		return false;
	mgr_file::Remove(junkpath);
	return ret;
}

} //end of sys_path namespace

namespace sys_app {

string IIS_appcmd() {
	return mgr_file::ConcatPath(sys_path::IIS_home(), "appcmd.exe");
}

} //end of sys_app namespace

void FirewallOpenLocalPort(const string & name, const string & ip, int port, const string & protocol)
{
	string res = mgr_proc::Execute("powershell.exe import-module NetSecurity; New-NetFirewallRule -Name "+name+" -DisplayName "+name+" -LocalPort "+str::Str(port)+" -LocalAddress "+ip+" -Protocol "+protocol, mgr_proc::Execute::efOut).Str();
	Debug("Add Rule res %s",res.c_str());
}

void RemoveFirewallRule(const string & name)
{
	string res = mgr_proc::Execute("powershell.exe import-module NetSecurity; Remove-NetFirewallRule -Name "+name , mgr_proc::Execute::efOut).Str();
	Debug("Remove Rule res %s",res.c_str());
}