/*
 *  player.c - example player for ffmpeg
 *
 *  Copyright (C) 2015 Intel Corporation
 *    Author: Zhao, Halley<halley.zhao@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

// gcc player.c `pkg-config --cflags --libs libavformat libavcodec libavutil` -o player

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55, 28, 1)
    #define av_frame_alloc avcodec_alloc_frame
    #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54, 28, 0)
        #define av_frame_free avcodec_free_frame
    #else
        #define av_frame_free av_freep
    #endif
#endif

#define PRINTF printf
#define DEBUG(format, ...)   printf("  %s, %d, " format, __FILE__, __LINE__, ##__VA_ARGS__)
#define ERROR(format, ...) fprintf(stderr, "!!ERROR  %s, %d, " format, __FILE__, __LINE__, ##__VA_ARGS__)

#ifndef ASSERT
#define ASSERT(expr) do {                                                                                               \
        if (!(expr))                                                                                                    \
            ERROR();                                                                                                    \
        assert( 0 && expr);                                                                                             \
    } while(0)
#endif

#define READ_VIDEO_PKT_COUNT    500          // after # of video pkt, we quit
#define CHANGE_VIDEO_TRACK_PKT_COUNT  200    // change video track after # of video pkt

int main(int argc, char *argv[])
{
    AVPacket pkt;
    int video_pkt_count = 0, audio_pkt_count = 0;
    int video_stream_index = -1, audio_stream_index = -1, i;    // stream_index in AVFormatContext
    int video_track_index = -1, audio_track_index = -1;         // track index of each (audio/video) content
    const char* input_file = "http://asp.cntv.lxdns.com/asp/hls/main/0303000a/3/default/438eb7a818b246c187e72f1cd4e1bc4c/main.m3u8";

    // libav* init
    av_register_all();
    avformat_network_init();

    // open input file
    AVFormatContext* pFormat = NULL;
    if (avformat_open_input(&pFormat, input_file, NULL, NULL) < 0) {
        ERROR("fail to open input file: %s by avformat\n", input_file);
        return -1;
    }
    if (avformat_find_stream_info(pFormat, NULL) < 0) {
        ERROR("fail to find out stream info\n");
        return -1;
    }
    av_dump_format(pFormat,0,input_file,0);

    // gather track info of each stream (audio/video)
    #define MAX_TRACK_COUNT     10
    uint32_t video_tracks[MAX_TRACK_COUNT], video_track_count = 0;
    uint32_t audio_tracks[MAX_TRACK_COUNT], audio_track_count = 0;
    for (i = 0; i < pFormat->nb_streams; i++) {
        if (pFormat->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_tracks[video_track_count++] = i;
        } else if (pFormat->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_tracks[audio_track_count++] = i;
        }
        pFormat->streams[i]->discard = AVDISCARD_ALL;
    }

    if (video_track_count) {
        video_track_index = 0;
        video_stream_index = video_tracks[video_track_index];
        pFormat->streams[video_stream_index]->discard = AVDISCARD_DEFAULT;
    }
    if (audio_track_count) {
        audio_track_index = 0;
        audio_stream_index = audio_tracks[audio_track_index];
        pFormat->streams[audio_stream_index]->discard = AVDISCARD_DEFAULT;
    }

    // dump stream discard flag
    for (i = 0; i < pFormat->nb_streams; i++)
        DEBUG("## stream index: %d, discard: %d\n", i, pFormat->streams[i]->discard);

    // read frames one by one
    av_init_packet(&pkt);
    while (1) {
        if(av_read_frame(pFormat, &pkt) < 0)
            break;
        DEBUG("got pkt with stream_index: %d\n", pkt.stream_index);

        if (pkt.stream_index == video_stream_index) {
            video_pkt_count++;

            // switch video track
            if (video_pkt_count && video_pkt_count % CHANGE_VIDEO_TRACK_PKT_COUNT == 0) {
                int old_stream_index = video_stream_index;
                pFormat->streams[video_stream_index]->discard = AVDISCARD_ALL;
                if (++video_track_index == video_track_count)
                    video_track_index = 0;
                video_stream_index = video_tracks[video_track_index];
                pFormat->streams[video_stream_index]->discard = AVDISCARD_DEFAULT;
                DEBUG("################## switch video index from %d to %d; video_track_index: %d\n",
                        old_stream_index, video_stream_index, video_track_index);

                // dump stream discard flag
                for (i = 0; i < pFormat->nb_streams; i++)
                    DEBUG("## stream index: %d, discard: %d\n", i, pFormat->streams[i]->discard);
            }
        }

        if (pkt.stream_index == audio_stream_index)
            audio_pkt_count++;

        if (video_pkt_count > READ_VIDEO_PKT_COUNT)
            break;
       usleep(5000);
   }

    PRINTF("decode %s ok, video_pkt_count=%d, audio_pkt_count=%d\n", input_file, video_pkt_count, audio_pkt_count);

    avformat_close_input(&pFormat);
    // todo, close pFormat
    return 0;
}


