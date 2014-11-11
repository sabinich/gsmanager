#ifndef NODES_H
#define NODES_H
#include <mgr/mgrssh.h>
#include <mgr/mgrclient.h>
#include "dbobject.h"

class Node
{
public:

	Node (const string &ip, const string &name);
	Node (int nodeID);
	Node (NodeTable::Cursor &cursor);

	mgr_rpc::SSH AssertSSHConnection ();
	inline int ID() const {return m_id;}
	inline string Name() const { return m_name;}
	inline string Ip() const {return m_ip;}
	mgr_rpc::SSH ssh();
	bool IsLocal();

	inline void SetMiniPassword(const string& password) { m_password = password;}

	mgr_client::Result MiniQuery(const string& query, int timeout = 0); //0 = no timeout

private:
	string m_ip, m_name, m_password;
	int m_id, m_miniPort;
	bool m_isLocal;
	std::shared_ptr<mgr_client::Client> m_mini_client;
	void Init(NodeTable::Cursor &cursor);
	bool m_use_https;
};
#endif // NODES_H
