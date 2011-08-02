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


#ifndef __LIBMOMENT__TYPES_H__
#define __LIBMOMENT__TYPES_H__


namespace Moment {

#if 0
// Replaced by VideoStream::CodecId
class VideoCodec {
public:
    enum Value {
	SorensonH263,
	ScreenVideo
    };
    operator Value () const { return value; }
    VideoCodec (Value const value) : value (value) {}
    VideoCodec () {}
private:
    Value value;
};
#endif

}


#endif /* __LIBMOMENT__TYPES_H__ */

