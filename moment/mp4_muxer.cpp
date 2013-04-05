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


#include <moment/rtmp_connection.h>

#include <moment/mp4_muxer.h>


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_mp4mux   ("moment.mp4mux", LogLevel::I);

mt_sync_domain (pass1) PagePool::PageListHead
Mp4Muxer::writeMoovAtom ()
{
  // TODO Сжатие таблиц в stbl

    PagePool::PageListHead pages;

    // mac time - 2082844800 = unixtime
    Uint32 const mactime = (Uint32) (getUnixtime() + 2082844800);

    Byte ftyp_data [] = {
        0x00, 0x00, 0x00, 0x14,
         'f',  't',  'y',  'p',
         'q',  't',  ' ',  ' ',
        0x20, 0x04, 0x06, 0x00,
         'q',  't',  ' ',  ' '
    };

    Byte moov_data [] = {
        0x00, 0x00, 0x00, 0x00,
         'm',  'o',  'o',  'v',
        0x00, 0x00, 0x00, 0x6c,
         'm',  'v',  'h',  'd',
        // 1 byte version, 3 bytes flags
        0x00, 0x00, 0x00, 0x6c,
        // creation time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // modification time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // timescale (1000)
        0x00, 0x00, 0x03, 0xe8,
#warning TODO duration
//        // duration (10000)
//        0x00, 0x00, 0x27, 0x10,
        // duration
        (Byte) (duration_millisec >> 24),
        (Byte) (duration_millisec >> 16),
        (Byte) (duration_millisec >>  8),
        (Byte) (duration_millisec >>  0),
        // rate (1.0)
        0x00, 0x01, 0x00, 0x00,
        // volume (1.0)
        0x10, 0x00,
        // reserved
                    0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        // matrix
        0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00,
        // preview time
        0x00, 0x00, 0x00, 0x00,
        // preview duration
        0x00, 0x00, 0x00, 0x00,
        // poster time
        0x00, 0x00, 0x00, 0x00,
        // selection time
        0x00, 0x00, 0x00, 0x00,
        // selection duration
        0x00, 0x00, 0x00, 0x00,
        // current time
        0x00, 0x00, 0x00, 0x00,
        // next track id
        0x00, 0x00, 0x00, 0x02
    };

    Byte trak_data [] = {
        0x00, 0x00, 0x00, 0x00,
         't',  'r',  'a',  'k',
    };

    Byte tkhd_data [] = {
        0x00, 0x00, 0x00, 0x5c,
         't',  'k',  'h',  'd',
        // version and flags
        0x00, 0x00, 0x00, 0x07,
        // creation time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // modification time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // track id
        0x00, 0x00, 0x00, 0x01,
        // reserved
        0x00, 0x00, 0x00, 0x00,
#warning TODO duration
//        // duration (10000)
//        0x00, 0x00, 0x27, 0x10,
        // duration
        (Byte) (duration_millisec >> 24),
        (Byte) (duration_millisec >> 16),
        (Byte) (duration_millisec >>  8),
        (Byte) (duration_millisec >>  0),
        // reserved
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        // layer, alternate group
        0x00, 0x00, 0x00, 0x00,
        // volume, reserved
        0x00, 0x00, 0x00, 0x00,
        // matrix
        0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00,
        // track width (320)
        0x01, 0x40, 0x00, 0x00,
        // track height (240)
        0x00, 0xf0, 0x00, 0x00,
    };

    Byte mdia_data [] = {
        0x00, 0x00, 0x00, 0x00,
         'm',  'd',  'i',  'a',
        0x00, 0x00, 0x00, 0x20,
         'm',  'd',  'h',  'd',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // creation time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // modification time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // time scale (3000)
        0x00, 0x00, 0x0b, 0xb8,
//        // duration (30000)
//        0x00, 0x00, 0x75, 0x30,
        // duration
        (Byte) ((duration_millisec * 3) >> 24),
        (Byte) ((duration_millisec * 3) >> 16),
        (Byte) ((duration_millisec * 3) >>  8),
        (Byte) ((duration_millisec * 3) >>  0),
        // language, quality
        0x00, 0x00, 0x00, 0x00
    };

    Byte mdia_hdlr_data [] = {
        0x00, 0x00, 0x00, 0x2d,
         'h',  'd',  'l',  'r',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // component type
        'm',  'h',  'l',  'r',
        // component subtype
        'v',  'i',  'd',  'e',
        // component manufacturer
        0x00, 0x00, 0x00, 0x00,
        // component flags
        0x00, 0x00, 0x00, 0x00,
        // component flags mask
        0x00, 0x00, 0x00, 0x00,
        // component name
        0x0c,
        'V','i','d','e',
        'o','H','a','n',
        'd','l','e','r'
    };

    Byte minf_data [] = {
        0x00, 0x00, 0x00, 0x00,
         'm',  'i',  'n',  'f',

        0x00, 0x00, 0x00, 0x14,
         'v',  'm',  'h',  'd',
        // version, flags
        0x00, 0x00, 0x00, 0x01,
        // graphics mode: dither copy
        0x00, 0x40,
        // opcolor
        0x80, 0x00, 0x80, 0x00, 0x80, 0x00,

        0x00, 0x00, 0x00, 0x21,
         'h',  'd',  'l',  'r',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // component type
         'd',  'h',  'l',  'r',
        // component subtype
         'a',  'l',  'i',  's',
        // component manufacturer
        0x00, 0x00, 0x00, 0x00,
        // component flags
        0x00, 0x00, 0x00, 0x00,
        // component flags mask
        0x00, 0x00, 0x00, 0x00,
        // component name
        0x00,

        0x00, 0x00, 0x00, 0x24,
         'd',  'i',  'n',  'f',

        0x00, 0x00, 0x00, 0x1c,
         'd',  'r',  'e',  'f',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        0x00, 0x00, 0x00, 0x01,

        0x00, 0x00, 0x00, 0x0c,
         'a',  'l',  'i',  's',
        // version, flags
        0x00, 0x00, 0x00, 0x01,
    };

    Size const avcc_size = avc_seq_hdr_size ? (8 + avc_seq_hdr_size) : 0;
    Size const avc1_size = 0x56 + avcc_size;
    Size const stsd_size = 16 + avc1_size;
    Byte stbl_data [] = {
        0x00, 0x00, 0x00, 0x00,
         's',  't',  'b',  'l',

        (Byte) (stsd_size >> 24),
        (Byte) (stsd_size >> 16),
        (Byte) (stsd_size >>  8),
        (Byte) (stsd_size >>  0),
         's',  't',  's',  'd',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        0x00, 0x00, 0x00, 0x01,

        (Byte) (avc1_size >> 24),
        (Byte) (avc1_size >> 16),
        (Byte) (avc1_size >>  8),
        (Byte) (avc1_size >>  0),
         'a',  'v',  'c',  '1',
        // reserved
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // data reference index
        0x00, 0x01,
        // version, revision
        0x00, 0x00, 0x00, 0x00,
        // vendor
        0x00, 0x00, 0x00, 0x00,
        // temporal quality
        0x00, 0x00, 0x02, 0x00,
        // spatial quality
        0x00, 0x00, 0x02, 0x00,
        // width, height (320x240)
        0x01, 0x40, 0x00, 0xf0,
        // horizontal resolution (72 dpi)
        0x00, 0x48, 0x00, 0x00,
        // vertical resolution (72 dpi)
        0x00, 0x48, 0x00, 0x00,
        // data size
        0x00, 0x00, 0x00, 0x00,
        // frame count
        0x00, 0x01,
        // compressor name
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        // depth (24 bit)
        0x00, 0x18,
        // color table id
        0xff, 0xff,
    };

    Byte avcc_data [] = {
        (Byte) (avcc_size >> 24),
        (Byte) (avcc_size >> 16),
        (Byte) (avcc_size >>  8),
        (Byte) (avcc_size >>  0),
         'a',  'v',  'c',  'C'
    };

    // sample-to-chunk atom
    Byte stsc_data [] = {
        0x00, 0x00, 0x00, 0x1c,
         's',  't',  's',  'c',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        0x00, 0x00, 0x00, 0x01,
        // first chunk
        0x00, 0x00, 0x00, 0x01,
        // samples per chunk
        (Byte) (num_video_frames >> 24),
        (Byte) (num_video_frames >> 16),
        (Byte) (num_video_frames >>  8),
        (Byte) (num_video_frames >>  0),
        // sample description id
        0x00, 0x00, 0x00, 0x01
    };

    // chunk offset atom
    Byte stco_data [] = {
        0x00, 0x00, 0x00, 0x14,
         's',  't',  'c',  'o',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        0x00, 0x00, 0x00, 0x01,
        // offset (filled below)
        0x00, 0x00, 0x00, 0x00
    };

    Size const stts_size = 16 + num_video_frames * 8;
    // time-to-sample atom
    Byte stts_data [] = {
        (Byte) (stts_size >> 24),
        (Byte) (stts_size >> 16),
        (Byte) (stts_size >>  8),
        (Byte) (stts_size >>  0),
         's',  't',  't',  's',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (num_video_frames >> 24),
        (Byte) (num_video_frames >> 16),
        (Byte) (num_video_frames >>  8),
        (Byte) (num_video_frames >>  0)
    };

    Size const stss_size = 16 + num_stss_entries * 4;
    // sync sample atom
    Byte stss_data [] = {
        (Byte) (stss_size >> 24),
        (Byte) (stss_size >> 16),
        (Byte) (stss_size >>  8),
        (Byte) (stss_size >>  0),
         's',  't',  's',  's',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (num_stss_entries >> 24),
        (Byte) (num_stss_entries >> 16),
        (Byte) (num_stss_entries >>  8),
        (Byte) (num_stss_entries >>  0)
    };

    Size const stsz_size = 20 + num_video_frames * 4;
    // sample size atom
    Byte stsz_data [] = {
        (Byte) (stsz_size >> 24),
        (Byte) (stsz_size >> 16),
        (Byte) (stsz_size >>  8),
        (Byte) (stsz_size >>  0),
         's',  't',  's',  'z',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // sample size
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (num_video_frames >> 24),
        (Byte) (num_video_frames >> 16),
        (Byte) (num_video_frames >>  8),
        (Byte) (num_video_frames >>  0)
    };

    Size const ctts_size = 16 + num_video_frames * 8;
    // composition offset atom
    Byte const ctts_data [] = {
        (Byte) (ctts_size >> 24),
        (Byte) (ctts_size >> 16),
        (Byte) (ctts_size >>  8),
        (Byte) (ctts_size >>  0),
         'c',  't',  't',  's',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (num_video_frames >> 24),
        (Byte) (num_video_frames >> 16),
        (Byte) (num_video_frames >>  8),
        (Byte) (num_video_frames >>  0)
    };

    Size const mdat_size = 8 + total_video_frame_size;
    Byte const mdat_data [] = {
        (Byte) (mdat_size >> 24),
        (Byte) (mdat_size >> 16),
        (Byte) (mdat_size >>  8),
        (Byte) (mdat_size >>  0),
         'm',  'd',  'a',  't'
    };

    Size const stbl_size = 8 + stsd_size + sizeof (stsc_data) + sizeof (stco_data) +
                           stts_size + stss_size + stsz_size + ctts_size;

    logD (mp4mux, _func, "sizeof (stsc_data): ", sizeof (stsc_data));
    logD (mp4mux, _func, "sizeof (stco_data): ", sizeof (stco_data));
    logD (mp4mux, _func, "stts: ", PagePool::countPageListDataLen (stts_pages.first, 0 /* msg_offset */), " = ", stts_size);
    logD (mp4mux, _func, "stss: ", PagePool::countPageListDataLen (stss_pages.first, 0 /* msg_offset */), " = ", stss_size);
    logD (mp4mux, _func, "stsz: ", PagePool::countPageListDataLen (stsz_pages.first, 0 /* msg_offset */), " = ", stsz_size);
    logD (mp4mux, _func, "ctts: ", PagePool::countPageListDataLen (ctts_pages.first, 0 /* msg_offset */), " = ", ctts_size);

    Size const minf_size = 0x14 /* vmhd */ + 0x21 /* hdlr */ + 0x24 /* dinf */ + stbl_size + 8 /* minf header */;
    Size const mdia_size = 0x20 /* mdhd */ + 0x2d /* mdia_hdlr */ + minf_size + 8 /* mdia header */;
    Size const trak_size = 0x5c /* tkhd */ + mdia_size + 8 /* trak header */;
    Size const moov_size = sizeof (moov_data) + trak_size;

    logD (mp4mux, _func, "stbl_size: ", stbl_size);
    logD (mp4mux, _func, "minf_size: ", minf_size);
    logD (mp4mux, _func, "mdia_size: ", mdia_size);
    logD (mp4mux, _func, "trak_size: ", trak_size);
    logD (mp4mux, _func, "moov_size: ", moov_size);

    stbl_data [0] = (Byte) (stbl_size >> 24);
    stbl_data [1] = (Byte) (stbl_size >> 16);
    stbl_data [2] = (Byte) (stbl_size >>  8);
    stbl_data [3] = (Byte) (stbl_size >>  0);

    minf_data [0] = (Byte) (minf_size >> 24);
    minf_data [1] = (Byte) (minf_size >> 16);
    minf_data [2] = (Byte) (minf_size >>  8);
    minf_data [3] = (Byte) (minf_size >>  0);

    mdia_data [0] = (Byte) (mdia_size >> 24);
    mdia_data [1] = (Byte) (mdia_size >> 16);
    mdia_data [2] = (Byte) (mdia_size >>  8);
    mdia_data [3] = (Byte) (mdia_size >>  0);

    trak_data [0] = (Byte) (trak_size >> 24);
    trak_data [1] = (Byte) (trak_size >> 16);
    trak_data [2] = (Byte) (trak_size >>  8);
    trak_data [3] = (Byte) (trak_size >>  0);

    moov_data [0] = (Byte) (moov_size >> 24);
    moov_data [1] = (Byte) (moov_size >> 16);
    moov_data [2] = (Byte) (moov_size >>  8);
    moov_data [3] = (Byte) (moov_size >>  0);

    Size const stco_offset = sizeof (ftyp_data) + moov_size + 8 /* mdat header */;
    stco_data [16] = (Byte) (stco_offset >> 24);
    stco_data [17] = (Byte) (stco_offset >> 16);
    stco_data [18] = (Byte) (stco_offset >>  8);
    stco_data [19] = (Byte) (stco_offset >>  0);

    page_pool->getFillPages (&pages, ConstMemory::forObject (ftyp_data));
    page_pool->getFillPages (&pages, ConstMemory::forObject (moov_data));
    page_pool->getFillPages (&pages, ConstMemory::forObject (trak_data));
    page_pool->getFillPages (&pages, ConstMemory::forObject (tkhd_data));
    page_pool->getFillPages (&pages, ConstMemory::forObject (mdia_data));
    page_pool->getFillPages (&pages, ConstMemory::forObject (mdia_hdlr_data));
    page_pool->getFillPages (&pages, ConstMemory::forObject (minf_data));
    page_pool->getFillPages (&pages, ConstMemory::forObject (stbl_data));

    if (avc_seq_hdr_size) {
        page_pool->getFillPages (&pages, ConstMemory::forObject (avcc_data));
        pages.appendPages (avc_seq_hdr_msg);
        avc_seq_hdr_msg = NULL;
    }

    page_pool->getFillPages (&pages, ConstMemory::forObject (stsc_data));
    page_pool->getFillPages (&pages, ConstMemory::forObject (stco_data));
    page_pool->getFillPages (&pages, ConstMemory::forObject (stts_data));

    pages.appendList (&stts_pages);
    stts_pages.reset ();

    page_pool->getFillPages (&pages, ConstMemory::forObject (stss_data));

    pages.appendList (&stss_pages);
    stss_pages.reset ();

    page_pool->getFillPages (&pages, ConstMemory::forObject (stsz_data));

    pages.appendList (&stsz_pages);
    stsz_pages.reset ();

    page_pool->getFillPages (&pages, ConstMemory::forObject (ctts_data));

    pages.appendList (&ctts_pages);
    ctts_pages.reset ();

    page_pool->getFillPages (&pages, ConstMemory::forObject (mdat_data));

    if (logLevelOn (mp4mux, LogLevel::Debug)) {
        logD (mp4mux, _func, "result: ", PagePool::countPageListDataLen (pages.first, 0 /* msg_offset */), " bytes:");
        PagePool::dumpPages (logs, &pages);
    }

    return pages;
}

void
Mp4Muxer::pass1_avcSequenceHeader (PagePool       * const mt_nonnull msg_page_pool,
                                   PagePool::Page * const msg,
                                   Size             const msg_offs,
                                   Size             const frame_size)
{
    if (avc_seq_hdr_msg) {
        logW (mp4mux, _func, "duplicate invocation");
        page_pool->msgUnref (avc_seq_hdr_msg);
    }

    msg_page_pool->msgRef (msg);

    avc_seq_hdr_page_pool = msg_page_pool;
    avc_seq_hdr_msg = msg;
    avc_seq_hdr_offs = msg_offs;
    avc_seq_hdr_size = frame_size;
}

void
Mp4Muxer::pass1_frame (FrameType const frame_type,
                       Time      const timestamp_nanosec,
                       Size      const frame_size,
                       bool      const is_sync_sample)
{
    if (frame_type == FrameType_Audio)
        return;

    logD (mp4mux, _func, " ts ", timestamp_nanosec, " len ", frame_size);

    ++num_video_frames;
    total_video_frame_size += frame_size;

    {
        Byte const stsz_entry [] = {
            (Byte) (frame_size >> 24),
            (Byte) (frame_size >> 16),
            (Byte) (frame_size >>  8),
            (Byte) (frame_size >>  0)
        };
        page_pool->getFillPages (&stsz_pages, ConstMemory::forObject (stsz_entry));
        stsz_pos += sizeof (stsz_entry);
        logD (mp4mux, _func, "stsz_pos: ", stsz_pos);
    }

    if (is_sync_sample) {
        Byte const stss_entry [] = {
            (Byte) (num_video_frames >> 24),
            (Byte) (num_video_frames >> 16),
            (Byte) (num_video_frames >>  8),
            (Byte) (num_video_frames >>  0)
        };
        page_pool->getFillPages (&stss_pages, ConstMemory::forObject (stss_entry));
        stss_pos += sizeof (stss_entry);
        ++num_stss_entries;
    }

    {
        Time pts = timestamp_nanosec / (1000000 / 3);
        Time dts = pts;

        // Canonical:
        //    ts   0 3 1 2 4
        //   dts   0 1 2 3 4
        //   pts   1 4 2 3 5
        //   cto   1 3 0 0 1
        //
        // We do:
        //    ts   0 3 1 2 4
        //   dts   0 1 1 2 4
        //   pts   0 3 1 2 4
        //  stts   1 0 1 2 .
        //   cto   0 2 0 0 0
        //
        //    ts   0  3  1
        //   dts   0  3
        //  stts  +0 +2
        //  stco   0  0
        //
        //    ts   0  3  1  2  4
        //   dts   0  1  1  2  4
        //  stts  +0 +1 +0 +1 +2
        //  stco   0  2  0  0  0
        //
        //         0 3 2 1 4
        //   dts   0 1 1 1 4
        //  stts   1 0 0 3 .
        //  stco   0 2 1 0 0
        //
        //         0 3 2
        //   dts   0 2 2
        //  stts   2 0 .
        //  stco   0 1 0
        //
        // Как строить таблицы?
        //
        // Если dts < prv_dts, то По stts идём назад и протягиваем нули.
        // При этом в stco меняем значения в соответствии с изменениями в stts.
        //
        // Ещё один момент: dts начинаются с 0, поэтому может потребоваться сдвиг.
        //
        // Сейчас один кадр -> одна запись в stts и stco.
        //
        // 3 0 1
        // 0 0 1
        // 0 1
        //
        // 3 1 2
        // 1 1 2 => 0 0 2 (новый минимум)
        // 0 0 1

        if (pts < prv_pts) {
            PagePool::PageListArray stts_arr (stts_pages.first, 0 /* offset*/, stts_pos /* data_len */);
            PagePool::PageListArray ctts_arr (ctts_pages.first, 0 /* offset*/, ctts_pos /* data_len */);

            Byte stts_entry [8];
            Byte ctts_entry [8];

            if (pts < min_pts) {
                for (Size ctts_i = ctts_pos; ctts_i > 0; ctts_i -= sizeof (ctts_entry)) {
                    ctts_arr.get (ctts_i - sizeof (ctts_entry), Memory::forObject (ctts_entry));
                    Uint32 const ctts_val = (min_pts - pts) + (((Uint32) ctts_entry [4] << 24) |
                                                               ((Uint32) ctts_entry [5] << 16) |
                                                               ((Uint32) ctts_entry [6] <<  8) |
                                                               ((Uint32) ctts_entry [7] <<  0));
                    ctts_entry [4] = (Byte) (ctts_val >> 24);
                    ctts_entry [5] = (Byte) (ctts_val >> 16);
                    ctts_entry [6] = (Byte) (ctts_val >>  8);
                    ctts_entry [7] = (Byte) (ctts_val >>  0);
                    ctts_arr.set (ctts_pos - sizeof (ctts_entry), ConstMemory::forObject (ctts_entry));
                }

                min_pts = pts;
            }

            Size stts_i = stts_pos;
            Size ctts_i = ctts_pos;

            Time cur_dts = prv_pts;
            for (;;) {
                assert (stts_i != 0);

                stts_arr.get (stts_i - sizeof (stts_entry), Memory::forObject (stts_entry));
                Uint32 const stts_val = ((Uint32) stts_entry [4] << 24) |
                                        ((Uint32) stts_entry [5] << 16) |
                                        ((Uint32) stts_entry [6] <<  8) |
                                        ((Uint32) stts_entry [7] <<  0);

                ctts_arr.get (ctts_i - sizeof (ctts_entry), Memory::forObject (ctts_entry));
                Uint32 const ctts_val = ((Uint32) ctts_entry [4] << 24) |
                                        ((Uint32) ctts_entry [5] << 16) |
                                        ((Uint32) ctts_entry [6] <<  8) |
                                        ((Uint32) ctts_entry [7] <<  0);

                bool last = false;
                Uint32 new_ctts_val;
                if (stts_val >= cur_dts - dts) {
                    last = true;

                    Uint32 const new_stts_val = stts_val - (cur_dts - dts);
                    stts_entry [4] = (Byte) (new_stts_val >> 24);
                    stts_entry [5] = (Byte) (new_stts_val >> 16);
                    stts_entry [6] = (Byte) (new_stts_val >>  8);
                    stts_entry [7] = (Byte) (new_stts_val >>  0);

                    new_ctts_val = ctts_val + (cur_dts - dts);
                } else {
                    stts_entry [4] = 0;
                    stts_entry [5] = 0;
                    stts_entry [6] = 0;
                    stts_entry [7] = 0;

                    new_ctts_val = stts_val;
                }

                ctts_entry [4] = (Byte) (new_ctts_val >> 24);
                ctts_entry [5] = (Byte) (new_ctts_val >> 16);
                ctts_entry [6] = (Byte) (new_ctts_val >>  8);
                ctts_entry [7] = (Byte) (new_ctts_val >>  0);

                stts_arr.set (stts_i - sizeof (stts_entry), ConstMemory::forObject (stts_entry));
                ctts_arr.set (ctts_i - sizeof (ctts_entry), ConstMemory::forObject (ctts_entry));

                if (last)
                    break;

                cur_dts -= stts_val;

                stts_i -= sizeof (stts_entry);
                ctts_i -= sizeof (ctts_entry);
            }
        }

        {
            Byte const stts_entry [] = {
                0, 0, 0, 1,
                (Byte) ((pts - prv_pts) >> 24),
                (Byte) ((pts - prv_pts) >> 16),
                (Byte) ((pts - prv_pts) >>  8),
                (Byte) ((pts - prv_pts) >>  0)
            };

            if (num_video_frames > 1) {
                page_pool->getFillPages (&stts_pages, ConstMemory::forObject (stts_entry));
                stts_pos += sizeof (stts_entry);
            }
        }

        {
            Byte const ctts_entry [] = {
                0, 0, 0, 1,
                0, 0, 0, 0
            };

            page_pool->getFillPages (&ctts_pages, ConstMemory::forObject (ctts_entry));
            ctts_pos += sizeof (ctts_entry);
        }

        prv_pts = pts;
    }
}

PagePool::PageListHead
Mp4Muxer::pass1_complete ()
{
    if (num_video_frames > 0) {
        // (total duration) - (last frame timestamp)
        Time last_duration = 0;
        if (duration_millisec * 3 > prv_pts)
            last_duration = duration_millisec * 3 - prv_pts;

        Byte const stts_entry [] = {
            0, 0, 0, 1,
            (Byte) (last_duration >> 24),
            (Byte) (last_duration >> 16),
            (Byte) (last_duration >>  8),
            (Byte) (last_duration >>  0),
        };

        page_pool->getFillPages (&stts_pages, ConstMemory::forObject (stts_entry));
        stts_pos += sizeof (stts_entry);
    }

    return writeMoovAtom ();
}

void
Mp4Muxer::clear ()
{
    if (avc_seq_hdr_page_pool) {
        avc_seq_hdr_page_pool->msgUnref (avc_seq_hdr_msg);
        avc_seq_hdr_page_pool = NULL;
        avc_seq_hdr_msg = NULL;
    }

    page_pool->msgUnref (stsz_pages.first);
    stsz_pages.reset ();

    page_pool->msgUnref (stss_pages.first);
    stss_pages.reset ();

    page_pool->msgUnref (stts_pages.first);
    stts_pages.reset ();

    page_pool->msgUnref (ctts_pages.first);
    ctts_pages.reset ();
}

void
Mp4Muxer::init (PagePool * const mt_nonnull page_pool,
                Time       const duration_millisec)
{
    this->page_pool = page_pool;
    this->duration_millisec = duration_millisec;
}

Mp4Muxer::~Mp4Muxer ()
{
    clear ();
}

}

