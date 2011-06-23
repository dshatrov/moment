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


#include <moment/video_stream.h>


using namespace M;

namespace Moment {

namespace {
    struct InformAudioMessage_Data {
	VideoStream::MessageInfo *msg_info;
	PagePool::PageListHead   *page_list;
	Size                      msg_len;

	InformAudioMessage_Data (VideoStream::MessageInfo * const msg_info,
				 PagePool::PageListHead   * const page_list,
				 Size                       const msg_len)
	    : msg_info (msg_info),
	      page_list (page_list),
	      msg_len (msg_len)
	{
	}
    };
}

void
VideoStream::informAudioMessage (EventHandler * const event_handler,
				 void * const cb_data,
				 void * const _inform_data)
{
    InformAudioMessage_Data * const inform_data = static_cast <InformAudioMessage_Data*> (_inform_data);
    event_handler->audioMessage (inform_data->msg_info,
				 inform_data->page_list,
				 inform_data->msg_len,
				 cb_data);
}

namespace {
    struct InformVideoMessage_Data {
	VideoStream::MessageInfo *msg_info;
	PagePool::PageListHead   *page_list;
	Size                      msg_len;

	InformVideoMessage_Data (VideoStream::MessageInfo * const msg_info,
				 PagePool::PageListHead   * const page_list,
				 Size                       const msg_len)
	    : msg_info (msg_info),
	      page_list (page_list),
	      msg_len (msg_len)
	{
	}
    };
}

void
VideoStream::informVideoMessage (EventHandler * const event_handler,
				 void * const cb_data,
				 void * const _inform_data)
{
    InformVideoMessage_Data * const inform_data = static_cast <InformVideoMessage_Data*> (_inform_data);
    event_handler->videoMessage (inform_data->msg_info,
				 inform_data->page_list,
				 inform_data->msg_len,
				 cb_data);
}

namespace {
    struct InformRtmpCommandMessage_Data {
	RtmpConnection           *conn;
	VideoStream::MessageInfo *msg_info;
	ConstMemory        const &method_name;
	AmfDecoder               *amf_decoder;

	InformRtmpCommandMessage_Data (RtmpConnection           * const  conn,
				       VideoStream::MessageInfo * const  msg_info,
				       ConstMemory                const &method_name,
				       AmfDecoder               * const amf_decoder)
	    : conn (conn),
	      msg_info (msg_info),
	      method_name (method_name),
	      amf_decoder (amf_decoder)
	{
	}
    };
}

void
VideoStream::informRtmpCommandMessage (EventHandler * const event_handler,
				       void * const cb_data,
				       void * const _inform_data)
{
    InformRtmpCommandMessage_Data * const inform_data = static_cast <InformRtmpCommandMessage_Data*> (_inform_data);
    // TODO Save/restore amf_decoder state between  callback invocations.
    //      Viable option - abstract away the parsing process.
    event_handler->rtmpCommandMessage (inform_data->conn,
				       inform_data->msg_info,
				       inform_data->method_name,
				       inform_data->amf_decoder,
				       cb_data);
}

void
VideoStream::informClosed (EventHandler * const event_handler,
			   void * const cb_data,
			   void * const /* inform_data */)
{
    event_handler->closed (cb_data);
}

void
VideoStream::fireAudioMessage (MessageInfo            * const mt_nonnull msg_info,
			       PagePool::PageListHead * const mt_nonnull page_list,
			       Size                     const msg_len)
{
    InformAudioMessage_Data inform_data (msg_info, page_list, msg_len);
    event_informer.informAll (informAudioMessage, &inform_data);
}

void
VideoStream::fireVideoMessage (MessageInfo            * const mt_nonnull msg_info,
			       PagePool::PageListHead * const mt_nonnull page_list,
			       Size                     const msg_len)
{
    InformVideoMessage_Data inform_data (msg_info, page_list, msg_len);
    event_informer.informAll (informVideoMessage, &inform_data);
}

void
VideoStream::fireRtmpCommandMessage (RtmpConnection    * const  mt_nonnull conn,
				     MessageInfo       * const  mt_nonnull msg_info,
				     ConstMemory         const &method_name,
				     AmfDecoder        * const  mt_nonnull amf_decoder)
{
    InformRtmpCommandMessage_Data inform_data (conn, msg_info, method_name, amf_decoder);
    event_informer.informAll (informVideoMessage, &inform_data);
}

void
VideoStream::close ()
{
    event_informer.informAll (informClosed, NULL /* inform_data */);
}

VideoStream::VideoStream ()
    : event_informer (this, &mutex)
{
}

VideoStream::~VideoStream ()
{
}

}

