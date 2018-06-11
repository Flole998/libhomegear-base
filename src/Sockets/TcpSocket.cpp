/* Copyright 2013-2017 Sathya Laufer
 *
 * libhomegear-base is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * libhomegear-base is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libhomegear-base.  If not, see
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

#include <gnutls/gnutls.h>
#include "../BaseLib.h"
#include "TcpSocket.h"

namespace BaseLib
{
TcpSocket::TcpSocket(BaseLib::SharedObjects* baseLib)
{
	_bl = baseLib;
	_stopServer = false;
	_autoConnect = false;
	_connecting = false;
	_socketDescriptor.reset(new FileDescriptor);
}

TcpSocket::TcpSocket(BaseLib::SharedObjects* baseLib, std::shared_ptr<FileDescriptor> socketDescriptor)
{
	_bl = baseLib;
	_stopServer = false;
	_autoConnect = false;
	_connecting = false;
	if(socketDescriptor) _socketDescriptor = socketDescriptor;
	else _socketDescriptor.reset(new FileDescriptor);
}

TcpSocket::TcpSocket(BaseLib::SharedObjects* baseLib, std::string hostname, std::string port)
{
	_bl = baseLib;
	_stopServer = false;
	_connecting = false;
	_socketDescriptor.reset(new FileDescriptor);
	_hostname = hostname;
    _verificationHostname = hostname;
	_port = port;
}

TcpSocket::TcpSocket(BaseLib::SharedObjects* baseLib, std::string hostname, std::string port, bool useSsl, std::string caFile, bool verifyCertificate) : TcpSocket(baseLib, hostname, port)
{
	_useSsl = useSsl;
	if(!caFile.empty())
	{
		PCertificateInfo certificateInfo = std::make_shared<CertificateInfo>();
		certificateInfo->caFile = caFile;
		_certificates.emplace("*", certificateInfo);
	}
	_verifyCertificate = verifyCertificate;

	if(_useSsl) initSsl();
}

TcpSocket::TcpSocket(BaseLib::SharedObjects* baseLib, std::string hostname, std::string port, bool useSsl, bool verifyCertificate, std::string caData) : TcpSocket(baseLib, hostname, port)
{
	_useSsl = useSsl;
	_verifyCertificate = verifyCertificate;
	if(!caData.empty())
	{
		PCertificateInfo certificateInfo = std::make_shared<CertificateInfo>();
		certificateInfo->caData = caData;
		_certificates.emplace("*", certificateInfo);
	}

	if(_useSsl) initSsl();
}

TcpSocket::TcpSocket(BaseLib::SharedObjects* baseLib, std::string hostname, std::string port, bool useSsl, std::string caFile, bool verifyCertificate, std::string clientCertFile, std::string clientKeyFile) : TcpSocket(baseLib, hostname, port)
{
	_useSsl = useSsl;
	_verifyCertificate = verifyCertificate;
	if(!caFile.empty() || !clientCertFile.empty() || !clientKeyFile.empty())
	{
		PCertificateInfo certificateInfo = std::make_shared<CertificateInfo>();
		certificateInfo->caFile = caFile;
		certificateInfo->certFile = clientCertFile;
		certificateInfo->keyFile = clientKeyFile;
		_certificates.emplace("*", certificateInfo);
	}

	if(_useSsl) initSsl();
}

TcpSocket::TcpSocket(BaseLib::SharedObjects* baseLib, std::string hostname, std::string port, bool useSsl, bool verifyCertificate, std::string caData, std::string clientCertData, std::string clientKeyData) : TcpSocket(baseLib, hostname, port)
{
	_useSsl = useSsl;
	_verifyCertificate = verifyCertificate;
	if(!caData.empty() || !clientCertData.empty() || !clientKeyData.empty())
	{
		PCertificateInfo certificateInfo = std::make_shared<CertificateInfo>();
		certificateInfo->caData = caData;
		certificateInfo->certData = clientCertData;
		certificateInfo->keyData = clientKeyData;
		_certificates.emplace("*", certificateInfo);
	}

	if(_useSsl) initSsl();
}

TcpSocket::TcpSocket(BaseLib::SharedObjects* baseLib, std::string hostname, std::string port, bool useSsl, bool verifyCertificate, std::string caFile, std::string caData, std::string clientCertFile, std::string clientCertData, std::string clientKeyFile, std::string clientKeyData) : TcpSocket(baseLib, hostname, port)
{
    _useSsl = useSsl;
    _verifyCertificate = verifyCertificate;
	if(!caFile.empty() || !caData.empty() || !clientCertFile.empty() || !clientCertData.empty() || !clientKeyFile.empty() || !clientKeyData.empty())
	{
		PCertificateInfo certificateInfo = std::make_shared<CertificateInfo>();
		certificateInfo->caFile = caFile;
		certificateInfo->caData = caData;
		certificateInfo->certFile = clientCertFile;
		certificateInfo->certData = clientCertData;
		certificateInfo->keyFile = clientKeyFile;
		certificateInfo->keyData = clientKeyData;
		_certificates.emplace("*", certificateInfo);
	}

    if(_useSsl) initSsl();
}

TcpSocket::TcpSocket(BaseLib::SharedObjects* baseLib, TcpServerInfo& serverInfo)
{
	_bl = baseLib;
	_connecting = false;

	_stopServer = false;
	_isServer = true;
	_useSsl = serverInfo.useSsl;
	_maxConnections = serverInfo.maxConnections;
	_certificates = serverInfo.certificates;
	_dhParamFile = serverInfo.dhParamFile;
	_dhParamData = serverInfo.dhParamData;
	_requireClientCert = serverInfo.requireClientCert;
	_newConnectionCallback.swap(serverInfo.newConnectionCallback);
    _connectionClosedCallback.swap(serverInfo.connectionClosedCallback);
	_packetReceivedCallback.swap(serverInfo.packetReceivedCallback);

    _serverThreads.resize(serverInfo.serverThreads);
}

TcpSocket::~TcpSocket()
{
	_stopServer = true;
    for(auto& serverThread : _serverThreads)
    {
        _bl->threadManager.join(serverThread);
    }

	_bl->fileDescriptorManager.close(_socketDescriptor);
    freeCredentials();
	if(_tlsPriorityCache) gnutls_priority_deinit(_tlsPriorityCache);
	if(_dhParams) gnutls_dh_params_deinit(_dhParams);
}

std::string TcpSocket::getIpAddress()
{
	if(!_ipAddress.empty()) return _ipAddress;
	_ipAddress = Net::resolveHostname(_hostname);
	return _ipAddress;
}


// {{{ Server
	void TcpSocket::bindSocket()
	{
		_socketDescriptor = bindAndReturnSocket(_bl->fileDescriptorManager, _listenAddress, _listenPort, _ipAddress, _boundListenPort);
	}

	void TcpSocket::startServer(std::string address, std::string port, std::string& listenAddress)
	{
		waitForServerStopped();

		if(_useSsl) initSsl();

		_stopServer = false;
		_listenAddress = address;
		_listenPort = port;
		bindSocket();
		listenAddress = _ipAddress;
        for(auto& serverThread : _serverThreads)
        {
            _bl->threadManager.start(serverThread, true, &TcpSocket::serverThread, this);
        }
	}

	void TcpSocket::startServer(std::string address, std::string& listenAddress, int32_t& listenPort)
	{
		waitForServerStopped();

		if(_useSsl) initSsl();

		_stopServer = false;
		_listenAddress = address;
		_listenPort = "0";
		bindSocket();
		listenAddress = _ipAddress;
		listenPort = _boundListenPort;
        for(auto& serverThread : _serverThreads)
        {
            _bl->threadManager.start(serverThread, true, &TcpSocket::serverThread, this);
        }
	}

	void TcpSocket::stopServer()
	{
		_stopServer = true;
	}

	void TcpSocket::waitForServerStopped()
	{
		_stopServer = true;
        for(auto& serverThread : _serverThreads)
        {
            _bl->threadManager.join(serverThread);
        }

		_bl->fileDescriptorManager.close(_socketDescriptor);
        freeCredentials();
		if(_tlsPriorityCache) gnutls_priority_deinit(_tlsPriorityCache);
		if(_dhParams) gnutls_dh_params_deinit(_dhParams);
		_tlsPriorityCache = nullptr;
		_dhParams = nullptr;
	}

	int verifyClientCert(gnutls_session_t tlsSession)
	{
		//Called during handshake just after the certificate message has been received.

		uint32_t status = (uint32_t)-1;
		if(gnutls_certificate_verify_peers3(tlsSession, 0, &status) != GNUTLS_E_SUCCESS) return GNUTLS_E_INTERNAL_ERROR; //Terminate handshake
		if(status > 0) return GNUTLS_E_INTERNAL_ERROR;
		return GNUTLS_E_SUCCESS;
	}

    int postClientHello(gnutls_session_t tlsSession)
    {
        TcpSocket* tcpSocket = (TcpSocket*)gnutls_session_get_ptr(tlsSession);
        if(!tcpSocket) return GNUTLS_E_INTERNAL_ERROR;

        auto& credentials = tcpSocket->getCredentials();

        int32_t result = 0;
        if(credentials.size() > 1)
        {
            std::array<char, 300> nameBuffer;
            size_t dataSize = nameBuffer.size() - 1;
            unsigned  int type = GNUTLS_NAME_DNS;
            if(gnutls_server_name_get(tlsSession, nameBuffer.data(), &dataSize, &type, 0) != GNUTLS_E_SUCCESS)
            {
                if((result = gnutls_credentials_set(tlsSession, GNUTLS_CRD_CERTIFICATE, credentials.begin()->second)) != GNUTLS_E_SUCCESS)
                {
                    return result;
                }
            }
            else
            {
                nameBuffer.at(nameBuffer.size() - 1) = 0;
                std::string serverName(nameBuffer.data());
                auto credentialsIterator = credentials.find(serverName);
                if(credentialsIterator != credentials.end())
                {
                    if((result = gnutls_credentials_set(tlsSession, GNUTLS_CRD_CERTIFICATE, credentialsIterator->second)) != GNUTLS_E_SUCCESS)
                    {
                        return result;
                    }
                }
                else
                {
                    if((result = gnutls_credentials_set(tlsSession, GNUTLS_CRD_CERTIFICATE, credentials.begin()->second)) != GNUTLS_E_SUCCESS)
                    {
                        return result;
                    }
                }
            }
        }
        else if(credentials.size() == 1)
        {
            if((result = gnutls_credentials_set(tlsSession, GNUTLS_CRD_CERTIFICATE, credentials.begin()->second)) != GNUTLS_E_SUCCESS)
            {
                return GNUTLS_E_CERTIFICATE_ERROR;
            }
        }
        else return GNUTLS_E_CERTIFICATE_ERROR;

        return GNUTLS_E_SUCCESS;
    }

	void TcpSocket::initClientSsl(PFileDescriptor fileDescriptor)
	{
		if(!_tlsPriorityCache)
		{
			_bl->fileDescriptorManager.shutdown(fileDescriptor);
			throw SocketSSLException("Error: Could not initiate TLS connection. _tlsPriorityCache is nullptr.");
		}
		if(_x509Credentials.empty())
		{
			_bl->fileDescriptorManager.shutdown(fileDescriptor);
			throw SocketSSLException("Error: Could not initiate TLS connection. _x509Credentials is empty.");
		}
		int32_t result = 0;
		if((result = gnutls_init(&fileDescriptor->tlsSession, GNUTLS_SERVER)) != GNUTLS_E_SUCCESS)
		{
			fileDescriptor->tlsSession = nullptr;
			_bl->fileDescriptorManager.shutdown(fileDescriptor);
			throw SocketSSLException("Error: Could not initialize TLS session: " + std::string(gnutls_strerror(result)));
		}
		if(!fileDescriptor->tlsSession)
		{
			_bl->fileDescriptorManager.shutdown(fileDescriptor);
			throw SocketSSLException("Error: Client TLS session is nullptr.");
		}

        gnutls_session_set_ptr(fileDescriptor->tlsSession, this);

		if((result = gnutls_priority_set(fileDescriptor->tlsSession, _tlsPriorityCache)) != GNUTLS_E_SUCCESS)
		{
			_bl->fileDescriptorManager.shutdown(fileDescriptor);
			throw SocketSSLException("Error: Could not set cipher priority on TLS session: " + std::string(gnutls_strerror(result)));
		}

        gnutls_handshake_set_post_client_hello_function(fileDescriptor->tlsSession, &postClientHello);

		gnutls_certificate_server_set_request(fileDescriptor->tlsSession, _requireClientCert ? GNUTLS_CERT_REQUIRE : GNUTLS_CERT_IGNORE);
		if(!fileDescriptor || fileDescriptor->descriptor == -1)
		{
			_bl->fileDescriptorManager.shutdown(fileDescriptor);
			throw SocketSSLException("Error setting TLS socket descriptor: Provided socket descriptor is invalid.");
		}
		gnutls_transport_set_ptr(fileDescriptor->tlsSession, (gnutls_transport_ptr_t)(uintptr_t)fileDescriptor->descriptor);
		result = gnutls_handshake(fileDescriptor->tlsSession);
		if(result < 0)
		{
			_bl->fileDescriptorManager.shutdown(fileDescriptor);
			throw SocketSSLException("TLS handshake has failed: " + std::string(gnutls_strerror(result)));
		}
	}

	void TcpSocket::readClient(PTcpClientData clientData)
	{
		try
		{
			int32_t bytesRead = 0;
			bool moreData = true;

			while(moreData)
			{
				bytesRead = clientData->socket->proofread((char*)clientData->buffer.data(), clientData->buffer.size(), moreData);

				if(bytesRead > (signed)clientData->buffer.size()) bytesRead = clientData->buffer.size();

				std::vector<uint8_t> bytesReceived(clientData->buffer.data(), clientData->buffer.data() + bytesRead);
				if(_packetReceivedCallback) _packetReceivedCallback(clientData->id, bytesReceived);
			}
		}
		catch(const std::exception& ex)
		{
			_bl->fileDescriptorManager.close(clientData->fileDescriptor);
            if(_connectionClosedCallback) _connectionClosedCallback(clientData->id);
		}
		catch(BaseLib::Exception& ex)
		{
			_bl->fileDescriptorManager.close(clientData->fileDescriptor);
            if(_connectionClosedCallback) _connectionClosedCallback(clientData->id);
		}
		catch(...)
		{
			_bl->fileDescriptorManager.close(clientData->fileDescriptor);
            if(_connectionClosedCallback) _connectionClosedCallback(clientData->id);
		}
	}

	void TcpSocket::sendToClient(int32_t clientId, TcpPacket packet, bool closeConnection)
	{
		PTcpClientData clientData;
		try
		{
			{
				std::lock_guard<std::mutex> clientsGuard(_clientsMutex);
				auto clientIterator = _clients.find(clientId);
				if(clientIterator == _clients.end()) return;
				clientData = clientIterator->second;
			}

			clientData->socket->proofwrite((char*)packet.data(), packet.size());
			if(closeConnection)
			{
				_bl->fileDescriptorManager.close(clientData->fileDescriptor);
				if(_connectionClosedCallback) _connectionClosedCallback(clientData->id);
			}
		}
		catch(const std::exception& ex)
		{
			_bl->fileDescriptorManager.close(clientData->fileDescriptor);
			if(_connectionClosedCallback) _connectionClosedCallback(clientData->id);
		}
		catch(BaseLib::Exception& ex)
		{
			_bl->fileDescriptorManager.close(clientData->fileDescriptor);
			if(_connectionClosedCallback) _connectionClosedCallback(clientData->id);
		}
		catch(...)
		{
			_bl->fileDescriptorManager.close(clientData->fileDescriptor);
			if(_connectionClosedCallback) _connectionClosedCallback(clientData->id);
		}
	}

    int32_t TcpSocket::clientCount()
    {
        std::lock_guard<std::mutex> clientsGuard(_clientsMutex);
        return _clients.size();
    }

	void TcpSocket::serverThread()
	{
		int32_t result = 0;
        int32_t socketDescriptor = -1;
        std::map<int32_t, PTcpClientData> clients;
		while(!_stopServer)
		{
			try
			{
                {
                    std::lock_guard<std::mutex> socketDescriptorGuard(_socketDescriptorMutex);
                    if(!_socketDescriptor || _socketDescriptor->descriptor == -1)
                    {
                        if(_stopServer) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                        bindSocket();
                        continue;
                    }
                    socketDescriptor = _socketDescriptor->descriptor;
                }

				timeval timeout;
				timeout.tv_sec = 0;
				timeout.tv_usec = 100000;
				fd_set readFileDescriptor;
				int32_t maxfd = 0;
				FD_ZERO(&readFileDescriptor);
				{
					auto fileDescriptorGuard = _bl->fileDescriptorManager.getLock();
					fileDescriptorGuard.lock();
					maxfd = socketDescriptor;
					FD_SET(socketDescriptor, &readFileDescriptor);

					{
						for(auto& client : clients)
						{
							if(!client.second->fileDescriptor || client.second->fileDescriptor->descriptor == -1) continue;
							FD_SET(client.second->fileDescriptor->descriptor, &readFileDescriptor);
							if(client.second->fileDescriptor->descriptor > maxfd) maxfd = client.second->fileDescriptor->descriptor;
						}
					}
				}

				result = select(maxfd + 1, &readFileDescriptor, NULL, NULL, &timeout);
				if(result == 0)
				{
					if(HelperFunctions::getTime() - _lastGarbageCollection > 60000 || _clients.size() >= _maxConnections)
                    {
                        collectGarbage();
                        collectGarbage(clients);
                    }
					continue;
				}
				else if(result == -1)
				{
					if(errno == EINTR) continue;
					_bl->out.printError("Error: select returned -1: " + std::string(strerror(errno)));
					continue;
				}

				if (FD_ISSET(socketDescriptor, &readFileDescriptor) && !_stopServer)
				{
					struct sockaddr_storage clientInfo;
					socklen_t addressSize = sizeof(addressSize);
					std::shared_ptr<BaseLib::FileDescriptor> clientFileDescriptor = _bl->fileDescriptorManager.add(accept(socketDescriptor, (struct sockaddr *) &clientInfo, &addressSize));
					if(!clientFileDescriptor || clientFileDescriptor->descriptor == -1) continue;

					try
					{
						getpeername(clientFileDescriptor->descriptor, (struct sockaddr*)&clientInfo, &addressSize);

						uint16_t port = 0;
						char ipString[INET6_ADDRSTRLEN];
						if (clientInfo.ss_family == AF_INET) {
							struct sockaddr_in *s = (struct sockaddr_in *)&clientInfo;
							port = ntohs(s->sin_port);
							inet_ntop(AF_INET, &s->sin_addr, ipString, sizeof(ipString));
						} else { // AF_INET6
							struct sockaddr_in6 *s = (struct sockaddr_in6 *)&clientInfo;
							port = ntohs(s->sin6_port);
							inet_ntop(AF_INET6, &s->sin6_addr, ipString, sizeof(ipString));
						}
						std::string address = std::string(ipString);

						if(_clients.size() > _maxConnections)
						{
							collectGarbage();
							if(_clients.size() > _maxConnections)
							{
                                _bl->out.printError("Error: No more clients can connect to me as the maximum number of allowed connections is reached. Listen IP: " + _listenAddress + ", bound port: " + _listenPort + ", client IP: " + ipString);
								_bl->fileDescriptorManager.shutdown(clientFileDescriptor);
								continue;
							}
						}

						int32_t currentClientId = 0;

                        if(_stopServer)
                        {
                            _bl->fileDescriptorManager.shutdown(clientFileDescriptor);
                            continue;
                        }

                        if(_useSsl) initClientSsl(clientFileDescriptor);

						{
                            std::lock_guard<std::mutex> clientsGuard(_clientsMutex);
							currentClientId = _currentClientId++;

							PTcpClientData clientData = std::make_shared<TcpClientData>();
							clientData->id = currentClientId;
							clientData->fileDescriptor = clientFileDescriptor;
							clientData->socket = std::make_shared<BaseLib::TcpSocket>(_bl, clientFileDescriptor);
							clientData->socket->setReadTimeout(100000);
							clientData->socket->setWriteTimeout(15000000);

							_clients[currentClientId] = clientData;
                            clients[currentClientId] = clientData;
						}

						if(_newConnectionCallback) _newConnectionCallback(currentClientId, address, port);
					}
					catch(const std::exception& ex)
					{
						_bl->fileDescriptorManager.shutdown(clientFileDescriptor);
						_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
					}
					catch(BaseLib::Exception& ex)
					{
						_bl->fileDescriptorManager.shutdown(clientFileDescriptor);
						_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
					}
					catch(...)
					{
						_bl->fileDescriptorManager.shutdown(clientFileDescriptor);
						_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
					}
					continue;
				}

				PTcpClientData clientData;
				{
					for(auto& client : clients)
					{
						if(client.second->fileDescriptor->descriptor == -1) continue;
						if(FD_ISSET(client.second->fileDescriptor->descriptor, &readFileDescriptor))
						{
							clientData = client.second;
							break;
						}
					}
				}

				if(clientData) readClient(clientData);
			}
			catch(const std::exception& ex)
			{
				_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
		}
        std::lock_guard<std::mutex> socketDescriptorGuard(_socketDescriptorMutex);
		_bl->fileDescriptorManager.close(_socketDescriptor);
	}

	void TcpSocket::collectGarbage()
	{
		_lastGarbageCollection = BaseLib::HelperFunctions::getTime();

		std::lock_guard<std::mutex> clientsGuard(_clientsMutex);
		std::vector<int32_t> clientsToRemove;
		{
			for(auto& client : _clients)
			{
				if(!client.second->fileDescriptor || client.second->fileDescriptor->descriptor == -1) clientsToRemove.push_back(client.first);
			}
		}
		for(auto& client : clientsToRemove)
		{
			_clients.erase(client);
		}
	}

    void TcpSocket::collectGarbage(std::map<int32_t, PTcpClientData>& clients)
    {
        std::vector<int32_t> clientsToRemove;
        {
            for(auto& client : clients)
            {
                if(!client.second->fileDescriptor || client.second->fileDescriptor->descriptor == -1) clientsToRemove.push_back(client.first);
            }
        }
        for(auto& client : clientsToRemove)
        {
            clients.erase(client);
        }
    }
// }}}

PFileDescriptor TcpSocket::bindAndReturnSocket(FileDescriptorManager& fileDescriptorManager, std::string address, std::string port, std::string& listenAddress, int32_t& listenPort)
{
	PFileDescriptor socketDescriptor;
	addrinfo hostInfo;
	addrinfo *serverInfo = nullptr;

	int32_t yes = 1;

	memset(&hostInfo, 0, sizeof(hostInfo));

	hostInfo.ai_family = AF_UNSPEC;
	hostInfo.ai_socktype = SOCK_STREAM;
	hostInfo.ai_flags = AI_PASSIVE;
	char buffer[100];
	int32_t result;
	if((result = getaddrinfo(address.c_str(), port.c_str(), &hostInfo, &serverInfo)) != 0)
	{
		throw SocketOperationException("Error: Could not get address information: " + std::string(gai_strerror(result)));
	}

	bool bound = false;
	int32_t error = 0;
	for(struct addrinfo *info = serverInfo; info != 0; info = info->ai_next)
	{
		socketDescriptor = fileDescriptorManager.add(socket(info->ai_family, info->ai_socktype, info->ai_protocol));
		if(socketDescriptor->descriptor == -1) continue;
		if(!(fcntl(socketDescriptor->descriptor, F_GETFL) & O_NONBLOCK))
		{
			if(fcntl(socketDescriptor->descriptor, F_SETFL, fcntl(socketDescriptor->descriptor, F_GETFL) | O_NONBLOCK) < 0) throw SocketOperationException("Error: Could not set socket options.");
		}
		if(setsockopt(socketDescriptor->descriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int32_t)) == -1) throw SocketOperationException("Error: Could not set socket options.");
		if(bind(socketDescriptor->descriptor, info->ai_addr, info->ai_addrlen) == -1)
		{
			socketDescriptor.reset();
			error = errno;
			continue;
		}
		switch (info->ai_family)
		{
			case AF_INET:
				inet_ntop (info->ai_family, &((struct sockaddr_in *) info->ai_addr)->sin_addr, buffer, 100);
				listenAddress = std::string(buffer);
				break;
			case AF_INET6:
				inet_ntop (info->ai_family, &((struct sockaddr_in6 *) info->ai_addr)->sin6_addr, buffer, 100);
				listenAddress = std::string(buffer);
				break;
		}
		bound = true;
		break;
	}
	freeaddrinfo(serverInfo);
	if(!bound)
	{
		fileDescriptorManager.shutdown(socketDescriptor);
		socketDescriptor.reset();
        if(errno == EADDRINUSE) throw SocketAddressInUseException("Error: Could not start listening on port " + port + ": " + std::string(strerror(error)));
		else throw SocketBindException("Error: Could not start listening on port " + port + ": " + std::string(strerror(error)));
	}
	else if(socketDescriptor->descriptor == -1 || listen(socketDescriptor->descriptor, 100) == -1)
	{
		fileDescriptorManager.shutdown(socketDescriptor);
		socketDescriptor.reset();
		throw SocketOperationException("Error: Server could not start listening on port " + port + ": " + std::string(strerror(errno)));
	}

	struct sockaddr_in addressInfo;
	socklen_t addressInfoLength = sizeof(addressInfo);
	if (getsockname(socketDescriptor->descriptor, (struct sockaddr*)&addressInfo, &addressInfoLength) == -1)
	{
		fileDescriptorManager.shutdown(socketDescriptor);
		socketDescriptor.reset();
		throw SocketOperationException("Error: Could get port listening on: " + std::string(strerror(error)));
	}
	listenPort = addressInfo.sin_port;

	if(listenAddress == "0.0.0.0") listenAddress = Net::getMyIpAddress();
	else if(listenAddress == "::")
	{

		listenAddress = Net::getMyIp6Address();
	}
	return socketDescriptor;
}

void TcpSocket::freeCredentials()
{
	for(auto& credentials : _x509Credentials)
	{
		if(credentials.second) gnutls_certificate_free_credentials(credentials.second);
	}
	_x509Credentials.clear();
}

void TcpSocket::initSsl()
{
    freeCredentials();
	if(_tlsPriorityCache) gnutls_priority_deinit(_tlsPriorityCache);
	if(_dhParams) gnutls_dh_params_deinit(_dhParams);

	int32_t result = 0;

    if(!_dhParamFile.empty() || !_dhParamData.empty())
    {
        if((result = gnutls_dh_params_init(&_dhParams)) != GNUTLS_E_SUCCESS)
        {
            _dhParams = nullptr;
            throw SocketSSLException("Error: Could not initialize DH parameters: " + std::string(gnutls_strerror(result)));
        }

        std::vector<uint8_t> binaryData;
        gnutls_datum_t data{};
        if(!_dhParamFile.empty())
        {
            try
            {
                binaryData = Io::getUBinaryFileContent(_dhParamFile);
                binaryData.push_back(0); //gnutls_datum_t.data needs to be null terminated
            }
            catch(BaseLib::Exception& ex)
            {
                gnutls_dh_params_deinit(_dhParams);
                _dhParams = nullptr;
                throw SocketSSLException("Error: Could not load DH parameters: " + std::string(ex.what()));
            }
            catch(...)
            {
                gnutls_dh_params_deinit(_dhParams);
                _dhParams = nullptr;
                throw SocketSSLException("Error: Could not load DH parameter file \"" + _dhParamFile + "\".");
            }
            data.data = binaryData.data();
            data.size = static_cast<unsigned int>(binaryData.size());
        }
        else
        {
            data.data = (unsigned char*) _dhParamData.c_str();
            data.size = static_cast<unsigned int>(_dhParamData.size());
        }

        if(data.size > 0)
        {
            if((result = gnutls_dh_params_import_pkcs3(_dhParams, &data, GNUTLS_X509_FMT_PEM)) != GNUTLS_E_SUCCESS)
            {
                gnutls_dh_params_deinit(_dhParams);
                _dhParams = nullptr;
                throw SocketSSLException("Error: Could not import DH parameters: " + std::string(gnutls_strerror(result)));
            }
        }
    }

    if(_certificates.empty())
    {
        if(_requireClientCert && _isServer)
        {
            throw SocketSSLException("No CA certificates specified (1).");
        }

        if(!_isServer)
        {
            gnutls_certificate_credentials_t x509Credentials = nullptr;
            if((result = gnutls_certificate_allocate_credentials(&x509Credentials)) != GNUTLS_E_SUCCESS)
            {
                x509Credentials = nullptr;
                throw SocketSSLException("Could not allocate certificate credentials: " + std::string(gnutls_strerror(result)));
            }

            if((result = gnutls_certificate_set_x509_system_trust(x509Credentials)) < 0)
            {
                gnutls_certificate_free_credentials(x509Credentials);
                x509Credentials = nullptr;
                throw SocketSSLException("Could not load system certificates: " + std::string(gnutls_strerror(result)));
            }
            _x509Credentials.emplace("*", x509Credentials);
        }
    }

    for(auto& certificateInfo : _certificates)
    {
        gnutls_certificate_credentials_t x509Credentials = nullptr;
        if((result = gnutls_certificate_allocate_credentials(&x509Credentials)) != GNUTLS_E_SUCCESS)
        {
            x509Credentials = nullptr;
			freeCredentials();
            throw SocketSSLException("Could not allocate certificate credentials: " + std::string(gnutls_strerror(result)));
        }

		int32_t caCertificateCount = 0;
        if(!certificateInfo.second->caData.empty())
        {
            gnutls_datum_t caData;
            caData.data = (unsigned char*) certificateInfo.second->caData.c_str();
            caData.size = certificateInfo.second->caData.size();
            if((result = gnutls_certificate_set_x509_trust_mem(x509Credentials, &caData, GNUTLS_X509_FMT_PEM)) < 0)
            {
                gnutls_certificate_free_credentials(x509Credentials);
                x509Credentials = nullptr;
				freeCredentials();
                throw SocketSSLException("Could not load trusted certificates: " + std::string(gnutls_strerror(result)));
            }
			caCertificateCount = result;
        }
        else if(!certificateInfo.second->caFile.empty())
        {
            if((result = gnutls_certificate_set_x509_trust_file(x509Credentials, certificateInfo.second->caFile.c_str(), GNUTLS_X509_FMT_PEM)) < 0)
            {
                gnutls_certificate_free_credentials(x509Credentials);
                x509Credentials = nullptr;
				freeCredentials();
                throw SocketSSLException("Could not load trusted certificates from \"" + certificateInfo.second->caFile + "\": " + std::string(gnutls_strerror(result)));
            }
			caCertificateCount = result;
        }
        else if(_requireClientCert && _isServer)
        {
            throw SocketSSLException("Client certificate authentication is enabled, but \"caFile\" and \"caData\" are not specified.");
        }

        if(caCertificateCount == 0 && ((_verifyCertificate && !_isServer) || (_requireClientCert && _isServer)))
        {
            gnutls_certificate_free_credentials(x509Credentials);
            x509Credentials = nullptr;
			freeCredentials();
            throw SocketSSLException("No CA certificates specified (2).");
        }

        if(_isServer)
        {
            if(!certificateInfo.second->certData.empty() && !certificateInfo.second->keyData.empty())
            {
                gnutls_datum_t certData;
                certData.data = (unsigned char*) certificateInfo.second->certData.c_str();
                certData.size = certificateInfo.second->certData.size();

                gnutls_datum_t keyData;
                keyData.data = (unsigned char*)certificateInfo.second->keyData.c_str();
                keyData.size = certificateInfo.second->keyData.size();

                if((result = gnutls_certificate_set_x509_key_mem(x509Credentials, &certData, &keyData, GNUTLS_X509_FMT_PEM)) < 0)
                {
                    gnutls_certificate_free_credentials(x509Credentials);
                    x509Credentials = nullptr;
					freeCredentials();
                    throw SocketSSLException("Could not load server certificate or key file: " + std::string(gnutls_strerror(result)));
                }
            }
            else if(!certificateInfo.second->certFile.empty() && !certificateInfo.second->keyFile.empty())
            {
                if((result = gnutls_certificate_set_x509_key_file(x509Credentials, certificateInfo.second->certFile.c_str(), certificateInfo.second->keyFile.c_str(), GNUTLS_X509_FMT_PEM)) < 0)
                {
                    gnutls_certificate_free_credentials(x509Credentials);
                    x509Credentials = nullptr;
					freeCredentials();
                    throw SocketSSLException("Could not load certificate or key file from \"" + certificateInfo.second->certFile + "\" or \"" + certificateInfo.second->keyFile + "\": " + std::string(gnutls_strerror(result)));
                }
            }
            else if(_useSsl)
            {
                throw SocketSSLException("SSL is enabled but no certificates are specified.");
            }

            if(_dhParams) gnutls_certificate_set_dh_params(x509Credentials, _dhParams);

            if(_requireClientCert) gnutls_certificate_set_verify_function(x509Credentials, &verifyClientCert);
        }
        else
        {
            if(!certificateInfo.second->certData.empty() && !certificateInfo.second->keyData.empty())
            {
                gnutls_datum_t clientCertData{};
                clientCertData.data = (unsigned char*) certificateInfo.second->certData.c_str();
                clientCertData.size = static_cast<unsigned int>(certificateInfo.second->certData.size());

                gnutls_datum_t clientKeyData{};
                clientKeyData.data = (unsigned char*) certificateInfo.second->keyData.c_str();
                clientKeyData.size = static_cast<unsigned int>(certificateInfo.second->keyData.size());

                if((result = gnutls_certificate_set_x509_key_mem(x509Credentials, &clientCertData, &clientKeyData, GNUTLS_X509_FMT_PEM)) < 0)
                {
                    gnutls_certificate_free_credentials(x509Credentials);
					x509Credentials = nullptr;
					freeCredentials();
                    throw SocketSSLException("Could not load client certificate or key: " + std::string(gnutls_strerror(result)));
                }
            }
            else if(!certificateInfo.second->certFile.empty() && !certificateInfo.second->keyFile.empty())
            {
                if((result = gnutls_certificate_set_x509_key_file(x509Credentials, certificateInfo.second->certFile.c_str(), certificateInfo.second->keyFile.c_str(), GNUTLS_X509_FMT_PEM)) < 0)
                {
                    gnutls_certificate_free_credentials(x509Credentials);
					x509Credentials = nullptr;
					freeCredentials();
                    throw SocketSSLException("Could not load client certificate and key from \"" + certificateInfo.second->certFile + "\" and \"" + certificateInfo.second->keyFile + "\": " + std::string(gnutls_strerror(result)));
                }
            }
        }

		_x509Credentials.emplace(certificateInfo.first, x509Credentials);
    }

    if(_isServer)
    {
        if((result = gnutls_priority_init(&_tlsPriorityCache, "NORMAL", nullptr)) != GNUTLS_E_SUCCESS)
        {
            freeCredentials();
            gnutls_dh_params_deinit(_dhParams);
            _dhParams = nullptr;
            _tlsPriorityCache = nullptr;
            throw SocketSSLException("Error: Could not initialize cipher priorities: " + std::string(gnutls_strerror(result)));
        }
    }
}

void TcpSocket::open()
{
	_connecting = true;
	if(!_socketDescriptor || _socketDescriptor->descriptor < 0) getSocketDescriptor();
	else if(!connected())
	{
		close();
		getSocketDescriptor();
	}
	_connecting = false;
}

void TcpSocket::autoConnect()
{
	if(!_autoConnect) return;
	_connecting = true;
	if(!_socketDescriptor || _socketDescriptor->descriptor < 0) getSocketDescriptor();
	else if(!connected())
	{
		close();
		getSocketDescriptor();
	}
	_connecting = false;
}

void TcpSocket::close()
{
	_readMutex.lock();
	_writeMutex.lock();
	_bl->fileDescriptorManager.close(_socketDescriptor);
	_writeMutex.unlock();
	_readMutex.unlock();
}

int32_t TcpSocket::proofread(char* buffer, int32_t bufferSize)
{
	bool moreData = false;
	return proofread(buffer, bufferSize, moreData);
}

int32_t TcpSocket::proofread(char* buffer, int32_t bufferSize, bool& moreData)
{
	moreData = false;

	if(!_socketDescriptor) throw SocketOperationException("Socket descriptor is nullptr.");
	_readMutex.lock();
	if(_autoConnect && !connected())
	{
		_readMutex.unlock();
		autoConnect();
		_readMutex.lock();
	}

	int32_t bytesRead = 0;
	if(_socketDescriptor->tlsSession && gnutls_record_check_pending(_socketDescriptor->tlsSession) > 0)
	{
		do
		{
			bytesRead = static_cast<int32_t>(gnutls_record_recv(_socketDescriptor->tlsSession, buffer, bufferSize));
		} while(bytesRead == GNUTLS_E_INTERRUPTED || bytesRead == GNUTLS_E_AGAIN);
		if(bytesRead > 0)
		{
			if(gnutls_record_check_pending(_socketDescriptor->tlsSession) > 0) moreData = true;
			_readMutex.unlock();
			if(bytesRead > bufferSize) bytesRead = bufferSize;
			return bytesRead;
		}
	}

	timeval timeout{};
	int32_t seconds = _readTimeout / 1000000;
	timeout.tv_sec = seconds;
	timeout.tv_usec = _readTimeout - (1000000 * seconds);
	fd_set readFileDescriptor{};
	FD_ZERO(&readFileDescriptor);
	auto fileDescriptorGuard = _bl->fileDescriptorManager.getLock();
	fileDescriptorGuard.lock();
	int32_t nfds = _socketDescriptor->descriptor + 1;
	if(nfds <= 0)
	{
		fileDescriptorGuard.unlock();
		_readMutex.unlock();
		throw SocketClosedException("Connection to client number " + std::to_string(_socketDescriptor->id) + " closed (1).");
	}
	FD_SET(_socketDescriptor->descriptor, &readFileDescriptor);
	fileDescriptorGuard.unlock();
	bytesRead = select(nfds, &readFileDescriptor, NULL, NULL, &timeout);
	if(bytesRead == 0)
	{
		_readMutex.unlock();
		throw SocketTimeOutException("Reading from socket timed out (1).", SocketTimeOutException::SocketTimeOutType::selectTimeout);
	}
	if(bytesRead != 1)
	{
		_readMutex.unlock();
		throw SocketClosedException("Connection to client number " + std::to_string(_socketDescriptor->id) + " closed (2).");
	}
	if(_socketDescriptor->tlsSession)
	{
		do
		{
			bytesRead = gnutls_record_recv(_socketDescriptor->tlsSession, buffer, bufferSize);
		} while(bytesRead == GNUTLS_E_INTERRUPTED || bytesRead == GNUTLS_E_AGAIN);

		if(gnutls_record_check_pending(_socketDescriptor->tlsSession) > 0) moreData = true;
	}
	else
	{
		do
		{
			bytesRead = read(_socketDescriptor->descriptor, buffer, bufferSize);
		} while(bytesRead < 0 && (errno == EAGAIN || errno == EINTR));
	}
	if(bytesRead <= 0)
	{
		_readMutex.unlock();
		if(bytesRead == -1)
		{
			if(errno == ETIMEDOUT) throw SocketTimeOutException("Reading from socket timed out (2).", SocketTimeOutException::SocketTimeOutType::readTimeout);
			else throw SocketClosedException("Connection to client number " + std::to_string(_socketDescriptor->id) + " closed (3): " + strerror(errno));
		}
		else throw SocketClosedException("Connection to client number " + std::to_string(_socketDescriptor->id) + " closed (3).");
	}
	_readMutex.unlock();
	if(bytesRead > bufferSize) bytesRead = bufferSize;
	return bytesRead;
}

int32_t TcpSocket::proofwrite(const std::shared_ptr<std::vector<char>> data)
{
	_writeMutex.lock();
	if(!connected())
	{
		_writeMutex.unlock();
		autoConnect();
	}
	else _writeMutex.unlock();
	if(!data || data->empty()) return 0;
	return proofwrite(*data);
}

int32_t TcpSocket::proofwrite(const std::vector<char>& data)
{
	if(!_socketDescriptor) throw SocketOperationException("Socket descriptor is nullptr.");
	_writeMutex.lock();
	if(!connected())
	{
		_writeMutex.unlock();
		autoConnect();
		_writeMutex.lock();
	}
	if(data.empty())
	{
		_writeMutex.unlock();
		return 0;
	}
	if(data.size() > 104857600)
	{
		_writeMutex.unlock();
		throw SocketDataLimitException("Data size is larger than 100 MiB.");
	}

	int32_t totalBytesWritten = 0;
	while (totalBytesWritten < (signed)data.size())
	{
		timeval timeout;
		int32_t seconds = _writeTimeout / 1000000;
		timeout.tv_sec = seconds;
		timeout.tv_usec = _writeTimeout - (1000000 * seconds);
		fd_set writeFileDescriptor;
		FD_ZERO(&writeFileDescriptor);
		auto fileDescriptorGuard = _bl->fileDescriptorManager.getLock();
		fileDescriptorGuard.lock();
		int32_t nfds = _socketDescriptor->descriptor + 1;
		if(nfds <= 0)
		{
			fileDescriptorGuard.unlock();
			_writeMutex.unlock();
			throw SocketClosedException("Connection to client number " + std::to_string(_socketDescriptor->id) + " closed (4).");
		}
		FD_SET(_socketDescriptor->descriptor, &writeFileDescriptor);
		fileDescriptorGuard.unlock();
		int32_t readyFds = select(nfds, NULL, &writeFileDescriptor, NULL, &timeout);
		if(readyFds == 0)
		{
			_writeMutex.unlock();
			throw SocketTimeOutException("Writing to socket timed out.");
		}
		if(readyFds != 1)
		{
			_writeMutex.unlock();
			throw SocketClosedException("Connection to client number " + std::to_string(_socketDescriptor->id) + " closed (5).");
		}

		int32_t bytesWritten = _socketDescriptor->tlsSession ? gnutls_record_send(_socketDescriptor->tlsSession, &data.at(totalBytesWritten), data.size() - totalBytesWritten) : send(_socketDescriptor->descriptor, &data.at(totalBytesWritten), data.size() - totalBytesWritten, MSG_NOSIGNAL);
		if(bytesWritten <= 0)
		{
			if(bytesWritten == -1 && (errno == EINTR || errno == EAGAIN)) continue;
			_writeMutex.unlock();
			close();
			_writeMutex.lock();
			if(_socketDescriptor->tlsSession)
			{
				_writeMutex.unlock();
				throw SocketOperationException(gnutls_strerror(bytesWritten));
			}
			else
			{
				_writeMutex.unlock();
				throw SocketOperationException(strerror(errno));
			}
		}
		totalBytesWritten += bytesWritten;
	}
	_writeMutex.unlock();
	return totalBytesWritten;
}

int32_t TcpSocket::proofwrite(const char* buffer, int32_t bytesToWrite)
{
	if(!_socketDescriptor) throw SocketOperationException("Socket descriptor is nullptr.");
	_writeMutex.lock();
	if(!connected())
	{
		_writeMutex.unlock();
		autoConnect();
		_writeMutex.lock();
	}
	if(bytesToWrite <= 0)
	{
		_writeMutex.unlock();
		return 0;
	}
	if(bytesToWrite > 104857600)
	{
		_writeMutex.unlock();
		throw SocketDataLimitException("Data size is larger than 100 MiB.");
	}

	int32_t totalBytesWritten = 0;
	while (totalBytesWritten < bytesToWrite)
	{
		timeval timeout;
		int32_t seconds = _writeTimeout / 1000000;
		timeout.tv_sec = seconds;
		timeout.tv_usec = _writeTimeout - (1000000 * seconds);
		fd_set writeFileDescriptor;
		FD_ZERO(&writeFileDescriptor);
		auto fileDescriptorGuard = _bl->fileDescriptorManager.getLock();
		fileDescriptorGuard.lock();
		int32_t nfds = _socketDescriptor->descriptor + 1;
		if(nfds <= 0)
		{
			fileDescriptorGuard.unlock();
			_writeMutex.unlock();
			throw SocketClosedException("Connection to client number " + std::to_string(_socketDescriptor->id) + " closed (4).");
		}
		FD_SET(_socketDescriptor->descriptor, &writeFileDescriptor);
		fileDescriptorGuard.unlock();
		int32_t readyFds = select(nfds, NULL, &writeFileDescriptor, NULL, &timeout);
		if(readyFds == 0)
		{
			_writeMutex.unlock();
			throw SocketTimeOutException("Writing to socket timed out.");
		}
		if(readyFds != 1)
		{
			_writeMutex.unlock();
			throw SocketClosedException("Connection to client number " + std::to_string(_socketDescriptor->id) + " closed (5).");
		}

		int32_t bytesWritten = _socketDescriptor->tlsSession ? gnutls_record_send(_socketDescriptor->tlsSession, buffer + totalBytesWritten, bytesToWrite - totalBytesWritten) : send(_socketDescriptor->descriptor, buffer + totalBytesWritten, bytesToWrite - totalBytesWritten, MSG_NOSIGNAL);
		if(bytesWritten <= 0)
		{
			if(bytesWritten == -1 && (errno == EINTR || errno == EAGAIN)) continue;
			_writeMutex.unlock();
			close();
			_writeMutex.lock();
			if(_socketDescriptor->tlsSession)
			{
				_writeMutex.unlock();
				throw SocketOperationException(gnutls_strerror(bytesWritten));
			}
			else
			{
				_writeMutex.unlock();
				throw SocketOperationException(strerror(errno));
			}
		}
		totalBytesWritten += bytesWritten;
	}
	_writeMutex.unlock();
	return totalBytesWritten;
}

int32_t TcpSocket::proofwrite(const std::string& data)
{
	if(!_socketDescriptor) throw SocketOperationException("Socket descriptor is nullptr.");
	_writeMutex.lock();
	if(!connected())
	{
		_writeMutex.unlock();
		autoConnect();
		_writeMutex.lock();
	}
	if(data.empty())
	{
		_writeMutex.unlock();
		return 0;
	}
	if(data.size() > 104857600)
	{
		_writeMutex.unlock();
		throw SocketDataLimitException("Data size is larger than 100 MiB.");
	}

	int32_t totalBytesWritten = 0;
	while (totalBytesWritten < (signed)data.size())
	{
		timeval timeout;
		int32_t seconds = _writeTimeout / 1000000;
		timeout.tv_sec = seconds;
		timeout.tv_usec = _writeTimeout - (1000000 * seconds);
		fd_set writeFileDescriptor;
		FD_ZERO(&writeFileDescriptor);
		auto fileDescriptorGuard = _bl->fileDescriptorManager.getLock();
		fileDescriptorGuard.lock();
		int32_t nfds = _socketDescriptor->descriptor + 1;
		if(nfds <= 0)
		{
			fileDescriptorGuard.unlock();
			_writeMutex.unlock();
			throw SocketClosedException("Connection to client number " + std::to_string(_socketDescriptor->id) + " closed (6).");
		}
		FD_SET(_socketDescriptor->descriptor, &writeFileDescriptor);
		fileDescriptorGuard.unlock();
		int32_t readyFds = select(nfds, NULL, &writeFileDescriptor, NULL, &timeout);
		if(readyFds == 0)
		{
			_writeMutex.unlock();
			throw SocketTimeOutException("Writing to socket timed out.");
		}
		if(readyFds != 1)
		{
			_writeMutex.unlock();
			throw SocketClosedException("Connection to client number " + std::to_string(_socketDescriptor->id) + " closed (7).");
		}

		int32_t bytesToSend = data.size() - totalBytesWritten;
		int32_t bytesWritten = _socketDescriptor->tlsSession ? gnutls_record_send(_socketDescriptor->tlsSession, &data.at(totalBytesWritten), bytesToSend) : send(_socketDescriptor->descriptor, &data.at(totalBytesWritten), bytesToSend, MSG_NOSIGNAL);
		if(bytesWritten <= 0)
		{
			if(bytesWritten == -1 && (errno == EINTR || errno == EAGAIN)) continue;
			_writeMutex.unlock();
			close();
			_writeMutex.lock();
			if(_socketDescriptor->tlsSession)
			{
				_writeMutex.unlock();
				throw SocketOperationException(gnutls_strerror(bytesWritten));
			}
			else
			{
				_writeMutex.unlock();
				throw SocketOperationException(strerror(errno));
			}
		}
		totalBytesWritten += bytesWritten;
	}
	_writeMutex.unlock();
	return totalBytesWritten;
}

bool TcpSocket::connected()
{
	if(!_socketDescriptor || _socketDescriptor->descriptor < 0 || _connecting) return false;
	char buffer[1];
	if(recv(_socketDescriptor->descriptor, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == -1 && errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) return false;
	return true;
}

void TcpSocket::getSocketDescriptor()
{
	_readMutex.lock();
	_writeMutex.lock();
	if(_bl->debugLevel >= 5) _bl->out.printDebug("Debug: Calling getFileDescriptor...");
	_bl->fileDescriptorManager.shutdown(_socketDescriptor);

	try
	{
		getConnection();
		if(!_socketDescriptor || _socketDescriptor->descriptor < 0)
		{
			_readMutex.unlock();
			_writeMutex.unlock();
			throw SocketOperationException("Could not connect to server.");
		}

		if(_useSsl) getSsl();
		_writeMutex.unlock();
		_readMutex.unlock();
	}
	catch(const std::exception& ex)
    {
		_writeMutex.unlock();
    	_readMutex.unlock();
		throw(ex);
    }
	catch(Exception& ex)
	{
		_writeMutex.unlock();
		_readMutex.unlock();
		throw(ex);
	}

}

void TcpSocket::getSsl()
{
	if(!_socketDescriptor || _socketDescriptor->descriptor < 0) throw SocketSSLException("Could not connect to server using SSL. File descriptor is invalid.");
	if(_x509Credentials.empty())
	{
		_bl->fileDescriptorManager.shutdown(_socketDescriptor);
		throw SocketSSLException("Could not connect to server using SSL. Certificate credentials are not initialized. Look for previous error messages.");
	}
	int32_t result = 0;
	//Disable TCP Nagle algorithm to improve performance
	const int32_t value = 1;
	if ((result = setsockopt(_socketDescriptor->descriptor, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value))) < 0)
    {
		_bl->fileDescriptorManager.shutdown(_socketDescriptor);
		throw SocketSSLException("Could not disable Nagle algorithm.");
	}
	if((result = gnutls_init(&_socketDescriptor->tlsSession, GNUTLS_CLIENT)) != GNUTLS_E_SUCCESS)
	{
		_bl->fileDescriptorManager.shutdown(_socketDescriptor);
		throw SocketSSLException("Could not initialize TLS session: " + std::string(gnutls_strerror(result)));
	}
	if(!_socketDescriptor->tlsSession) throw SocketSSLException("Could not initialize TLS session.");
	if((result = gnutls_priority_set_direct(_socketDescriptor->tlsSession, "NORMAL", NULL)) != GNUTLS_E_SUCCESS)
	{
		_bl->fileDescriptorManager.shutdown(_socketDescriptor);
		throw SocketSSLException("Could not set cipher priorities: " + std::string(gnutls_strerror(result)));
	}

    if((result = gnutls_credentials_set(_socketDescriptor->tlsSession, GNUTLS_CRD_CERTIFICATE, _x509Credentials.begin()->second)) != GNUTLS_E_SUCCESS)
    {
        _bl->fileDescriptorManager.shutdown(_socketDescriptor);
        throw SocketSSLException("Could not set trusted certificates: " + std::string(gnutls_strerror(result)));
    }

	gnutls_transport_set_ptr(_socketDescriptor->tlsSession, (gnutls_transport_ptr_t)(uintptr_t)_socketDescriptor->descriptor);
	if((result = gnutls_server_name_set(_socketDescriptor->tlsSession, GNUTLS_NAME_DNS, _hostname.c_str(), _hostname.length())) != GNUTLS_E_SUCCESS)
	{
		_bl->fileDescriptorManager.shutdown(_socketDescriptor);
		throw SocketSSLException("Could not set server's hostname: " + std::string(gnutls_strerror(result)));
	}
	do
	{
		result = gnutls_handshake(_socketDescriptor->tlsSession);
	} while (result < 0 && gnutls_error_is_fatal(result) == 0);
	if(result != GNUTLS_E_SUCCESS)
	{
		_bl->fileDescriptorManager.shutdown(_socketDescriptor);
		throw SocketSSLException("Error during TLS handshake: " + std::string(gnutls_strerror(result)));
	}

	//Now verify the certificate
	uint32_t serverCertChainLength = 0;
	const gnutls_datum_t* const serverCertChain = gnutls_certificate_get_peers(_socketDescriptor->tlsSession, &serverCertChainLength);
	if(!serverCertChain || serverCertChainLength == 0)
	{
		_bl->fileDescriptorManager.shutdown(_socketDescriptor);
		throw SocketSSLException("Could not get server certificate.");
	}
	uint32_t status = (uint32_t)-1;
	if((result = gnutls_certificate_verify_peers2(_socketDescriptor->tlsSession, &status)) != GNUTLS_E_SUCCESS)
	{
		_bl->fileDescriptorManager.shutdown(_socketDescriptor);
		throw SocketSSLException("Could not verify server certificate: " + std::string(gnutls_strerror(result)));
	}
	if(status > 0)
	{
		if(_verifyCertificate)
		{
			_bl->fileDescriptorManager.shutdown(_socketDescriptor);
			throw SocketSSLException("Error verifying server certificate (Code: " + std::to_string(status) + "): " + HelperFunctions::getGNUTLSCertVerificationError(status));
		}
		else if((status & GNUTLS_CERT_REVOKED) || (status & GNUTLS_CERT_INSECURE_ALGORITHM) || (status & GNUTLS_CERT_NOT_ACTIVATED) || (status & GNUTLS_CERT_EXPIRED))
		{
			_bl->fileDescriptorManager.shutdown(_socketDescriptor);
			throw SocketSSLException("Error verifying server certificate (Code: " + std::to_string(status) + "): " + HelperFunctions::getGNUTLSCertVerificationError(status));
		}
		else _bl->out.printWarning("Warning: Certificate verification failed (Code: " + std::to_string(status) + "): " + HelperFunctions::getGNUTLSCertVerificationError(status));
	}
	else //Verify hostname
	{
		gnutls_x509_crt_t serverCert;
		if((result = gnutls_x509_crt_init(&serverCert)) != GNUTLS_E_SUCCESS)
		{
			_bl->fileDescriptorManager.shutdown(_socketDescriptor);
			throw SocketSSLException("Could not initialize server certificate structure: " + std::string(gnutls_strerror(result)));
		}
		//The peer certificate is the first certificate in the list
		if((result = gnutls_x509_crt_import(serverCert, serverCertChain, GNUTLS_X509_FMT_DER)) != GNUTLS_E_SUCCESS)
		{
			gnutls_x509_crt_deinit(serverCert);
			_bl->fileDescriptorManager.shutdown(_socketDescriptor);
			throw SocketSSLException("Could not import server certificate: " + std::string(gnutls_strerror(result)));
		}
        if(_verifyHostname && (result = gnutls_x509_crt_check_hostname(serverCert, _verificationHostname.c_str())) == 0)
        {
            gnutls_x509_crt_deinit(serverCert);
            _bl->fileDescriptorManager.shutdown(_socketDescriptor);
            throw SocketSSLException("Server's hostname does not match the server certificate.");
        }
		gnutls_x509_crt_deinit(serverCert);
	}
	_bl->out.printInfo("Info: SSL handshake with client " + std::to_string(_socketDescriptor->id) + " completed successfully.");
}

/*bool TcpSocket::waitForSocket()
{
	if(!_socketDescriptor) throw SocketOperationException("Socket descriptor is nullptr.");
	timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	fd_set readFileDescriptor;
	FD_ZERO(&readFileDescriptor);
	_bl->fileDescriptorManager.lock();
	int32_t nfds = _socketDescriptor->descriptor + 1;
	if(nfds <= 0)
	{
		_bl->fileDescriptorManager.unlock();
		throw SocketClosedException("Connection to client number " + std::to_string(_socketDescriptor->id) + " closed (8).");
	}
	FD_SET(_socketDescriptor->descriptor, &readFileDescriptor);
	_bl->fileDescriptorManager.unlock();
	int32_t bytesRead = select(nfds, &readFileDescriptor, NULL, NULL, &timeout);
	if(bytesRead != 1) return false;
	return true;
}*/

void TcpSocket::getConnection()
{
	_socketDescriptor.reset();
	if(_hostname.empty()) throw SocketInvalidParametersException("Hostname is empty");
	if(_port.empty()) throw SocketInvalidParametersException("Port is empty");
	if(_connectionRetries < 1) _connectionRetries = 1;
	else if(_connectionRetries > 10) _connectionRetries = 10;

	if(_bl->debugLevel >= 5) _bl->out.printDebug("Debug: Connecting to host " + _hostname + " on port " + _port + (_useSsl ? " using SSL" : "") + "...");

	//Retry for two minutes
	for(int32_t i = 0; i < _connectionRetries; ++i)
	{
		struct addrinfo *serverInfo = nullptr;
		struct addrinfo hostInfo;
		memset(&hostInfo, 0, sizeof hostInfo);

		hostInfo.ai_family = AF_UNSPEC;
		hostInfo.ai_socktype = SOCK_STREAM;

		if(getaddrinfo(_hostname.c_str(), _port.c_str(), &hostInfo, &serverInfo) != 0)
		{
			freeaddrinfo(serverInfo);
			throw SocketOperationException("Could not get address information: " + std::string(strerror(errno)));
		}

		char ipStringBuffer[INET6_ADDRSTRLEN];
		if (serverInfo->ai_family == AF_INET) {
			struct sockaddr_in *s = (struct sockaddr_in *)serverInfo->ai_addr;
			inet_ntop(AF_INET, &s->sin_addr, ipStringBuffer, sizeof(ipStringBuffer));
		} else { // AF_INET6
			struct sockaddr_in6 *s = (struct sockaddr_in6 *)serverInfo->ai_addr;
			inet_ntop(AF_INET6, &s->sin6_addr, ipStringBuffer, sizeof(ipStringBuffer));
		}
		_ipAddress = std::string(&ipStringBuffer[0]);

		_socketDescriptor = _bl->fileDescriptorManager.add(socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol));
		if(!_socketDescriptor || _socketDescriptor->descriptor == -1)
		{
			freeaddrinfo(serverInfo);
			throw SocketOperationException("Could not create socket for server " + _ipAddress + " on port " + _port + ": " + strerror(errno));
		}
		int32_t optValue = 1;
		if(setsockopt(_socketDescriptor->descriptor, SOL_SOCKET, SO_KEEPALIVE, (void*)&optValue, sizeof(int32_t)) == -1)
		{
			freeaddrinfo(serverInfo);
			_bl->fileDescriptorManager.shutdown(_socketDescriptor);
			throw SocketOperationException("Could not set socket options for server " + _ipAddress + " on port " + _port + ": " + strerror(errno));
		}
		optValue = 30;
		//Don't use SOL_TCP, as this constant doesn't exists in BSD
		if(setsockopt(_socketDescriptor->descriptor, getprotobyname("TCP")->p_proto, TCP_KEEPIDLE, (void*)&optValue, sizeof(int32_t)) == -1)
		{
			freeaddrinfo(serverInfo);
			_bl->fileDescriptorManager.shutdown(_socketDescriptor);
			throw SocketOperationException("Could not set socket options for server " + _ipAddress + " on port " + _port + ": " + strerror(errno));
		}
		optValue = 4;
		if(setsockopt(_socketDescriptor->descriptor, getprotobyname("TCP")->p_proto, TCP_KEEPCNT, (void*)&optValue, sizeof(int32_t)) == -1)
		{
			freeaddrinfo(serverInfo);
			_bl->fileDescriptorManager.shutdown(_socketDescriptor);
			throw SocketOperationException("Could not set socket options for server " + _ipAddress + " on port " + _port + ": " + strerror(errno));
		}
		optValue = 15;
		if(setsockopt(_socketDescriptor->descriptor, getprotobyname("TCP")->p_proto, TCP_KEEPINTVL, (void*)&optValue, sizeof(int32_t)) == -1)
		{
			freeaddrinfo(serverInfo);
			_bl->fileDescriptorManager.shutdown(_socketDescriptor);
			throw SocketOperationException("Could not set socket options for server " + _ipAddress + " on port " + _port + ": " + strerror(errno));
		}

		if(!(fcntl(_socketDescriptor->descriptor, F_GETFL) & O_NONBLOCK))
		{
			if(fcntl(_socketDescriptor->descriptor, F_SETFL, fcntl(_socketDescriptor->descriptor, F_GETFL) | O_NONBLOCK) < 0)
			{
				freeaddrinfo(serverInfo);
				_bl->fileDescriptorManager.shutdown(_socketDescriptor);
				throw SocketOperationException("Could not set socket options for server " + _ipAddress + " on port " + _port + ": " + strerror(errno));
			}
		}

		int32_t connectResult;
		if((connectResult = connect(_socketDescriptor->descriptor, serverInfo->ai_addr, serverInfo->ai_addrlen)) == -1 && errno != EINPROGRESS)
		{
			if(i < _connectionRetries - 1)
			{
				freeaddrinfo(serverInfo);
				_bl->fileDescriptorManager.shutdown(_socketDescriptor);
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				continue;
			}
			else
			{
				freeaddrinfo(serverInfo);
				_bl->fileDescriptorManager.shutdown(_socketDescriptor);
				throw SocketTimeOutException("Connecting to server " + _ipAddress + " on port " + _port + " timed out: " + strerror(errno));
			}
		}
		freeaddrinfo(serverInfo);

		if(connectResult != 0) //We have to wait for the connection
		{
			pollfd pollstruct
			{
				(int)_socketDescriptor->descriptor,
				(short)(POLLIN | POLLOUT | POLLERR),
				(short)0
			};

			int32_t pollResult = poll(&pollstruct, 1, _readTimeout / 1000);
			if(pollResult < 0 || (pollstruct.revents & POLLERR))
			{
				if(i < _connectionRetries - 1)
				{
					_bl->fileDescriptorManager.shutdown(_socketDescriptor);
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
					continue;
				}
				else
				{
					_bl->fileDescriptorManager.shutdown(_socketDescriptor);
					throw SocketTimeOutException("Could not connect to server " + _ipAddress + " on port " + _port + ". Poll failed with error code: " + std::to_string(pollResult) + ".");
				}
			}
			else if(pollResult > 0)
			{
				socklen_t resultLength = sizeof(connectResult);
				if(getsockopt(_socketDescriptor->descriptor, SOL_SOCKET, SO_ERROR, &connectResult, &resultLength) < 0)
				{
					_bl->fileDescriptorManager.shutdown(_socketDescriptor);
					throw SocketOperationException("Could not connect to server " + _ipAddress + " on port " + _port + ": " + strerror(errno) + ".");
				}
				break;
			}
			else if(pollResult == 0)
			{
				if(i < _connectionRetries - 1)
				{
					_bl->fileDescriptorManager.shutdown(_socketDescriptor);
					continue;
				}
				else
				{
					_bl->fileDescriptorManager.shutdown(_socketDescriptor);
					throw SocketTimeOutException("Connecting to server " + _ipAddress + " on port " + _port + " timed out.");
				}
			}
		}
	}
	if(_bl->debugLevel >= 5) _bl->out.printDebug("Debug: Connected to host " + _hostname + " on port " + _port + ". Client number is: " + std::to_string(_socketDescriptor->id));
}

}
