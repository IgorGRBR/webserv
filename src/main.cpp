#include <iostream>
#include <new>
#include <ostream>
#include <vector>
#include "config.hpp"
#include "error.hpp"
#include "dispatcher.hpp"
#include "tasks.hpp"
#include "webserv.hpp"
#include "ystl.hpp"
#include "url.hpp"

typedef Webserv::Url Url;
typedef Webserv::Config Config;
typedef Webserv::Error Error;
typedef Webserv::FDTaskDispatcher FDTaskDispatcher;
typedef Webserv::ClientListener ClientListener;

void urlTests() {
	Webserv::Url test = Webserv::Url::fromString("localhost:655/search/thing").get();

	std::cout << test.getProtocol() << std::endl;
	for (std::vector<std::string>::const_iterator it = test.getSegments().begin(); it != test.getSegments().end(); it++) {
		std::cout << "  " << *it << std::endl;
	}
	std::cout << test.getPort() << std::endl;

	Url tail = test.tail();
	for (std::vector<std::string>::const_iterator it = tail.getSegments().begin(); it != tail.getSegments().end(); it++) {
		std::cout << "  " << *it << std::endl;
	}
}

void hexDecTests() {
	std::cout << Webserv::hexStrToUInt("const std::string &").isNone() << std::endl;
	std::cout << Webserv::hexStrToUInt("0").get() << std::endl;
	std::cout << Webserv::hexStrToUInt("9").get() << std::endl;
	std::cout << Webserv::hexStrToUInt("a").get() << std::endl;
	std::cout << Webserv::hexStrToUInt("b").get() << std::endl;
	std::cout << Webserv::hexStrToUInt("f").get() << std::endl;
	std::cout << Webserv::hexStrToUInt("10").get() << std::endl;
	std::cout << Webserv::hexStrToUInt("11").get() << std::endl;
}

void optionTests() {
	Option<int> i = 32;
	int iReal = i.get();
	std::cout << "i is " << iReal << std::endl;

	Option<double> d = 2.545;
	double dReal = d.get();
	std::cout << "d is " << dReal << std::endl;

	Option<std::string> s = std::string("hello");
	std::string sReal = s.get();
	std::cout << "s is " << sReal << std::endl;

	Option<std::vector<std::string> > v = std::vector<std::string>();
	std::vector<std::string> vReal = v.get();
	std::cout << "v is " << vReal.size() << std::endl;

	Option<Url> u = Url::fromString("/happy");
	Url uReal = u.get();
	std::cout << "u is " << uReal.toString() << std::endl;
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
	// optionTests();
	// hexDecTests();
	// Load the config
	std::cout << "A webserver has started" << std::endl;
	Config config;
	try {
		Result<Config, Webserv::ConfigError> maybeConfig = Webserv::readConfigFromFile(configPath).getValue();
		if (maybeConfig.isError()) {
			std::cerr << Webserv::configErrorString(maybeConfig.getError()) << std::endl;
			return 1;
		}
		config = maybeConfig.getValue();
	}
	catch (const std::bad_alloc& ex) {
		std::cerr << "Out of memory exception when parsing config file." << std::endl;
		return 1;
	}

	if (config.servers.size() == 0) {
		std::cerr << "At least a single server directive must be provided in the config file." << std::endl;
		return 1;
	}

	std::cout << "Running the server" << std::endl;
	
	FDTaskDispatcher dispatcher;
	try {
		// Configure the initial client connection listeners
		for (uint i = 0; i < config.servers.size(); i++) {
			Result<ClientListener*, Error> maybeListener = ClientListener::tryMake(config, config.servers[i]);
			if (maybeListener.isError()) {
				std::cout << "Critical error when trying to construct a client listener: "
					<< maybeListener.getError().getTagMessage() << std::endl;
			}
			else {
				dispatcher.registerTask(maybeListener.getValue());
			}
		}
	}
	catch (const std::bad_alloc& ex) {
		std::cerr << "Out of memory exception when constructing crucial webserver tasks." << std::endl;
		return 1;
	}
	
	// Run the task-event loop
	bool running = true;
	while (running) {
		try {
			Option<Error> error = dispatcher.update();
			if (error.isSome()) {
				running = false;
				std::cout << "Got critical unhandled error: " << error.get().getTagMessage() << "\n" << error.get().message << std::endl;
			}
		}
		catch (const std::bad_alloc& ex) {
			std::cout << "Out of memory exception during server runtime, trying to recover..." << std::endl;
			try {
				dispatcher.oomUpdate();
			}
			catch (const std::bad_alloc& ex) {
				std::cout << "Recovery failed, shutting down!" << std::endl;
				return 1;
			}
		}
	}

	return 0;
}
