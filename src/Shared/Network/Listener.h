#ifndef __LISTENER_H_
#define __LISTENER_H_

#include <memory>
#include <thread>
#include <vector>

#include <asio.hpp>
#include "NetworkThread.h"

namespace Origin
{
	template <typename SocketType>
	class Listener
	{
	private:
		boost::asio::io_service m_service;
		boost::asio::ip::tcp::acceptor m_acceptor;

		std::thread m_acceptorThread;
		std::vector<std::unique_ptr<NetworkThread<SocketType>>> m_workerThreads;

		// the time in milliseconds to sleep a worker thread at the end of each tick
		const int SleepInterval = 100;

		NetworkThread<SocketType> *SelectWorker() const
		{
			int minIndex = 0;
			size_t minSize = m_workerThreads[minIndex]->Size();

			for (size_t i = 1; i < m_workerThreads.size(); ++i)
			{
				const size_t size = m_workerThreads[i]->Size();

				if (size < minSize)
				{
					minSize = size;
					minIndex = i;
				}
			}

			return m_workerThreads[minIndex].get();
		}

		void BeginAccept();
		void OnAccept(NetworkThread<SocketType> *worker, SocketType *socket, const boost::system::error_code &ec);

	public:
		Listener(int port, int workerThreads);
		~Listener();
	};

	template <typename SocketType>
	Listener<SocketType>::Listener(int port, int workerThreads)
		: m_acceptor(m_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
	{
		m_workerThreads.reserve(workerThreads);
		for (int i = 0; i < workerThreads; ++i)
			m_workerThreads.push_back(std::unique_ptr<NetworkThread<SocketType>>(new NetworkThread<SocketType>));

		BeginAccept();

		m_acceptorThread = std::thread([this]() { this->m_service.run(); });
	}

	// FIXME - is this needed?
	template <typename SocketType>
	Listener<SocketType>::~Listener()
	{
		m_service.stop();
		m_acceptor.close();
		m_acceptorThread.join();
	}

	template <typename SocketType>
	void Listener<SocketType>::BeginAccept()
	{
		NetworkThread<SocketType> *worker = SelectWorker();
		SocketType *socket = worker->CreateSocket();
		m_acceptor.async_accept(socket->GetAsioSocket(), [this, worker, socket](const boost::system::error_code &ec) { this->OnAccept(worker, socket, ec); });
	}

	template <typename SocketType>
	void Listener<SocketType>::OnAccept(NetworkThread<SocketType> *worker, SocketType *socket, const boost::system::error_code &ec)
	{
		// an error has occurred
		if (ec)
		{
			worker->RemoveSocket(socket);
		}
		else
		{
			socket->Open();
		}
		BeginAccept();
	}
}

#endif /* !__LISTENER_H_ */
