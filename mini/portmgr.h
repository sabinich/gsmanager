#ifndef PORTMGR_H
#define PORTMGR_H
#include <parser/conf_cache.h>
#include <parser/mgrconf.h>

extern ConfRef PortMgrConf;
namespace PortMgr
{
	class PortRec : public HeadDataSet {
	public:
		dd::Field Ip;
		dd::IntField Port;
		dd::Field Owner;
		string NewRecordTemplate()
		{
				return "Port __ip__ __port__ __owner__\n";
		}
		virtual string KeyVal(RecordPtr rec) const
		{
			return rec.Get(Ip.index)+"_"+rec.Get(Port.index);
		}
		
		bool Parse(Cf cf);
		PortRec():Ip(this,"ip"), Port(this,"port"), Owner(this,"owner"){}
	};
	
	class PortConfig : public MgrCachedConfig<PortConfig> {
	public:
		PortConfig() {
				mgr_token::ParamsPtr params(new mgr_token::Params());
				params->Add(new mgr_token::CharToken(" \t", mgr_token::Token::T_DELIM));
				params->Add(new mgr_token::CharToken("\n", mgr_token::Token::T_SPECTOKEN));
				Register(params);
				Add<PortRec>();
		}
	};
	
	/*
	 * Занимает порт. 
	 * owner - владелец (в данном случае идентификатор инстанса сервера игры)
	 * ip - IP-адрес
	 * port - порт. Если 0 то будет выдан из пула. Сюда нужно передавать то значение которое прилетело из gsmgr. Тогда при изменении порта всё старый корректно освободится и новый заёмется.
	 */
	int AllocPort(const string & owner, const string & ip, int port=0, string poll="main");

	/*
	 * Освобождает порт. Если всё правильно сделано должна зваться только при удалении инстанса 
	 * owner - владелец (в данном случае идентификатор инстанса сервера игры)
	 * ip - IP-адрес - если не указаьт удалятся все порты владельца
	 * port - порт
	*/
	bool FreePort(const string& owner, const string& ip="", int port=0);
	
	int AllocPortDecade(const string& owner, const string& ip, string poll="main");
};
#endif
