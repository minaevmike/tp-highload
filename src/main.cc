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
#include "const.h"

const evhttp_uri* getUri(evhttp_request *req){
	return evhttp_request_get_evhttp_uri(req);
}

std::string getPath(evhttp_request *req){
	const char *path= evhttp_uri_get_path(getUri(req));
	return path==NULL?std::string(""):std::string(path);
}

void onReq(evhttp_request *req, void*) {
	std::cout << getPath(req) << std::endl;
	auto *outBuf = evhttp_request_get_output_buffer(req);
	if (!outBuf)
		return;
	evbuffer_add_printf(outBuf, "<html><body><center><h1>Hello Wotld!</h1></center></body></html>");
	evhttp_send_reply(req, HTTP_OK, "", outBuf);


}
int main()
{
	if (!event_init())
	{
		std::cerr << "Failed to init libevent." << std::endl;
		return -1;
	}
	std::unique_ptr<evhttp, decltype(&evhttp_free)> Server(evhttp_start(addr.c_str(), port), &evhttp_free);
	if(!Server) {
		std::cerr << "Failed to init http server." << std::endl;
	}
	std::cout << "Starting http server on " << addr << ":" << port <<std::endl;
	evhttp_set_gencb(Server.get(), onReq, NULL);
	if (event_dispatch() == -1)
	{
		std::cerr << "Failed to run messahe loop." << std::endl;
		return -1;
	}
	return 0;                             
}