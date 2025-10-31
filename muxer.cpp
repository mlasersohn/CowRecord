#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>

#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>

extern "C" 
{
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include	<vorbis/codec.h>
#include	<vorbis/vorbisenc.h>
#include	<vorbis/vorbisfile.h>

#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/video.hpp>
#include <opencv2/core.hpp>

#include "muxer.h"

extern long int	precise_time();
int				my_find_codec_by_id(int type, int in_id, char *result);

void	crash_webcam()
{
	int xx = 12;
	int yy = 6 + 6;
	int nn = xx / (xx - yy);
}

int		create_task(int (*funct)(int *), void *flag);

MyFormat	*global_my_format[1024];
int			global_my_format_cnt = 0;
char		*global_log[128];
int			global_log_cnt = 0;

void logging_callback(void* ptr, int level, const char* fmt, va_list vl)
{
char	part[1024];
char	buf[2048];
int		type;

	if(global_log_cnt < 128)
	{
		av_log_format_line(ptr, level, fmt, vl, part, 1024, &type);
		char *cp = part;
		while(*cp != '\0')
		{
			if(*cp == '@') *cp = '_';
			if((*cp == 10) || (*cp == 13)) *cp = '\0';
			cp++;
		}
		snprintf(buf, 2048, "%ld: %s", time(0), part);
		global_log[global_log_cnt] = strdup(buf);
		global_log_cnt++;
	}
}

Fifo::Fifo()
{
int	loop;

	top = 0;
	bottom = 0;
	for(loop = 0;loop < 1024;loop++)
	{
		frame[loop] = NULL;
	}
}

Fifo::~Fifo()
{
int	loop;

	for(loop = 0;loop < 1024;loop++)
	{
		if(frame[loop] != NULL)
		{
			free(frame[loop]);
		}
	}
}

void	Fifo::push(void *in_frame, int sz)
{
	if(frame[top] != NULL)
	{
		free(frame[top]);
		frame[top] = NULL;
	}
	frame[top] = malloc(sz);
	if(frame[top] != NULL)
	{
		memcpy(frame[top], in_frame, sz);
		top++;
		if(top > 1023)
		{
			top = 0;
		}
	}
}

void	*Fifo::pull()
{
	void *rr = frame[bottom];
	bottom++;
	if(bottom > 1023)
	{
		bottom = 0;
	}
	return(rr);
}

Muxer::Muxer()
{
	initialized = 0;
	frame_ptr = NULL;
	mute = 0;
	stop_activity = 0;
	url = NULL;
	fresh_image = 0;
	oc = NULL;

	encode_video = 0;
	encode_audio = 0;
	current_frame = 0;
	video_st = { 0 };
	audio_st = { 0 };
	have_video = 0;
	have_audio = 0;
	recording = 0;
	video_frames = 0;
	audio_samples = 0;
	paused = 0;

	recordedSamples = NULL;
	number_of_audio_samples = 0;

 	av_log_set_callback(logging_callback);
}

Muxer::~Muxer()
{
	initialized = 0;
	if(oc != NULL)
	{
		close_stream(oc, &video_st);
		close_stream(oc, &audio_st);
		avformat_free_context(oc);
		oc = NULL;
	}
	if(recordedSamples != NULL)
	{
		free(recordedSamples);
		recordedSamples = NULL;
	}
}

int Muxer::write_frame(AVFormatContext *fmt_ctx, AVCodecContext *c, AVStream *st, AVFrame *frame)
{
int ret;

	// send the frame to the encoder
	time_t start = precise_time();
	ret = avcodec_send_frame(c, frame);
	if(ret > -1) 
	{
		while(ret >= 0) 
		{
			AVPacket pkt = { 0 };
			ret = avcodec_receive_packet(c, &pkt);
			if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			{
				break;
			}
			else if(ret > -1) 
			{
				// rescale output packet timestamp values from codec to stream timebase
				av_packet_rescale_ts(&pkt, c->time_base, st->time_base);
				pkt.stream_index = st->index;
		
				ret = av_interleaved_write_frame(fmt_ctx, &pkt);
				av_packet_unref(&pkt);
				if(ret < 0) 
				{
					fprintf(stdout, "Error: Failed while writing output packet: %s\n", av_err2str(ret));
				}
			}
			else
			{
				fprintf(stdout, "Error: failed encoding a frame: %s\n", av_err2str(ret));
			}
		}
	}
	else
	{
		fprintf(stdout, "Error: While sending a frame to the encoder: %s\n", av_err2str(ret));
	}
	time_t elapsed = precise_time() - start;
	int div = 1;
	if(video_frames > 0)
	{
		div = video_frames;
	}
	return ret == AVERROR_EOF ? 1 : 0;
}

AVOutputFormat	*find_output_format_by_name(char *name)
{
	AVOutputFormat *rr = NULL;
	void *muxerState = nullptr;
	const AVOutputFormat *ofmt = av_muxer_iterate(&muxerState);
	while(ofmt) 
	{
		if(strcmp(name, ofmt->name) == 0)
		{
			rr = (AVOutputFormat *)ofmt;
		}
		ofmt = av_muxer_iterate(&muxerState);
	}
	return(rr);
}

enum AVCodecID	codec_by_name(char *name)
{
	AVCodecID id = (AVCodecID)0;
	AVCodec *codec = (AVCodec *)avcodec_find_encoder_by_name(name);
	if(codec != NULL)
	{
		id = codec->id;
	}
	return(id);
}

// Add an output stream.
void Muxer::add_stream(int use_nvidia, OutputStream *ost, AVFormatContext *oc, const AVCodec **codec, enum AVCodecID codec_id, int in_width, int in_height, double in_fps, double in_hz)
{
	AVCodecContext *c;
	int i;

	// find the encoder
	// COW - Forced "h264_nvenc" to use nvidia hardware to encode
	// GPU *codec = avcodec_find_encoder_by_name("h264_nvenc");
	// CPU *codec = avcodec_find_encoder(codec_id);

	if((codec_id == AV_CODEC_ID_H264) && (use_nvidia == 1))
	{
		*codec = avcodec_find_encoder_by_name("h264_nvenc");
		if(*codec == NULL)
		{
			*codec = avcodec_find_encoder(codec_id);
		}
	}
	else
	{
		*codec = avcodec_find_encoder(codec_id);
	}
	if(!(*codec)) 
	{
		fprintf(stdout, "Error: Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
		exit(1);
	}
	ost->st = avformat_new_stream(oc, NULL);
	if(!ost->st) 
	{
		fprintf(stdout, "Error: Could not allocate stream\n");
		exit(1);
	}
	ost->st->id = oc->nb_streams - 1;
	c = avcodec_alloc_context3(*codec);
	if(c) 
	{
		ost->enc = c;
		switch ((*codec)->type) 
		{
			case AVMEDIA_TYPE_AUDIO:
			{
				if(url != NULL)
				{
					c->sample_fmt = AV_SAMPLE_FMT_FLTP;
				}
				else
				{
					c->sample_fmt = (*codec)->sample_fmts ?  (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_S16;
				}
				c->bit_rate = 128000;
				c->sample_rate = -1;
				if((*codec)->supported_samplerates) 
				{
					for(i = 0; (*codec)->supported_samplerates[i]; i++) 
					{
						if((*codec)->supported_samplerates[i] == (int)in_hz)
						{
							c->sample_rate = (int)in_hz;
						}
					}
				}
				else
				{
					c->sample_rate = (int)in_hz;
				}
				if(c->sample_rate != -1)
				{
					c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
					if(used_channels == 1)
					{
						c->channel_layout = AV_CH_LAYOUT_MONO;
					}
					else
					{
						c->channel_layout = AV_CH_LAYOUT_STEREO;
					}
					if((*codec)->channel_layouts) 
					{
						c->channel_layout = (*codec)->channel_layouts[0];
						for(i = 0; (*codec)->channel_layouts[i]; i++) 
						{
							if((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
							{
								c->channel_layout = AV_CH_LAYOUT_STEREO;
							}
							else
							{
							}
						}
					}
					c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
					ost->st->time_base = (AVRational){ 1, c->sample_rate };
				}
				else
				{
					fprintf(stdout, "Error: Illegal sample rate %d for this codec\n", (int)in_hz);
					if((*codec)->supported_samplerates) 
					{
						for(i = 0; (*codec)->supported_samplerates[i]; i++) 
						{
							fprintf(stdout, "%d ", (*codec)->supported_samplerates[i]);
						}
						fprintf(stdout, "\n");
					}
				}
			}
			break;
			case AVMEDIA_TYPE_VIDEO:
			{
				c->codec_id = codec_id;
				c->bit_rate = (in_height * in_width * 24 * in_fps) / 1024; 

				// COW - does there need to be a minimum? 
				if(c->bit_rate < 2500000) c->bit_rate = 2500000;

				// Resolution must be a multiple of two.
				c->width = in_width;
				c->height = in_height;

				// timebase: This is the fundamental unit of time (in seconds) in terms
				// of which frame timestamps are represented. For fixed-fps content,
				// timebase should be 1/framerate and timestamp increments should be
				// identical to 1.

				ost->st->time_base = (AVRational){ 1, (int)in_fps };
				c->time_base = ost->st->time_base;

				c->gop_size = 30; // emit one intra frame every 30 frames at most
				c->pix_fmt = STREAM_PIX_FMT;
				if((*codec)->pix_fmts)
				{
					// COW - TRY TO USE FORMAT SUPPLIED BY CODEC
					c->pix_fmt = (*codec)->pix_fmts[0];
				}
				if(c->codec_id == AV_CODEC_ID_MPEG2VIDEO) 
				{
					// just for testing, we also add B-frames
					c->max_b_frames = 2;
				}
				if(c->codec_id == AV_CODEC_ID_MPEG1VIDEO) 
				{
					// Needed to avoid using macroblocks in which some coeffs overflow.
					// This does not happen with normal video, it just happens here as
					// the motion of the chroma plane does not match the luma plane.
					c->mb_decision = 2;
				}
			}
			break;
			default:
			break;
		}
		// Some formats want stream headers to be separate.
		if(oc->oformat->flags & AVFMT_GLOBALHEADER)
		{
			c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}
	}
	else
	{
		fprintf(stdout, "Error: Could not alloc an encoding context\n");
	}
}

// audio output
AVFrame *Muxer::alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples)
{
int ret;

	AVFrame *frame = av_frame_alloc();
	if(frame) 
	{
		frame->format = sample_fmt;
		frame->channel_layout = channel_layout;
		frame->sample_rate = sample_rate;
		frame->nb_samples = nb_samples;
		if(nb_samples) 
		{
			ret = av_frame_get_buffer(frame, 0);
			if(ret < 0) 
			{
				fprintf(stdout, "Error: Failed allocating an audio buffer\n");
				exit(1);
			}
		}
	}
	else
	{
		fprintf(stdout, "Error: Failed allocating an audio frame\n");
	}
	return(frame);
}

int Muxer::open_audio(AVFormatContext *oc, const AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
AVCodecContext *c;
int nb_samples;
int ret;
AVDictionary *opt = NULL;

	c = ost->enc;
	
	// open it
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if(ret < 0) 
	{
		fprintf(stdout, "Error: Could not open audio codec: %s\n", av_err2str(ret));
		return(-100);
	}
	// init signal generator
	ost->t = 0;
	if(c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
	{
		nb_samples = 1024;
	}
	else
	{
		nb_samples = c->frame_size;
	}
	number_of_audio_samples = nb_samples;

	// COW - NOTE FORCING NUMBER OF AUDIO SAMPLES TO WHAT MY MICS WANT
	nb_samples = 1024;
	c->frame_size = 1024;
	number_of_audio_samples = 1024;

	ost->frame = alloc_audio_frame(c->sample_fmt, c->channel_layout, c->sample_rate, nb_samples);
	if(used_channels == 1)
	{
		ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, AV_CH_LAYOUT_MONO, c->sample_rate, nb_samples);
	}
	else
	{
		ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, AV_CH_LAYOUT_STEREO, c->sample_rate, nb_samples);
	}
	// copy the stream parameters to the muxer
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if(ret < 0)
	{
		fprintf(stdout, "Error: Could not copy the stream parameters\n");
		return(-200);
	}

	// create resampler context
	ost->swr_ctx = swr_alloc();
	if(!ost->swr_ctx) 
	{
		fprintf(stdout, "Error: Could not allocate resampler context\n");
		return(-300);
	}
	// set options
	av_opt_set_int	   (ost->swr_ctx, "in_channel_count",   c->channels,	   0);
	av_opt_set_int	   (ost->swr_ctx, "in_sample_rate",	 c->sample_rate,	0);
	av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",	  AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int	   (ost->swr_ctx, "out_channel_count",  c->channels,	   0);
	av_opt_set_int	   (ost->swr_ctx, "out_sample_rate",	c->sample_rate,	0);
	av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",	 c->sample_fmt,	 0);

	// initialize the resampling context
	if((ret = swr_init(ost->swr_ctx)) < 0) 
	{
		fprintf(stdout, "Error: Failed to initialize the resampling context\n");
		return(-400);
	}
	return(0);
}

AVFrame *Muxer::get_audio_frame(OutputStream *ost, void *in_buffer)
{
AVFrame *frame = ost->tmp_frame;

	memcpy(frame->data[0], (uint8_t *)in_buffer, frame->nb_samples * 2 * used_channels);
	frame->pts = ost->next_pts;
	ost->next_pts += frame->nb_samples;
	return(frame);
}

// Encode one audio frame and send it to the muxer
// return 1 when encoding is finished, 0 otherwise
int Muxer::write_audio_frame(AVFormatContext *oc, OutputStream *ost, void *in_buffer)
{
AVCodecContext *c;
AVFrame *frame;
int ret;
int dst_nb_samples;

	int rr = -1;
	c = ost->enc;
	frame = get_audio_frame(ost, in_buffer);
	if(frame != NULL) 
	{
		// convert samples from native format to destination codec format, using the resampler
		// compute destination number of samples
		dst_nb_samples = av_rescale_rnd(frame->nb_samples, c->sample_rate, c->sample_rate, AV_ROUND_UP);
		av_assert0(dst_nb_samples == frame->nb_samples);
		
		// when we pass a frame to the encoder, it may keep a reference to it internally;
		// make sure we do not overwrite it here
		ret = av_frame_make_writable(ost->frame);
		if(ret > -1)
		{
			// convert to destination format
			ret = swr_convert(ost->swr_ctx, ost->frame->data, dst_nb_samples, (const uint8_t **)frame->data, frame->nb_samples);
			if(ret > -1) 
			{
				frame = ost->frame;
				frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
				ost->samples_count += dst_nb_samples;
				if(c->codec != NULL)
				{
					rr = write_frame(oc, c, ost->st, frame);
				}
				else
				{
					fprintf(stdout, "Error: Audio codec is NULL\n");
				}
			}
			else
			{
				fprintf(stdout, "Error: while converting\n");
			}
		}
		else
		{
			fprintf(stdout, "Error: Cannot make frame writable\n");
		}
	}
	return(rr);
}

// video output
AVFrame *Muxer::alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
AVFrame *picture;
int ret;

	picture = av_frame_alloc();
	if(!picture)
	{
		return(NULL);
	}
	picture->format = pix_fmt;
	picture->width  = width;
	picture->height = height;

	// allocate the buffers for the frame data
	ret = av_frame_get_buffer(picture, 0);
	if(ret < 0) 
	{
		fprintf(stdout, "Error: Could not allocate frame data.\n");
	}
	return(picture);
}

int Muxer::open_video(AVFormatContext *oc, const AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
int ret;
AVCodecContext *c = ost->enc;
AVDictionary *opt = NULL;

	av_dict_copy(&opt, opt_arg, 0);

	// open the codec
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if(ret < 0) 
	{
		fprintf(stdout, "Error: Could not open video codec: %s\n", av_err2str(ret));
		return(-1);
	}
	// allocate and init a re-usable frame
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if(!ost->frame) 
	{
		fprintf(stdout, "Error: Could not allocate video frame\n");
		return(-2);
	}
	// If the output format is not YUV420P, then a temporary YUV420P
	// picture is needed too. It is then converted to the required
	// output format.

	ost->tmp_frame = NULL;
	if(c->pix_fmt != AV_PIX_FMT_YUV420P) 
	{
		ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
		if(!ost->tmp_frame) 
		{
			fprintf(stdout, "Error: Could not allocate temporary picture\n");
			return(-3);
		}
	}
	// copy the stream parameters to the muxer
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if(ret < 0) 
	{
		fprintf(stdout, "Error: Could not copy the stream parameters\n");
		return(-3);
	}
	return(0);
}

void Muxer::fill_yuv_image(AVFrame *pict, int frame_index, int width, int height)
{
	// Y, Cb and Cr
	unsigned char *ptr = (unsigned char *)GetFrame();
	if(ptr != NULL)
	{
		// COW NOTE: I am no longer copying this data from one frame to the other
		// COW NOTE: This may cause the original "data" to be left unfreed
		// COW NOTE: and this data (from a Mat) to be freed in its stead
		int size = height * width;
		pict->data[0] = ptr;						// Y plane
		pict->data[1] = pict->data[0] + size;		// U plane
		pict->data[2] = pict->data[1] + size / 4;	// V plane (assuming 4:2:0 subsampling)

		// manually setting linesizes (assuming no padding for simplicity)
		pict->linesize[0] = width;
		pict->linesize[1] = width / 2;
		pict->linesize[2] = width / 2;
	}
}

AVFrame *Muxer::get_video_frame(OutputStream *ost)
{
	AVCodecContext *c = ost->enc;
	// when we pass a frame to the encoder, it may keep a reference to it
	// internally; make sure we do not overwrite it here
	if(av_frame_make_writable(ost->frame) > -1)
	{
		if(c->pix_fmt != AV_PIX_FMT_YUV420P) 
		{
			// as we only generate a YUV420P picture, we must convert it
			// to the codec pixel format if needed
			int no_go = 0;
			if(!ost->sws_ctx) 
			{
				ost->sws_ctx = sws_getContext(
					c->width, 
					c->height, 
					AV_PIX_FMT_YUV420P, 
					c->width, 
					c->height, 
					c->pix_fmt, 
					SCALE_FLAGS, 
					NULL, 
					NULL, 
					NULL);
				if(!ost->sws_ctx) 
				{
					fprintf(stdout, "Error: Could not initialize the conversion context\n");
					no_go = 1;
				}
			}
			if(no_go == 0)
			{
				fill_yuv_image(ost->tmp_frame, ost->next_pts, c->width, c->height);
				sws_scale(
					ost->sws_ctx, 
					(const uint8_t * const *)ost->tmp_frame->data, 
					ost->tmp_frame->linesize, 
					0, 
					c->height, 
					ost->frame->data, 
					ost->frame->linesize);
			}
		}
		else 
		{
			fill_yuv_image(ost->frame, ost->next_pts, c->width, c->height);
		}
	}
	else
	{
		fprintf(stdout, "Error: Cannot make frame writable\n");
	}
	ost->frame->pts = ost->next_pts;
	ost->next_pts++;
	return(ost->frame);
}

// encode one video frame and send it to the muxer
// return 1 when encoding is finished, 0 otherwise
int Muxer::write_video_frame(AVFormatContext *oc, OutputStream *ost)
{
	int rr = 0;
	AVFrame *frame = get_video_frame(ost);
	if(frame != NULL)
	{
		if((frame->width > 0) && (frame->height > 0))
		{
			if(ost != NULL)
			{
				if(ost->enc != NULL)
				{
					if(ost->enc->codec != NULL)
					{
						rr = write_frame(oc, ost->enc, ost->st, frame);
					}
					else
					{
						fprintf(stdout, "Error: OST->ENC->CODEC is NULL\n");
					}
				}
				else
				{
					fprintf(stdout, "Error: OST->ENC is NULL\n");
				}
			}
			else
			{
				fprintf(stdout, "Error: OST is NULL\n");
			}
		}
		else
		{
			fprintf(stdout, "Error: Video frame size is zero\n");
		}
	}
	return(rr);
}

void	Muxer::Flush()
{
	avio_flush(oc->pb);
}

void Muxer::close_stream(AVFormatContext *oc, OutputStream *ost)
{
	if(ost != NULL)
	{
		avcodec_free_context(&ost->enc);
		if(ost->frame)
		{
			av_frame_free(&ost->frame);
		}
		if(ost->tmp_frame)
		{
			av_frame_free(&ost->tmp_frame);
		}
		if(ost->sws_ctx) 
		{
			sws_freeContext(ost->sws_ctx);
		}
		if(ost->swr_ctx) 
		{
			swr_free(&ost->swr_ctx);
		}
	}
}

time_t last_time = 0;

void	Muxer::EncodeAudioAndVideo(void *in_buffer)
{
extern long int precise_time();

	int save_encode_audio = encode_audio;
	int encode_done = 0;
	while((encode_done == 0) && (stop_activity == 0))
	{
		if((video_st.enc != NULL) && (audio_st.enc != NULL))
		{
			if(encode_video && (!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base, audio_st.next_pts, audio_st.enc->time_base) <= 0)) 
			{
				int rr = write_video_frame(oc, &video_st);
				if(rr == -1)
				{
					encode_done = 1;
				}
				current_frame++;
			}
			else
			{
				encode_done = 1;
			}
		}
	}
	if(encode_audio)
	{
		if(in_buffer != NULL)
		{
			write_audio_frame(oc, &audio_st, in_buffer);
		}
	}
	encode_audio = save_encode_audio;
}

int	Muxer::InitMux(char *in_container, enum AVCodecID video_codec_id, enum AVCodecID audio_codec_id, char *output_filename, char *in_url, int in_width, int in_height, double in_fps, double in_rate, int in_channels)
{
const AVOutputFormat *fmt;
const char *filename;
const AVCodec *audio_codec, *video_codec;
int ret;
AVDictionary *opt = NULL;
int i;

	current_frame = 0;
	filename = output_filename;
	used_rate = in_rate;
	used_channels = in_channels;
	url = in_url;
	used_rate = Open(in_rate, in_channels);
	AVOutputFormat *found_container = NULL;
	if(in_container != NULL)
	{
		found_container = find_output_format_by_name(in_container);
	}
	if(found_container != NULL)
	{
		// allocate the output media context
		avformat_alloc_output_context2(&oc, found_container, NULL, filename);
		if(!oc) 
		{
			printf("Could not deduce output format from file extension: using FLV.\n");
			avformat_alloc_output_context2(&oc, NULL, "flv", filename);
		}
		if(!oc)
		{
			return(1);
		}
		fmt = oc->oformat;

		// Add the audio and video streams using the default format codecs
		// and initialize the codecs.

		if(in_fps < 0.01) 
		{
			in_fps = 0.01;
		}
		enum AVCodecID use_video_codec = fmt->video_codec;
		if(video_codec_id > 0)
		{
			use_video_codec = video_codec_id;
		}
		// COW - Forced AV_CODEC_ID_H264 to use nvidia hardware to encode
		// GPU add_stream(1, &video_st, oc, &video_codec, AV_CODEC_ID_H264, in_width, in_height, in_fps, 0.0);
		// CPU add_stream(0, &video_st, oc, &video_codec, fmt->video_codec, in_width, in_height, in_fps, 0.0);
		// CPU IF GPU FAILS add_stream(0, &video_st, oc, &video_codec, use_video_codec, in_width, in_height, in_fps, 0.0);
		add_stream(1, &video_st, oc, &video_codec, use_video_codec, in_width, in_height, in_fps, 0.0);
		have_video = 1;
		encode_video = 1;
		enum AVCodecID use_audio_codec = fmt->audio_codec;
		if(audio_codec_id > 0)
		{
			use_audio_codec = audio_codec_id;
		}
		add_stream(0, &audio_st, oc, &audio_codec, use_audio_codec, 0, 0, 0, used_rate);
		have_audio = 1;
		encode_audio = 1;
		// Now that all the parameters are set, we can open the audio and
		// video codecs and allocate the necessary encode buffers.
		int cow_err = 0;
		if(have_video)
		{
			cow_err = open_video(oc, video_codec, &video_st, opt);
		}
		if((have_audio) && (cow_err == 0))
		{
			cow_err = open_audio(oc, audio_codec, &audio_st, opt);
		}
		// open the output file, if needed
		if(cow_err == 0)
		{
			if(url == NULL)
			{
				av_dump_format(oc, 0, filename, 1);
				ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
			}
			else
			{
				av_dump_format(oc, 0, url, 1);
				ret = avio_open(&oc->pb, url, AVIO_FLAG_WRITE);
			}
			if(ret < 0) 
			{
				fprintf(stdout, "Error: Could not open '%s': %s\n", filename, av_err2str(ret));
				return(1);
			}
			// Write the stream header, if any.
			ret = avformat_write_header(oc, &opt);
			if(ret < 0) 
			{
				fprintf(stdout, "Error: Error occurred when opening output file: %s\n", av_err2str(ret));
				return(1);
			}
		}
		else
		{
			fprintf(stdout, "Error: Error occurred when opening either audio or video\n");
			return(1);
		}
	}
	else
	{
		fprintf(stdout, "Error: No valid container format specified.\n");
		return(1);
	}
	initialized = 1;
	return(0);
}

void	Muxer::Finish()
{
	initialized = 0;
	if(recordedSamples != NULL)
	{
		free(recordedSamples);
		recordedSamples = NULL;
	}
	av_write_trailer(oc);
	if(have_video)
	{
		close_stream(oc, &video_st);
	}
	if(have_audio)
	{
		close_stream(oc, &audio_st);
	}
	avio_closep(&oc->pb);
	avformat_free_context(oc);
	oc = NULL;
}

void	 Muxer::Record(SAMPLE *audio_input)
{
int	loop;

	int nn = FRAMES_PER_BUFFER * sizeof(SAMPLE) * used_channels;
	int error;
	SAMPLE *p1 = recordedSamples;
	SAMPLE *p2 = audio_input;
	if((p1 != NULL) && (p2 != NULL))
	{
		for(loop = 0;loop < FRAMES_PER_BUFFER * used_channels;loop++)
		{
			SAMPLE one = *p2;
			*p1 = one;
			p1++;
			p2++;
		}
	}
	if(mute == 1)
	{
		memset(recordedSamples, 0, nn);
	}
	EncodeAudioAndVideo((void *)recordedSamples);
	audio_samples += FRAMES_PER_BUFFER;
}

double	Muxer::Open(double requested_rate, int requested_channels)
{
	double rate = 0.0;
	stop_activity = 0;
	int nn = FRAMES_PER_BUFFER * sizeof(SAMPLE) * requested_channels;
	recordedSamples = (SAMPLE *)malloc(nn);
	rate = requested_rate;
	return(rate);
}

void	*Muxer::GetFrame()
{
	void *ptr = NULL;
	ptr = frame_ptr;
	if(ptr != NULL)
	{
		if(fresh_image > 0)
		{
			fresh_image--;
		}
		video_frames++;
	}
	return(ptr);
}
