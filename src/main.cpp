#include <iostream>
#include <vector>
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

void urlTests() {
	Webserv::Url test = Webserv::Url::fromString("localhost:655/search").get();

	std::cout << test.getProtocol() << std::endl;
	for (std::vector<std::string>::const_iterator it = test.getSegments().begin(); it != test.getSegments().end(); it++) {
		std::cout << "  " << *it << std::endl;
	}
	std::cout << test.getPort() << std::endl;
}

int main(int argc, char* argv[]) {
	// Get the path of a config file
	std::string configPath;
	if (argc == 1) {
		configPath = "wsconf.wsv";
	}
	else {
		configPath = argv[1];
	}

	// urlTests();
	// Load the config
	std::cout << "A webserver has started" << std::endl;
	Config config = Webserv::readConfigFromFile(configPath).getValue(); // TODO: handle this better

	std::cout << "Running the server" << std::endl;

	// Configure the initial client connection listeners
	FDTaskDispatcher dispatcher;
	for (uint i = 0; i < config.servers.size(); i++) {
		Result<ClientListener*, Error> maybeListener = ClientListener::tryMake(config.servers[i],
			config.servers[i].port.getOr(config.defaultPort));
		if (maybeListener.isError()) {
			std::cout << "Critical error when trying to construct a client listener: "
				<< maybeListener.getError().getTagMessage() << std::endl;
		}
		else {
			dispatcher.registerHandler(maybeListener.getValue());
		}
	}
	
	// Run the task-event loop
	bool running = true;
	while (running) {
		Option<Error> error = dispatcher.update();
		if (error.isSome()) {
			running = false;
			std::cout << "Got critical unhandled error: " << error.get().getTagMessage() << "\n" << error.get().message << std::endl;
		}
	}

	// Webserv::ServerData data;
	return 0;
}
