#ifndef EVENT_HPP
#define EVENT_HPP

#ifndef EPOLL_EVENT_COUNT
#define EPOLL_EVENT_COUNT 20
#endif

#ifndef READ_BUFFER_SIZE
#define READ_BUFFER_SIZE 4096
#endif

#include <map>
#include <set>
#include "ystl.hpp"
#include "error.hpp"
#include <sys/types.h>
#include <vector>
#ifdef OSX
 #include <sys/event.h>
#endif
#ifdef LINUX
#include <sys/epoll.h>
#endif

#define FD_READABLE (1 << 0)
#define FD_WRITEABLE (1 << 1)

namespace Webserv {
	class FDTaskDispatcher;

	// `IOMode` is used to specify whether a file descriptor of a task is used for reading or writing.
	enum IOMode {
		READ_MODE,
		WRITE_MODE,
	};

	// `IFDTask` is an interface that represents a file-descriptor-bound task.
	// These tasks are the core functionality as to how this server operates, allowing it to operate efficiently.
	// They represent a unit of work that is directly tied to some blocking resource that is represented with a file descriptor.
	// By coupling `IFDTask` instances with their file descriptors and utilizing a task scheduler (like `FDTaskDispatcher`),
	// a server can operate at an optimal efficiency in regards to blocking and CPU utilization. 
	class IFDTask {
	public:
		virtual ~IFDTask();
		
		// Runs the task. Additionally provides the `FDTaskDispatcher` instance reference, as running a task may produce
		// more tasks. This method should return a boolean indicating if the task should remain alive, or an unrecovereable
		// runtime error.
		virtual Result<bool, Error> runTask(FDTaskDispatcher&) = 0;

		// Returns the file descriptor associated with the task.
		// TODO: this can probably be a simple const field.
		virtual int getDescriptor() const = 0;

		// Returns the IO mode of the task.
		// TODO: same as the above.
		virtual IOMode getIOMode() const = 0;
	};

	// `IFDConsumer` is an interface that may represent a task that gradually consumes data from a file
	// descriptor. It is not used currently, but may be useful in the future, idk.
	class IFDConsumer {
	public:
		virtual ~IFDConsumer();
		virtual void consumeFileData(const std::string &) = 0;
	};

	// `FDTaskDispatcher` is responsible for maintaining a queue of pending tasks, maintaining a list of currently active tasks
	// and updating their states. It also manages the state of `epoll` instance.
	class FDTaskDispatcher {
	public:
		// Constructs the `FDTaskDispatcher` instance.
		// TODO: `epoll_create1` inside the constructor might fail, make it private and introduce a static constructor here.
		FDTaskDispatcher();

		// Cleans up the internal `epoll` instance and closes all of the file descriptors.
		~FDTaskDispatcher();

		// Adds the task to the queue, Note that after passing `task` to this method, its lifetime will be managed by the
		// `FDTaskDispatcher`!
		void registerTask(IFDTask* task);

		// Executes a number of active tasks, based on the list of available file descriptors returned from `epoll_wait`.
		// Completed tasks are removed from the active task list and deleted. New tasks are added to the active tasks list based
		// on the availability of their file descriptors.
		Option<Error> update();

		Option<Error> oomUpdate();
	private:
		// Removing copying constructors significantly simplifies memory management of the tasks.
		FDTaskDispatcher(const FDTaskDispatcher&); // No implementation
		FDTaskDispatcher& operator=(const FDTaskDispatcher&); // No implementation

		// Attemts to register a file descriptor in the set of active file descriptors.
		bool tryRegisterDescriptor(int, IOMode);

		// Attempts to remove a file descriptor from the set of active file descriptors.
		bool tryUnregisterDescriptor(int);

		void registerDescriptor(int);
		void tryCloseDescriptor(int);

		std::map<int, UniquePtr<IFDTask> > activeHandlers;
		std::vector<UniquePtr<IFDTask> > insertionQueue;
		std::set<int> activeDescriptors;
		std::map<int, uint> aliveDescriptors;
#ifdef OSX
		int kqueueFd;
#endif
#ifdef LINUX
		int epollFd;
#endif
	};
}

class Thing;
class Thing {
public:
	SharedPtr<Thing> thing2;
};

#endif