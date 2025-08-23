#include "dispatcher.hpp"
#include "error.hpp"
#include "ystl.hpp"
#include <string>
#ifdef OSX
#include <sys/event.h>
#endif
#ifdef LINUX
#include <sys/epoll.h>
#endif
#include <unistd.h>
#include <utility>
#include <vector>
#include <fcntl.h>

typedef Webserv::FDTaskDispatcher FDTaskDispatcher;
typedef Webserv::IFDTask IFDTask;
typedef Webserv::IFDConsumer IFDConsumer;

IFDTask::~IFDTask() {}

IFDConsumer::~IFDConsumer() {}

FDTaskDispatcher::FDTaskDispatcher() {
#ifdef LINUX
	epollFd = epoll_create1(0); // TODO: handle this properly
#endif
#ifdef OSX
	kqueueFd = kqueue();
#endif
}

FDTaskDispatcher::~FDTaskDispatcher() {
	for (std::map<int, UniquePtr<IFDTask> >::iterator it = activeHandlers.begin(); it != activeHandlers.end(); it++) {
		tryUnregisterDescriptor(it->first);
		close(it->first);
	}
#ifdef OSX
	close(kqueueFd);
#endif
#ifdef LINUX
	close(epollFd);
#endif
}

void FDTaskDispatcher::registerTask(IFDTask* task) {
	insertionQueue.push_back(task);
	registerDescriptor(task->getDescriptor());
}

bool FDTaskDispatcher::tryUnregisterDescriptor(int fd) {
	if (activeDescriptors.find(fd) != activeDescriptors.end()) {
#ifdef LINUX
		epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL);
#endif
#ifdef OSX
		// Just do nothing I guess?
#endif
		activeDescriptors.erase(fd);
		return true;
	}
	return false;
}

bool FDTaskDispatcher::tryRegisterDescriptor(int fd, IOMode mode) {
	if (activeDescriptors.find(fd) != activeDescriptors.end()) {
		return false;
	}
#ifdef LINUX
	struct epoll_event ev;
	ev.events = 0;
	if (mode == READ_MODE) ev.events = EPOLLIN;
	if (mode == WRITE_MODE) ev.events = EPOLLOUT;
	ev.data.fd = fd;
	int ctlResult = epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev);
	std::cout << "epoll_ctl: " << ctlResult << std::endl;
#endif
#ifdef OSX
	struct kevent ev;
	ushort flags;
	if (mode == READ_MODE) flags = EVFILT_READ;
	if (mode == WRITE_MODE) flags = EVFILT_WRITE;
	EV_SET(&ev, fd, flags, EV_ADD, 0, 0, NULL);
	kevent(kqueueFd, &ev, 1, NULL, 0, NULL);
#endif
	activeDescriptors.insert(fd);
	return true;
}

void FDTaskDispatcher::registerDescriptor(int fd) {
	if (aliveDescriptors.find(fd) == aliveDescriptors.end()) {
		aliveDescriptors[fd] = 1;
	}
	else {
		aliveDescriptors[fd] += 1;
	}
	std::cout << "Registered descriptor: " << fd << " - " << aliveDescriptors[fd] << std::endl;
}

void FDTaskDispatcher::tryCloseDescriptor(int fd) {
	if (aliveDescriptors.find(fd) == aliveDescriptors.end()) {
		std::cerr << "Attempt to remove a non-inserted descriptor " << fd << std::endl;
		std::exit(1);
	}
	else {
		aliveDescriptors[fd] -= 1;
		std::cout << "TryClosed descriptor: " << fd << " - " << aliveDescriptors[fd] << std::endl;
		if (aliveDescriptors[fd] == 0) {
			aliveDescriptors.erase(fd);
			close(fd);
		}
	}
}

Option<Webserv::Error> FDTaskDispatcher::update() {
	// Refresh storage
	std::vector<UniquePtr<IFDTask> > newInserted;
	for (std::vector<UniquePtr<IFDTask> >::iterator it = insertionQueue.begin(); it != insertionQueue.end(); it++) {
		if (!tryRegisterDescriptor((*it)->getDescriptor(), (*it)->getIOMode())) {
			newInserted.push_back(*it);
		}
		else {
			activeHandlers[(*it)->getDescriptor()] = *it;
		}
	}
	insertionQueue = newInserted;
	
	// Handle events with available descriptors
	std::vector<UniquePtr<IFDTask> > freedHandlers;
	std::vector<int> activeFds;
#ifdef LINUX
	struct epoll_event events[EPOLL_EVENT_COUNT];
	int fdNum = epoll_wait(epollFd, events, EPOLL_EVENT_COUNT, -1);
	if (fdNum == -1) {
		return Error(Error::GENERIC_ERROR, "epoll_wait pooped itself");
	}

	for (int i = 0; i < fdNum; i++)
		activeFds.push_back(events[i].data.fd);
#endif
#ifdef OSX
	struct kevent kevents[EPOLL_EVENT_COUNT];
	int fdNum = kevent(kqueueFd, NULL, 0, kevents, EPOLL_EVENT_COUNT, NULL);
	if (fdNum == -1) {
		return Error(Error::GENERIC_ERROR, "kevent pooped itself");
	}

	for (int i = 0; i < fdNum; i++)
		activeFds.push_back(kevents[i].ident);
#endif

	for (uint i = 0; i < activeFds.size(); i++) {
		int activeFd = activeFds[i];
		Result<bool, Error> res = activeHandlers[activeFd]->runTask(*this);
		if (res.isError()) {
			return res.getError();
		}
		if (!res.getValue()) {
			freedHandlers.push_back(activeHandlers[activeFd]);
		}
	}
	// Remove completed events
	for (std::vector<UniquePtr<IFDTask> >::iterator it = freedHandlers.begin(); it != freedHandlers.end(); it++) {
		if (!tryUnregisterDescriptor((*it)->getDescriptor())) {
			return Error(Error::GENERIC_ERROR, "Attempt to close non-existent descriptor");
		}
		activeHandlers.erase((*it)->getDescriptor());
		tryCloseDescriptor((*it)->getDescriptor());
	}

	return NONE;
}
