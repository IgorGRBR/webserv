#ifndef EVENT_HPP
#define EVENT_HPP

#include <map>
#include <set>
#ifndef EPOLL_EVENT_COUNT
#define EPOLL_EVENT_COUNT 20
#endif

#ifndef READ_BUFFER_SIZE
#define READ_BUFFER_SIZE 4096
#endif

#include "ystl.hpp"
#include "error.hpp"
#include <sys/types.h>
#include <vector>
#include <sys/epoll.h>

#define FD_READABLE (1 << 0)
#define FD_WRITEABLE (1 << 1)

namespace Webserv {
	class FDTaskDispatcher;

	enum IOMode {
		READ_MODE,
		WRITE_MODE,
	};

	class IFDTask {
	public:
		virtual ~IFDTask();
		
		virtual Result<bool, Error> runTask(FDTaskDispatcher&) = 0;
		virtual int getDescriptor() const = 0;
		virtual IOMode getIOMode() const = 0;
	};

	class IFDConsumer {
	public:
		virtual ~IFDConsumer();
		virtual void consumeFileData(const std::string &) = 0;
	};

	class FDTaskDispatcher {
	public:
		FDTaskDispatcher();
		~FDTaskDispatcher();
		void registerHandler(IFDTask*);
		Option<Error> update();

	private:
		FDTaskDispatcher(const FDTaskDispatcher&); // No implementation
		FDTaskDispatcher& operator=(const FDTaskDispatcher&); // No implementation
		bool tryRegisterDescriptor(int, IOMode);
		bool tryUnregisterDescriptor(int);

		std::map<int, UniquePtr<IFDTask> > handlers;
		std::vector<UniquePtr<IFDTask> > inserted;
		std::set<int> descriptors;
		struct epoll_event events[EPOLL_EVENT_COUNT];
		int epollFd;
	};
}

#endif