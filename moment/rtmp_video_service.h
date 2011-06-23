/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#ifndef __LIBMOMENT__RTMP_VIDEO_SERVICE__H__
#define __LIBMOMENT__RTMP_VIDEO_SERVICE__H__


#include <libmary/libmary.h>
#include <moment/rtmp_connection.h>


namespace Moment {

using namespace M;

class RtmpVideoService
{
public:
    struct Frontend {
	Result (*clientConnected) (RtmpConnection * mt_nonnull rtmp_conn,
				   void *cb_data);
    };

protected:
    Cb<Frontend> frontend;

public:
    void setFrontend (Cb<Frontend> const &frontend)
    {
	this->frontend = frontend;
    }
};

}


#endif /* __LIBMOMENT__RTMP_VIDEO_SERVICE__H__ */

