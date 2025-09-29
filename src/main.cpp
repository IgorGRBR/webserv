#include <csignal>
#include <iostream>
#include <ostream>
#include <vector>
#include "config.hpp"
#include "error.hpp"
#include "dispatcher.hpp"
#include "tasks.hpp"
#include "ystl.hpp"
#include "url.hpp"

typedef Webserv::Url Url;
typedef Webserv::Config Config;
typedef Webserv::Error Error;
typedef Webserv::FDTaskDispatcher FDTaskDispatcher;
typedef Webserv::ClientListener ClientListener;

int main(int argc, char* argv[], char *envp[]) {
	// Get the path of a config file
	std::string configPath;
	if (argc == 1) {
		configPath = "wsconf.wsv";
	}
	else {
		configPath = argv[1];
	}

	// Load the config
	std::cout << "A webserver has started" << std::endl;
	Result<Config, Webserv::ConfigError> maybeConfig = Webserv::readConfigFromFile(configPath);
	if (maybeConfig.isError()) {
		std::cerr << "Error occured when loading config file: " << Webserv::configErrorDescription(maybeConfig.getError()) << std::endl;
		return 1;
	}
	Config config = maybeConfig.getValue();

	std::cout << "Running the server" << std::endl;

	// Configure the initial client connection listeners
	Option<UniquePtr<FDTaskDispatcher> > maybeDispatcher = FDTaskDispatcher::tryMake();
	if (maybeDispatcher.isNone()) {
		std::cerr << "Could not initialize task dispatcher. Likely a problem with internal event queue handler." << std::endl;
		return 1;
	}
	UniquePtr<FDTaskDispatcher>  dispatcher = maybeDispatcher.get();
	for (uint i = 0; i < config.servers.size(); i++) {
		Result<ClientListener*, Error> maybeListener = ClientListener::tryMake(config, config.servers[i], envp);
		if (maybeListener.isError()) {
			std::cout << "Critical error when trying to construct a client listener: "
				<< maybeListener.getError().getTagMessage() << std::endl;
		}
		else {
			dispatcher->registerTask(maybeListener.getValue());
		}
	}

	// I'm not entirely sure this is a good idea yet...
	// Nevermind, apparently it is (source: https://stackoverflow.com/questions/108183/how-to-prevent-sigpipes-or-handle-them-properly).
	signal(SIGPIPE, SIG_IGN);
	
	// Run the task-event loop
	bool running = true;
	while (running) {
		Option<Error> error = dispatcher->update();
		if (error.isSome()) {
			running = false;
			std::cout << "Got critical unhandled error: " << error.get().getTagMessage() << "\n" << error.get().message << std::endl;
		}
	}

	return 0;
}
