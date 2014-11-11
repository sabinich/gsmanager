/*
 * sysctl.h
 *
 *  Created on: 12.07.2013
 *      Author: boom
 */

#ifndef SYSCTL_H_
#define SYSCTL_H_
#include <classes.h>
#include <mgr/mgrssh.h>
#include <mgr/mgrjob.h>
namespace sysctl {

class ConnectionBase
{
public:
	typedef std::shared_ptr<ConnectionBase> Ptr;
	virtual string ReadConf() = 0;
	virtual void WriteConf (const string & content) = 0;
	virtual void ExecSysctl() = 0;
	virtual ~ConnectionBase(){}
};

ConnectionBase::Ptr Local();
ConnectionBase::Ptr Remote(mgr_rpc::SSH ssh);

class Conf : public mgr_job::SafeJob {
public:
	Conf(ConnectionBase::Ptr connection);
	virtual ~Conf() throw() {}

	/**
	@brief Задает новое значение параметра
	*/
	Conf& SetParamValue(const string& param, int value);
	Conf& SetParamValue(const string& param, const string& value);

	/**
	@brief Выполняет замену значений параметров в конфиге, если параметр не найден, то он создается
	*/
	void Run();

protected:
	virtual void onRollback() throw ();
private:
	ConnectionBase::Ptr m_Connection;
	std::map<string, string> m_values;
	string m_OldContent;

};

/* usage:
void ModifySysctl() const {
	sysctl::Conf * sysctl = new sysctl::Conf (sysctl::Local());
	sysctl->SetParamValue("net.ipv4.ip_forward", 1)
			.SetParamValue("net.ipv6.conf.default.forwarding", 1)
			.SetParamValue("net.ipv6.conf.all.forwarding", 1)
			.SetParamValue("net.ipv4.conf.default.proxy_arp", 0)
			.SetParamValue("net.ipv4.conf.all.rp_filter", 1)
			.SetParamValue("kernel.sysrq", 1)
			.SetParamValue("net.ipv4.conf.default.send_redirects", 1)
			.SetParamValue("net.ipv4.conf.all.send_redirects", 0)
			.SetParamValue("net.ipv4.conf.default.forwarding", 1)
			.SetParamValue("net.ipv4.conf.eth0.proxy_arp", 1)
			.SetParamValue("net.ipv6.conf.default.proxy_ndp", 1)
			.SetParamValue("net.ipv4.tcp_rmem", "4096 8192 16384")
			.SetParamValue("net.ipv4.tcp_wmem", "4096 8192 16384")
			.SetParamValue("net.ipv6.conf.all.proxy_ndp", 1)
			.Run();
//		net.ipv4.netfilter.ip_conntrack_max=524288
//		net.netfilter.nf_conntrack_max=524288
	}*/


} /* namespace sysctl */
#endif /* SYSCTL_H_ */
