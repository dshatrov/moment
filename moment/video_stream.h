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


#ifndef __LIBMOMENT__VIDEO_STREAM__H__
#define __LIBMOMENT__VIDEO_STREAM__H__


#include <libmary/libmary.h>

#include <moment/rtmp_connection.h>


namespace Moment {

using namespace M;

class VideoStream : public Object
{
public:
    // Must be copyable.
    struct MessageInfo
    {
    public:
	Uint64 timestamp;
	bool is_keyframe;

	// Greater than zero for prechunked messages.
	Uint32 prechunk_size;

	MessageInfo ()
	    : timestamp (0),
	      is_keyframe (true),
	      prechunk_size (0)
	{
	}
    };

    struct EventHandler
    {
	void (*audioMessage) (MessageInfo            * mt_nonnull msg_info,
			      PagePool               * mt_nonnull page_pool,
			      PagePool::PageListHead * mt_nonnull page_list,
			      Size                    msg_len,
			      void                  *cb_data);

	void (*videoMessage) (MessageInfo            * mt_nonnull msg_info,
			      PagePool               * mt_nonnull page_pool,
			      PagePool::PageListHead * mt_nonnull page_list,
			      Size                    msg_len,
			      void                   *cb_data);

	void (*rtmpCommandMessage) (RtmpConnection    * mt_nonnull conn,
				    MessageInfo       * mt_nonnull msg_info,
				    ConstMemory const &method_name,
				    AmfDecoder        * mt_nonnull amf_decoder,
				    void              *cb_data);

	void (*closed) (void *cb_data);
    };

    struct SavedFrame
    {
	VideoStream::MessageInfo msg_info;
	PagePool *page_pool;
	PagePool::PageListHead page_list;
	Size msg_len;
    };

private:
    mt_mutex (mutex) bool got_saved_keyframe;
    mt_mutex (mutex) SavedFrame saved_keyframe;

    Informer_<EventHandler> event_informer;

    static void informAudioMessage (EventHandler *event_handler,
				    void *cb_data,
				    void *inform_data);

    static void informVideoMessage (EventHandler *event_handler,
				    void *cb_data,
				    void *inform_data);

    static void informRtmpCommandMessage (EventHandler *event_handler,
					  void *cb_data,
					  void *inform_data);

    static void informClosed (EventHandler *event_handler,
			      void *cb_data,
			      void *inform_data);

public:
    Informer_<EventHandler>* getEventInformer ()
    {
	return &event_informer;
    }

    void fireAudioMessage (MessageInfo            * mt_nonnull msg_info,
			   PagePool               * mt_nonnull page_pool,
			   PagePool::PageListHead * mt_nonnull page_list,
			   Size                    msg_len);

    void fireVideoMessage (MessageInfo            * mt_nonnull msg_info,
			   PagePool               * mt_nonnull page_pool,
			   PagePool::PageListHead * mt_nonnull page_list,
			   Size                    msg_len);

    void fireRtmpCommandMessage (RtmpConnection    * mt_nonnull conn,
				 MessageInfo       * mt_nonnull msg_info,
				 ConstMemory const &method_name,
				 AmfDecoder        * mt_nonnull amf_decoder);

    void close ();

    bool getSavedKeyframe (SavedFrame * mt_nonnull ret_frame);

    VideoStream ();

    ~VideoStream ();
};

}


#endif /* __LIBMOMENT__VIDEO_STREAM__H__ */

