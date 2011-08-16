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


#ifndef __LIBMOMENT__AMF_ENCODER__H__
#define __LIBMOMENT__AMF_ENCODER__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class AmfMarker
{
public:
    enum Value {
	Number        = 0x00,
	Boolean       = 0x01,
	String        = 0x02,
	Object        = 0x03,
	MovieClip     = 0x04,
	Null          = 0x05,
	Undefined     = 0x06,
	Reference     = 0x07,
	EcmaArray     = 0x08,
	ObjectEnd     = 0x09,
	StringArray   = 0x0a,
	Date          = 0x0b,
	LongString    = 0x0c,
	Unsupported   = 0x0d,
	RecordSet     = 0x0e,
	XmlDocument   = 0x0f,
	TypedObject   = 0x10,
	AvmPlusObject = 0x11
    };
    AmfMarker (Value const value) : value (value) {}
    AmfMarker () {}
private:
    Value value;
};

class AmfEncoding
{
public:
    enum Value {
	AMF0,
	AMF3,
	Unknown
    };
    operator Value () const { return value; }
    AmfEncoding (Value const value) : value (value) {}
    AmfEncoding () {}
private:
    Value value;
};

class AmfEncoder;

class AmfAtom
{
    friend class AmfEncoder;

public:
    enum Type {
	Number,
	Boolean,
	String,
	NullObject,
	BeginObject,
	EndObject,
	FieldName,
	EcmaArray,
	Null
    };

private:
    Type type;

    union {
	double number;
	bool boolean;
	Uint32 integer;

	struct {
	    Byte const *data;
	    Size len;
	} string;
    };

public:
    void setEcmaArraySize (Uint32 const num_entries)
    {
	integer = num_entries;
    }

    AmfAtom (double const number)
	: type (Number),
	  number (number)
    {
    }

    AmfAtom (bool const boolean)
	: type (Boolean),
	  boolean (boolean)
    {
    }

    AmfAtom (ConstMemory const &mem)
	: type (String)
    {
	string.data = mem.mem();
	string.len = mem.len();
    }

    AmfAtom (Type const type,
	     ConstMemory const &mem)
	: type (type)
    {
	string.data = mem.mem();
	string.len = mem.len();
    }

    AmfAtom (Type const type)
	: type (type)
    {
    }

    AmfAtom ()
    {
    }
};

class AmfEncoder
{
private:
    AmfAtom * const atoms;
    Count const num_atoms;

    Count num_encoded;

public:
    AmfAtom* getLastAtom ()
    {
	return &atoms [num_encoded - 1];
    }

    void addNumber (double const number)
    {
	assert (num_encoded < num_atoms);
	atoms [num_encoded].type = AmfAtom::Number;
	atoms [num_encoded].number = number;
	++num_encoded;
    }

    void addBoolean (bool const boolean)
    {
	assert (num_encoded < num_atoms);
	atoms [num_encoded].type = AmfAtom::Boolean;
	atoms [num_encoded].boolean = boolean;
	++num_encoded;
    }

    void addString (ConstMemory const &mem)
    {
	assert (num_encoded < num_atoms);
	atoms [num_encoded].type = AmfAtom::String;
	atoms [num_encoded].string.data = mem.mem();
	atoms [num_encoded].string.len = mem.len();
	++num_encoded;
    }

    void addNullObject ()
    {
	assert (num_encoded < num_atoms);
	atoms [num_encoded].type = AmfAtom::NullObject;
	++num_encoded;
    }

    void beginObject ()
    {
	assert (num_encoded < num_atoms);
	atoms [num_encoded].type = AmfAtom::BeginObject;
	++num_encoded;
    }

    void endObject ()
    {
	assert (num_encoded < num_atoms);
	atoms [num_encoded].type = AmfAtom::EndObject;
	++num_encoded;
    }

    void addFieldName (ConstMemory const &field_name)
    {
	assert (num_encoded < num_atoms);
	atoms [num_encoded].type = AmfAtom::FieldName;
	atoms [num_encoded].string.data = field_name.mem();
	atoms [num_encoded].string.len = field_name.len();
	++num_encoded;
    }

    void addEcmaArray (Uint32 const num_entries)
    {
	assert (num_encoded < num_atoms);
	atoms [num_encoded].type = AmfAtom::EcmaArray;
	atoms [num_encoded].integer = num_entries;
	++num_encoded;
    }

    void addNull ()
    {
	assert (num_encoded < num_atoms);
	atoms [num_encoded].type = AmfAtom::Null;
	++num_encoded;
    }

    static Result encode (Memory const  &mem,
			  AmfEncoding    encoding,
			  Size          *ret_len,
			  AmfAtom const *atoms,
			  Count          num_atoms);

    template <Size N>
    static Result encode (Memory         const &mem,
			  AmfEncoding    const  encoding,
			  Size         * const  ret_len,
			  AmfAtom const (&atoms) [N])
    {
	return encode (mem, encoding, ret_len, atoms, sizeof (atoms) / sizeof (AmfAtom));
    }

    Result encode (Memory         const &mem,
		   AmfEncoding    const  encoding,
		   Size         * const  ret_len)
    {
	return encode (mem, encoding, ret_len, atoms, num_encoded);
    }

    AmfEncoder (AmfAtom * const atoms,
		Count const num_atoms)
	: atoms (atoms),
	  num_atoms (num_atoms),
	  num_encoded (0)
    {
    }

    template <Size N>
    AmfEncoder (AmfAtom (&atoms) [N])
	: atoms (atoms),
	  num_atoms (N),
	  num_encoded (0)
    {
    }
};

}


#endif /* __LIBMOMENT__AMF_ENCODER__H__ */

