#ifndef INSTANCER_H
#define INSTANCER_H
#include <api/action.h>
#include <mgr/mgrfile.h>
#include <mgr/mgruser.h>
#include <mgr/mgrproc.h>
#include <mgr/mgrstr.h>
#include "portmgr.h"
#include "distr.h"
void GetInstanceList(StringList & list, const string & game = "");

//Класс для работы с файлами игрового сервера
class ServerInstanceInfo {
public:
	string UserName;
	string Id;
	string Game;
	ServerInstanceInfo(isp_api::Session & ses); // Сконструировать класс на основе сесии
	ServerInstanceInfo(const string & game, const string & id); // Сконструировать класс на основе имени модуля и id сервера
	ServerInstanceInfo(const string & user); // Сконструировать на основе имени пользователя
	string GetUserAccessPath(bool absolute = true) // Получить путь к файлам игрового сервера для которых есть дотуп.
	{
		
		string res = absolute?GetInstancePath():"instance";
		if (Game == "csgo") res = mgr_file::ConcatPath(res, Game);
		if (Game == "teamfortress") res = mgr_file::ConcatPath(res, "tf");
		if (Game == "cs16") res = mgr_file::ConcatPath(res, "cstrike");

		return res;
	}
	string GetInstancePath(); // Получить путь до файлов игрового сервера
	string GetMgrDataPath(); // Получить поть до служебный файлов gsmini 
	string GetParam(const string & param); // Получить служевный параметр по имени
	void SetParam(const string & param, const string & val); // Установить служебный параметр
	
	string GetIp(); // Получить ip адрес сервера
	void SetIp(const string & ip); // Установить ip - адрес сервера
	int GetSlots(); // Слоты сервер
	void SetSlots(int slots);
	int GetPort(); // Порт сервера
	void SetPort(int slots);
	string GetStatus(); // Последний известные статус сервера
	void SetStatus(const string & status);
	void InstallDist(DistInfo & dist); // Развернуть дитрибутив
	void ApplyRight(); // Установить права на файлы
	void ReinitPort(); // Перепроверить порт
	bool NeedRestart(); // Тербует ли сервер перезапуска
	void SetNeedRestart(bool restart = true);
	void SetDisabled(bool disabled = true);
	bool IsDisabled(); // Сервер отключен администратором
	void ClearInstance();
};
#endif