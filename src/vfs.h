/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * vfs.h
 * Copyright (C) 2014 mike <mike@ubuntu>
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

#ifndef _VFS_H_
#define _VFS_H_
#include <string>
class vfs
{
public:
	static vfs &getInstanse(){
		static vfs v;
		return v;
	}
	void setRoot(std::string root);
	
protected:
	std::string documentRoot;

private:

};

#endif // _VFS_H_

