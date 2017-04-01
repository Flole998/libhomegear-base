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

#ifndef SERIALREADERWRITER_H_
#define SERIALREADERWRITER_H_

#include "../Exception.h"
#include "../Managers/FileDescriptorManager.h"
#include "../IEvents.h"

#include <thread>
#include <atomic>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>

namespace BaseLib
{
class SerialReaderWriterException : public Exception
{
public:
	SerialReaderWriterException(std::string message) : Exception(message) {}
};

class SerialReaderWriter : public IEventsEx
{
public:
	// {{{ Event handling
	class ISerialReaderWriterEventSink : public IEventSinkBase
	{
	public:
		virtual void lineReceived(const std::string& data) = 0;
	};
	// }}}

	SerialReaderWriter(BaseLib::SharedObjects* baseLib, std::string device, int32_t baudrate, int32_t flags, bool createLockFile, int32_t readThreadPriority);
	virtual ~SerialReaderWriter();

	bool isOpen() { return _fileDescriptor && _fileDescriptor->descriptor != -1; }
	std::shared_ptr<FileDescriptor> fileDescriptor() { return _fileDescriptor; }
	void openDevice(bool parity, bool oddParity, bool events = true);
	void closeDevice();

	/**
	 * SerialReaderWriter can either be used through events (by implementing ISerialReaderWriterEventSink and usage of addEventHandler) or by polling using this method.
	 * @param data The variable to write the returned line into.
	 * @param timeout The maximum amount of time to wait in microseconds before the function returns (default: 500000).
	 * @return Returns "0" on success, "1" on timeout or "-1" on error.
	 */
	int32_t readLine(std::string& data, uint32_t timeout = 500000);

	/**
	 * SerialReaderWriter can either be used through events (by implementing ISerialReaderWriterEventSink and usage of addEventHandler) or by polling using this method.
	 * @param data The variable to write the returned character into.
	 * @param timeout The maximum amount of time to wait in microseconds before the function returns (default: 500000).
	 * @return Returns "0" on success, "1" on timeout or "-1" on error.
	 */
	int32_t readChar(char& data, uint32_t timeout = 500000);

	/**
	 * Writes one line of data.
	 * @param data The data to write. If data is not terminated by a new line character, it is appended.
	 */
	void writeLine(std::string& data);

	/**
	 * Writes binary data to the serial device.
	 * @param data The data to write. It is written as is without any modification.
	 */
	void writeData(const std::vector<char>& data);

	/**
	 * Writes one character to the serial device.
	 * @param data The (binary) character to write.
	 */
	void writeChar(char data);
protected:
	BaseLib::SharedObjects* _bl = nullptr;
	std::shared_ptr<FileDescriptor> _fileDescriptor;
	std::string _device;
	struct termios _termios;
	int32_t _baudrate = 0;
	int32_t _flags = 0;
	bool _createLockFile;
	std::string _lockfile;
	int32_t _readThreadPriority = 0;
	int32_t _handles = 0;

	std::atomic_bool _stopReadThread;
	std::mutex _readThreadMutex;
	std::thread _readThread;
	std::mutex _sendMutex;
	std::mutex _openDeviceThreadMutex;
	std::thread _openDeviceThread;

	void createLockFile();
	void readThread(bool parity, bool oddParity);
};

}
#endif
