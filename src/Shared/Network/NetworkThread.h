#ifndef __NETWORK_THREAD_H_
#define __NETWORK_THREAD_H_

#include <thread>
#include <list>
#include <mutex>
#include <chrono>
#include <atomic>
#include <utility>

#include <asio.hpp>

#include "Scoket.h"

namespace Origin
{
	template <typename SocketType>
	class NetworkThread
	{
	private:
		static const int WorkDelay;

		boost::asio::io_service m_service;

		std::mutex m_socketLock;
		// most people think that you should always use a vector rather than a list, but i believe that this is an exception
		// because this collection can potentially get rather large and we will frequently be removing elements at an arbitrary
		// position within it.
		std::list<std::unique_ptr<SocketType>> m_sockets;

		// note that the work member *must* be declared after the service member for the work constructor to function correctly
		boost::asio::io_service::work m_work;

		std::mutex m_closingSocketLock;
		std::list<std::unique_ptr<SocketType>> m_closingSockets;

		std::atomic<bool> m_pendingShutdown;

		std::thread m_serviceThread;
		std::thread m_socketCleanupThread;

		void SocketCleanupWork();

	public:
		NetworkThread() : m_work(m_service), m_pendingShutdown(false),
			m_serviceThread([this] { boost::system::error_code ec; this->m_service.run(ec); }),
			m_socketCleanupThread([this] { this->SocketCleanupWork(); })
		{
			m_serviceThread.detach();
		}

		~NetworkThread()
		{
			// we do not lock the list here because Close() will call RemoveSocket which needs the lock
			while (!m_sockets.empty())
			{
				// if it is already closed (which can happen with the placeholder socket for a pending accept), just remove it
				if (m_sockets.front()->IsClosed())
					m_sockets.erase(m_sockets.begin());
				else
					m_sockets.front()->Close();
			}

			m_pendingShutdown = true;
			m_socketCleanupThread.join();
		}

		size_t Size() const { return m_sockets.size(); }

		SocketType *CreateSocket();

		void RemoveSocket(Socket *socket)
		{
			std::lock_guard<std::mutex> guard(m_socketLock);
			std::lock_guard<std::mutex> closingGuard(m_closingSocketLock);

			for (auto i = m_sockets.begin(); i != m_sockets.end(); ++i)
				if (i->get() == socket)
				{
					m_closingSockets.push_front(std::move(*i));
					m_sockets.erase(i);
					return;
				}
		}
	};

	template <typename SocketType>
	const int NetworkThread<SocketType>::WorkDelay = 500;

	template <typename SocketType>
	void NetworkThread<SocketType>::SocketCleanupWork()
	{
		while (!m_pendingShutdown || !m_closingSockets.empty())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(WorkDelay));

			std::lock_guard<std::mutex> guard(m_closingSocketLock);

			for (auto i = m_closingSockets.begin(); i != m_closingSockets.end(); )
			{
				if ((*i)->Deletable())
					i = m_closingSockets.erase(i);
				else
					++i;
			}
		}
	}

	template <typename SocketType>
	SocketType *NetworkThread<SocketType>::CreateSocket()
	{
		std::lock_guard<std::mutex> guard(m_socketLock);

		m_sockets.push_front(std::unique_ptr<SocketType>(new SocketType(m_service, [this](Socket *socket) { this->RemoveSocket(socket); })));

		return m_sockets.begin()->get();
	}
}

#endif /* !__NETWORK_THREAD_H_ */