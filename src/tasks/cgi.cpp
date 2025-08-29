#include "dispatcher.hpp"
#include "tasks.hpp"
#include "error.hpp"

namespace Webserv {
	Result<bool, Error> CGIReader::runTask(FDTaskDispatcher& dispatcher) {
		(void)dispatcher;
		return false;
	}

	int CGIReader::getDescriptor() const {
		return 0; // TODO
	}

	IOMode CGIReader::getIOMode() const {
		return READ_MODE;
	}

	Result<bool, Error> CGIWriter::runTask(FDTaskDispatcher& dispatcher) {
		(void)dispatcher;
		return false;
	}

	int CGIWriter::getDescriptor() const {
		return 0; // TODO
	}

	IOMode CGIWriter::getIOMode() const {
		return WRITE_MODE;
	}

	Result<std::pair<CGIWriter, CGIReader>, Error> makeCGIPipeline(const std::map<std::string, std::string>& env) {
		(void)env;
		return Error(Error::GENERIC_ERROR);
	}
}
