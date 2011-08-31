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


#ifndef __MOMENT__API_AMF__H__
#define __MOMENT__API_AMF__H__


#include <moment/api.h>


#ifdef __cplusplus
extern "C" {
#endif


// ________________________________ AMF Decoder ________________________________

typedef struct MomentAmfDecoder MomentAmfDecoder;

MomentAmfDecoder* moment_amf_decoder_new_AMF0 (MomentMessage *msg);

void moment_amf_decoder_delete (MomentAmfDecoder *decoder);

void moment_amf_decoder_reset (MomentAmfDecoder *decoder,
			       MomentMessage    *msg);

int moment_amf_decode_number (MomentAmfDecoder *decoder,
			      double           *ret_number);

int moment_amf_decode_boolean (MomentAmfDecoder *decoder,
			       int              *ret_boolean);

int moment_amf_decode_string (MomentAmfDecoder *decoder,
			      char             *buf,
			      size_t            buf_len,
			      size_t           *ret_len,
			      size_t           *ret_full_len);

int moment_amf_decode_field_name (MomentAmfDecoder *decoder,
				  char             *buf,
				  size_t            buf_len,
				  size_t           *ret_len,
				  size_t           *ret_full_len);

int moment_amf_decoder_begin_object (MomentAmfDecoder *decoder);

int moment_amf_decoder_skip_value (MomentAmfDecoder *decoder);

int moment_amf_decoder_skip_object (MomentAmfDecoder *decoder);

size_t moment_amf_decoder_get_position (MomentAmfDecoder *decoder);

void moment_amf_decoder_set_position (MomentAmfDecoder *decoder,
				      size_t            pos);


// ________________________________ AMF Encoder ________________________________

typedef struct MomentAmfEncoder MomentAmfEncoder;

MomentAmfEncoder* moment_amf_encoder_new_AMF0 (void);

void moment_amf_encoder_delete (MomentAmfEncoder *encoder);

void moment_amf_encoder_reset (MomentAmfEncoder *encoder);

void moment_amf_encoder_add_number (MomentAmfEncoder *encoder,
				    double            number);

void moment_amf_encoder_add_boolean (MomentAmfEncoder *encoder,
				     int               boolean);

void moment_amf_encoder_add_string (MomentAmfEncoder *encoder,
				    char const       *str,
				    size_t            str_len);

void moment_amf_encoder_add_null_object (MomentAmfEncoder *encoder);

void moment_amf_encoder_begin_object (MomentAmfEncoder *encoder);

void moment_amf_encoder_end_object (MomentAmfEncoder *encoder);

void moment_amf_encoder_begin_ecma_array (MomentAmfEncoder *encoder,
					  unsigned long     num_entries);

void moment_amf_encoder_end_ecma_array (MomentAmfEncoder *encoder);

void moment_amf_encoder_add_field_name (MomentAmfEncoder *encoder,
					char const       *name,
					size_t            name_len);

void moment_amf_encoder_add_null (MomentAmfEncoder *encoder);

int moment_amf_encoder_encode (MomentAmfEncoder *encoder,
			       unsigned char    *buf,
			       size_t            buf_len,
			       size_t           *ret_len);


#ifdef __cplusplus
}
#endif


#endif /* __MOMENT__API_AMF__H__ */

