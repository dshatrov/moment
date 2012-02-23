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


#include <moment/amf_decoder.h>


using namespace M;

namespace Moment {

Result
AmfDecoder::decodeNumber (double * const ret_number)
{
    if (msg_len - cur_offset < 9) {
	logE_ (_func, "no number");
	return Result::Failure;
    }

    Byte data [9];
    array->get (cur_offset, Memory::forObject (data));

    if (data [0] != AmfMarker::Number) {
	logE_ (_func, "not a number");
	return Result::Failure;
    }

    if (ret_number) {
	((Byte*) ret_number) [0] = data [8];
	((Byte*) ret_number) [1] = data [7];
	((Byte*) ret_number) [2] = data [6];
	((Byte*) ret_number) [3] = data [5];
	((Byte*) ret_number) [4] = data [4];
	((Byte*) ret_number) [5] = data [3];
	((Byte*) ret_number) [6] = data [2];
	((Byte*) ret_number) [7] = data [1];
    }

    cur_offset += 9;

    return Result::Success;
}

Result
AmfDecoder::decodeBoolean (bool * const ret_boolean)
{
    if (msg_len - cur_offset < 2) {
	logE_ (_func, "no boolean");
	return Result::Failure;
    }

    Byte data [2];
    array->get (cur_offset, Memory::forObject (data));

    if (data [0] != AmfMarker::Boolean) {
	logE_ (_func, "not a boolean");
	return Result::Failure;
    }

    if (ret_boolean)
	*ret_boolean = data [1];

    cur_offset += 2;

    return Result::Success;
}

Result
AmfDecoder::decodeString (Memory const &mem,
			  Size * const ret_len,
			  Size * const ret_full_len)
{
    if (msg_len - cur_offset < 3) {
	logE_ (_func, "no string");
	return Result::Failure;
    }

    Byte header_data [3];
    array->get (cur_offset, Memory::forObject (header_data));

    if (header_data [0] != AmfMarker::String) {
	logE_ (_func, "not a string");
	return Result::Failure;
    }

    cur_offset += 3;

    Uint32 const string_len = ((Uint32) header_data [1] << 8) |
			      ((Uint32) header_data [2] << 0);

    if (msg_len - cur_offset < string_len) {
	logE_ (_func, "message is too short");
	return Result::Failure;
    }

    Size const tocopy = (mem.len() > string_len ? string_len : mem.len());
    array->get (cur_offset, mem.region (0, tocopy));

    cur_offset += string_len;

    if (ret_len)
	*ret_len = tocopy;

    if (ret_full_len)
	*ret_full_len = string_len;

    return Result::Success;
}

Result
AmfDecoder::decodeFieldName (Memory const &mem,
			     Size * const ret_len,
			     Size * const ret_full_len)
{
    if (msg_len - cur_offset < 2) {
	logE_ (_func, "no field name");
	return Result::Failure;
    }

    Byte header_data [2];
    array->get (cur_offset, Memory::forObject (header_data));

    cur_offset += 2;

    Uint32 const string_len = ((Uint32) header_data [0] << 8) |
			      ((Uint32) header_data [1] << 0);

    if (msg_len - cur_offset < string_len) {
	logE_ (_func, "message is too short");
	return Result::Failure;
    }

    Size const tocopy = (mem.len() > string_len ? string_len : mem.len());
    array->get (cur_offset, mem.region (0, tocopy));

    cur_offset += string_len;

    if (ret_len)
	*ret_len = tocopy;

    if (ret_full_len)
	*ret_full_len = string_len;

    return Result::Success;
}

Result
AmfDecoder::beginObject ()
{
    if (msg_len - cur_offset < 1) {
	logE_ (_func, "no object marker");
	return Result::Failure;
    }

    Byte obj_marker;
    array->get (cur_offset, Memory::forObject (obj_marker));
    cur_offset += 1;

    if (obj_marker != AmfMarker::Object) {
	logE_ (_func, "not an object");
	return Result::Failure;
    }

    return Result::Success;
}

Result
AmfDecoder::skipValue ()
{
  // TODO
    return Result::Failure;
}

Result
AmfDecoder::skipObject ()
{
    if (msg_len - cur_offset < 1) {
	logE_ (_func, "no object marker");
	return Result::Failure;
    }

    Byte obj_marker;
    array->get (cur_offset, Memory::forObject (obj_marker));
    cur_offset += 1;

    if (obj_marker == AmfMarker::Null)
	return Result::Success;

    if (obj_marker != AmfMarker::Object) {
	logE_ (_func, "not an object");
	return Result::Failure;
    }

    for (;;) {
	if (msg_len - cur_offset < 2) {
	    logE_ (_func, "no field name length");
	    return Result::Failure;
	}

	Byte fnamelen_arr [2];
	array->get (cur_offset, Memory::forObject (fnamelen_arr));
	Uint32 const field_name_len = ((Uint32) fnamelen_arr [0] << 8) |
				      ((Uint32) fnamelen_arr [1] << 0);
	cur_offset += 2;

	if (msg_len - cur_offset < field_name_len) {
	    logE_ (_func, "no field name");
	    return Result::Failure;
	}
	cur_offset += field_name_len;

	if (msg_len - cur_offset < 1) {
	    logE_ (_func, "no field value marker");
	    return Result::Failure;
	}

	Byte field_marker;
	array->get (cur_offset, Memory::forObject (field_marker));
	cur_offset += 1;

	bool done = false;
	switch (field_marker) {
	    case AmfMarker::Number:
		if (!decodeNumber (NULL))
		    return Result::Failure;
		break;
	    case AmfMarker::Boolean:
		if (!decodeBoolean (NULL))
		    return Result::Failure;
		break;
	    case AmfMarker::String:
		// TODO Implement object skipping.
//		logD_ (_func, "String");
		if (!decodeString (Memory(), NULL, NULL))
		    return Result::Failure;
		break;
	    case AmfMarker::Object:
		if (!skipObject ())
		    return Result::Failure;
		break;
	    case AmfMarker::MovieClip:
		logE_ (_func, "MovieClip marker not supported");
		return Result::Failure;
	    case AmfMarker::Null:
		logE_ (_func, "Null marker not supported");
		return Result::Failure;
	    case AmfMarker::Undefined:
		logE_ (_func, "Undefined marker not supported");
		return Result::Failure;
	    case AmfMarker::Reference:
		logE_ (_func, "Reference marker not supported");
		return Result::Failure;
	    case AmfMarker::EcmaArray:
		logE_ (_func, "EcmaArray marker not supported");
		return Result::Failure;
	    case AmfMarker::ObjectEnd:
		done = true;
		break;
	    case AmfMarker::StringArray:
		logE_ (_func, "StringArray marker not supported");
		return Result::Failure;
	    case AmfMarker::Date:
		logE_ (_func, "Date marker not supported");
		return Result::Failure;
	    case AmfMarker::LongString:
		logE_ (_func, "LongString marker not supported");
		return Result::Failure;
	    case AmfMarker::Unsupported:
		logE_ (_func, "Unsupported marker not supported");
		return Result::Failure;
	    case AmfMarker::RecordSet:
		logE_ (_func, "RecordSet marker not supported");
		return Result::Failure;
	    case AmfMarker::XmlDocument:
		logE_ (_func, "XmlDocument marker not supported");
		return Result::Failure;
	    case AmfMarker::TypedObject:
		logE_ (_func, "TypedObject marker not supported");
		return Result::Failure;
	    case AmfMarker::AvmPlusObject:
		logE_ (_func, "AvmPlusObject marker not supported");
		return Result::Failure;
	    default:
		logE_ (_func, "unknown field value type");
		return Result::Failure;
	}

	if (done)
	    break;
    } // for (;;)

    return Result::Success;
}

}

