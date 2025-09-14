#include "dispatcher.hpp"
#include "http.hpp"
#include "tasks.hpp"
#include "error.hpp"
#include "webserv.hpp"
#include "ystl.hpp"
#include <cstdlib>
#include <sstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <sys/wait.h>

namespace Webserv {
	CGIReader::CGIReader(
		ConnectionInfo conn,
		const SharedPtr<ResponseHandler>& resp,
		int pid,
		int fd,
		uint rSize
	):
		fd(fd), pid(pid), readSize(rSize), writer(NONE), responseHandler(resp), connectionInfo(conn) {}

	void CGIReader::setWriter(const SharedPtr<CGIWriter> wPtr) {
		writer = wPtr;
	}

	SharedPtr<ResponseHandler> CGIReader::getResponseHandler() {
		return responseHandler;
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

			// TODO: move the exit code check here.
			int wstatus;
			// No WNOHANG, because otherwise we will never be able to determine if it finishes running.
			int waitResult = waitpid(pid, &wstatus, 0);
			if (waitResult < 0) return Error(Error::GENERIC_ERROR, "waitpid error");
			if (WIFEXITED(wstatus)) {
#ifdef DEBUG
				std::cout << "Process " << pid << " has stopeed, cooking it rn" << std::endl;
#endif
				int exitCode = WEXITSTATUS(wstatus);

				HTTPResponse resp((Url()));
				std::stringstream finalBuffer;
				for (std::vector<std::string>::iterator it = readBuffer.begin(); it != readBuffer.end(); it++) {
					finalBuffer << *it;
				}

				if (exitCode != 0) {
					resp.setCode(HTTP_INTERNAL_SERVER_ERROR);
					resp.setContentType(contentTypeString(HTML));
					resp.setData(makeErrorPage(Error(Error::CGI_RUNTIME_FAULT, finalBuffer.str())));
				}
				else {
					std::string finalBufferStr = "HTTP/1.1 200 OK\n" + finalBuffer.str();
					Option<HTTPResponse> maybeResponse = HTTPResponse::fromString(finalBufferStr);
					if (maybeResponse.isNone()) {
						return Error(Error::HTTP_ERROR, "Failed to construct a response. " + finalBufferStr);
					}
					resp = maybeResponse.get();
				}
				responseHandler->setResponse(resp);
				return false;
			}

			return false;
		}

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
		std::cout << "Trying to write: " << str << std::endl;
		int writeError = write(fd, str.c_str(), str.size());
		if (writeError == -1) return Error(Error::CGI_IO_ERROR, "CGIWriter fail");
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

	char** setupEnvp(const std::map<std::string, std::string>& extraEnvs, char** parentEnvp) {
		uint sizeOfEnvp = 0;
		char** envpPtr = parentEnvp;
		while (*envpPtr != NULL) {
			envpPtr++;
			sizeOfEnvp++;
		}
		char** newEnvp = new char*[sizeOfEnvp + extraEnvs.size() + 1]();
		envpPtr = parentEnvp;
		uint i = 0;
		while (*envpPtr != NULL) {
			newEnvp[i] = *envpPtr;
			envpPtr++;
			i++;
		}
		for (std::map<std::string, std::string>::const_iterator it = extraEnvs.begin(); it != extraEnvs.end(); it++) {
			char* cstr = new char[it->second.size() + 1];
			it->second.copy(cstr, it->second.size());
			cstr[it->second.size()] = '\0';
			newEnvp[i] = cstr;
			i++;
		}
		newEnvp[i] = NULL;
		return newEnvp;
	}

	Result<CGIPipeline, Error> makeCGIPipeline(
		ConnectionInfo conn,
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
			dup2(readPipe[1], STDERR_FILENO); // Eh, screw it.

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

			std::map<std::string, std::string> extraEnvs;
			extraEnvs["PATH_INFO"] = extraPath.toString(false, true);
			extraEnvs["QUERY_INFO"] = "";

			char** newEnvp = setupEnvp(extraEnvs, envp);

			chdir(scriptLocation.exceptLast().toString(false, true).c_str());
			execve(binaryLocation.toString(false, true).c_str(), args, newEnvp);

			// TODO: the following may be redundant;
			delete[] args[0];
			delete[] args[1];
			char** cstr = newEnvp;
			while(*cstr) {
				delete[] *cstr;
				cstr++;
			}
			delete[] newEnvp;
			std::cerr << "Something bad happened, and execve failed" << std::endl;
			exit(1);
		}
		
		close(writePipe[0]);
		close(readPipe[1]);

		SharedPtr<ResponseHandler> respHandler = new ResponseHandler(conn);
		SharedPtr<CGIWriter> writer = new CGIWriter(writePipe[1]);
		SharedPtr<CGIReader> reader = new CGIReader(conn, respHandler, forkResult, readPipe[0], MSG_BUF_SIZE);
		reader->setWriter(writer);

		return std::make_pair(writer, reader);
	}
}
