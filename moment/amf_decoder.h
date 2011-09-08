/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov
    e-mail: shatrov@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef __LIBMOMENT__AMF_DECODER__H__
#define __LIBMOMENT__AMF_DECODER__H__


#include <libmary/libmary.h>

#include <moment/amf_encoder.h>


namespace Moment {

using namespace M;

class AmfDecoder
{
private:
    AmfEncoding encoding;
    Array *array;
    Size msg_len;

    Size cur_offset;

public:
    Result decodeNumber (double *ret_number);

    Result decodeBoolean (bool *ret_boolean);

    Result decodeString (Memory const &mem,
			 Size *ret_len,
			 Size *ret_full_len);

    Result decodeFieldName (Memory const &mem,
			    Size *ret_len,
			    Size *ret_full_len);

    Result beginObject ();

    Result skipValue ();

    Result skipObject ();

    Size getCurOffset ()
    {
	return cur_offset;
    }

    void setOffset (Size const offs)
    {
	cur_offset = offs;
    }

    void reset (AmfEncoding   const encoding,
		Array       * const array,
		Size          const msg_len)
    {
	this->encoding = encoding;
	this->array = array;
	this->msg_len = msg_len;

	cur_offset = 0;
    }

    AmfDecoder (AmfEncoding   const encoding,
		Array       * const array,
		Size          const msg_len)
	: encoding (encoding),
	  array (array),
	  msg_len (msg_len),
	  cur_offset (0)
    {
    }
};

}


#endif /* __LIBMOMENT__AMF_DECODER__H__ */

