#include "dispatcher.hpp"
#include "error.hpp"
#include "ystl.hpp"
#include <cerrno>
#include <cstdlib>
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
#include <sys/wait.h>

typedef Webserv::FDTaskDispatcher FDTaskDispatcher;
typedef Webserv::IFDTask IFDTask;
typedef Webserv::IFDConsumer IFDConsumer;

IFDTask::~IFDTask() {}

IFDConsumer::~IFDConsumer() {}

// Webserv::IProcessTask::~IProcessTask() {};

FDTaskDispatcher::FDTaskDispatcher() {
#ifdef LINUX
	epollFd = epoll_create1(0); // TODO: handle this properly
#endif
#ifdef OSX
	kqueueFd = kqueue();
#endif
}

FDTaskDispatcher::~FDTaskDispatcher() {
	for (std::map<int, SharedPtr<IFDTask> >::iterator it = activeHandlers.begin(); it != activeHandlers.end(); it++) {
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

void FDTaskDispatcher::registerTask(SharedPtr<IFDTask> task) {
	insertionQueue.push_back(task);
	registerDescriptor(task->getDescriptor());
	// Option<SharedPtr<IProcessTask> > maybeProcessTask = task.tryAs<IProcessTask>();
	// if (maybeProcessTask.isSome()) {
	// 	processTasks[maybeProcessTask.get()->getPID()] = maybeProcessTask.get();
	// }
}

bool FDTaskDispatcher::tryUnregisterDescriptor(int fd) {
	if (activeDescriptors.find(fd) != activeDescriptors.end()) {
		std::cout << "Unregistering " << fd << std::endl;
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
	(void)ctlResult;
#ifdef DEBUG
	std::cout << "epoll_ctl: " << ctlResult << std::endl;
#endif
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
#ifdef DEBUG
	std::cout << "Registered descriptor: " << fd << " - " << aliveDescriptors[fd] << std::endl;
#endif
}

void FDTaskDispatcher::tryCloseDescriptor(int fd) {
	if (aliveDescriptors.find(fd) == aliveDescriptors.end()) {
#ifdef DEBUG
		std::cerr << "Attempt to remove a non-inserted descriptor " << fd << std::endl;
#endif
		std::exit(1);
	}
	else {
		aliveDescriptors[fd] -= 1;
#ifdef DEBUG
		std::cout << "TryClosed descriptor: " << fd << " - " << aliveDescriptors[fd] << std::endl;
#endif
		if (aliveDescriptors[fd] == 0) {
			aliveDescriptors.erase(fd);
			close(fd);
		}
	}
}

// Option<Webserv::Error> FDTaskDispatcher::updateProcessTasks() {
// 	std::cout << "Updating process tasks" << std::endl;
// 	for (std::vector< SharedPtr<IProcessTask> >::iterator it = makedForRemovalPTasks.begin(); it != makedForRemovalPTasks.end(); it++) {
// 		(*it)->onProcessExit(*this);
// 		std::cout << "Removing process " << (*it)->getPID() << std::endl;
// 		processTasks.erase((*it)->getPID());
// 	}
// 	makedForRemovalPTasks = std::vector<SharedPtr<IProcessTask> >();
	
// 	int wstatus;
// 	for (std::map<int, SharedPtr<IProcessTask> >::iterator it = processTasks.begin(); it != processTasks.end(); it++) {
// 		int waitResult = waitpid(it->first, &wstatus, WNOHANG);
// 		if (waitResult < 0)
// 			return Error(Error::GENERIC_ERROR, "waitpid error");
// 		if (WIFEXITED(wstatus)) {
// 			std::cout << "Process " << it->first << " has stopeed, cooking it rn" << std::endl;
// 			makedForRemovalPTasks.push_back(it->second);
// 		}
// 	}
// 	return NONE;
// }

Option<Webserv::Error> FDTaskDispatcher::update() {
	std::cout << "Task dispatcher update" << std::endl;
	// std::cout << "Pre-refresh active handlers:" << std::endl;
	// for (std::map<int, SharedPtr<IFDTask> >::iterator it = activeHandlers.begin(); it != activeHandlers.end(); it++) {
	// 	std::cout << " " << it->first << std::endl;
	// }

	// Refresh storage
	std::vector<SharedPtr<IFDTask> > newInserted;
	for (std::vector<SharedPtr<IFDTask> >::iterator it = insertionQueue.begin(); it != insertionQueue.end(); it++) {
		if (!tryRegisterDescriptor((*it)->getDescriptor(), (*it)->getIOMode())) {
			newInserted.push_back(*it);
		}
		else {
			activeHandlers[(*it)->getDescriptor()] = *it;
		}
	}
	insertionQueue = newInserted;

	// std::cout << "Active handlers:" << std::endl;
	// for (std::map<int, SharedPtr<IFDTask> >::iterator it = activeHandlers.begin(); it != activeHandlers.end(); it++) {
	// 	std::cout << " " << it->first << std::endl;
	// }
	
	// Handle events with available descriptors
	std::vector<SharedPtr<IFDTask> > freedHandlers;
	std::vector<int> activeFds;
#ifdef LINUX
	struct epoll_event events[EPOLL_EVENT_COUNT];
	int fdNum = epoll_wait(epollFd, events, EPOLL_EVENT_COUNT, -1);
	if (fdNum == -1) {
		std::cout << strerror(errno) << std::endl;
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
		// std::cout << "Updating task on fd: " << activeFd << std::endl;
		Result<bool, Error> res = activeHandlers[activeFd]->runTask(*this);
		if (res.isError()) {
			if (res.getError().tag == Error::CGI_IO_ERROR) {
				freedHandlers.push_back(activeHandlers[activeFd]);
				std::cerr << "CGI SIGPIPE occured. Shutting down related tasks...";
			}
			else
				return res.getError();
		}
		else if (!res.getValue()) {
			freedHandlers.push_back(activeHandlers[activeFd]);
		}
	}

	// Remove completed events
	for (std::vector<SharedPtr<IFDTask> >::iterator it = freedHandlers.begin(); it != freedHandlers.end(); it++) {
		if (!tryUnregisterDescriptor((*it)->getDescriptor())) {
			return Error(Error::GENERIC_ERROR, "Attempt to close non-existent descriptor");
		}
		activeHandlers.erase((*it)->getDescriptor());
		tryCloseDescriptor((*it)->getDescriptor());
	}

	// return updateProcessTasks();
	return NONE;
}

void FDTaskDispatcher::removeByFd(int fd) {
	tryUnregisterDescriptor(fd);
	std::vector<SharedPtr<IFDTask> > newInserted;
	for (std::vector<SharedPtr<IFDTask> >::iterator it = insertionQueue.begin(); it != insertionQueue.end(); it++) {
		if ((*it)->getDescriptor() != fd) {
			newInserted.push_back(*it);
		}
		else {
			tryCloseDescriptor(fd);
		}
	}
	insertionQueue = newInserted;

	if (activeHandlers.find(fd) != activeHandlers.end()) {
		std::cout << "Removing " << fd << " from active handlers" << std::endl;
		activeHandlers.erase(fd);
		tryCloseDescriptor(fd);
	}	
}