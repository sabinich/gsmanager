#ifndef TEXTLOCK_H
#define TEXTLOCK_H
#include <mgr/mgrthread.h>
#include <mgr/mgrlog.h>

namespace mgr_thread {

/**
 * @brief Лочка по текстовому ключу
 * @details использует shared_ptr, поэтому блокировка копируется.
 * Разблокируется только когда отработают все деструкторы, либо будет позван Unlock
 */

template <class Object>
class TextLock
{
protected:
	class MutexWThreadId {
	public:
		MutexWThreadId() {
		}

		void Lock() {
			MODULE("thread");
			Debug("m_thread_id = %d Current() = %d", m_thread_id, Current());
			if (m_thread_id != Current())
			{
				m_mutex.Lock();
				m_thread_id = Current();
				Trace("lock mutex");
			} else {
				Trace("already locked");
			}
		}

		bool TryLock() {
			bool res = m_mutex.TryLock();
			if (res) {
				m_thread_id = Current();
				MODULE("thread");
				Trace("lock mutex");
			}
			return res;
		}

		void Unlock() {
			if (m_thread_id == Current()) {
				m_thread_id = None;
				m_mutex.Unlock();
				MODULE("thread");
				Trace("unlock mutex");
			}
		}

		bool LockedMe() {
			return m_thread_id == Current();
		}

	private:
		Mutex m_mutex;
		Id m_thread_id;
	};

	class RealLock {
	public:
		RealLock(MutexWThreadId* mtx)
			: m_mutex(mtx)
		{}

		MutexWThreadId *m_mutex;
		~RealLock() {
			m_mutex->Unlock();
		}
	};
public:
	//Интересно, а зачем я защитил конструктор?! Вроде нафиг не нужно.
	static TextLock GetLock(const string& name) {
		static SafeLock m_MapLock;
		SafeSection ss(m_MapLock);
		static std::map<string,MutexWThreadId*> LockMap;
		auto it = LockMap.find(name);
		if (it == LockMap.end()) {
			MODULE("thread");
			Trace("new mutex %s", name.c_str());
			it = LockMap.insert(std::make_pair(name, new MutexWThreadId())).first;
		}
		return TextLock(it->second);
	}

	void Lock() {
		m_lock->m_mutex->Lock();
	}

	bool TryLock() {
		return m_lock->m_mutex->TryLock();
	}

	void Unlock() {
		m_lock->m_mutex->Unlock();
	}

	/**
	 * @brief LockedMe залоченно мной
	 * @return
	 */
	bool LockedMe() {
		return m_lock->m_mutex->LockedMe();
	}

protected:
	TextLock(MutexWThreadId* mtx)
		: m_lock(new RealLock(mtx))
	{
	}

	std::shared_ptr<RealLock> m_lock;
private:

};

}
#endif // TEXTLOCK_H
