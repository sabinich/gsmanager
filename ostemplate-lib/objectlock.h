#ifndef OBJECTLOCK_H
#define OBJECTLOCK_H
#include "textlock.h"
#include <mgr/mgrfile.h>
#include <typeinfo>
#define OBJECT_LOCK_PATH "var/run/objectlock"

namespace mgr_thread {

/**
 * @brief Лочка по текстовому ключу, плюс блокировка файла.
 * @details В рамках одного процесса (не путать с тредом) работает как текстовая лочка
 * Если же заблокирован файл, т.е. полочена лочка в рамках другого процесса, то генерируется
 * исключение: throw mgr_err::Error("locked_domain");
 */

template <class Object>
class ObjectLock {
protected:
	TextLock<Object> m_lock;
	mgr_file::Lock m_file_lock;
	const string m_filename;
public:
	ObjectLock (const string& name)
		: m_lock			(TextLock<Object>::GetLock(name))
		, m_filename		(mgr_file::ConcatPath(string(OBJECT_LOCK_PATH),typeid(Object).name() + ("_" + name)))
	{
		mgr_file::MkDir(OBJECT_LOCK_PATH);
	}
	void Lock()
	{
		const bool locked_me = m_lock.LockedMe();
		m_lock.Lock();
		if (!locked_me && !m_file_lock.TryLock(m_filename, true)) {
			m_lock.Unlock();
			throw mgr_err::Error("locked_domain");
		}
	}
	bool TryLock()
	{
		const bool locked_me = m_lock.LockedMe();
		if (m_lock.TryLock()) {
			if (!locked_me && !m_file_lock.TryLock(m_filename, true)) {
				m_lock.Unlock();
				return false;
			} else
				return true;
		}
		return false;
	}
	void Unlock()
	{
		m_file_lock.Release();
		m_lock.Unlock();
	}


};
}

#endif // OBJECTLOCK_H
