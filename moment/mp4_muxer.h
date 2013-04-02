/*  Moment Video Server - High performance media server
    Copyright (C) 2013 Dmitry Shatrov
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


#ifndef MOMENT__MP4_MUXER__H__
#define MOMENT__MP4_MUXER__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class Mp4Muxer : public DependentCodeReferenced
{
private:
    StateMutex mutex;

public:
    enum MuxState {
        MuxState_Ready,
        MuxState_Overloaded,
        MuxState_Error,
        MuxState_Complete
    };

    enum FrameType {
        FrameType_Audio,
        FrameType_Video
    };

    struct Frontend {
        void (*muxStateChanged) (MuxState  mux_state,
                                 void     *cb_data);
    };

private:
    mt_const DataDepRef<PagePool> page_pool;
    mt_const DataDepRef<Sender> sender;

    mt_const Cb<Frontend> frontend;

// TODO FIXME Synchronize page lists for dtor.

    mt_sync_domain (pass1) PagePool::Page *avc_seq_hdr_msg;
    mt_sync_domain (pass1) Size avc_seq_hdr_offs;
    mt_sync_domain (pass1) Size avc_seq_hdr_size;

    mt_sync_domain (pass1) Size num_video_frames;
    mt_sync_domain (pass1) Size total_video_frame_size;

    mt_sync_domain (pass1) PagePool::PageListHead header;
    mt_sync_domain (pass1) Size header_pos;

    mt_sync_domain (pass1) PagePool::PageListHead stsz_pages;
    mt_sync_domain (pass1) Size stsz_pos;

    mt_sync_domain (pass1) PagePool::PageListHead stss_pages;
    mt_sync_domain (pass1) Size stss_pos;
    mt_sync_domain (pass1) Count num_stss_entries;

    mt_sync_domain (pass1) PagePool::PageListHead stts_pages;
    mt_sync_domain (pass1) Size stts_pos;
    mt_sync_domain (pass1) Time prv_stts_value;

    mt_sync_domain (pass1) PagePool::PageListHead ctts_pages;
    mt_sync_domain (pass1) Size ctts_pos;

    mt_sync_domain (pass1) Time prv_pts;
    mt_sync_domain (pass1) Time min_pts;

    mt_mutex (mutex) MuxState mux_state;

    mt_sync_domain (pass1) void writeMoovAtom ();

public:
    mt_sync_domain (pass1) Result pass1_avcSequenceHeader (PagePool::Page *msg,
                                                           Size            msg_offs,
                                                           Size            frame_size);

    mt_sync_domain (pass1) Result pass1_frame (FrameType frame_type,
                                               Time      timestamp_nanosec,
                                               Size      frame_size,
                                               bool      is_sync_sample);

    mt_sync_domain (pass1) Result pass1_complete ();

    mt_mutex (mutex) Result pass2_frame (FrameType       frame_type,
                                         Time            timestamp_nanosec,
                                         PagePool::Page *msg,
                                         Size            msg_offs,
                                         Size            frame_size);

    mt_mutex (mutex) Result pass2_complete ();

    mt_locks   (mutex) void lock   () { mutex.lock   (); }
    mt_unlocks (mutex) void unlock () { mutex.unlock (); }

    mt_mutex (mutex) MuxState getMuxState_unlocked () { return mux_state; }

    mt_const void init (PagePool * mt_nonnull page_pool,
                        Sender   * mt_nonnull sender,
                        CbDesc<Frontend> const &frontend);

    Mp4Muxer (Object * const coderef_container)
        : DependentCodeReferenced (coderef_container),
          page_pool  (coderef_container),
          sender     (coderef_container),
          avc_seq_hdr_msg (NULL),
          avc_seq_hdr_offs (0),
          avc_seq_hdr_size (0),
          num_video_frames (0),
          total_video_frame_size (0),
          header_pos (0),
          stsz_pos (0),
          stss_pos (0),
          num_stss_entries (0),
          stts_pos (0),
          prv_stts_value (0),
          ctts_pos (0),
          prv_pts (0),
          min_pts (0)
    {}

    ~Mp4Muxer ();
};

}


#endif /* MOMENT__MP4_MUXER__H__ */

