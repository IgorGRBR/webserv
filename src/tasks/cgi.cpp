#include "dispatcher.hpp"
#include "tasks.hpp"
#include "error.hpp"
#include "webserv.hpp"
#include "ystl.hpp"
#include <sstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <sys/wait.h>

namespace Webserv {
	CGIReader::CGIReader(int pid, int fd, uint rSize): fd(fd), pid(pid), readSize(rSize), writer(NONE) {}

	void CGIReader::setWriter(const SharedPtr<CGIWriter> wPtr) {
		writer = wPtr;
	}

	Result<bool, Error> CGIReader::runTask(FDTaskDispatcher& dispatcher) {
		(void)dispatcher;
		char* buffer = new char[readSize + 1]();
		int readResult = read(fd, buffer, readSize);
		if (readResult < 0) return Error(Error::GENERIC_ERROR, "CGIReader fail");
		std::string bufStr = std::string(buffer, readResult);
		delete[] buffer;
#ifdef DEBUG
		std::cout << "Got this from CGI: " << bufStr << std::endl;
#endif
		readBuffer.push_back(bufStr);

		if (readResult == 0) {
			std::cout << "CGIReader is done" << std::endl;
			if (writer.isSome()) {
				writer.get()->close();
			}
			return false;
		}

		// int wstatus;
		// int waitResult = waitpid(pid, &wstatus, WNOHANG);
		// if (waitResult < 0) return Error(Error::GENERIC_ERROR, "waitpid error");
		// if (WIFEXITED(wstatus)) {
		// 	std::cout << "Process " << pid << " has stopeed, cooking it rn" << std::endl;
		// 	onProcessExit(dispatcher);
		// 	return false;
		// }
		return true;
	}

	int CGIReader::getDescriptor() const {
		return fd;
	}

	IOMode CGIReader::getIOMode() const {
		return READ_MODE;
	}

	Option<SharedPtr<CGIWriter> > CGIReader::getWriter() {
		return writer;
	}

// 	void CGIReader::onProcessExit(FDTaskDispatcher& dispatcher) {
// 		if (writer.isSome()) {
// 			writer.get()->close();
// 			// dispatcher.removeByFd(writer.get()->getDescriptor());
// 			(void)dispatcher;
// #ifdef DEBUG
// 		std::cout << "CGIReader signing off..." << std::endl;
// #endif
// 		}
// 		// dispatcher.removeByFd(getDescriptor());
// 	}

// 	int CGIReader::getPID() const {
// 		return pid;
// 	}

	CGIWriter::CGIWriter(int fd, bool cont): fd(fd), continuous(cont), closed(false) {
		std::cout << "CGIWriter: " << fd << std::endl;
	}

	Result<bool, Error> CGIWriter::runTask(FDTaskDispatcher& dispatcher) {
		(void)dispatcher;

		if (closed) return false;
		std::stringstream bufStream;
		for (std::vector<std::string>::const_iterator it = writeBuffer.begin(); it != writeBuffer.end(); it++) {
			bufStream << *it;
		}
		writeBuffer.clear();
		std::string str = bufStream.str();
		int writeError = write(fd, str.c_str(), str.size());
		if (writeError == -1) return Error(Error::GENERIC_ERROR, "CGIWriter fail");
		return continuous;
	}

	int CGIWriter::getDescriptor() const {
		return fd;
	}

	IOMode CGIWriter::getIOMode() const {
		return WRITE_MODE;
	}

	void CGIWriter::consumeFileData(const std::string& data) {
		writeBuffer.push_back(data);
	}

	void CGIWriter::close() {
		closed = true;
	}

	char** setupEnvp(char* pathInfo, char* queryInfo, char** parentEnvp) {
		uint sizeOfEnvp = 0;
		char** envpPtr = parentEnvp;
		while (*envpPtr != NULL) {
			envpPtr++;
			sizeOfEnvp++;
		}
		char** newEnvp = new char*[sizeOfEnvp + 3]();
		envpPtr = parentEnvp;
		uint i = 0;
		while (*envpPtr != NULL) {
			newEnvp[i] = *envpPtr;
			envpPtr++;
			i++;
		}
		newEnvp[i] = pathInfo;
		newEnvp[i + 1] = queryInfo;
		newEnvp[i + 2] = NULL;
		return newEnvp;
	}

	Result<CGIPipeline, Error> makeCGIPipeline(
		const Url& binaryLocation,
		const Url& scriptLocation,
		const Url& extraPath,
		char** envp
	) {
		(void)binaryLocation;
		std::string scriptName = scriptLocation.getSegments().back();
		// std::string env = getenv
		int readPipe[2];
		int writePipe[2];

		if (pipe(readPipe) == -1) {
			return Error(Error::GENERIC_ERROR, "Pipe error");
		}

		if (pipe(writePipe) == -1) {
			return Error(Error::GENERIC_ERROR, "Pipe error");
		}

		int forkResult = fork();
		if (forkResult == 0) {
			dup2(writePipe[0], STDIN_FILENO);
			dup2(readPipe[1], STDOUT_FILENO);

			close(writePipe[1]);
			close(readPipe[0]);

			// Wha... why???? (source: https://stackoverflow.com/questions/7369286/c-passing-a-pipe-thru-execve)
			close(writePipe[0]);
			close(readPipe[1]);

			char* args[3];// = new char[2]();
			args[0] = new char[1]();
			args[1] = new char[scriptName.size() + 1]();
			args[2] = NULL;

			scriptName.copy(args[1], scriptName.size());
			args[1][scriptName.size()] = 0; // TODO: this may be redundant

			std::string pathInfo = "PATH_INFO=" + extraPath.toString(false, true);
			char* pathInfoCStr = new char[pathInfo.size() + 1]();
			pathInfo.copy(pathInfoCStr, pathInfo.size());
			pathInfoCStr[pathInfo.size()] = '\0';

			std::string queryInfo = "QUERY_INFO=";
			char* queryInfoCStr = new char[queryInfo.size() + 1]();
			queryInfo.copy(queryInfoCStr, queryInfo.size());
			queryInfoCStr[queryInfo.size()] = '\0';

#ifdef DEBUG
			std::cout << "Reporting from the other side:" << std::endl;
			std::cout << pathInfoCStr << std::endl;
			std::cout << queryInfoCStr << std::endl;
#endif

			char** newEnvp = setupEnvp(pathInfoCStr, queryInfoCStr, envp);

			chdir(scriptLocation.exceptLast().toString(false, true).c_str());
			execve(binaryLocation.toString(false, true).c_str(), args, newEnvp);

			// TODO: the following may be redundant;
			delete[] args[0];
			delete[] args[1];
			delete[] pathInfoCStr;
			delete[] queryInfoCStr;
			delete[] newEnvp;
			std::cout << "Something bad happened, and execve failed" << std::endl;
			exit(1);
		}
		
		close(writePipe[0]);
		close(readPipe[1]);

		SharedPtr<CGIWriter> writer = new CGIWriter(writePipe[1]);
		SharedPtr<CGIReader> reader = new CGIReader(forkResult, readPipe[0], MSG_BUF_SIZE);
		reader->setWriter(writer);

		return std::make_pair(writer, reader);
	}
}
