/* Copyright 2013-2017 Sathya Laufer
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Homegear.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU Lesser General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
*/

#ifndef HTTPSERVER_H_
#define HTTPSERVER_H_

#include "../Exception.h"
#include "../Managers/FileDescriptorManager.h"
#include "../Encoding/Http.h"
#include "TcpSocket.h"

#include <atomic>

namespace BaseLib
{

/**
 * Exception class for the HTTP server.
 *
 * @see HttpServer
 */
class HttpServerException : public Exception
{
public:
	HttpServerException(std::string message) : Exception(message) {}
};

/**
 * This class provides a basic HTTP server. The class is thread safe.
 *
 * @see HttpServerException
 */
class HttpServer {
public:
	HttpServer(BaseLib::SharedObjects* baseLib, std::string listenAddress, uint16_t port, bool useSsl, std::string certFile, std::string certData, std::string keyFile, std::string keyData, std::string dhParamFile, std::string dhParamData);
	virtual ~HttpServer();

	bool isRunning() { return !_stopped; }
	void start();
	void stop();
	uint32_t connectionCount();
protected:
	BaseLib::SharedObjects* _bl = nullptr;
	std::atomic_bool _stopped;
	std::atomic_bool _stopServer;
	std::shared_ptr<TcpSocket> _socket;
	std::mutex _stateMutex;
	std::map<int32_t, std::shared_ptr<BaseLib::FileDescriptor>> _clients;
private:
	int32_t _currentClientId = 0;
	int32_t _backlog = 100;
	std::thread _mainThread;

	std::string _listenAddress;
	uint16_t _port = 8080;

	bool _useSsl = false;
	std::string _certFile;
	std::string _certData;
	std::string _keyFile;
	std::string _keyData;
	std::string _dhParamFile;
	std::string _dhParamData;

	void mainThread();
};

}
#endif
