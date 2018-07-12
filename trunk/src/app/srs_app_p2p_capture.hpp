/*
   The MIT License (MIT)

   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
   the Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
   FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
   COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
   */

//Added by kevin to support p2p capturer

#ifndef SRS_APP_P2P_CAPTURE_HPP
#define SRS_APP_P2P_CAPTURE_HPP

#include <srs_core.hpp>

#include <srs_app_http_conn.hpp>

#ifdef SRS_AUTO_P2P_CAPTURE
#include <srs_app_st.hpp>

enum WriteStatus
{
	WRITE_NONE,
	WRITE_HEADER,
	WRITE_METADATA,
	WRITE_AUDIO,
	WRITE_VIDEO,
	WRITE_VIDEO_SPS_PPS
};

class SrsP2PCapture;
class SrsSimpleBuffer;

class SrsP2PStreamWriter : public SrsFileWriter
{
	private:
		SrsP2PCapture* writer;
	public:
		SrsP2PStreamWriter(SrsP2PCapture* w);
		virtual ~SrsP2PStreamWriter();
		virtual int open(std::string file);
		virtual void close();
		virtual bool is_open();
		virtual int64_t tellg();
		virtual int write(void* buf, size_t count, ssize_t* pnwrite);
		virtual int writev(iovec* iov, int iovcnt, ssize_t* pnwrite);
};


class SrsP2PCapture
{
	public:
		SrsP2PCapture();
		virtual ~SrsP2PCapture();
	public:
		virtual int initialize(SrsSource* s, SrsRequest* r);
		virtual int on_publish(SrsRequest* r);
		virtual void on_unpublish();
		virtual int on_meta_data(SrsOnMetaDataPacket* m);
		virtual int on_audio(SrsSharedPtrMessage* shared_audio, bool is_header);
		virtual int on_video(SrsSharedPtrMessage* shared_video, bool is_sps_pps);
	public:
		//for SrsP2PStreamWriter
		virtual int write(void* buf, size_t count, ssize_t* pnwrite);
		virtual int writev(iovec* iov, int iovcnt, ssize_t* pnwrite);
	private:
		int cache_msg(SrsSharedPtrMessage* msg, bool is_sps_pps = false);
		uint32_t calc_data_offset();
	private:
		std::vector<uint32_t> buffer_size;
		SrsSimpleBuffer* buffer;
		bool media_type_update;
		SrsSharedPtrMessage* cache_metadata;
		// the cached video sequence header.
		SrsSharedPtrMessage* cache_sh_video;
		// the cached audio sequence header.
		SrsSharedPtrMessage* cache_sh_audio;
		//for communication with sp
		int send_register();
		int send_mediatype();
		int send_block();
		int cache_buffer(iovec* iov, int iovcnt, ssize_t* pnwrite);
	private:
		st_netfd_t stfd;
		SrsStSocket* io;
		SrsSource* source;
		int msg_total_size;
		std::vector<SrsSharedPtrMessage*> msg_cache;
		SrsP2PStreamWriter* writer;
		SrsFlvEncoder* enc;
		WriteStatus write_status;
		int64_t write_count;
		uint64_t block_id;
};

#endif

#endif
