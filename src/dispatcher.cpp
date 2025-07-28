#include "dispatcher.hpp"
#include "error.hpp"
#include "ystl.hpp"
#include <string>
#include <sys/epoll.h>
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
	epollFd = epoll_create1(0); // TODO: handle this properly
}

FDTaskDispatcher::~FDTaskDispatcher() {
	for (std::map<int, UniquePtr<IFDTask> >::iterator it = activeHandlers.begin(); it != activeHandlers.end(); it++) {
		tryUnregisterDescriptor(it->first);
		close(it->first);
	}
	close(epollFd);
}

void FDTaskDispatcher::registerTask(IFDTask* task) {
	insertionQueue.push_back(task);
}

bool FDTaskDispatcher::tryUnregisterDescriptor(int fd) {
	if (activeDescriptors.find(fd) != activeDescriptors.end()) {
		epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL);
		activeDescriptors.erase(fd);
		return true;
	}
	return false;
}

bool FDTaskDispatcher::tryRegisterDescriptor(int fd, IOMode mode) {
	if (activeDescriptors.find(fd) != activeDescriptors.end()) {
		return false;
	}
	struct epoll_event ev;
	ev.events = 0;
	if (mode == READ_MODE) ev.events = EPOLLIN;
	if (mode == WRITE_MODE) ev.events = EPOLLOUT;
	ev.data.fd = fd;
	int ctlResult = epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev);
	std::cout << "epoll_ctl: " << ctlResult << std::endl;
	activeDescriptors.insert(fd);
	return true;
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
	int fdNum = epoll_wait(epollFd, events, EPOLL_EVENT_COUNT, -1);
	if (fdNum == -1) {
		return Error(Error::GENERIC_ERROR, "epoll_wait pooped itself");
	}

	for (int i = 0; i < fdNum; i++) {
		int activeFd = events[i].data.fd;
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
		it->move();
	}

	return NONE;
}
