/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * main.cc
 * Copyright (C) 2014 ubuntu 14.04 <mike@ubuntu>
	 * 
 * http is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
	 * 
 * http is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <memory>
#include <cstdint>
#include <iostream>
#include <evhttp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>
#include <vector>
#include <mutex>
#include <map>
#include <fstream>
#include "const.h"

void setAttr(evkeyvalq *outHeader, std::string key, std::string value);
void setResponseFile(evbuffer *buf, std::string const &fileName);
const evhttp_uri* getUri(evhttp_request *req);
void requestThread();
std::string getPath(evhttp_request *req);
void onReq(evhttp_request *req, void*);
std::mutex m;
std::map<std::string, std::string> cache;
std::exception_ptr initExcept;
bool volatile isRun = true;
evutil_socket_t soc = -1;

void setAttr(evkeyvalq *outHeader, std::string key, std::string value) {
	evhttp_add_header(outHeader, key.c_str(), value.c_str());
}

void setResponseFile(evbuffer *buf, std::string const &fileName) {           
	auto FileDeleter = [] (int *f) { if (*f != -1) close(*f); delete f; };
	std::lock_guard<std::mutex> lock(m);
	/*auto it = cache.find(fileName);
	if(it != cache.end()) {
		std::cout << "Cached!" << std::endl;
		evbuffer_add_printf(buf, it->second.c_str());
	}
	else {*/
	std::unique_ptr<int, decltype(FileDeleter)> File(new int(open(fileName.c_str(), 0)), FileDeleter);

		/*std::ifstream t(fileName, std::ifstream::in);
		if(!t) {
			std::cout << "Could not find content for uri: " << fileName << std::endl;
			return;
}
t.seekg(0, std::ios::end);
size_t size = t.tellg();
std::string buffer(size, ' ');
t.seekg(0);
t.read(&buffer[0], size);
cache[fileName] = buffer;
std::cout << buffer << std::endl;
t.close();
evbuffer_add_printf(buf, buffer.c_str());*/
		if (*File == -1) {
			std::cout << "Could not find content for uri: " << fileName << std::endl;
			return;
		}
	ev_off_t Length = lseek(*File, 0, SEEK_END);
	if (Length == -1 || lseek(*File, 0, SEEK_SET) == -1) {
		std::cout << "Failed to calc file size " << std::endl;
		return;
	}
	if (evbuffer_add_file(buf, *File, 0, Length) == -1) {
		std::cout << "Failed to make response." << std::endl;
		return;
	}
	*File.get() = -1;
	//}
}

const evhttp_uri* getUri(evhttp_request *req){
	return evhttp_request_get_evhttp_uri(req);
}
void requestThread(){
	try
	{
		std::unique_ptr<event_base, decltype(&event_base_free)> eventBase(event_base_new(), &event_base_free);
		if (!eventBase)
			throw std::runtime_error("Failed to create new base_event.");
		std::unique_ptr<evhttp, decltype(&evhttp_free)> evHttp(evhttp_new(eventBase.get()), &evhttp_free);
		if (!evHttp)
			throw std::runtime_error("Failed to create new evhttp.");
		evhttp_set_gencb(evHttp.get(), onReq, nullptr);
		if (soc == -1)
		{
			auto *boundSock = evhttp_bind_socket_with_handle(evHttp.get(), addr.c_str(), port);
			if (!boundSock)
				throw std::runtime_error("Failed to bind server socket.");
			if ((soc = evhttp_bound_socket_get_fd(boundSock)) == -1)
				throw std::runtime_error("Failed to get server socket for next instance.");
		}
		else
		{
			if (evhttp_accept_socket(evHttp.get(), soc) == -1)
				throw std::runtime_error("Failed to bind server socket for new instance.");
		}
		for ( ; isRun ; )
		{
			event_base_loop(eventBase.get(), EVLOOP_NONBLOCK);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	catch (...)
	{
		initExcept = std::current_exception();
	}
}

std::string getPath(evhttp_request *req){
	std::cout << "URI: " << getUri(req) << std::endl;
	const char *path = evhttp_uri_get_path(getUri(req));
	return path==NULL ? std::string("") : std::string(path);
}

void onReq(evhttp_request *req, void*) {
	auto *outBuf = evhttp_request_get_output_buffer(req);
	evbuffer_set_flags(outBuf, EVBUFFER_FLAG_DRAINS_TO_FD);
	if (!outBuf)
		return;
	auto *outHeader = evhttp_request_get_output_headers(req);
	setAttr(outHeader, std::string("Content-Type"), std::string("text/html"));
	setResponseFile(outBuf, documentRoot + getPath(req));
	//evbuffer_add_printf(OutBuf, "<html><body><center><h1>Hello Wotld!</h1></center></body></html>");
	evhttp_send_reply(req, HTTP_OK, "", outBuf);
}
int main()
{
	try {
		auto threadDeleter = [&] (std::thread *t) { isRun = false; t->join(); delete t; };
		typedef std::unique_ptr<std::thread, decltype(threadDeleter)> threadPtr;
		typedef std::vector<threadPtr> threadPool;
		threadPool threads;
		for (int i = 0 ; i < srvThreads; ++i)
		{
			threadPtr t(new std::thread(requestThread), threadDeleter);
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			if (initExcept != std::exception_ptr())
			{
				isRun = false;
				std::rethrow_exception(initExcept);
			}
			threads.push_back(std::move(t));
		}
		std::cin.get();
		isRun = false;
	} catch (std::exception const &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
	}
	return 0;                          
}