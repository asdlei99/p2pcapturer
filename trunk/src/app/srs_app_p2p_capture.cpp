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

#include <srs_app_p2p_capture.hpp>


#if defined(SRS_AUTO_HTTP_CORE)

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sstream>
using namespace std;

#include <srs_protocol_buffer.hpp>
#include <srs_rtmp_utility.hpp>
#include <srs_kernel_log.hpp>
#include <srs_kernel_error.hpp>
#include <srs_app_st.hpp>
#include <srs_core_autofree.hpp>
#include <srs_app_config.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_kernel_file.hpp>
#include <srs_kernel_flv.hpp>
#include <srs_rtmp_stack.hpp>
#include <srs_app_source.hpp>
#include <srs_rtmp_msg_array.hpp>
#include <srs_kernel_aac.hpp>
#include <srs_kernel_mp3.hpp>
#include <srs_kernel_ts.hpp>
#include <srs_app_pithy_print.hpp>
#include <srs_app_source.hpp>
#include <srs_app_server.hpp>

#endif

#define DUMP_P2P_CAPTURE

#ifdef SRS_AUTO_P2P_CAPTURE
#include <srs_app_st.hpp>
#include <srs_app_utility.hpp>
#include <srs_kernel_stream.hpp>

#include <protocol.h>

#define P2P_MEDIA_BUFFER_SIZE 16384

#define MD5_LEN  32


SrsP2PCapture::SrsP2PCapture()
{
	msg_total_size = 0;

	write_status = WRITE_NONE;

	write_count = 0;

	media_type_update = false;

	block_id = 0;

	buffer = new SrsSimpleBuffer();
}

SrsP2PCapture::~SrsP2PCapture()
{
	srs_freep(buffer);
}

int SrsP2PCapture::send_register()
{
	int ret = ERROR_SUCCESS;
	bool islive = 1;
	char channelName[100];
	int userID = 123456;
	char password[MD5_LEN];
	int media_buffer_size = P2P_MEDIA_BUFFER_SIZE;
	int file_size = -1;
	int bitRate = 0;
	int channel_data_size = 0;

	SrsStream* tag_stream = new SrsStream();

	int32_t register_size = 4 + 1 + 1 + 100 + 4 + MD5_LEN +
		4 + 4 + 4 + 1 + 4;
	char* register_buffer = new char[register_size];
	if ((ret = tag_stream->initialize(register_buffer, register_size)) != ERROR_SUCCESS) {
		return ret;
	}

	int channel_name_size = strlen("livestream");
	strncpy(channelName, "livestream", channel_name_size);

	tag_stream->write_bytes((char*)&register_size, 4);
	tag_stream->write_1bytes(CS2SP_REGISTER);
	tag_stream->write_1bytes(channel_name_size);
	tag_stream->write_bytes(channelName, channel_name_size);
	tag_stream->write_bytes((char*)&userID, 4);
	tag_stream->write_bytes(password, MD5_LEN);
	tag_stream->write_bytes((char*)&media_buffer_size, 4);
	tag_stream->write_bytes((char*)&file_size, 4);
	tag_stream->write_bytes((char*)&bitRate, 4);
	tag_stream->write_1bytes(islive);
	tag_stream->write_bytes((char*)&channel_data_size, 4);

	ssize_t nwrite = 0;
	io->write(register_buffer, register_size, &nwrite);

	if(nwrite != register_size)
		srs_error("SrsP2PCapture::write fail to write expect:%d actual:%d", register_size, nwrite);

	return ERROR_SUCCESS;
}

int SrsP2PCapture::send_mediatype()
{
	int ret = ERROR_SUCCESS;

	//media_type_update = false;//hard code, todo, remove it
	if(media_type_update == false)
		return ret;

	SrsStream* tag_stream = new SrsStream();

	//send media type to the sp
	int32_t media_type_size = 13 + //flv header
		cache_metadata->size + SRS_FLV_TAG_HEADER_SIZE + SRS_FLV_PREVIOUS_TAG_SIZE + //metadata
		cache_sh_audio->size + SRS_FLV_TAG_HEADER_SIZE + SRS_FLV_PREVIOUS_TAG_SIZE + //audio
		cache_sh_video->size + SRS_FLV_TAG_HEADER_SIZE + SRS_FLV_PREVIOUS_TAG_SIZE; //video

	char* media_type_buffer = new char[4+1+4];
	if ((ret = tag_stream->initialize(media_type_buffer, 4+1+4)) != ERROR_SUCCESS) {
		return ret;
	}

	int msg_size = media_type_size+4+1+4;
	tag_stream->write_bytes((char*)&msg_size, 4);
	tag_stream->write_1bytes(CS2SP_MEDIA_TYPE);
	tag_stream->write_bytes((char*)&media_type_size, 4);

	ssize_t nwrite = 0;
	io->write(media_type_buffer, 4+4+1, &nwrite);

	if(nwrite != 4+4+1)
		srs_error("SrsP2PCapture::write fail to write expect:%d actual:%d", 4+4+1, nwrite);

	srs_freep(tag_stream);

	write_status = WRITE_HEADER;

	// write flv header.
	if ((ret = enc->write_header())  != ERROR_SUCCESS) {
		return ret;
	}

	if (ret = enc->write_metadata(cache_metadata->timestamp, cache_metadata->payload, cache_metadata->size)!= ERROR_SUCCESS) {
		return ret;
	}

	if (ret = enc->write_audio(cache_sh_audio->timestamp, cache_sh_audio->payload, cache_sh_audio->size)!= ERROR_SUCCESS) {
		return ret;
	}

	if (ret = enc->write_video(cache_sh_video->timestamp, cache_sh_video->payload, cache_sh_video->size)!= ERROR_SUCCESS) {
		return ret;
	}

	media_type_update = false;

	return ERROR_SUCCESS;
}

int SrsP2PCapture::send_block()
{
	int ret = ERROR_SUCCESS;
	std::vector<SrsSharedPtrMessage*>::iterator iter = msg_cache.begin();
	for (; iter != msg_cache.end(); iter++) {
		SrsSharedPtrMessage* msg = *iter;

		if (msg->is_audio()) {
			write_status = WRITE_AUDIO;
			srs_error("SrsP2PCapture: send_block is_audio size =%d msg:%p", msg->size, msg);
			ret = enc->write_audio(msg->timestamp, msg->payload, msg->size);
		} else if (msg->is_video()) {
			write_status = WRITE_VIDEO;
			srs_error("SrsP2PCapture: send_block is_video size =%d msg:%p", msg->size, msg);
			ret = enc->write_video(msg->timestamp, msg->payload, msg->size);
		} else {
			write_status = WRITE_METADATA;
			ret = enc->write_metadata(msg->timestamp, msg->payload, msg->size);
		}

		if (ret != ERROR_SUCCESS) {
			return ret;
		}
	}

	msg_cache.clear();
}

int SrsP2PCapture::initialize(SrsSource* s, SrsRequest* r)
{
	int ret = ERROR_SUCCESS;
	// connect super node.
	if ((ret = srs_socket_connect("192.168.40.200", 12345, ST_UTIME_NO_TIMEOUT, &stfd)) != ERROR_SUCCESS) {
		srs_error("SrsP2PCapture: connect server failed. ret=%d", ret);
		return ret;
	}
	else
	{
		srs_error("SrsP2PCapture: connect server success. ret=%d", ret);
		io = new SrsStSocket(stfd);
	}

	writer = new SrsP2PStreamWriter(this);

	enc = new SrsFlvEncoder;

	if ((ret = enc->initialize(writer)) != ERROR_SUCCESS) {
		return ret;
	}

	send_register();
	return ret;
}

int SrsP2PCapture::on_publish(SrsRequest* r)
{
	return ERROR_SUCCESS;
}

void SrsP2PCapture::on_unpublish()
{

}

int SrsP2PCapture::on_meta_data(SrsOnMetaDataPacket* m)
{
	int ret = ERROR_SUCCESS;

	int size = 0;
	char* payload = NULL;
	if ((ret = m->encode(size, payload)) != ERROR_SUCCESS) {
		return ret;
	}

	SrsSharedPtrMessage metadata;
	if ((ret = metadata.create(NULL, payload, size)) != ERROR_SUCCESS) {
		return ret;
	}

	media_type_update = true;
	cache_metadata = metadata.copy();

	srs_trace("on_meta_data. payload %p size= %d", payload, size);
	return ERROR_SUCCESS;
}

int SrsP2PCapture::cache_msg(SrsSharedPtrMessage* msg, bool is_sps_pps)
{
	int ret = ERROR_SUCCESS;

	msg_total_size += msg->size + SRS_FLV_PREVIOUS_TAG_SIZE + SRS_FLV_TAG_HEADER_SIZE;

	SrsSharedPtrMessage* backup_msg = msg->copy();
	msg_cache.push_back(backup_msg);

	srs_trace("cache_msg. msg_total_size %d msg:%p", msg_total_size, backup_msg);

	send_mediatype();

	if(msg_total_size < (P2P_MEDIA_BUFFER_SIZE-4))
		return ERROR_SUCCESS;
	else
		send_block();

	return ret;
}

int SrsP2PCapture::on_audio(SrsSharedPtrMessage* shared_audio, bool is_header)
{
	char* payload = shared_audio->payload;
	int size = shared_audio->size;

	srs_trace("on_audio. is_header:%d payload %p size= %d", is_header, payload, size);
	if(is_header)
	{
		media_type_update = true;
		cache_sh_audio = shared_audio->copy();
	}
	else
		cache_msg(shared_audio);

	return ERROR_SUCCESS;
}

int SrsP2PCapture::on_video(SrsSharedPtrMessage* shared_video, bool is_sps_pps)
{
	char* payload = shared_video->payload;
	int size = shared_video->size;

	srs_trace("on_video. is_sps_pps:%d payload %p size= %d", is_sps_pps, payload, size);

	if(is_sps_pps)
	{
		media_type_update = true;
		cache_sh_video = shared_video->copy();
	}
	else
		cache_msg(shared_video, true);

	return ERROR_SUCCESS;
}

int SrsP2PCapture::write(void* buf, size_t count, ssize_t* pnwrite)
{
	if (pnwrite) {
		*pnwrite = count;
	}

	srs_trace("SrsP2PCapture::write");

	ssize_t nwrite = 0;
	io->write(buf, count, &nwrite);

	if(nwrite != count)
		srs_error("SrsP2PCapture::write fail to write expect:%d actual:%d", count, nwrite);

	return ERROR_SUCCESS;
}

uint32_t SrsP2PCapture::calc_data_offset()
{
	//remove the itme for this packet(P2P_MEDIA_BUFFER_SIZE)
	std::vector<uint32_t>::iterator iter = buffer_size.begin();
	uint32_t total_size = 0;
	uint32_t first_size = *iter;
	for(; iter != buffer_size.end();)
	{
		total_size += *iter;

		iter = buffer_size.erase(iter);

		if( total_size >= (P2P_MEDIA_BUFFER_SIZE-4))
		{
			uint32_t offset = total_size - (P2P_MEDIA_BUFFER_SIZE-4);
			buffer_size.insert(iter, offset);
			break;
		}

	}

	if(block_id == 0)
		return 0;
	else if(first_size > (P2P_MEDIA_BUFFER_SIZE-4))
		return P2P_MEDIA_BUFFER_SIZE-4;
	else
		return first_size;
}

int SrsP2PCapture::cache_buffer(iovec* iov, int iovcnt, ssize_t* pnwrite)
{
	int i = 0;
	uint32_t total_size = 0;
	for(; i < iovcnt; i++)
	{
		buffer->append((char*)(iov[i].iov_base), iov[i].iov_len);
		total_size += iov[i].iov_len;
	}
	buffer_size.push_back(total_size);

	total_size = buffer->length();
	while(total_size >= (P2P_MEDIA_BUFFER_SIZE-4))
	{
		SrsStream* tag_stream = new SrsStream();

		//send media block to the sp
		int32_t block_header_size = 4 + 1 + 4 + 4 + 4;

		char* block_header = new char[block_header_size];
		if ((ret = tag_stream->initialize(block_header, block_header_size)) != ERROR_SUCCESS) {
			return ret;
		}

		int msg_size = block_header_size+(P2P_MEDIA_BUFFER_SIZE-4);
		int block_size = P2P_MEDIA_BUFFER_SIZE;
		int data_offset = calc_data_offset();

		tag_stream->write_bytes((char*)&msg_size, 4);
		tag_stream->write_1bytes(CS2SP_BLOCK);
		tag_stream->write_bytes((char*)&block_id, 4);
		tag_stream->write_bytes((char*)&block_size, 4);
		tag_stream->write_bytes((char*)&data_offset, 4);

		ssize_t nwrite = 0;
		io->write(block_header, block_header_size, &nwrite);

		if(nwrite != block_header_size)
			srs_error("SrsP2PCapture::write fail to write expect:%d actual:%d", block_header_size, nwrite);

		srs_error("SrsP2PCapture::write block_id:%d data_offset:%d", block_id, data_offset);

		write_count++;

		io->write(buffer->bytes(), (P2P_MEDIA_BUFFER_SIZE-4), &nwrite);

		if(nwrite != ((P2P_MEDIA_BUFFER_SIZE-4)))
			srs_error("SrsP2PCapture::write fail to write expect:%d actual:%d", (P2P_MEDIA_BUFFER_SIZE-4), nwrite);

		srs_freep(tag_stream);

		block_id++;

		total_size -= (P2P_MEDIA_BUFFER_SIZE-4);
		buffer->erase((P2P_MEDIA_BUFFER_SIZE-4));
	}
}

int SrsP2PCapture::writev(iovec* iov, int iovcnt, ssize_t* pnwrite)
{
	srs_trace("SrsP2PCapture::writev iovcnt:%d", iovcnt);

	if(write_status == WRITE_HEADER)
		io->writev(iov, iovcnt, pnwrite);
	else
		cache_buffer(iov, iovcnt, pnwrite);

	return ERROR_SUCCESS;
}

SrsP2PStreamWriter::SrsP2PStreamWriter(SrsP2PCapture* w)
{
	writer = w;
}

SrsP2PStreamWriter::~SrsP2PStreamWriter()
{

}

int SrsP2PStreamWriter::open(std::string /*file*/)
{
	return ERROR_SUCCESS;
}

void SrsP2PStreamWriter::close()
{
}

bool SrsP2PStreamWriter::is_open()
{
	return true;
}

int64_t SrsP2PStreamWriter::tellg()
{
	return 0;
}

int SrsP2PStreamWriter::write(void* buf, size_t count, ssize_t* pnwrite)
{
	if (pnwrite) {
		*pnwrite = count;
	}
	return writer->write((char*)buf, (int)count, pnwrite);
}

int SrsP2PStreamWriter::writev(iovec* iov, int iovcnt, ssize_t* pnwrite)
{
	return writer->writev(iov, iovcnt, pnwrite);
}

#endif
