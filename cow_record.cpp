#include	<unistd.h>
#include	<sched.h>
#include	<time.h>
#include	<sys/time.h>
#include	<fcntl.h>
#include	<semaphore.h>
#include	<sys/sendfile.h>
#include	<signal.h>
#include	<link.h>
#include	<dlfcn.h>
#include	<dirent.h>
#include	<libgen.h>
#include	<magic.h>
#include	<pthread.h>

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

#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/video.hpp>
#include <opencv2/dnn.hpp>

#include <opencv2/core/core_c.h>

#include	<X11/Xlib.h>
#include	<X11/X.h>
#include	<sys/shm.h>
#include	<sys/ipc.h>
#include	<X11/extensions/XShm.h>
#include	<X11/extensions/Xfixes.h>
#include	<X11/extensions/Xcomposite.h>
#include	<X11/extensions/Xrender.h>
#include	<X11/extensions/Xrender.h>
#include	<X11/XKBlib.h>
#include	<X11/Xatom.h>

#include	<Processing.NDI.Lib.h>
#include	<Processing.NDI.DynamicLoad.h>

#include	<FL/Fl.H>
#include	<FL/platform.H>
#include	<Fl/Fl_Cairo.H>
#include	<FL/Fl_Window.H>
#include	<FL/Fl_Double_Window.H>
#include	<FL/fl_draw.H>

#include	<pulse/simple.h>
#include	<pulse/error.h>

#include	<cairo.h>

#include	"muxer.h"

#define	NDI_SEND_VIDEO_FORMAT_BGRX	0
#define	NDI_SEND_VIDEO_FORMAT_RGBX	1
#define	NDI_SEND_VIDEO_FORMAT_I420	2
#define	NDI_SEND_VIDEO_FORMAT_UYVA	3
#define	NDI_SEND_VIDEO_FORMAT_UYVY	4

#define	SEND_NDI_VIDEO	0
#define	SEND_NDI_AUDIO	1

#define NDI_MODE_RECV	1
#define NDI_MODE_SEND	2

#define	MODE_IN_CAMERA			1
#define	MODE_IN_NDI				2
#define	MODE_IN_WINDOW			3
#define	MODE_IN_FULLSCREEN		4
#define	MODE_IN_IMAGE			5
#define	MODE_IN_BACKGROUND		6
#define	MODE_IN_TEXT			7
#define	MODE_IN_TIMESTAMP		8

#define	MODE_OUT_NDI			1
#define	MODE_OUT_RTMP			2
#define	MODE_OUT_FILE			4
#define	MODE_OUT_SNAPSHOT		8

const NDIlib_v3							*NDILib;
void									*hNDILib;
pthread_mutex_t							protect_output;

int		global_window_error = 0;

class	Output;
class	Monitor;

class	Color
{
public:
		int	red;
		int	green;
		int	blue;
		int	alpha;
	
};

class	VideoSource
{
public:
											VideoSource(Display *display, Output *out, VideoSource *old = NULL);
											~VideoSource();
	int										OpenCamera(char *camera, char *format_code, int force_width, int force_height);
	int										OpenWindow(Window win);
	Window									FindWindow(char *requested_window, int in_instance);
	Window									FindFullScreen(int instance);
	void									SetOutputSize(int width, int height, double scale, int cx, int cy, int cw, int ch);
	void									Acquire(int is_base_image);
	unsigned char 							*GrabRawImage();
	void									FormatCode(char *format_code);
	void									Scale(double in_scale);
	void									DrawText(char *text);

	int										Source(char *path, int in_instance);
	int										Source(Window in_win);
	int										WindowSource(char *window_name, int in_instance);
	int										WindowSource(Window in_win);
	int										FullScreenSource(int instance);
	int										DesktopSource();
	int										CameraSource(char *path);
	int										ImageSource(char *path);
	int										BackgroundSource(char *in_color);
	int										TextSource(char *in_text);

	void									RecvNDI(cv::Mat& in_mat, int& width, int& height);
	int										resolve_ndi(char *source);

	int										initialized;

	NDIlib_recv_instance_t					ndi_recv;
	NDIlib_send_instance_t					ndi_send;
	time_t									ndi_initialized;

	Output									*output;

	double									start_time;
	double									stop_time;

	Color									foreground;
	Color									background;
	int										mode;
	int										ndi_mode;
	char									*path;
	cv::Mat									mat;
	cv::Mat									image_mat;
	cv::VideoCapture						*cap;
	Window									window;
	Display									*display;
	int										valid;
	
	int										input_mode;
	int										output_mode;
	int										input_width;
	int										input_height;
	int										output_x;
	int										output_y;
	int										output_width;
	int										output_height;
	double									scale;
	int										crop_x;
	int										crop_y;
	int										crop_w;
	int										crop_h;
	int										frame_thickness;

	XShmSegmentInfo							shminfo;
	XImage									*image;
	int										old_w;
	int										old_h;
	int										no_alpha;
	int										show_mouse;
	int										request_mouse;
	int										available_frames;
	int										repeat;

	cairo_surface_t							*surface;
	cairo_t									*context;
	char									*text;
	char									*font;
	int										font_sz;
	int										font_bold;
	int										font_italic;

	int										monitor_flag;
	Monitor									*monitor;
};

class	Output
{
public:
							Output(char *in_path, int ww, int hh, double fps);
							~Output();
	int						OpenMuxer();
	void					Record(int wait_on_keyboard, double wait_on_delay, double record_time);
	void					Run();
	void					Add(VideoSource *vs);
	void					Fit();
	void					Send();
	void					PasteWithAlpha(cv::Mat source, int xx, int yy);
	void					SetSize(int ww, int hh);
	void					StreamToNDI(int which, char *ndi_stream_name, SAMPLE *audio_buffer, int ndi_send_video_format, cv::Mat in_mat, int output_width, int output_height);
	void					CheckStopped();
	void					Prep(VideoSource *vs, int base_image);
	
	char					path[4096];
	int						mode;
	cv::Mat					mat;
	cv::Mat					output_mat;
	Muxer					*muxer;
	VideoSource				*video_source[128];
	int						video_source_cnt;
	int						width;
	int						height;
	double					scale;
	double					fps;
	double					frequency;
	int						channels;
	char					*audio_device[128];
	double					audio_volume[128];
	int						audio_device_cnt;
	char					*container;
	AVCodecID				video_codec;
	AVCodecID				audio_codec;
	time_t					start;
	int						monitor_flag;
	Monitor					*monitor;
	int						stop_audio;
	int						stopped;
	char					*msg;

	NDIlib_send_instance_t					ndi_send;
	NDIlib_video_frame_v2_t					ndi_video_frame;
	NDIlib_audio_frame_interleaved_16s_t	ndi_audio_frame;
	time_t									ndi_initialized;

	cairo_surface_t			*dest_surface;
	cairo_t					*dest_context;

	int						crop_x;
	int						crop_y;
	int						crop_w;
	int						crop_h;

	int						frame_cnt;
	double					recorded_time;
	char					*snapshot_filename;
	double					snapshot_interval;
	double					snapshot_time;
	int						audio_delay;
	double					monitor_ratio;
	int						paused;
	int						force_wait;
};

class	Monitor : public Fl_Double_Window
{
public:
				Monitor(int ww, int hh, Output *out);
				Monitor(int ww, int hh, VideoSource *vs);
				~Monitor();
	void		draw();
	int			handle(int event);

	Output		*out;
	VideoSource	*video_source;
};

#define MAX_PROPERTY_VALUE_LEN 4096

long int precise_time()
{
struct timeval tv;
	
	gettimeofday(&tv, NULL);
	long int ts = (tv.tv_sec * 1000000) + tv.tv_usec;
	return(ts);
}

long int local_timestamp()
{
struct timeval tv;
	
	gettimeofday(&tv, NULL);
	long int ts = ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
	return(ts);
}

void	parse_timestamp_format(char *in, char *out, int frame_cnt)
{
	char buf[256];
	char out_buf[256];
	strcpy(out_buf, "");
	char *cp = in;
	time_t now = time(0);
	struct tm *tm = localtime(&now);
	while(*cp != '\0')
	{
		if(strncmp(cp, "%Y", 2) == 0)
		{
			sprintf(buf, "%04d", tm->tm_year + 1900);
			strcat(out_buf, buf);
			cp += 2;
		}
		else if(strncmp(cp, "%M", 2) == 0)
		{
			sprintf(buf, "%02d", tm->tm_mon + 1);
			strcat(out_buf, buf);
			cp += 2;
		}
		else if(strncmp(cp, "%D", 2) == 0)
		{
			sprintf(buf, "%02d", tm->tm_mday);
			strcat(out_buf, buf);
			cp += 2;
		}
		else if(strncmp(cp, "%h", 2) == 0)
		{
			sprintf(buf, "%02d", tm->tm_hour);
			strcat(out_buf, buf);
			cp += 2;
		}
		else if(strncmp(cp, "%m", 2) == 0)
		{
			sprintf(buf, "%02d", tm->tm_min);
			strcat(out_buf, buf);
			cp += 2;
		}
		else if(strncmp(cp, "%s", 2) == 0)
		{
			sprintf(buf, "%02d", tm->tm_sec);
			strcat(out_buf, buf);
			cp += 2;
		}
		else if(strncmp(cp, "%f", 2) == 0)
		{
			sprintf(buf, "%d", frame_cnt);
			strcat(out_buf, buf);
			cp += 2;
		}
		else
		{
			char mini[2];
			mini[0] = *cp;
			mini[1] = '\0';
			strcat(out_buf, mini);
			cp++;
		}
	}
	strcpy(out, out_buf);
}

void force_crash() 
{
	raise(SIGTRAP);
}

void	scan_pulse(int use_source_list)
{
int	pulse_list_devices(char *def_src, char *def_sink, int in_or_out, int limit, char **list, char **description, int *index, int *channels, int *rate);
int	loop;

	char *list[128];
	char *description[128];
	char def_source[4096];
	char def_sink[4096];
	int index[128];
	int channels[128];
	int rate[128];
	int nn = pulse_list_devices(def_source, def_sink, 1, 128, list, description, index, channels, rate);
	for(loop = 0;loop < nn;loop++)
	{
		if(list[loop] != NULL)
		{
			char cc = ' ';
			if(strcmp(def_source, list[loop]) == 0)
			{
				cc = '*';
			}
			printf("%c %03d NAME: [%s]\n", cc, index[loop], list[loop]);
			free(list[loop]);
		}
		if(description[loop] != NULL)
		{
			printf("\tDescription: %s\n", description[loop]);
			printf("\tChannels: %d\n", channels[loop]);
			printf("\tRate: %d\n", rate[loop]);
			free(description[loop]);
		}
		printf("\n");
	}
}

void	scan_pulse_for_index(int use_source_list, int in_index, char *in_name)
{
int	pulse_list_devices(char *def_src, char *def_sink, int in_or_out, int limit, char **list, char **description, int *index, int *channels, int *rate);
int	loop;

	char *list[128];
	char *description[128];
	char def_source[4096];
	char def_sink[4096];
	int index[128];
	int channels[128];
	int rate[128];
	int nn = pulse_list_devices(def_source, def_sink, 1, 128, list, description, index, channels, rate);
	for(loop = 0;loop < nn;loop++)
	{
		if(list[loop] != NULL)
		{
			if(in_index == index[loop])
			{
				strcpy(in_name, list[loop]);
			}
			free(list[loop]);
		}
		if(description[loop] != NULL)
		{
			free(description[loop]);
		}
		printf("\n");
	}
}

void	resolve_four_numbers(char *str, int& one, int& two, int& three, int& four)
{
	char *cp = str;
	while(((*cp < '0') || (*cp > '9')) && (*cp != '\0')) cp++;
	if(*cp != '\0')
	{
		one = atoi(cp);
		while((*cp != ':') && (*cp != '\0')) cp++;
		if(*cp == ':')
		{
			cp++;
			while(((*cp < '0') || (*cp > '9')) && (*cp != '\0')) cp++;
			if(*cp != '\0')
			{
				two = atoi(cp);
				while((*cp != ':') && (*cp != '\0')) cp++;
				if(*cp == ':')
				{
					cp++;
					while(((*cp < '0') || (*cp > '9')) && (*cp != '\0')) cp++;
					if(*cp != '\0')
					{
						three = atoi(cp);
						while((*cp != ':') && (*cp != '\0')) cp++;
						if(*cp == ':')
						{
							cp++;
							while(((*cp < '0') || (*cp > '9')) && (*cp != '\0')) cp++;
							if(*cp != '\0')
							{
								four = atoi(cp);
							}
						}
					}
				}
			}
		}
	}
}

char	*find_extension(char *filename)
{
	char *rr = NULL;
	char *cp = filename;
	while((*cp != '\0') && (rr == NULL))
	{
		if(*cp == '.')
		{
			rr = cp + 1;
		}
		cp++;
	}
	return(rr);
}

void	my_cairo_set_source_rgba(cairo_t *context, int r, int g, int b, int a)
{
	double rr = (double)r / 255.0;
	double gg = (double)g / 255.0;
	double bb = (double)b / 255.0;
	double aa = (double)a / 255.0;
	
	cairo_set_source_rgba(context, rr, gg, bb, aa);
}

void	VideoSource::DrawText(char *text)
{
int		loop;
int		inner;

	surface = cairo_image_surface_create_for_data(mat.ptr(), CAIRO_FORMAT_ARGB32, mat.cols, mat.rows, mat.step);
	context = cairo_create(surface);
	cairo_t *cr = context;
	if(cr != NULL)
	{
		int sz = font_sz;
		int xx = 0;
		int yy = sz;
		int save_x = xx;
		int rr = foreground.red;
		int gg = foreground.green;
		int bb = foreground.blue;
		int aa = foreground.alpha;
		cairo_font_weight_t weight = CAIRO_FONT_WEIGHT_NORMAL;
		if(font_bold == 1)
		{
			weight = CAIRO_FONT_WEIGHT_BOLD;
		}
		cairo_font_slant_t slant = CAIRO_FONT_SLANT_NORMAL;
		if(font_italic == 1)
		{
			slant = CAIRO_FONT_SLANT_ITALIC;
		}
		my_cairo_set_source_rgba(cr, rr, gg, bb, aa);
		cairo_select_font_face(cr, font, slant, weight);
		cairo_set_font_size(cr, sz);

		char *cp = text;
		while(*cp != '\0')
		{
			if(*cp != '\0')
			{
				char cc[2];
				cc[0] = *cp;
				cc[1] = '\0';
				if(*cp == 10)
				{
					yy += sz;
					xx = save_x;
				}
				else
				{
					cairo_move_to(cr, xx, yy);
					cairo_show_text(cr, cc);
					cairo_stroke(cr);
					cairo_text_extents_t extents;
					cairo_text_extents(cr, cc, &extents);
					xx += extents.x_advance;
				}
				cp++;
			}
		}
		if(context != NULL)
		{
			cairo_destroy(context);
		}
		if(surface != NULL)
		{
			cairo_surface_destroy(surface);
		}
	}
	else
	{
		fprintf(stderr, "\nError: destination context is NULL in text command.\n");
	}
}

int	create_task(int (*funct)(int *), void *flag)
{
int	 	status;
pthread_t	mythread;
pthread_attr_t	attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if(pthread_create(&mythread, &attr, (void * (*)(void *))funct, flag))
	{
		fprintf(stderr, "\nError: Cannot create a new thread.\n");
	}
	return((int)mythread);
}

void rgb_to_yuv422_uyvy(const cv::Mat& rgb, cv::Mat& yuv) 
{
	assert(rgb.size() == yuv.size() &&
		   rgb.depth() == CV_8U &&
		   rgb.channels() == 3 &&
		   yuv.depth() == CV_8U &&
		   yuv.channels() == 2);
	for(int ih = 0; ih < rgb.rows; ih++) 
	{
		const uint8_t* rgbRowPtr = rgb.ptr<uint8_t>(ih);
		uint8_t* yuvRowPtr = yuv.ptr<uint8_t>(ih);

		for(int iw = 0; iw < rgb.cols; iw = iw + 2) 
		{
			const int rgbColIdxBytes = iw * rgb.elemSize();
			const int yuvColIdxBytes = iw * yuv.elemSize();

			const uint8_t R1 = rgbRowPtr[rgbColIdxBytes + 0];
			const uint8_t G1 = rgbRowPtr[rgbColIdxBytes + 1];
			const uint8_t B1 = rgbRowPtr[rgbColIdxBytes + 2];
			const uint8_t R2 = rgbRowPtr[rgbColIdxBytes + 3];
			const uint8_t G2 = rgbRowPtr[rgbColIdxBytes + 4];
			const uint8_t B2 = rgbRowPtr[rgbColIdxBytes + 5];

			const int Y  =  (0.257f * R1) + (0.504f * G1) + (0.098f * B1) + 16.0f ;
			const int U  = -(0.148f * R1) - (0.291f * G1) + (0.439f * B1) + 128.0f;
			const int V  =  (0.439f * R1) - (0.368f * G1) - (0.071f * B1) + 128.0f;
			const int Y2 =  (0.257f * R2) + (0.504f * G2) + (0.098f * B2) + 16.0f ;

			yuvRowPtr[yuvColIdxBytes + 0] = cv::saturate_cast<uint8_t>(U );
			yuvRowPtr[yuvColIdxBytes + 1] = cv::saturate_cast<uint8_t>(Y );
			yuvRowPtr[yuvColIdxBytes + 2] = cv::saturate_cast<uint8_t>(V );
			yuvRowPtr[yuvColIdxBytes + 3] = cv::saturate_cast<uint8_t>(Y2);
		}
	}
}

void	BGRA_UYVY(cv::Mat in_mat, cv::Mat& out_mat, int ww, int hh)
{
	cv::Mat uyvy = cv::Mat(hh, ww, CV_8UC2);
	cv::Mat rgb;
	cv::cvtColor(in_mat, rgb, cv::COLOR_RGBA2BGR);
	rgb_to_yuv422_uyvy(rgb, uyvy); 
	uyvy.copyTo(out_mat);
}

void	*BGRA_UYVA(cv::Mat in_mat, int ww, int hh)
{
	cv::Mat uyvy = cv::Mat(hh, ww, CV_8UC2);
	cv::Mat rgb;
	cv::cvtColor(in_mat, rgb, cv::COLOR_BGRA2BGR);
	rgb_to_yuv422_uyvy(rgb, uyvy); 

	void *result = (void *)malloc(((ww * 2) * hh) + (ww * hh));
	memcpy(result, uyvy.ptr(), ((ww * 2) * hh));
	char *cp = (char *)result + ((ww * 2) * hh);
	memset(cp, 255, (ww * hh));
	return(result);
}

void	UYVY_BGR(cv::Mat& out_mat, void *data, int ww, int hh)
{
	cv::Mat frame = cv::Mat(hh, ww, CV_8UC2, data);
	cv::cvtColor(frame, out_mat, cv::COLOR_YUV2BGRA_UYVY, 0);
}

void	I420_BGR(cv::Mat& out_mat, void *data, int ww, int hh)
{
	cv::Mat frame = cv::Mat(hh, ww, CV_8UC2, data);
	cv::cvtColor(frame, out_mat, cv::COLOR_YUV420p2BGRA);
}

void	P216_BGR(cv::Mat& out_mat, void *data, int ww, int hh)
{
int	xx, yy;

	cv::Mat frame = cv::Mat(hh, ww, CV_8UC4, cv::Scalar(76, 89, 100, 255));
	unsigned char *ptr = (unsigned char *)frame.ptr();

	int16_t *p_y = (int16_t *)data; 
	int stride = ww * sizeof(int16_t);
	int offset = hh * stride; 
	int16_t *p_uv = (int16_t *)((char *)data + offset); 
	int16_t Y = 0;
	int16_t U = 0;
	int16_t V = 0;
	int cnt = 0;
	for(yy = 0;yy < hh;yy++)
	{
		for(xx = 0;xx < ww;xx++)
		{
			Y = *p_y++;
			Y -= 32768;
			if((cnt % 2) == 0)
			{
				U = *p_uv++;
				U -= 32768;
				V = *p_uv++;
				V -= 32768;
			}
			int16_t R = Y + 1.370705f * V;
			int16_t G = Y - (0.698001f * V) - (0.337633f * U);
			int16_t B = Y + 1.732446f * U;

			B /= 255;
			G /= 255;
			R /= 255;

			B += 128;
			G += 128;
			R += 128;

			if(B < 0) B = 0;
			else if(B > 255) B = 255;
			if(G < 0) G = 0;
			else if(G > 255) G = 255;
			if(R < 0) R = 0;
			else if(R > 255) R = 255;

			*ptr++ = (uchar)B;
			*ptr++ = (uchar)G;
			*ptr++ = (uchar)R;
			*ptr++ = 255;
			cnt++;
		}
	}
	out_mat = frame.clone();
}

void	DynamicallyLoadNDILibrary(char *library_path)
{
	hNDILib = dlopen(library_path, RTLD_LOCAL | RTLD_LAZY);
	const NDIlib_v3 *(*NDIlib_v3_load)(void) = NULL;
	if(hNDILib)
	{
		*((void**)&NDIlib_v3_load) = dlsym(hNDILib, "NDIlib_v3_load");
		if(!NDIlib_v3_load)
		{
			if(hNDILib)
			{
				dlclose(hNDILib);
				hNDILib = NULL;
			}
		}
		else
		{
			NDILib = (const NDIlib_v3 *)NDIlib_v3_load();
		}
	}
}

NDIlib_recv_instance_t OpenNDI(NDIlib_source_t source, int format)
{
	NDIlib_recv_instance_t local_ndi_recv = NULL;
	if(NDILib != NULL)
	{
		NDIlib_recv_create_v3_t NDI_recv_create_desc;

		NDI_recv_create_desc.source_to_connect_to = source;
		NDI_recv_create_desc.p_ndi_recv_name = "NDI Source";
		if(format == 1)
		{
			NDI_recv_create_desc.color_format = NDIlib_recv_color_format_fastest;
		}
		else if(format == 2)
		{
			NDI_recv_create_desc.color_format = NDIlib_recv_color_format_best;
		}
		else if(format == 3)
		{
			NDI_recv_create_desc.color_format = NDIlib_recv_color_format_BGRX_BGRA;
		}
		else if(format == 3)
		{
			NDI_recv_create_desc.color_format = NDIlib_recv_color_format_RGBX_RGBA;
		}
		local_ndi_recv = NDILib->NDIlib_recv_create_v3(&NDI_recv_create_desc);

		NDIlib_metadata_frame_t enable_hw_accel;
		enable_hw_accel.p_data = "<ndi_hwaccel enabled=\"true\"/>";
		NDILib->NDIlib_recv_send_metadata(local_ndi_recv, &enable_hw_accel);
	}
	return(local_ndi_recv);
}

NDIlib_recv_instance_t	FindNDI(char *name, int format = 0)
{
int	loop;

	NDIlib_recv_instance_t local_ndi_recv = NULL;
	if(NDILib != NULL)
	{
		uint32_t num_sources = 0;
		const NDIlib_source_t *sources = NULL;
		const NDIlib_find_create_t NDI_find_create_desc;
		NDIlib_find_instance_t ndi_find = NDILib->NDIlib_find_create_v2(&NDI_find_create_desc);
		NDILib->NDIlib_find_wait_for_sources(ndi_find, 1000);
		sources = NDILib->NDIlib_find_get_current_sources(ndi_find, &num_sources);

		int done = 0;
		for(loop = 0;((loop < num_sources) && (done == 0));loop++)
		{
			if(strcmp(name, sources[loop].p_ndi_name) == 0)
			{
				local_ndi_recv = OpenNDI(sources[loop], format);
				done = 1;
			}
		}
		NDILib->NDIlib_find_destroy(ndi_find);
	}
	return(local_ndi_recv);
}

int		VideoSource::resolve_ndi(char *source)
{
int	ndi_capture = 0;

	if(strncasecmp(source, "ndi://", strlen("ndi://")) == 0)
	{
		char *cp = source + strlen("ndi://");
		if(strlen(cp) > 0)
		{
			ndi_recv = NULL;
			ndi_recv = FindNDI(cp, 0);
			if(ndi_recv != NULL)
			{
				ndi_capture = 1;
			}
		}
	}
	else if(strncasecmp(source, "ndi_uyvy://", strlen("ndi_uyvy://")) == 0)
	{
		char *cp = source + strlen("ndi_uyvy://");
		if(strlen(cp) > 0)
		{
			ndi_recv = NULL;
			ndi_recv = FindNDI(cp, 1);
			if(ndi_recv != NULL)
			{
				ndi_capture = 1;
			}
		}
	}
	else if(strncasecmp(source, "ndi_p216://", strlen("ndi_p216://")) == 0)
	{
		char *cp = source + strlen("ndi_p216://");
		if(strlen(cp) > 0)
		{
			ndi_recv = NULL;
			ndi_recv = FindNDI(cp, 2);
			if(ndi_recv != NULL)
			{
				ndi_capture = 1;
			}
		}
	}
	else if(strncasecmp(source, "ndi_bgrx://", strlen("ndi_bgrx://")) == 0)
	{
		char *cp = source + strlen("ndi_bgrx://");
		if(strlen(cp) > 0)
		{
			ndi_recv = NULL;
			ndi_recv = FindNDI(cp, 3);
			if(ndi_recv != NULL)
			{
				ndi_capture = 1;
			}
		}
	}
	else if(strncasecmp(source, "ndi_rgbx://", strlen("ndi_rgbx://")) == 0)
	{
		char *cp = source + strlen("ndi_rgbx://");
		if(strlen(cp) > 0)
		{
			ndi_recv = NULL;
			ndi_recv = FindNDI(cp, 4);
			if(ndi_recv != NULL)
			{
				ndi_capture = 1;
			}
		}
	}
	return(ndi_capture);
}

void	VideoSource::RecvNDI(cv::Mat& in_mat, int& width, int& height)
{
NDIlib_video_frame_v2_t	video_frame;
NDIlib_metadata_frame_t	metadata_frame;

	if(NDILib != NULL)
	{
		int nn = NDILib->NDIlib_recv_capture_v3(ndi_recv, &video_frame, NULL, &metadata_frame, 1000);
		switch(nn)
		{
			// No data
			case(NDIlib_frame_type_none):
			{
				fprintf(stderr, "No NDI video data received.\n");
				if(in_mat.empty())
				{
					cv::Mat frame = cv::Mat(height, width, CV_8UC4, cv::Scalar(76, 89, 100, 255));
					frame.copyTo(in_mat);
				}
			}
			break;
			// Video data
			case(NDIlib_frame_type_video):
			{
				char *str = "UNKNOWN";
				if(video_frame.FourCC == NDIlib_FourCC_type_UYVY) str = "NDIlib_FourCC_type_UYVY";
				else if(video_frame.FourCC == NDIlib_FourCC_type_UYVA) str = "NDIlib_FourCC_type_UYVA";
				else if(video_frame.FourCC == NDIlib_FourCC_type_P216) str = "NDIlib_FourCC_type_P216";
				else if(video_frame.FourCC == NDIlib_FourCC_type_PA16) str = "NDIlib_FourCC_type_A216";
				else if(video_frame.FourCC == NDIlib_FourCC_type_YV12) str = "NDIlib_FourCC_type_YV12";
				else if(video_frame.FourCC == NDIlib_FourCC_type_I420) str = "NDIlib_FourCC_type_I420";
				else if(video_frame.FourCC == NDIlib_FourCC_type_NV12) str = "NDIlib_FourCC_type_NV12";
				else if(video_frame.FourCC == NDIlib_FourCC_type_BGRA) str = "NDIlib_FourCC_type_BGRA";
				else if(video_frame.FourCC == NDIlib_FourCC_type_BGRX) str = "NDIlib_FourCC_type_BGRX";
				else if(video_frame.FourCC == NDIlib_FourCC_type_RGBA) str = "NDIlib_FourCC_type_RGBA";
				else if(video_frame.FourCC == NDIlib_FourCC_type_RGBX) str = "NDIlib_FourCC_type_RGBX";

				str = "UNKNOWN";
				if(video_frame.frame_format_type == NDIlib_frame_format_type_progressive) str = "Progressive";
				else if(video_frame.frame_format_type == NDIlib_frame_format_type_interleaved) str = "Interleaved";
				else if(video_frame.frame_format_type == NDIlib_frame_format_type_field_0) str = "Field 0";
				else if(video_frame.frame_format_type == NDIlib_frame_format_type_field_1) str = "Field 1";
				
				width = video_frame.xres;
				height = video_frame.yres;

				unsigned char *use_data = (unsigned char *)video_frame.p_data;
				if((video_frame.FourCC == NDIlib_FourCC_type_UYVY)
				|| (video_frame.FourCC == NDIlib_FourCC_type_UYVA))
				{
					cv::Mat frame;
					UYVY_BGR(frame, use_data, video_frame.xres, video_frame.yres);
					frame.copyTo(in_mat);
				}
				else if((video_frame.FourCC == NDIlib_FourCC_type_P216)
				|| (video_frame.FourCC == NDIlib_FourCC_type_PA16))
				{
					cv::Mat frame;
					P216_BGR(frame, use_data, video_frame.xres, video_frame.yres);
					frame.copyTo(in_mat);
				}
				else if(video_frame.FourCC == NDIlib_FourCC_type_I420)
				{
					cv::Mat frame;
					I420_BGR(frame, use_data, video_frame.xres, video_frame.yres);
					frame.copyTo(in_mat);
				}
				else if(video_frame.FourCC == NDIlib_FourCC_type_YV12)
				{
					cv::Mat frame;
					cv::Mat yuv(video_frame.yres, video_frame.xres, CV_8UC2, use_data);
					cvtColor(yuv, frame, cv::COLOR_YUV2BGR_YV12);
					frame.copyTo(in_mat);
				}
				else if(video_frame.FourCC == NDIlib_FourCC_type_NV12)
				{
					cv::Mat frame;
					cv::Mat yuv(video_frame.yres + video_frame.yres / 2, video_frame.xres, CV_8UC1, use_data);
					cvtColor(yuv, frame, cv::COLOR_YUV2BGR_NV12);
					frame.copyTo(in_mat);
				}
				else if((video_frame.FourCC == NDIlib_FourCC_type_RGBX)
				|| (video_frame.FourCC == NDIlib_FourCC_type_RGBA))
				{
					cv::Mat frame;
					int cv_flag = CV_8UC4;
					cv::Mat rgb(cv::Size(video_frame.xres, video_frame.yres), cv_flag, use_data);
					cvtColor(rgb, frame, cv::COLOR_RGB2BGR);
					frame.copyTo(in_mat);
				}
				else
				{
					int cv_flag = CV_8UC4;
					cv::Mat frame(cv::Size(video_frame.xres, video_frame.yres), cv_flag, use_data);
					frame.copyTo(in_mat);
				}
				NDILib->NDIlib_recv_free_video_v2(ndi_recv, &video_frame);
			}
			break;
			case(NDIlib_frame_type_metadata):
			{
				if(metadata_frame.p_data) 
				{
				}
				NDILib->NDIlib_recv_free_metadata(ndi_recv, &metadata_frame); 
			}
			break;
		}
	}
}

void	Output::StreamToNDI(int which, char *ndi_stream_name, SAMPLE *audio_buffer, int ndi_send_video_format, cv::Mat in_mat, int output_width, int output_height)
{
int	loop;

	if(NDILib != NULL)
	{
		if(((time(0) - ndi_initialized) > 5) && (ndi_initialized > 0))
		{
			fprintf(stderr, "\nError: NDI output timed out.\n");
		}
		if(ndi_send == NULL)
		{
			NDIlib_send_create_t NDI_send_create_desc;
			if(strlen(ndi_stream_name) > 0)
			{
				NDI_send_create_desc.p_ndi_name = ndi_stream_name;
			}
			else
			{
				NDI_send_create_desc.p_ndi_name = "CowCam NDI";
			}
			ndi_send = NDILib->NDIlib_send_create(&NDI_send_create_desc);
			ndi_video_frame.xres = output_width;
			ndi_video_frame.yres = output_height;
			if(ndi_send_video_format == NDI_SEND_VIDEO_FORMAT_BGRX)
			{
				ndi_video_frame.FourCC = NDIlib_FourCC_type_BGRX;
			}
			else if(ndi_send_video_format == NDI_SEND_VIDEO_FORMAT_RGBX)
			{
				ndi_video_frame.FourCC = NDIlib_FourCC_type_RGBX;
			}
			else if(ndi_send_video_format == NDI_SEND_VIDEO_FORMAT_I420)
			{
				ndi_video_frame.FourCC = NDIlib_FourCC_type_I420;
			}
			else if(ndi_send_video_format == NDI_SEND_VIDEO_FORMAT_UYVA)
			{
				ndi_video_frame.FourCC = NDIlib_FourCC_type_UYVA;
			}
			else if(ndi_send_video_format == NDI_SEND_VIDEO_FORMAT_UYVY)
			{
				ndi_video_frame.FourCC = NDIlib_FourCC_type_UYVY;
			}
			ndi_audio_frame.no_samples = AUDIO_BUFFER_SIZE;
			ndi_audio_frame.no_channels = channels;
			ndi_audio_frame.sample_rate = frequency;
			ndi_initialized = time(0);
		}
		if(ndi_send != NULL)
		{
			if(which == SEND_NDI_AUDIO)
			{
				ndi_audio_frame.p_data = audio_buffer;
				NDILib->NDIlib_util_send_send_audio_interleaved_16s(ndi_send, &ndi_audio_frame);
			}
			else
			{
				cv::Mat out_mat;
				void *result = NULL;
				cv::Mat local_mat = in_mat.clone();
				if(ndi_send_video_format == NDI_SEND_VIDEO_FORMAT_BGRX)
				{
					cv::cvtColor(local_mat, local_mat, cv::COLOR_BGRA2RGBA);
					ndi_video_frame.p_data = local_mat.ptr();
				}
				else if(ndi_send_video_format == NDI_SEND_VIDEO_FORMAT_RGBX)
				{
					ndi_video_frame.p_data = local_mat.ptr();
				}
				else if(ndi_send_video_format == NDI_SEND_VIDEO_FORMAT_I420)
				{
					cv::cvtColor(local_mat, local_mat, cv::COLOR_RGB2YUV_I420);
					ndi_video_frame.p_data = local_mat.ptr();
				}
				else if(ndi_send_video_format == NDI_SEND_VIDEO_FORMAT_UYVA)
				{
					result = BGRA_UYVA(local_mat, output_width, output_height);
					if(result != NULL)
					{
						ndi_video_frame.p_data = (unsigned char *)result;
					}
				}
				else if(ndi_send_video_format == NDI_SEND_VIDEO_FORMAT_UYVY)
				{
					BGRA_UYVY(local_mat, out_mat, output_width, output_height);
					ndi_video_frame.p_data = (unsigned char *)out_mat.ptr();
				}
				NDILib->NDIlib_send_send_video_v2(ndi_send, &ndi_video_frame);
				ndi_initialized = time(0);
				if(result != NULL)
				{
					free(result);
				}
			}
		}
	}
}

void	x11_mouse(Display *display, Window win, int& xx, int& yy, int& mask)
{
Window		window_returned;
int			root_x, root_y;
int			win_x, win_y;
unsigned	int mask_return;

	Screen *screen = DefaultScreenOfDisplay(display);
	int screen_number = XScreenNumberOfScreen(screen);
	if(win == None)
	{
		win = RootWindow(display, screen_number); 
	}
	int result = XQueryPointer(
		display, 
		win, 
		&window_returned, 
		&window_returned, 
		&root_x, 
		&root_y, 
		&win_x, 
		&win_y, 
		&mask_return);
	xx = win_x;
	yy = win_y;
	mask = mask_return;
}

void	draw_transparency(cv::Mat frame, cv::Mat transp, int xPos, int yPos)
{
cv::Mat mask;
std::vector<cv::Mat> layers;

	cv::Mat use;
	cv::split(transp, layers); // seperate channels
	cv::Mat rgb[3] = { layers[0], layers[1], layers[2] };
	mask = layers[3]; // png's alpha channel used as mask
	cv::merge(rgb, 3, use);  // put together the RGB channels, now transp is not transparent
	int sx = xPos;
	if(sx < 0)
	{
		sx = 0;
	}
	int sy = yPos;
	if(sy < 0)
	{
		sy = 0;
	}
	int ex = xPos + transp.cols;
	if(ex >= frame.cols)
	{
		ex = (frame.cols - 1);
	}
	int ey = yPos + transp.rows;
	if(ey >= frame.rows)
	{
		ey = (frame.rows - 1);
	}
	int tw = (ex - sx);
	int th = (ey - sy);
	if((tw > 0) && (th > 0)
	&& (tw == use.cols) && (th == use.rows))
	{
		cv::cvtColor(use, use, cv::COLOR_RGB2RGBA);
		use.copyTo(frame.rowRange(sy, ey).colRange(sx, ex), mask);
	}
}

void	adjust_mouse_image(XFixesCursorImage *img, unsigned char *out)
{
unsigned char r, g, b, a;
unsigned short row, col, pos;

	unsigned char *cp = out;
	pos = 0;
	for(row = 0;row < img->height;row++)
	{
		for(col = 0;col < img->width;col++)
		{
			a = (unsigned char)((img->pixels[pos] >> 24) & 0xff);  
			r = (unsigned char)((img->pixels[pos] >> 16) & 0xff);  
			g = (unsigned char)((img->pixels[pos] >>  8) & 0xff);
			b = (unsigned char)((img->pixels[pos] >>  0) & 0xff);
		
			*cp++ = r;
			*cp++ = g;
			*cp++ = b;
			*cp++ = a;

			pos++;
		}
	}
}

void	x11_get_mouse_image(Display *display, cv::Mat& use)
{
	XFixesCursorImage *cur = XFixesGetCursorImage(display);
	cv::Mat local_mat(cur->height, cur->width, CV_8UC4, (unsigned char *)cur->pixels);
	adjust_mouse_image(cur, local_mat.ptr());
	use = local_mat;
	XFree(cur);
}

void	x11_add_mouse(Display *display, cv::Mat& dest, int mouse_x, int mouse_y)
{
	cv::Mat mouse_mat;
	x11_get_mouse_image(display, mouse_mat);
	draw_transparency(dest, mouse_mat, mouse_x, mouse_y);
}

static char *x11_get_property(Display *disp, Window win, Atom xa_prop_type, char *prop_name, unsigned long *size)
{
Atom xa_prop_name;
Atom xa_ret_type;
int ret_format;
unsigned long ret_nitems;
unsigned long ret_bytes_after;
unsigned long tmp_size;
unsigned char *ret_prop;
char *ret;
	
	xa_prop_name = XInternAtom(disp, prop_name, False);
	
	// MAX_PROPERTY_VALUE_LEN / 4 explanation (XGetWindowProperty manpage):
	//
	// long_length = Specifies the length in 32-bit multiples of the
	//			   data to be retrieved.
	//
	if(XGetWindowProperty(disp, win, xa_prop_name, 0, MAX_PROPERTY_VALUE_LEN / 4, False, xa_prop_type, &xa_ret_type, &ret_format, &ret_nitems, &ret_bytes_after, &ret_prop) != Success)
	{
		return NULL;
	}
	if(xa_ret_type != xa_prop_type)
	{
		XFree(ret_prop);
		return NULL;
	}
	// null terminate the result to make string handling easier
	tmp_size = (ret_format / (32 / sizeof(long))) * ret_nitems;
	ret = (char *)malloc(tmp_size + 1);
	memcpy(ret, ret_prop, tmp_size);
	ret[tmp_size] = '\0';
	if(size)
	{
		*size = tmp_size;
	}
	XFree(ret_prop);
	return ret;
}

static char *x11_get_window_class(Display *disp, Window win)
{
char *class_utf8;
char *wm_class;
unsigned long size;

	wm_class = x11_get_property(disp, win, XA_STRING, "WM_CLASS", &size);
	if(wm_class)
	{
		char *p_0 = strchr(wm_class, '\0');
		if(wm_class + size - 1 > p_0)
		{
			*(p_0) = '.';
		}
		class_utf8 = strdup(wm_class);
	}
	else
	{
		class_utf8 = NULL;
	}
	free(wm_class);
	return class_utf8;
}

char *x11_get_window_title(Display *disp, Window win)
{
char *title_utf8;
char *wm_name;
char *net_wm_name;

	wm_name = x11_get_property(disp, win, XA_STRING, "WM_NAME", NULL);
	net_wm_name = x11_get_property(disp, win, XInternAtom(disp, "UTF8_STRING", False), "_NET_WM_NAME", NULL);
	if(net_wm_name)
	{
		title_utf8 = (char *)strdup((char *)net_wm_name);
		free(net_wm_name);
	}
	else
	{
		if(wm_name)
		{
			title_utf8 = (char *)strdup((char *)wm_name);
		}
		else
		{
			title_utf8 = NULL;
		}
		free(wm_name);
	}
	return(title_utf8);
}

static Window *x11_get_client_list(Display *disp, unsigned long *size)
{
	Window *client_list;

	if((client_list = (Window *)x11_get_property(disp, DefaultRootWindow(disp), XA_WINDOW, "_NET_CLIENT_LIST", size)) == NULL)
	{
		if((client_list = (Window *)x11_get_property(disp, DefaultRootWindow(disp), XA_CARDINAL, "_WIN_CLIENT_LIST", size)) == NULL)
		{
			fputs("Cannot get client list properties. \n" "(_NET_CLIENT_LIST or _WIN_CLIENT_LIST)" "\n", stderr);
			return NULL;
		}
	}
	return client_list;
}

int	x11_iterate_windows2(Display *disp, char **window_list)
{
Window *client_list;
unsigned long client_list_size;
int i;
int max_client_machine_len = 0;
	
	if((client_list = x11_get_client_list(disp, &client_list_size)) == NULL)
	{
		return EXIT_FAILURE;
	}
	// find the longest client_machine name
	for(i = 0; i < client_list_size / sizeof(Window); i++)
	{
		char *client_machine;
		if((client_machine = x11_get_property(disp, client_list[i], XA_STRING, "WM_CLIENT_MACHINE", NULL)))
		{
			max_client_machine_len = strlen((char *)client_machine);	
		}
		free(client_machine);
	}
	// print the list
	int cnt = 0;
	for(i = 0; i < client_list_size / sizeof(Window); i++)
	{
		char *title_utf8 = x11_get_window_title(disp, client_list[i]);
		char *title_out = strdup(title_utf8);
		char *client_machine;
		char *class_out = x11_get_window_class(disp, client_list[i]);
		unsigned long *pid;
		unsigned long *desktop;
		int x, y, junkx, junky;
		unsigned int wwidth, wheight, bw, depth;
		Window junkroot;

		// desktop ID
		if((desktop = (unsigned long *)x11_get_property(disp, client_list[i], XA_CARDINAL, "_NET_WM_DESKTOP", NULL)) == NULL)
		{
			desktop = (unsigned long *)x11_get_property(disp, client_list[i], XA_CARDINAL, "_WIN_WORKSPACE", NULL);
		}
		// client machine
		client_machine = x11_get_property(disp, client_list[i], XA_STRING, "WM_CLIENT_MACHINE", NULL);
	
		// pid
		pid = (unsigned long *)x11_get_property(disp, client_list[i], XA_CARDINAL, "_NET_WM_PID", NULL);

		// geometry
		XGetGeometry(disp, client_list[i], &junkroot, &junkx, &junky, &wwidth, &wheight, &bw, &depth);
		XTranslateCoordinates(disp, client_list[i], junkroot, junkx, junky, &x, &y, &junkroot);
	
		// special desktop ID -1 means "all desktops", so we have to convert the desktop value to signed long
		// printf("0x%.8lx %2ld", client_list[i], desktop ? (signed long)*desktop : 0);
		// printf(" %*s %s\n", max_client_machine_len, client_machine ? client_machine : "N/A", title_out ? title_out : "N/A");
		// printf("%d %d %d %d\n", x, y, wwidth, wheight);

		char buf[4096];
		sprintf(buf, "%lu::%s", client_list[i], title_out ? title_out : "N/A");
		window_list[cnt] = strdup(buf);
		cnt++;

		free(title_utf8);
		free(title_out);
		free(desktop);
		free(client_machine);
		free(class_out);
		free(pid);
	}
	free(client_list);
	return(cnt);
}

Window	find_requested_window(Display *display, char *requested_name)
{
char	*window_list[128];
int		loop;

	Window rr = 0;
	int cnt = x11_iterate_windows2(display, window_list);
	if(cnt > 0)
	{
		unsigned long int id = 0;
		int done = 0;
		for(loop = 0;((loop < cnt) && (done == 0));loop++)
		{
			unsigned long int id = (unsigned long int)atol(window_list[loop]);
			char *cp = window_list[loop];
			char *name = cp;
			while(*cp != '\0')
			{
				if(strncmp(cp, "::", 2) == 0)
				{
					name = cp + 2;
				}
				cp++;
			}
			if(strcasecmp(requested_name, name) == 0)
			{
				rr = id;
			}
		}
		for(loop = 0;loop < cnt;loop++)
		{
			free(window_list[loop]);
		}
	}
	return(rr);
}

void	window_names(Display *display)
{
char	*window_list[128];
int		loop;

	Window rr = 0;
	int cnt = x11_iterate_windows2(display, window_list);
	if(cnt > 0)
	{
		printf("\n");
		unsigned long int id = 0;
		int done = 0;
		for(loop = 0;loop < cnt;loop++)
		{
			unsigned long int id = (unsigned long int)atol(window_list[loop]);
			char *cp = window_list[loop];
			char *name = cp;
			while(*cp != '\0')
			{
				if(strncmp(cp, "::", 2) == 0)
				{
					name = cp + 2;
				}
				cp++;
			}
			printf("%d: %s\n", loop + 1, name);
		}
		for(loop = 0;loop < cnt;loop++)
		{
			free(window_list[loop]);
		}
	}
}

char	*select_window_by_name(Display *display)
{
char	*window_list[128];
char	*name[128];
int		loop;

	char *selected = NULL;
	Window rr = 0;
	int cnt = x11_iterate_windows2(display, window_list);
	if(cnt > 0)
	{
		printf("\n");
		unsigned long int id = 0;
		int done = 0;
		for(loop = 0;loop < cnt;loop++)
		{
			unsigned long int id = (unsigned long int)atol(window_list[loop]);
			char *cp = window_list[loop];
			name[loop] = cp;
			while(*cp != '\0')
			{
				if(strncmp(cp, "::", 2) == 0)
				{
					name[loop] = cp + 2;
				}
				cp++;
			}
			printf("%d: %s\n", loop + 1, name[loop]);
		}
		printf("Select 0 to cancel\n");
		printf("> ");
		char buf[256];
		fgets(buf, 256, stdin);
		int nn = atoi(buf);
		nn--;
		if((nn > -1) && (nn < cnt))
		{
			selected = strdup(name[nn]);
		}
		for(loop = 0;loop < cnt;loop++)
		{
			free(window_list[loop]);
		}
	}
	return(selected);
}

Window	find_new_window(Display *display)
{
char						*window_list[128];
int							loop;
int							inner;
static unsigned long int	old_list[128];
static int					old_cnt = 0;
unsigned long int			not_found[128];
int							not_found_cnt = 0;

	if(old_cnt == 0)
	{
		for(loop = 0;loop < 128;loop++)
		{
			old_list[loop] = 0;
		}
	}
	Window rr = 0;
	int cnt = x11_iterate_windows2(display, window_list);
	if(cnt > 0)
	{
		if(old_cnt > 0)
		{
			unsigned long int id = 0;
			for(loop = 0;loop < cnt;loop++)
			{
				unsigned long int id = (unsigned long int)atol(window_list[loop]);
				int found = 0;
				for(inner = 0;((inner < old_cnt) && (found == 0));inner++)
				{
					if(id == old_list[inner])
					{
						found = 1;
					}
				}
				if(found == 0)
				{
					not_found[not_found_cnt] = id;
					not_found_cnt++;
				}
			}
		}
		old_cnt = cnt;
		for(loop = 0;loop < cnt;loop++)
		{
			unsigned long int id = (unsigned long int)atol(window_list[loop]);
			old_list[loop] = id;
		}
		for(loop = 0;loop < cnt;loop++)
		{
			free(window_list[loop]);
		}
	}
	if(not_found_cnt > 0)
	{
		rr = (Window)not_found[0];
	}
	return(rr);
}

bool is_fullscreen(Display *display, Window window) 
{
	Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
	Atom wm_fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);

	Atom actual_type;
	int actual_format;
	unsigned long nitems;
	unsigned long bytes_after;
	unsigned char *prop_return = nullptr;

	int status = XGetWindowProperty(display, window, wm_state, 0, 1024, False,
								   AnyPropertyType, &actual_type, &actual_format,
								   &nitems, &bytes_after, &prop_return);

	if((status == Success) && (prop_return != nullptr))
	{
		for(unsigned long i = 0; i < nitems; ++i) 
		{
			if(((Atom*)prop_return)[i] == wm_fullscreen) 
			{
				XFree(prop_return);
				return(true);
			}
		}
		XFree(prop_return);
	}
	return(false);
}

Window	find_fullscreen(Display *display, int instance)
{
int		x11_iterate_windows2(Display *disp, char **window_list);
char	*window_list[128];
int		loop;

	Window rr = 0;
	int cnt = x11_iterate_windows2(display, window_list);
	if(cnt > 0)
	{
		int f_cnt = 0;
		bool got_it = false;
		for(loop = 0;((loop < cnt) && (rr == 0));loop++)
		{
			unsigned long int id = (unsigned long int)atol(window_list[loop]);
			if(id > 0)
			{
				got_it = is_fullscreen(display, id);
				if(got_it == true)
				{
					f_cnt++;
					if((f_cnt == instance) || (instance == 0))
					{
						rr = id;
					}
				}
			}
		}
		for(loop = 0;loop < cnt;loop++)
		{
			free(window_list[loop]);
		}
	}
	return(rr);
}

class	FakeWindow : public Fl_Double_Window
{
public:
		FakeWindow(char *raw);
		~FakeWindow();
	void	draw();
	int	handle(int event);

	char	*raw_data;
	int	start_x;
	int	start_y;
	int	end_x;
	int	end_y;
	int	final_sx;
	int	final_sy;
	int	final_ww;
	int	final_hh;
	int	dragging;
	int cancel;
};

FakeWindow::FakeWindow(char *raw) : Fl_Double_Window(0, 0, Fl::w(), Fl::h())
{
	border(0);
	fullscreen();
	set_modal();
	raw_data = raw;
	start_x = -1;
	start_y = -1;
	end_x = -1;
	end_y = -1;
	final_sx = -1;
	final_sy = -1;
	final_ww = -1;
	final_hh = -1;
	dragging = 0;
	cancel = 0;
	end();
	show();
}

FakeWindow::~FakeWindow()
{
}

void	FakeWindow::draw()
{
	Fl_Window::draw();
	if(raw_data != NULL)
	{
		fl_draw_image((unsigned char *)raw_data, 0, 0, w(), h(), 3);
		if((start_x > -1)
		&& (start_y > -1)
		&& (end_x > -1)
		&& (end_y > -1))
		{
			int sx = 0;
			int sy = 0;
			int ww = Fl::w();
			int hh = Fl::h();
			if(start_x < end_x)
			{
				ww = end_x - start_x;
				sx = start_x;
			}
			else
			{
				ww = start_x - end_x;
				sx = end_x;
			}
			if(start_y < end_y)
			{
				hh = end_y - start_y;
				sy = start_y;
			}
			else
			{
				hh = start_y - end_y;
				sy = end_y;
			}
			fl_color(FL_YELLOW);
			fl_rect(sx, sy, ww, hh);
			final_sx = sx;
			final_sy = sy;
			final_ww = ww;
			final_hh = hh;
		}
	}
}

int	FakeWindow::handle(int event)
{
	int flag = 0;
	if(event == FL_PUSH)
	{
		if(Fl::event_button() == 1)
		{
			start_x = Fl::event_x();
			start_y = Fl::event_y();
			end_x = start_x;
			end_y = start_y;
			redraw();
			dragging = 1;
		}
		else if(Fl::event_button() == 3)
		{
			if((final_sx > -1)
			&& (final_sy > -1)
			&& (final_ww > -1)
			&& (final_hh > -1))
			{
				hide();
			}
			dragging = 0;
		}
		flag = 1;
	}
	else if((event == FL_DRAG) || (event == FL_DRAG))
	{
		if(dragging == 1)
		{
			end_x = Fl::event_x();
			end_y = Fl::event_y();
			redraw();
			flag = 1;
		}
	}
	else if(event == FL_RELEASE)
	{
		end_x = Fl::event_x();
		end_y = Fl::event_y();
		dragging = 0;
		redraw();
		flag = 1;
	}
	else if(event == FL_KEYBOARD)
	{
		int key = Fl::event_key();
		if(key == FL_Escape)
		{
			cancel = 1;
		}
	}
	if(flag == 0)
	{
		flag = Fl_Double_Window::handle(event);
	}
	return(flag);
}

unsigned char *grab_raw_desktop_image(int& ww, int& hh)
{
unsigned char	*fl_read_image(unsigned char *, int, int, int, int, int = 0);

	unsigned char *r_data = NULL;
	int start_x = 0;
	int start_y = 0;
	int start_w = Fl::w();
	int start_h = Fl::h();
	Window xwin = RootWindow(fl_display, fl_screen);

	int use_x = 0;
	int use_y = 0;
	int my_depth = 4;

	fl_window = xwin;
	unsigned char *b = fl_read_image((unsigned char *)NULL, use_x, use_y, start_w, start_h, 0);
	r_data = b;
	ww = start_w;
	hh = start_h;
	return(r_data);
}

Window	select_by_rectangle(Display *display, int& xx, int& yy, int& ww, int& hh)
{
	xx = 0;
	yy = 0;
	ww = 0;
	hh = 0;
	fprintf(stderr, "\nSelect a rectangle by dragging the mouse with the left mouse button pressed.\n");
	fprintf(stderr, "Click the right mouse button to confirm the selection.\n");
	fprintf(stderr, "Be aware, different codecs have different mininum dimensions.\n");
	fflush(stderr);
	usleep(100000);
	char *raw = (char *)grab_raw_desktop_image(ww, hh);
	if(raw != NULL)
	{
		FakeWindow *fake = new FakeWindow(raw);
		fake->show();
		while(fake->visible())
		{
			Fl::wait(1);
			Fl::flush();
		}
		if(fake->cancel == 0)
		{
			xx = fake->final_sx;
			yy = fake->final_sy;
			ww = fake->final_ww;
			hh = fake->final_hh;
	
			double s = Fl::screen_scale(0);
			xx *= s;
			yy *= s;
			ww *= s;
			hh *= s;
		}
		else
		{
			xx = 0;
			yy = 0;
			ww = 0;
			hh = 0;
		}
		Fl::delete_widget(fake);
	}
	return(0);
}

XImage	*x11_create_shared_image(Display *display, XShmSegmentInfo *shminfo, int in_ww, int in_hh)
{
	XImage *image = XShmCreateImage(display, DefaultVisual(display, 0), 24, ZPixmap, NULL, shminfo, in_ww, in_hh);
	shminfo->shmid = shmget(IPC_PRIVATE, image->bytes_per_line * image->height, IPC_CREAT | 0777);
	shminfo->shmaddr = image->data = (char *)shmat(shminfo->shmid, 0, 0);
	shminfo->readOnly = False;
	XShmAttach(display, shminfo);

	return(image);
}

void	x11_destroy_shared_image(Display *display, XImage *image, XShmSegmentInfo *shminfo)
{
	if(image != NULL)
	{
		XShmDetach(display, shminfo);
		XDestroyImage(image);
		shmdt(shminfo->shmaddr);
		shmctl(shminfo->shmid, IPC_RMID, 0);
	}
}

unsigned char *grab_raw_image(Display *display, Window win, int& ww, int& hh, int *sz, int *in_depth)
{
static XShmSegmentInfo	shminfo;
static XImage			*image = NULL;
static int				old_w = -1;
static int				old_h = -1;

	int outlen = 0;
	unsigned char *r_data = NULL;
	int n_ww, n_hh;

	Window xwin;
	int desktop = 0;
	if(win != None)
	{
		xwin = win;
	}
	else
	{
		xwin = DefaultRootWindow(display);
		desktop = 1;
	}
	XWindowAttributes attributes;
	XGetWindowAttributes(display, xwin, &attributes);
	int start_x = attributes.x;
	int start_y = attributes.y;
	int start_w = attributes.width;
	int start_h = attributes.height;
	XSetWindowAttributes setwinattr;
	setwinattr.backing_store = Always;
	XChangeWindowAttributes(display, xwin, CWBackingStore, &setwinattr);

	Screen *screen = DefaultScreenOfDisplay(display);
	int true_w = screen->width;
	int true_h = screen->height;
	int screen_number = XScreenNumberOfScreen(screen);

	int use_x = 0;
	int use_y = 0;
	int my_depth = 4;

	if((image != NULL) && ((old_w != start_w) || (old_h != start_h)))
	{
		x11_destroy_shared_image(display, image, &shminfo);
		image = NULL;
	}
	if(image == NULL)
	{
		image = x11_create_shared_image(display, &shminfo, start_w, start_h);
		old_w = start_w;
		old_h = start_h;
	}
	if(image != NULL)
	{
		XWindowAttributes win_info;
		XGetWindowAttributes(display, xwin, &win_info);
		Pixmap pixmap = None;
		if(((start_x + start_w) > true_w)
		|| ((start_y + start_h) > true_h)
		|| (start_x < 0)
		|| (start_y < 0))
		{
			XSetSubwindowMode(display, DefaultGC(display, screen_number), IncludeInferiors);
			pixmap = XCreatePixmap(display, xwin, start_w, start_h, win_info.depth);
			XCopyArea(display, xwin, pixmap, DefaultGC(display, screen_number), use_x, use_y, start_w, start_h, 0, 0);
		}
		int nn = 0;
		unsigned char *use_data = NULL;
		if(pixmap == None)
		{
			nn = (int)XShmGetImage(display, xwin, image, 0, 0, AllPlanes);
		}
		else
		{
			nn = (int)XShmGetImage(display, pixmap, image, start_x, start_y, AllPlanes);
			XFreePixmap(display, pixmap);
		}
		r_data = (unsigned char *)image->data;
		outlen = image->width * image->height * 4;
	}
	*sz = outlen;
	*in_depth = my_depth;
	ww = start_w;
	hh = start_h;
	return(r_data);
}

int custom_error_handler(Display *display, XErrorEvent *event) 
{
	if(event->error_code == BadWindow) 
	{
		global_window_error = 1;
	}
	return(0);
}

bool is_window_valid(Display *display, Window window) 
{
static int (*old_error_handler)(Display *, XErrorEvent *);

	global_window_error = 0;
	if(window != 0)
	{
		old_error_handler = XSetErrorHandler(custom_error_handler);
		
		// Perform a benign, synchronous request to trigger a BadWindow error if it exists.
		XWindowAttributes attributes;
		XGetWindowAttributes(display, window, &attributes);

		// Force synchronization to process any pending errors.
		XSync(display, False);

		// Restore the old error handler.
		XSetErrorHandler(old_error_handler);
	}
	int rr = 1;
	if(global_window_error != 0)
	{
		rr = 0;
	}
	return(rr);
}


Window	select_by_mouse(Display *display)
{
XEvent event;
	
	fprintf(stderr, "\nSelect the window with the left mouse button. Press the right mouse button to cancel.\n");
	Window rr = None;
	Window root_window = DefaultRootWindow(display);
	XGrabButton(display, AnyButton, AnyModifier, root_window, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
	int done = 0;
	while(done == 0) 
	{
		XNextEvent(display, &event);
		if(event.type == ButtonPress) 
		{
			if(event.xbutton.button == Button1)
			{
				rr = event.xbutton.subwindow;
				done = 1;
			}
			else if(event.xbutton.button == Button3)
			{
				done = 1;
			}
		}
	}
	XUngrabButton(display, Button1, AnyModifier, root_window);
	return(rr);
}

void	crop_mat(cv::Mat& mat, int xx, int yy, int ww, int hh)
{
	cv::Rect roi;
	roi.x = xx;
	roi.y = yy;
	roi.width = ww;
	roi.height = hh;
	mat = mat(roi).clone();
}

int record_audio_thread(int *flag)
{
int		loop;
int		inner;
	
	Output *output = (Output *)flag;
	pa_simple *s = NULL;
	pa_simple *mult[128];
	pa_sample_spec ss;
	int error;

	// Specify the sample format
	ss.format = PA_SAMPLE_S16NE;
	ss.channels = output->channels;
	ss.rate = output->frequency;

	int calc_sz = sizeof(SAMPLE) * FRAMES_PER_BUFFER * output->channels;
	SAMPLE *audio_buffer = (SAMPLE *)malloc(calc_sz);
	int *final_audio_buffer = (int *)malloc(sizeof(int) * FRAMES_PER_BUFFER * output->channels);
	if(audio_buffer != NULL)
	{
		// Create a new recording stream
		static const pa_buffer_attr ba = 
		{
			.maxlength = 8192,
				.tlength = 1,
				.prebuf = 1,
				.minreq = 1,
				.fragsize = 2048
		};
		if(output->audio_device_cnt > 0)
		{
			for(loop = 0;loop < output->audio_device_cnt;loop++)
			{
				mult[loop] = pa_simple_new(NULL,		// Use the default server
					"SimpleRecorder",				// Application name
					PA_STREAM_RECORD,				// Recording stream
					output->audio_device[loop],		// the device
					"Audio Capture",				// Stream description
					&ss,							// Sample format
					NULL,							// Use default channel map
					&ba,							// Use default buffering attributes
					&error);						// Error code
				if(mult[loop] == NULL) 
				{
					fprintf(stderr, "\nError: Unable to initialize audio device %s.\n", output->audio_device[loop]);
				}
			}
		}
		else
		{
			s = pa_simple_new(NULL,				// Use the default server
				"SimpleRecorder",				// Application name
				PA_STREAM_RECORD,				// Recording stream
				NULL,							// Use the default device
				"Audio Capture",				// Stream description
				&ss,							// Sample format
				NULL,							// Use default channel map
				&ba,							// Use default buffering attributes
				&error);						// Error code
			if(s == NULL) 
			{
				fprintf(stderr, "\nError: Unable to initialize default audio device.\n");
			}
		}
		int success = 1;
		while(output->stop_audio == 0) 
		{
			int nn = 0;
			if(output->audio_device_cnt > 0)
			{
				for(inner = 0;inner < (FRAMES_PER_BUFFER * output->channels);inner++)
				{
					final_audio_buffer[inner] = 0;
				}
				for(loop = 0;loop < output->audio_device_cnt;loop++)
				{
					if(mult[loop] != NULL)
					{
						nn = pa_simple_read(mult[loop], audio_buffer, calc_sz, &error);
					}
					else
					{
						memset(audio_buffer, 0, calc_sz);
						usleep(output->audio_delay);
					}
					for(inner = 0;inner < (FRAMES_PER_BUFFER * output->channels);inner++)
					{
						final_audio_buffer[inner] += (int)(((double)audio_buffer[inner] / (double)output->audio_device_cnt) * output->audio_volume[inner]);
					}
				}
				for(inner = 0;inner < (FRAMES_PER_BUFFER * output->channels);inner++)
				{
					audio_buffer[inner] = (SAMPLE)final_audio_buffer[inner];
				}
			}
			else
			{
				if(s != NULL)
				{
					nn = pa_simple_read(s, audio_buffer, calc_sz, &error);
				}
				else
				{
					memset(audio_buffer, 0, calc_sz);
					usleep(output->audio_delay);
				}
			}
			if(output->paused == 0)
			{
				if(nn >= 0)
				{
					if(output != NULL)
					{
						if((output->mode & MODE_OUT_NDI) != MODE_OUT_NDI)
						{
							if(output->muxer->frame_ptr != NULL)
							{
								output->muxer->Record(audio_buffer);
								if(success == 0)
								{
									output->start = precise_time();
								}
								success = 1;
							}
						}
						else
						{
							cv::Mat fake;
							output->StreamToNDI(SEND_NDI_AUDIO, "CowRecord", audio_buffer, NDI_SEND_VIDEO_FORMAT_UYVY, fake, 0, 0);
						}
					}
				}
			}
		}
		if(s != NULL)
		{
			pa_simple_free(s);
		}
		output->stop_audio = 0;
		free(audio_buffer);
		free(final_audio_buffer);
	}
	else
	{
		fprintf(stderr, "\nError: Unable to allocate audio buffer.\n");
	}
	return(0);
}

int	start_monitor(Output *out)
{
	out->monitor = new Monitor(out->width * out->monitor_ratio, out->height * out->monitor_ratio, out);
	out->monitor->show();
	out->force_wait = 1;
	return(0);
}

int	force_wait_thread(int *flag)
{
	Output *out = (Output *)flag;
	int do_it = 1;
	while(do_it == 1)
	{
		do_it = 0;
		Fl_Window *win = Fl::first_window();
		while(win != NULL)
		{
			if(win->visible())
			{
				do_it = 1;
			}
			win = Fl::next_window(win);
		}
		if(do_it == 1)
		{
			Fl::wait(0);
		}
	}
	if(out->monitor_flag == 2)
	{
		out->stopped = 1;
	}
	return(0);
}

int	start_source_monitor(VideoSource *vs)
{
	vs->monitor = new Monitor(vs->output_width * vs->output->monitor_ratio, vs->output_height * vs->output->monitor_ratio, vs);
	vs->monitor->show();
	vs->output->force_wait = 1;
	return(0);
}

int	read_keyboard(int *flag)
{
	Output *out = (Output *)flag;
	char *msg = out->msg;
	char buf[256];
	fprintf(stderr, "%s", msg);
	fgets(buf, 256, stdin);
	out->stopped = 1;
	return(0);
}

Monitor::Monitor(int ww, int hh, Output *in_out) : Fl_Double_Window(ww, hh, "Monitor")
{
	out = in_out;
	video_source = NULL;
}

Monitor::Monitor(int ww, int hh, VideoSource *in_vs) : Fl_Double_Window(ww, hh, "Source Monitor")
{
	video_source = in_vs;
	out = NULL;
}

Monitor::~Monitor()
{
}

void	Monitor::draw()
{
	pthread_mutex_lock(&protect_output);
	Fl_Double_Window::draw();
	cv::Mat use;
	if(out != NULL)
	{
		use = out->mat;
	}
	else if(video_source != NULL)
	{
		use = video_source->mat;
	}
	if((use.cols > 0) && (use.rows > 0))
	{
		if(out != NULL)
		{
			if(out->monitor_flag == 2)
			{
				cv::cvtColor(use, use, cv::COLOR_BGRA2RGB);
			}
			else
			{
				cv::cvtColor(use, use, cv::COLOR_BGRA2BGR);
			}
		}
		else
		{
			cv::cvtColor(use, use, cv::COLOR_BGRA2RGB);
		}
		cv::resize(use, use, cv::Size(w(), h()));
		fl_draw_image(use.ptr(), 0, 0, w(), h(), use.channels());
		if(out != NULL)
		{
			if(out->paused == 1)
			{
				fl_color(FL_YELLOW);
				fl_draw("Paused", 20, 0, w() - 20, 32, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
			}
		}
	}
	pthread_mutex_unlock(&protect_output);
}

int		Monitor::handle(int event)
{
	int	flag = 0;
	if(out != NULL)
	{
		if(event == FL_KEYBOARD)
		{
			int key = Fl::event_key();
			if(key == ' ')
			{
				if(out->paused == 0)
				{
					out->paused = 1;
				}
				else
				{
					out->paused = 0;
				}
				flag = 1;
			}
		}
	}
	if(flag == 0)
	{
		flag = Fl_Window::handle(event);
	}
	return(flag);
}

VideoSource::VideoSource(Display *in_display, Output *in_output, VideoSource *old)
{
	display = in_display;
	output = in_output;
	surface = NULL;
	context = NULL;
	valid = 1;
	ndi_recv = NULL;
	ndi_send = NULL;
	ndi_mode = 0;
	window = None;
	cap = NULL;
	input_mode = 0;
	output_mode = 0;
	input_width = 1280;
	input_height = 720;
	output_x = 0;
	output_y = 0;
	output_width = 0;
	output_height = 0;
	scale = 1.0;
	crop_x = -1;
	crop_y = -1;
	crop_w = -1;
	crop_h = -1;
	frame_thickness = 0;
	text = NULL;
	font = "Sans";
	font_sz = 24;
	font_bold = 0;
	font_italic = 0;

	image = NULL;
	old_w = -1;
	old_h = -1;
	no_alpha = 0;
	show_mouse = 0;
	request_mouse = 0;

	start_time = 0.0;
	stop_time = 10000000000000.0;

	initialized = 0;
	available_frames = 0;
	repeat = 0;

	background.red = 0;
	background.green = 0;
	background.blue = 0;
	background.alpha = 0;

	foreground.red = 255;
	foreground.green = 255;
	foreground.blue = 255;
	foreground.alpha = 255;

	if(old != NULL)
	{
		input_width = old->input_width;
		input_height = old->input_height;
		output_width = old->output_width;
		output_height = old->output_height;
		output_x = old->output_x;
		output_y = old->output_y;
		frame_thickness = old->frame_thickness;
		scale = old->scale;
		text = old->text;
		font = old->font;
		font_sz = old->font_sz;
		font_bold = old->font_bold;
		font_italic = old->font_italic;

		start_time = old->start_time;
		stop_time = old->stop_time;

		background.red = old->background.red;
		background.green = old->background.green;
		background.blue = old->background.blue;
		background.alpha = old->background.alpha;

		foreground.red = old->foreground.red;
		foreground.green = old->foreground.green;
		foreground.blue = old->foreground.blue;
		foreground.alpha = old->foreground.alpha;
	}
	output->Add(this);
}

VideoSource::~VideoSource()
{
}

int		VideoSource::Source(Window in_win)
{
	mode = MODE_IN_WINDOW;
	int rr = WindowSource(in_win);
	initialized = 1;
	return(rr);
}

int		VideoSource::Source(char *in_path, int in_instance)
{
	int rr = 0;
	path = in_path;
	if(path != NULL)
	{
		initialized = 1;
		if(strncmp(path, "launch://", strlen("launch://")) == 0)
		{
			mode = MODE_IN_WINDOW;
			char *cp = path + strlen("launch://");
			find_new_window(display);

			char buffer[4100];
			strcpy(buffer, cp);
			if(strlen(buffer) > 0)
			{
				strcat(buffer, " &");
				system(buffer);
				sleep(1);
			}
			rr = WindowSource((char *)NULL, in_instance);
		}
		else if(strncmp(path, "http://", strlen("http://")) == 0)
		{
			mode = MODE_IN_CAMERA;
			rr = CameraSource(path);
		}
		else if(strncmp(path, "https://", strlen("https://")) == 0)
		{
			mode = MODE_IN_CAMERA;
			rr = CameraSource(path);
		}
		else if(strncmp(path, "ndi://", strlen("ndi://")) == 0)
		{
			mode = MODE_IN_NDI;
			rr = CameraSource(path);
		}
		else if(strncmp(path, "image://", strlen("image://")) == 0)
		{
			mode = MODE_IN_IMAGE;
			char *cp = path + strlen("image://");
			rr = ImageSource(cp);
		}
		else if(strncmp(path, "background://", strlen("background://")) == 0)
		{
			mode = MODE_IN_BACKGROUND;
			char *cp = path + strlen("background://");
			rr = BackgroundSource(cp);
		}
		else if(strncmp(path, "text://", strlen("text://")) == 0)
		{
			mode = MODE_IN_TEXT;
			char *cp = path + strlen("text://");
			rr = TextSource(cp);
		}
		else if(strncmp(path, "timestamp://", strlen("timestamp://")) == 0)
		{
			mode = MODE_IN_TIMESTAMP;
			char *cp = path + strlen("timestamp://");
			rr = TextSource(cp);
		}
		else if(strncmp(path, "camera://", strlen("camera://")) == 0)
		{
			mode = MODE_IN_CAMERA;
			char *cp = path + strlen("camera://");
			rr = CameraSource(cp);
		}
		else if(strncmp(path, "fullscreen://", strlen("fullscreen://")) == 0)
		{
			mode = MODE_IN_WINDOW;
			char *cp = path + strlen("fullscreen://");
			int instance = atoi(cp);
			rr = FullScreenSource(instance);
		}
		else if(strncmp(path, "desktop://", strlen("desktop://")) == 0)
		{
			mode = MODE_IN_WINDOW;
			rr = DesktopSource();
		}
		else if(strncmp(path, "mouse://", strlen("mouse://")) == 0)
		{
			Window win = select_by_mouse(display);
			if(win != None)
			{
				mode = MODE_IN_WINDOW;
				rr = Source(win);
			}
			else
			{
				fprintf(stderr, "\nCanceled.\n");
				rr = -1;
			}
		}
		else if(strncmp(path, "rectangle://", strlen("rectangle://")) == 0)
		{
			int xx = 0;
			int yy = 0;
			int ww = 0;
			int hh = 0;
			Window win = select_by_rectangle(display, xx, yy, ww, hh);
			if((ww > 0) && (hh > 0))
			{
				crop_x = xx;
				crop_y = yy;
				crop_w = ww;
				crop_h = hh;
				mode = MODE_IN_WINDOW;
				rr = DesktopSource();
			}
			else
			{
				fprintf(stderr, "Error: selected area is too small.\n");
				rr = -1;
			}
		}
		else
		{
			mode = MODE_IN_WINDOW;
			char *cp = path;
			rr = WindowSource(cp, in_instance);
		}
	}
	else
	{
		rr = WindowSource((char *)NULL, in_instance);
	}
	return(rr);
}

int		VideoSource::WindowSource(Window in_win)
{
	int rr = 0;
	if(in_win != 0)
	{
		window = in_win;
		XWindowAttributes attributes;
		XGetWindowAttributes(display, in_win, &attributes);
		if(global_window_error == 0)
		{
			input_width = attributes.width;
			input_height = attributes.height;
			rr = 1;
		}
	}
	return(rr);
}

int		VideoSource::WindowSource(char *window_name, int in_instance)
{
	int rr = 0;
	path = window_name;
	Window win = FindWindow(window_name, in_instance);
	if(win != 0)
	{
		window = win;
		XWindowAttributes attributes;
		XGetWindowAttributes(display, win, &attributes);
		if(global_window_error == 0)
		{
			input_width = attributes.width;
			input_height = attributes.height;
			rr = 1;
		}
	}
	return(rr);
}

int		VideoSource::FullScreenSource(int instance)
{
	int rr = 0;
	Window win = FindFullScreen(instance);
	if(win != 0)
	{
		window = win;
		XWindowAttributes attributes;
		XGetWindowAttributes(display, win, &attributes);
		if(global_window_error == 0)
		{
			input_width = attributes.width;
			input_height = attributes.height;
			rr = 1;
		}
	}
	return(rr);
}

int		VideoSource::DesktopSource()
{
	int rr = 0;
	window = None;
	mode = MODE_IN_WINDOW;
	Window xwin = DefaultRootWindow(display);
	if(xwin != None)
	{
		XWindowAttributes attributes;
		XGetWindowAttributes(display, xwin, &attributes);
		if(global_window_error == 0)
		{
			input_width = attributes.width;
			input_height = attributes.height;
			no_alpha = 1;
			rr = 1;
		}
	}
	return(rr);
}

int		VideoSource::BackgroundSource(char *color)
{
	int r = 1;
	if(color != NULL)
	{
		mode = MODE_IN_BACKGROUND;
		int rr = background.red;
		int gg = background.green;
		int bb = background.blue;
		int aa = background.alpha;
		resolve_four_numbers(color, rr, gg, bb, aa);
		background.red = rr;
		background.green = gg;
		background.blue = bb;
		background.alpha = aa;
	}
	return(r);
}

int		VideoSource::TextSource(char *in_text)
{
	int r = 1;
	text = in_text;
	return(r);
}

int		VideoSource::ImageSource(char *path)
{
	int rr = 0;
	if(path != NULL)
	{
		if(strlen(path) > 0)
		{
			if(access(path, F_OK) == 0)
			{
				mode = MODE_IN_IMAGE;
				cv::Mat local = cv::imread(path, cv::IMREAD_COLOR);
				cv::cvtColor(local, image_mat, cv::COLOR_BGR2BGRA);
				if((image_mat.cols % 2) != 0)
				{
					cv::resize(image_mat, image_mat, cv::Size(image_mat.cols + 1, image_mat.rows));
				}
				if((image_mat.rows % 2) != 0)
				{
					cv::resize(image_mat, image_mat, cv::Size(image_mat.cols, image_mat.rows + 1));
				}
				input_width = (int)image_mat.cols;
				input_height = (int)image_mat.rows;
				output_width = (int)image_mat.cols;
				output_height = (int)image_mat.rows;
				rr = 1;
			}
		}
	}
	return(rr);
}

int		VideoSource::CameraSource(char *path)
{
	int rr = 0;
	if(path != NULL)
	{
		if(strlen(path) > 0)
		{
			rr = OpenCamera(path, NULL, 0, 0);
		}
	}
	if(rr == 0)
	{
		fprintf(stderr, "\nError: Camera cannot be opened.\n");
	}
	return(rr);
}

int	VideoSource::OpenCamera(char *camera, char *format_code, int force_width, int force_height)
{
	int rr = 1;
	int use_ndi = resolve_ndi(camera);
	if(use_ndi == 1)
	{
		int go = 0;
		int go_cnt = 0;
		while((go == 0) && (go_cnt < 100))
		{
			cv::Mat hot_mat;
			int ndi_w = 1280;
			int ndi_h = 720;
			RecvNDI(hot_mat, ndi_w, ndi_h);
			if(hot_mat.ptr())
			{
				cv::cvtColor(hot_mat, hot_mat, cv::COLOR_BGR2RGBA);
				int ww = hot_mat.cols;
				int hh = hot_mat.rows;
				input_width = (int)ww;
				input_height = (int)hh;
				output_width = (int)ww;
				output_height = (int)hh;
				if((ww > 10) && (hh > 10))
				{
					go = 1;
				}
			}
			go_cnt++;
		}
		if(go == 0)
		{
			fprintf(stderr, "\nError: NDI did not receive a legal image after 100 attempts.\n\n");
			rr = 0;
		}
		else
		{
			mode = MODE_IN_NDI;
		}
	}
	else
	{
		int flag = cv::CAP_ANY;
		std::vector<int> params = {cv::CAP_PROP_HW_ACCELERATION, cv::VIDEO_ACCELERATION_ANY};
		if(strlen(camera) > 2)
		{
			std::string camera_path;
			camera_path = camera;
			cap = new cv::VideoCapture(camera_path, flag, params);
		}
		else
		{
			int nn = atoi(camera);
			cap = new cv::VideoCapture(nn, flag, params);
		}
		if(cap != NULL)
		{
			if(format_code != NULL)
			{
				if(strlen(format_code) > 0)
				{
					cap->set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc(format_code[0], format_code[1], format_code[2], format_code[3]));
				}
			}
			if(force_width > 0)
			{
				cap->set(cv::CAP_PROP_FRAME_WIDTH, force_width);
			}
			if(force_height > 0)
			{
				cap->set(cv::CAP_PROP_FRAME_HEIGHT, force_height);
			}
			available_frames = cap->get(cv::CAP_PROP_FRAME_COUNT);
			double frame_width = cap->get(cv::CAP_PROP_FRAME_WIDTH);
			double frame_height = cap->get(cv::CAP_PROP_FRAME_HEIGHT);
			input_width = (int)frame_width;
			input_height = (int)frame_height;
			output_width = (int)frame_width;
			output_height = (int)frame_height;
			mode = MODE_IN_CAMERA;
		}
		else
		{
			fprintf(stderr, "\nError: Unable to open a video capture from %s.\n\n", camera);
			rr = 0;
		}
	}
	return(rr);
}

void	VideoSource::FormatCode(char *format_code)
{
	if(format_code != NULL)
	{
		if(strlen(format_code) > 0)
		{
			cap->set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc(format_code[0], format_code[1], format_code[2], format_code[3]));
		}
	}
}

void	VideoSource::Scale(double in_scale)
{
	scale = in_scale;
}

int	VideoSource::OpenWindow(Window win)
{
	XWindowAttributes attributes;
	XGetWindowAttributes(display, win, &attributes);
	input_width = attributes.width;
	input_height = attributes.height;
	output_width = input_width;
	output_height = input_height;
	return(1);
}

Window	VideoSource::FindWindow(char *requested_window, int in_instance)
{
int	loop;

	int cnt = 0;
	Window win = 0;
	if(requested_window != NULL)
	{
		if(strlen(requested_window) < 1)
		{
			requested_window = NULL;
		}
	}
	output->msg = "Press [ENTER] to cancel.\n";
	create_task(read_keyboard, output);
	if(in_instance < 1) in_instance = 1;
	for(loop = 0;loop < in_instance;loop++)
	{
		win = 0;
		while((win == 0) && (output->stopped == 0))
		{
			if(requested_window == NULL)
			{
				win = find_new_window(display);
			}
			else
			{
				win = find_requested_window(display, requested_window);
			}
			if(win == 0)
			{
				cnt++;
				fprintf(stderr, "Waiting % 3d\r", cnt); 
				sleep(1);
			}
			else
			{
				mode = MODE_IN_WINDOW;
			}
		}
	}
	return(win);
}

Window	VideoSource::FindFullScreen(int instance)
{
	int cnt = 0;
	Window win = 0;
	output->msg = "Press [ENTER] to cancel.\n";
	create_task(read_keyboard, output);
	while((win == 0) && (output->stopped == 0))
	{
		win = find_fullscreen(display, instance);
		if(win == 0)
		{
			cnt++;
			fprintf(stderr, "Waiting % 3d\r", cnt); 
			sleep(1);
		}
		if(win != 0)
		{
			mode = MODE_IN_FULLSCREEN;
		}
	}
	return(win);
}

void	VideoSource::SetOutputSize(int width, int height, double in_scale, int cx, int cy, int cw, int ch)
{
	if(width > 0)
	{
		output_width = width;
	}
	if(height > 0)
	{
		output_height = height;
	}
	if(scale > 0.0)
	{
		output_width = ((double)width * in_scale);
		output_height = ((double)height * in_scale);
		scale = 1.0;
	}
	if((cx >= 0) && (cy >= 0)
	&& (cw < output_width) && (ch < output_height))
	{
		crop_x = cx;
		crop_y = cy;
		crop_w = cw;
		crop_h = ch;
	}
}

void	VideoSource::Acquire(int is_base_image)
{
	if(valid == 1)
	{
		if(mode == MODE_IN_CAMERA)
		{
			if(cap->isOpened())
			{
				int nn = cap->grab();
				if(nn)
				{
					cap->retrieve(mat);
					cv::cvtColor(mat, mat, cv::COLOR_RGB2RGBA);
					if(available_frames > 0)
					{
						usleep(14000);
					}
					if(monitor != NULL)
					{
						monitor->redraw();
					}
				}
				else
				{
					int black_screen = 1;
					if(repeat == 1)
					{
						cap->set(cv::CAP_PROP_POS_FRAMES, 0);
						int nn = cap->grab();
						if(nn)
						{
							cap->retrieve(mat);
							cv::cvtColor(mat, mat, cv::COLOR_RGB2RGBA);
							black_screen = 0;
						}
					}
					if(black_screen == 1)
					{
						mat = cv::Mat(output_height, output_width, CV_8UC4, cv::Scalar(0, 0, 0, 0));
						fprintf(stderr, "\nError: Bad frame from camera.\n");
						valid = 0;
					}
				}
			}
			else
			{
				mat = cv::Mat(output_height, output_width, CV_8UC4, cv::Scalar(0, 0, 0, 0));
				fprintf(stderr, "\nError: Camera is not open.\n");
			}
		}
		else if(mode == MODE_IN_NDI)
		{
			RecvNDI(mat, input_width, input_height);
			if(mat.ptr() != NULL)
			{
				cv::cvtColor(mat, mat, cv::COLOR_BGR2BGRA);
			}
			else
			{
				mat = cv::Mat(output_height, output_width, CV_8UC4, cv::Scalar(0, 0, 0, 0));
				fprintf(stderr, "\nError: Bad frame from NDI.\n");
			}
		}
		else if(mode == MODE_IN_IMAGE)
		{
			mat = image_mat.clone();
			output_width = mat.cols;
			output_height = mat.rows;
		}
		else if(mode == MODE_IN_BACKGROUND)
		{
			int rr = background.red;
			int gg = background.green;
			int bb = background.blue;
			int aa = background.alpha;
			image_mat = cv::Mat(input_height, input_width, CV_8UC4, cv::Scalar(bb, gg, rr, 0));
			mat = image_mat.clone();
			output_width = mat.cols;
			output_height = mat.rows;
		}
		else if(mode == MODE_IN_TEXT)
		{
			int rr = background.red;
			int gg = background.green;
			int bb = background.blue;
			int aa = background.alpha;
			image_mat = cv::Mat(input_height, input_width, CV_8UC4, cv::Scalar(bb, gg, rr, aa));
			mat = image_mat.clone();
			DrawText(text);
			output_width = mat.cols;
			output_height = mat.rows;
		}
		else if(mode == MODE_IN_TIMESTAMP)
		{
			char buf[256];
			strcpy(buf, "");
			if(text != NULL)
			{
				if(strlen(text) > 0)
				{
					parse_timestamp_format(text, buf, output->frame_cnt);
				}
			}
			if(strlen(buf) < 1)
			{
				text = "%Y/%M/%D %h:%m:%s (%f)";
				parse_timestamp_format(text, buf, output->frame_cnt);
			}
			int rr = background.red;
			int gg = background.green;
			int bb = background.blue;
			int aa = background.alpha;
			image_mat = cv::Mat(input_height, input_width, CV_8UC4, cv::Scalar(bb, gg, rr, aa));
			mat = image_mat.clone();
			DrawText(buf);
			output_width = mat.cols;
			output_height = mat.rows;
		}
		else if((mode == MODE_IN_WINDOW) || (mode == MODE_IN_FULLSCREEN))
		{
			if(request_mouse == 1)
			{
				show_mouse = 1;
			}
			valid = is_window_valid(display, window);
			if(valid)
			{
				XSetErrorHandler(custom_error_handler);
				void *image_data = GrabRawImage();
				if(image_data != NULL)
				{
					int cv_flag = CV_8UC4;
					cv::Mat frame(cv::Size(input_width, input_height), cv_flag, image_data);
					if((no_alpha == 1) || (is_base_image == 1))
					{
						cv::cvtColor(frame, frame, cv::COLOR_RGBA2RGB);
						cv::cvtColor(frame, frame, cv::COLOR_RGB2RGBA);
					}
					mat = frame.clone();
					if((crop_w > 0) && (crop_h > 0))
					{
						crop_mat(mat, crop_x, crop_y, crop_w, crop_h);
					}
					if(show_mouse == 1)
					{
						int mx = 0;
						int my = 0;
						int buttons = 0;
						x11_mouse(display, window, mx, my, buttons);
						x11_add_mouse(display, mat, mx, my);
					}
					if((output_width > 0) && (output_height > 0))
					{
						if((output_width != mat.cols) || (output_height != mat.rows))
						{
							cv::resize(mat, mat, cv::Size(output_width, output_height));
						}
					}
					output_width = mat.cols;
					output_height = mat.rows;
				}
				else
				{
					if(global_window_error == 0)
					{
						fprintf(stderr, "\nError: No image data after grabbing from a window.\n");
					}
				}
			}
			else
			{
				fprintf(stderr, "\nWarning: Window has closed.\n");
				output->CheckStopped();
			}
		}
	}
	else
	{
		mat = cv::Mat(output_height, output_width, CV_8UC4, cv::Scalar(0, 0, 0, 0));
	}
	if(frame_thickness > 0)
	{
		if((mat.cols > 0) && (mat.rows > 0))
		{
			int nn = frame_thickness;
			cv::rectangle(mat, cv::Point(nn, nn), cv::Point(mat.cols - nn, mat.rows - nn), cv::Scalar(255, 255, 255), nn);
		}
	}
}

unsigned char *VideoSource::GrabRawImage()
{
	int outlen = 0;
	unsigned char *r_data = NULL;
	int n_ww, n_hh;
	Window xwin;
	int desktop = 0;
	if(window != None)
	{
		xwin = window;
	}
	else
	{
		xwin = DefaultRootWindow(display);
		desktop = 1;
	}
	XWindowAttributes attributes;
	XGetWindowAttributes(display, xwin, &attributes);
	if(global_window_error == 0)
	{
		int start_x = attributes.x;
		int start_y = attributes.y;
		int start_w = attributes.width;
		int start_h = attributes.height;
		XSetWindowAttributes setwinattr;
		setwinattr.backing_store = Always;
		XChangeWindowAttributes(display, xwin, CWBackingStore, &setwinattr);
		if(global_window_error == 0)
		{
			Screen *screen = DefaultScreenOfDisplay(display);
			int true_w = screen->width;
			int true_h = screen->height;
			int screen_number = XScreenNumberOfScreen(screen);

			int use_x = 0;
			int use_y = 0;
			int my_depth = 4;

			if((image != NULL) && ((old_w != start_w) || (old_h != start_h)))
			{
				x11_destroy_shared_image(display, image, &shminfo);
				image = NULL;
			}
			if(image == NULL)
			{
				image = x11_create_shared_image(display, &shminfo, start_w, start_h);
				old_w = start_w;
				old_h = start_h;
			}
			if(image != NULL)
			{
				if(desktop == 0)
				{
					XCompositeRedirectWindow(display, xwin, CompositeRedirectAutomatic);
					Pixmap pixmap = XCompositeNameWindowPixmap(display, xwin);
					XFlush(display);
					XShmGetImage(display, pixmap, image, 0, 0, AllPlanes);
	
					r_data = (unsigned char *)image->data;
					outlen = image->width * image->height * 4;
	
					XFreePixmap(display, pixmap);
					XCompositeUnredirectWindow(display, xwin, CompositeRedirectAutomatic);
				}
				else
				{
					XShmGetImage(display, xwin, image, start_x, start_y, AllPlanes);
					r_data = (unsigned char *)image->data;
					outlen = image->width * image->height * 4;
				}
			}
			input_width = start_w;
			input_height = start_h;

		}
	}
	return(r_data);
}

Output::Output(char *in_path, int ww, int hh, double in_fps)
{
int	loop;

	stopped = 0;
	paused = 0;
	force_wait = 0;
	width = 0;
	height = 0;
	scale = 1.0;
	mode = 0;
	strcpy(path, "");
	muxer = NULL;
	video_source_cnt = 0;
	frequency = 44100.0;
	channels = 2;
	audio_delay = 20000;
	for(loop = 0;loop < 128;loop++)
	{
		audio_device[loop] = NULL;
		audio_volume[loop] = 1.0;
	}
	audio_device_cnt = 0;
	container = NULL;
	video_codec = (AVCodecID)0;
	audio_codec = (AVCodecID)0;
	fps = in_fps;
	crop_x = -1;
	crop_y = -1;
	crop_w = -1;
	crop_h = -1;
	start = 0;
	monitor_ratio = 0.25;
	frame_cnt = 0;
	recorded_time = 0.0;
	monitor_flag = 0;
	monitor = NULL;
	stop_audio = 0;
	msg = "";
	ndi_send = NULL;
	ndi_initialized = 0;
	ndi_video_frame = NULL;
	ndi_audio_frame = NULL;
	snapshot_filename = "snapshot";
	snapshot_interval = 30.0;
	snapshot_time = 0.0;

	dest_surface = NULL;
	dest_context = NULL;

	for(loop = 0;loop < 128;loop++)
	{
		video_source[loop] = NULL;
	}
	if(strlen(in_path) > 0)
	{
		if(strncasecmp(in_path, "ndi://", strlen("ndi://")) == 0)
		{
			char *cp = in_path + strlen("ndi://");
			mode |= MODE_OUT_NDI;
			strcpy(path, cp);
		}
		else if(strncasecmp(in_path, "rtmp://", strlen("rtmp://")) == 0)
		{
			strcpy(path, in_path);
			mode |= MODE_OUT_RTMP;
		}
		else if(strncasecmp(in_path, "rtmps://", strlen("rtmps://")) == 0)
		{
			strcpy(path, in_path);
			mode |= MODE_OUT_RTMP;
		}
		else
		{
			strcpy(path, in_path);
			mode |= MODE_OUT_FILE;
		}
		if((ww > 0) && (hh > 0))
		{
			width = ww;
			height = hh;
			mat = cv::Mat(height, width, CV_8UC4, cv::Scalar(0, 0, 0, 0));
		}
	}
}

Output::~Output()
{
int		loop;

	for(loop = 0;loop < video_source_cnt;loop++)
	{
		if(video_source[loop] != NULL)
		{
			delete video_source[loop];
			video_source[loop] = NULL;
		}
	}
	if(dest_context != NULL)
	{
		cairo_destroy(dest_context);
	}
	if(dest_surface != NULL)
	{
		cairo_surface_destroy(dest_surface);
	}
	if(muxer != NULL)
	{
		delete muxer;
	}
	for(loop = 0;loop < audio_device_cnt;loop++)
	{
		if(audio_device[loop] != NULL)
		{
			free(audio_device[loop]);
			audio_device[loop] = NULL;
		}
	}
}

void	Output::CheckStopped()
{
int	loop;

	int go_on = 0;
	for(loop = 0;loop < video_source_cnt;loop++)
	{
		if(video_source[loop] != NULL)
		{
			VideoSource *vs = video_source[loop];
			if(vs->valid == 1)
			{
				go_on = 1;
			}
		}
	}
	if(go_on == 0)
	{
		stopped = 1;
	}
}

void	Output::Record(int wait_on_keyboard, double wait_on_delay, double record_time)
{
	int rr = 0;
	if((mode & MODE_OUT_NDI) != MODE_OUT_NDI)
	{
		if(monitor_flag < 2)
		{
			rr = OpenMuxer();
		}
	}
	if(rr == 0)
	{
		if(wait_on_keyboard == 1)
		{
			char buf[256];
			fprintf(stderr, "\nPress ENTER to start recording");
			fgets(buf, 256, stdin);
			fprintf(stderr, "\n");
		}
		if(wait_on_delay > 0.0)
		{
			int delay_cnt = 0;
			time_t start = precise_time();
			while((precise_time() - start) < (1000000.0 * wait_on_delay))
			{
				delay_cnt++;
				if((delay_cnt % 100) == 0)
				{
					fprintf(stderr, "\rDelaying: %08.2f", (wait_on_delay - ((precise_time() - start) / 1000000.0)));
				}
				usleep(1000);
			}
		}
		stop_audio = 0;
		if(monitor_flag > 0)
		{
			start_monitor(this);
		}
		if(force_wait == 1)
		{
			create_task(force_wait_thread, this);
		}
		if(monitor_flag < 2)
		{
			msg = "Press [ENTER] to stop recording.\n";
			create_task(read_keyboard, this);
			create_task(record_audio_thread, this);
		}
		stopped = 0;
		double diff = 0.0;
		frame_cnt = 0;
		recorded_time = 0;
		fprintf(stderr, "\n");
		while(stopped == 0)
		{
			if(paused == 0)
			{
				start = precise_time();
				Run();
				frame_cnt++;
				diff = (precise_time() - start) / 1000000.0;
				if(record_time > 0.0)
				{
					if(diff >= record_time)
					{
						stopped = 1;
					}
				}
				recorded_time += diff;
				double fps = 0.0;
				if(recorded_time > 0.0)
				{
					fps = (double)frame_cnt / recorded_time;
				}
				if(monitor_flag < 2)
				{
					fprintf(stderr, "\rRecording: % 8.2f AFPS: % 4.2f (%d)", recorded_time, fps, frame_cnt);
				}
				else
				{
					fprintf(stderr, "\rMonitoring: % 8.2f AFPS: % 4.2f (%d)", recorded_time, fps, frame_cnt);
				}
			}
		}
		if(monitor_flag < 2)
		{
			if(diff > 0.0)
			{
				if(muxer != NULL)
				{
					fprintf(stderr, "\nDone recording %d of %d frames to %s in %.2f seconds. Recorded FPS: %f Acquired FPS: %f\n", 
						muxer->video_frames,
						frame_cnt, 
						path, 
						recorded_time, 
						muxer->video_frames / recorded_time,
						(double)frame_cnt / recorded_time);
				}
				else
				{
					fprintf(stderr, "\nDone recording %d frames to %s in %.2f seconds. Acquired FPS: %f\n", 
						frame_cnt, 
						path, 
						recorded_time, 
						(double)frame_cnt / recorded_time);
				}
			}
			else
			{
				fprintf(stderr, "\nError: Recording time is zero or less.\n");
			}
			if(muxer != NULL)
			{
				muxer->frame_ptr = NULL;
			}
			stop_audio = 1; 
			while(stop_audio == 1) 
			{
				usleep(10000);
			}
			if(muxer != NULL)
			{
				muxer->Finish();
			}
		}
		else
		{
			fprintf(stderr, "\n");
		}
	}
	else
	{
		fprintf(stderr, "\nError: Could not initialize the muxer.\n");
	}
}

void	Output::PasteWithAlpha(cv::Mat source, int xx, int yy)
{
	if(dest_surface != NULL)
	{
		if(dest_context != NULL)
		{
			int half_w = source.cols / 2;
			int half_h = source.rows / 2;
			cairo_save(dest_context);
			cairo_translate(dest_context, xx + half_w, yy + half_h);

			cairo_surface_t *source_surface = cairo_image_surface_create_for_data(source.ptr(), CAIRO_FORMAT_ARGB32, source.cols, source.rows, source.step);
			if(source_surface != NULL)
			{
				cairo_set_source_surface(dest_context, source_surface, -half_w, -half_h);
				cairo_paint(dest_context);
				cairo_surface_destroy(source_surface);
			}
			cairo_restore(dest_context);
		}
		else
		{
			fprintf(stderr, "Error: Destination context is NULL.\n");
		}
	}
	else
	{
		fprintf(stderr, "Error: Destination surface is NULL.\n");
	}
}


int		Output::OpenMuxer()
{
	int rr = 0;
	muxer = new Muxer();
	int ow = (int)((double)width * scale);
	int oh = (int)((double)height * scale);
	if((mode & MODE_OUT_FILE) == MODE_OUT_FILE)
	{
		if(strlen(path) > 0)
		{
			if(container == NULL)
			{
				container = find_extension(path);
				if(container == NULL)
				{
					fprintf(stderr, "\nError: No extension was found in the filename \"%s\".\nDefaulting to \"mp4\".\n\n", path);
					container = "mp4";
				}
			}
			rr = muxer->InitMux(container, video_codec, audio_codec, path, NULL, ow, oh, fps, frequency, channels);
		}
	}
	else if((mode & MODE_OUT_RTMP) == MODE_OUT_RTMP)
	{
		if(strlen(path) > 0)
		{
			if(container == NULL)
			{
				container = "flv";
			}
			rr = muxer->InitMux(container, video_codec, audio_codec, "out.flv", path, ow, oh, fps, frequency, channels);
		}
	}
	return(rr);
}

void	Output::Prep(VideoSource *vs, int base_image)
{
	cv::Mat local = vs->mat.clone();
	if((local.cols > 0) && (local.rows > 0))
	{
		if(base_image == 1)
		{
			int ow = (int)((double)vs->output_width * scale);
			int oh = (int)((double)vs->output_height * scale);
			if((ow > 0) && (oh > 0))
			{
				if((ow % 2) != 0)
				{
					ow++;
				}
				if((oh % 2) != 0)
				{
					oh++;
				}
				cv::resize(local, local, cv::Size(ow, oh));
				if((mat.cols == 0) || (mat.rows == 0))
				{
					mat = cv::Mat(local.rows, local.cols, CV_8UC4, cv::Scalar(0, 0, 0, 0));
				}
				cv::resize(mat, mat, cv::Size(ow, oh));
				width = ow;
				height = oh;

				cv::cvtColor(local, local, cv::COLOR_RGBA2RGB);
				cv::cvtColor(local, local, cv::COLOR_RGB2RGBA);

				local.copyTo(mat);
				dest_surface = cairo_image_surface_create_for_data(mat.ptr(), CAIRO_FORMAT_ARGB32, mat.cols, mat.rows, mat.step);
				dest_context = cairo_create(dest_surface);
			}
			else
			{
				fprintf(stderr, "\nError: Scaling produced a Mat with illegal dimensions.\n");
			}
		}
		else
		{
			int ow = (int)((double)vs->output_width * vs->scale);
			int oh = (int)((double)vs->output_height * vs->scale);
			if((ow > 0) && (oh > 0))
			{
				cv::resize(local, local, cv::Size(ow, oh));
				PasteWithAlpha(local, vs->output_x, vs->output_y);
			}
		}
	}
	else
	{
		fprintf(stderr, "\nError: Mat has illegal dimensions.\n");
	}
}

void	Output::Run()
{
int		loop;

	pthread_mutex_lock(&protect_output);
	for(loop = 0;loop < video_source_cnt;loop++)
	{
		if(video_source[loop] != NULL)
		{
			VideoSource *vs = video_source[loop];
			int go = 1;
			if((vs->start_time > -1) || (vs->stop_time > -1))
			{
				go = 0;
				if((recorded_time >= vs->start_time)
				&& (recorded_time <= vs->stop_time))
				{
					go = 1;
				}
			}
			if(go == 1)
			{
				int base_image = 0;
				if(loop == 0)
				{
					base_image = 1;
				}
				vs->Acquire(base_image);
				if(vs->mat.ptr() != NULL)
				{
					Prep(vs, base_image);
				}
				else
				{
					fprintf(stderr, "\nError: Mat is NULL after acquisition.\n");
				}
			}
		}
		else
		{
			fprintf(stderr, "\nError: Somehow a NULL video source was specified.\n");
		}
	}
	Fit();
	Send();
	pthread_mutex_unlock(&protect_output);
	if(monitor != NULL)
	{
		monitor->redraw();
	}
}

void	Output::Fit()
{
	if((width != mat.cols) || (height != mat.rows))
	{
		cv::resize(mat, mat, cv::Size(width, height));
	}
}

void	Output::Send()
{
	if(monitor_flag < 2)
	{
		if((mode & MODE_OUT_NDI) == MODE_OUT_NDI)
		{
			char *send_path = path;
			if(strlen(send_path) < 1)
			{
				send_path = "CowRecord";
			}
			StreamToNDI(SEND_NDI_VIDEO, send_path, NULL, NDI_SEND_VIDEO_FORMAT_UYVY, mat, width, height);
		}
		else if((mode & MODE_OUT_SNAPSHOT) == MODE_OUT_SNAPSHOT)
		{
			if(snapshot_filename != NULL)
			{
				if(recorded_time >= snapshot_time)
				{
					char buf[256];
					time_t now = time(0);
					struct tm *tm = localtime(&now);
					sprintf(buf, "%s_%04d_%02d_%02d_%02d_%02d_%02d_%06d.png", snapshot_filename, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, frame_cnt);

					std::string filename = buf;
					cv::imwrite(filename, mat);
					snapshot_time += snapshot_interval;
				}
			}
		}
		else
		{
			if(muxer != NULL)
			{
				cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
				cv::cvtColor(mat, output_mat, cv::COLOR_RGB2YUV_I420);
				muxer->frame_ptr = output_mat.ptr();
			}
			else
			{
				fprintf(stderr, "Error: Muxer is NULL.\n");
			}
		}
	}
}

void	Output::Add(VideoSource *vs)
{
	if(video_source_cnt < 128)
	{
		video_source[video_source_cnt] = vs;
		video_source_cnt++;
	}
	else
	{
		fprintf(stderr, "Error: Only 127 video sources may be added.\n");
	}
}

void	Output::SetSize(int ww, int hh)
{
	width = ww;
	height = hh;
}

void	print_help()
{
	printf("\ncow_record\n");
	printf("\tcow_record with no arguments waits for a new window to open and records that window.\n\n");

	printf("--source=[source]\n");
	printf("\tWhere [source] is the name of a window to be recorded, or:\n\n");

	printf("\t--source=launch://[command name]\n");
	printf("\t\tWhere [command name] is any program that will open a window.\n");
	printf("\t\tcow_record will then record the next window that opens, which likely to be that command's window.\n\n");

	printf("\t--source=http://[URL]\n");
	printf("\t--source=https://[URL]\n");
	printf("\t\tWhere [URL] indicates the location of a video file available on a website.\n\n");

	printf("\t--source=ndi://[NDI SPECIFIER]\n");
	printf("\t\tWhere [NDI SPECIFIER] is a valid NDI video stream available on your network.\n\n");

	printf("\t--source=camera://[PATH TO A V4L CAMERA]\n");
	printf("\t\tWhere [PATH TO A V4L CAMERA] is the file system path to camera device. PATH may also\n");
	printf("\t\tpoint to a video file, rather than a camera, in which case the video file will be played\n");
	printf("\t\tand recorded. This may be either a device path, video file, or the camera number.\n\n");

	printf("\t--source=desktop://\n");
	printf("\t\tRecords the entire desktop.\n\n");

	printf("\t--source=fullscreen://[n]\n");
	printf("\t\tcow_record waits for a fullscreen window to open. If [n] is greater than zero,\n");
	printf("\t\tcow_record waits until the [n] instance of a fullscreen window opens and records that window.\n\n");

	printf("\t--source=mouse://\n");
	printf("\t\tSelect a window to record by clicking on it with the mouse.\n\n");

	printf("\t--source=rectangle://\n");
	printf("\t\tSelect an area to record by rectangle selection (\"lasso\"). After drawing the rectangle,\n");
	printf("\t\tconfirm the selection by pressing the right mouse button.\n\n");

	printf("\t--source=text://[text]\n");
	printf("\t\tThe source is an ascii text that will appear at --x and --y. See the font and color modifiers.\n\n");

	printf("\t--source=timestamp://[format]\n");
	printf("\t\tPrint a timestamp onto the frame. The format codes are:\n");
	printf("\t\t\t%%Y -- a four digit year\n");
	printf("\t\t\t%%M -- a two digit month\n");
	printf("\t\t\t%%D -- a two digit day\n");
	printf("\t\t\t%%h -- a two digit hour\n");
	printf("\t\t\t%%m -- a two digit minute\n");
	printf("\t\t\t%%s -- a two digit second\n");
	printf("\t\t\t%%f -- the frame number\n");
	printf("\t\tThe format defaults to: \"%%Y/%%M/%%D %%h:%%m:%%s (%%f)\".\n\n");

	printf("\t--source=image://[image_file_path]\n");
	printf("\t\tThe source is an image file.\n\n");

	printf("\t--source=background://[r:g:b:a]\n");
	printf("\t\tThe source is a solid color. Each color component is specified in a range from 0 to 255.\n\n");

	printf("--pip=[source]\n");
	printf("\tUses the same syntax as --source, but places the recorded window on top of the window recorded\n");
	printf("\tby the --source command. All commands subsequent to a --pip command will operate on the source\n");
	printf("\tspecified in the last --pip command.\n\n");

	printf("--monitor\n");
	printf("\tProvides a window to monitor the output being recorded. It defaults to 1/4 scale.\n\n");

	printf("--monitor_source\n");
	printf("\tProvides a window to monitor the last video source specfifed. It defaults to 1/4 scale.\n\n");

	printf("--monitor_ratio=[n]\n");
	printf("\tThe ratio of the monitor to the actually output size, defaulting to 0.25\n");
	printf("\tThe output size is multiplied by [n] to determine the height and with of\n");
	printf("\tthe monitor window.\n\n");

	printf("--monitor_only\n");
	printf("\tOnly monitor the output in a window. Do not record.\n\n");

	printf("--instance=[n]\n");
	printf("\tWhen no source is specified, wait until the [n] instance of a new window is opened, and record\n");
	printf("\tthat window. This is used in the cases where a program opens a series of windows, possibly title windows,\n");
	printf("\tbefore the main window is opened.\n\n");

	printf("--repeat\n");
	printf("\tIf the source is --camera:// and the path points at a video file, repeat the playback of that\n");
	printf("\tfile when it reaches the end, as opposed to having the background go black if the primary source\n");
	printf("\tor transparent, if a --pip.\n\n");

	printf("--x=[n1] --y=[n1]\n");
	printf("\tThe pixel position of the --pip window on top of the main source.\n\n");

	printf("--width=[n1] --height=[n1]\n");
	printf("\tThe pixel width and height of either the main window or the last --pip source\n\n");

	printf("--audio_delay=[n]\n");
	printf("\tWhen no audio is being recorded, delay each frame by [n] microseconds to provide correct timing.\n");
	printf("\tThe default is 20,000 microseconds.\n\n");

	printf("--background=[r:g:b:a]\n");
	printf("\tSet the background color for the last source. Each color component is specified in a range from 0 to 255.\n");
	printf("\tThe default background color is 0:0:0:0.\n\n");

	printf("--foreground=[r:g:b:a]\n");
	printf("\tSet the foreground color for the last source. Each color component is specified in a range from 0 to 255.\n");
	printf("\tThe default background color is 255:255:255:255.\n\n");

	printf("--start=[time]\n");
	printf("\tThe last source specified will only appear after [time] is recorded. Time is specified in seconds or portions\n");
	printf("\thereof.\n\n");

	printf("--stop=[time]\n");
	printf("\tThe last source specified will cease appearing after [time] is recorded. Time is specified in seconds or portions\n");
	printf("\thereof.\n\n");

	printf("--wait=[n]\n");
	printf("\tWait [n] seconds before recording.\n\n");

	printf("--record_time=[n]\n");
	printf("\tRecord for [n] seconds.\n\n");

	printf("--wait_on_key\n");
	printf("\tOnly start recording when ENTER is pressed.\n\n");

	printf("--select_existing\n");
	printf("\tSelect a window to record from a list of current windows.\n\n");

	printf("--window_names\n");
	printf("\tPrint a list of currently open window names and then exit without recording.\n\n");

	printf("--launch=[PATH TO EXECUTABLE]\n");
	printf("\tLaunch a program. Unlike the --source=launch:// syntax, this does not necessarily\n");
	printf("\trecord the launched program's window.\n\n");

	printf("--format_code=[FOURCC]\n");
	printf("\tSpecify a preferred FOURCC code for your camera to use. This is only useful if your source is a camera.\n\n");

	printf("--scale=[n]\n");
	printf("\tThe scale of the incoming source or --pip.\n\n");

	printf("--fps=[n]\n");
	printf("\tThe frame rate for the produced video file or stream.\n\n");

	printf("--snapshot=[filename]\n");
	printf("\tAt the snapshot interval, save a png of the current frame. The snapshot filename is\n");
	printf("\tonly a prefix for the full filename, which also includes a timestamp. The default is\n");
	printf("\t\"snapshot\".\n\n");

	printf("--snapshot_interval=[n]\n");
	printf("\tTake a snapshot at every [n] seconds.\n\n");

	printf("--font=[font_name]\n");
	printf("\tA font used by the last \"--source=text://\" command. The default is \"Sans\".\n\n");

	printf("--font_size=[font_name]\n");
	printf("\tA font size used by the last \"--source=text://\" command. The default is 24.\n\n");

	printf("--bold\n");
	printf("\tCauses the font selected by \"--font=\" to be bold.\n\n");

	printf("--italic\n");
	printf("\tCauses the font selected by \"--font=\" to be italic.\n\n");

	printf("--record_mouse\n");
	printf("\tWhen recording, record the mouse pointer. This only applies to window sources, not cameras.\n\n");

	printf("--video_codec=[CODE]\n");
	printf("\tThe preferred video codec for the produced video file or stream.\n\n");

	printf("--audio_codec=[CODE]\n");
	printf("\tThe preferred audio codec for the produced video file or stream.\n\n");

	printf("--audio_device=[AUDIO DEVICE NAME]\n");
	printf("\tUse an alternative to the default audio input. This can be specified as an audio device name\n");
	printf("\tor the audio device index.\n\n");

	printf("--audio_device_list\n");
	printf("\tList the audio input devices along with their indexes, descriptions, channels, and rates.\n\n");

	printf("--volume=[n]\n");
	printf("\tSets the volume for the last audio device specified. [n] should be between 0.0 and 1.0.\n");
	printf("\tif specifying volume for more than one device, all should add up to no more than 1.0.\n\n");

	printf("--frequency=[n]\n");
	printf("\tUse an audio frequency other than 44100.\n\n");

	printf("--channels=[n]\n");
	printf("\tSet the number of audio channels. The default is 2.\n\n");

	printf("--output=[filename]\n");
	printf("\tThe preferred filename for the produced video file. The default is \"out.mp4\", or:\n\n");

	printf("\t--output=ndi://[STREAM_NAME]\n");
	printf("\t\tWill stream output to a NDI stream under the specified name.\n\n");

	printf("\t--output=rtmp://[URL] or --output=rtmps://[URL]\n");
	printf("\t\tWrite to a network streaming site like Twitch, YouTube, or Facebook.\n\n");

	printf("Note: The order of --source and --pip commands is important. The first specified, whether it is\n");
	printf("a --source or a --pip will always be the primary source, acting as the background and determining\n");
	printf("the height and width of the final video file or stream (unless otherwise specified). Commands that\n");
	printf("act on a video source, like --scale, will always act on the last video --source or --pip specified\n");
	printf("before the command in question. So, \"cow_record --source=camera:///dev/video0 --pip=calculator --scale=0.24\"\n");
	printf("will use the video coming in from video0 as the background. Screen capture from the window with the name\n");
	printf("\"calculator\" will be placed on top of the video at 0.25 scale. Closing all of the sources, will\n");
	printf("terminate recording. Closing a primary source will present a black background.\n\n");
}

int			main(int argc, char **argv)
{
int			loop;
int			outer;
double		use_fps = 60.0;
char		*use_filename = NULL;
double		use_delay_start = 0.0;
double		use_record_time = 0.0;
double		use_scale = 1.0;
int			use_keyboard = 0;
int			use_forced_width = 0;
int			use_forced_height = 0;
int			use_select_existing = 0;
int			use_crop_xx = 0;
int			use_crop_yy = 0;
int			use_crop_ww = 0;
int			use_crop_hh = 0;
int			instance = 0;
char		*camera = NULL;
char		*format_code = NULL;
char		*use_container = NULL;
AVCodecID	use_video_codec = (AVCodecID)0;
AVCodecID	use_audio_codec = (AVCodecID)0;

	Display *display = XOpenDisplay(NULL);
	if(display != NULL)
	{
		protect_output = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
		NDILib = NULL;
		hNDILib = NULL;
		DynamicallyLoadNDILibrary("libndi.so");
		double fps = 60.0;
		double wait = 0.0;
		double record_time = 0.0;
		int wait_on_key = 0;
		int width = 0;
		int height = 0;
		char *output_filename = "out.mp4";
		int rr = 0;
		if(argc > 1)
		{
			for(outer = 1;outer < argc;outer++)
			{
				if(strncmp(argv[outer], "--fps=", strlen("--fps=")) == 0)
				{
					char *cp = argv[outer] + strlen("--fps=");
					fps = atof(cp);
					if(fps <= 0.0)
					{
						fprintf(stderr, "\nError: FPS must be greater than 0.0\n");
						rr = -1;
					}
				}
				else if(strncmp(argv[outer], "--output=", strlen("--output=")) == 0)
				{
					char *cp = argv[outer] + strlen("--output=");
					output_filename = cp;
					if(strlen(output_filename) < 1)
					{
						fprintf(stderr, "\nWarning: a zero-length has been specified. It will default to \"out.mp4\".\n");
						output_filename = "out.mp4";
					}
				}
				else if(strncmp(argv[outer], "--width=", strlen("--width=")) == 0)
				{
					char *cp = argv[outer] + strlen("--width=");
					width = atoi(cp);
					if(width < 10)
					{
						fprintf(stderr, "\nError: Width must be equal to or greater than 10.\n");
						rr = -1;
					}
				}
				else if(strncmp(argv[outer], "--height=", strlen("--height=")) == 0)
				{
					char *cp = argv[outer] + strlen("--height=");
					height = atoi(cp);
					if(height < 10)
					{
						fprintf(stderr, "\nError: Height must be equal to or greater than 10.\n");
						rr = -1;
					}
				}
				else if(strncmp(argv[outer], "--window_names", strlen("--window_names")) == 0)
				{
					window_names(display);
					rr = -1;
				}
			}
		}
		Output *out = new Output(output_filename, width, height, fps);
		VideoSource *vs = new VideoSource(display, out);
		VideoSource *first_vs = vs;
		VideoSource *last_vs = vs;
		if(argc > 1)
		{
			for(outer = 1;outer < argc;outer++)
			{
				if(strncmp(argv[outer], "--source=", strlen("--source=")) == 0)
				{
					char *cp = argv[outer] + strlen("--source=");
					if(strlen(cp) < 1)
					{
						fprintf(stderr, "\nWarning: Because of a zero-length source, recording the next window to open.\n");
					}
					if(vs->initialized == 0)
					{
						rr = vs->Source(cp, 1);
					}
					else
					{
						vs = new VideoSource(display, out, vs);
						rr = vs->Source(cp, 1);
						last_vs = vs;
						fprintf(stderr, "\nWarning: Because a primary source was previously specified, \"%s\" is being used as a PIP video source.\n", cp);
					}
				}
				else if(strncmp(argv[outer], "--pip=", strlen("--pip=")) == 0)
				{
					char *cp = argv[outer] + strlen("--pip=");
					if(strlen(cp) > 0)
					{
						if(vs->initialized == 0)
						{
							rr = vs->Source(cp, 1);
							fprintf(stderr, "\nWarning: Because no previous source was specified, \"%s\" is being used as the primary video source.\n", cp);
						}
						else
						{
							vs = new VideoSource(display, out, vs);
							rr = vs->Source(cp, 1);
							last_vs = vs;
						}
					}
					else
					{
						fprintf(stderr, "\nError: A PIP source must be specified.\n");
						rr = -1;
					}
				}
				else if(strncmp(argv[outer], "--repeat", strlen("--repeat")) == 0)
				{
					vs->repeat = 1;
				}
				else if(strncmp(argv[outer], "--instance=", strlen("--instance=")) == 0)
				{
					char *cp = argv[outer] + strlen("--instance=");
					instance = atoi(cp);
				}
				else if(strncmp(argv[outer], "--monitor_ratio=", strlen("--monitor_ratio=")) == 0)
				{
					char *cp = argv[outer] + strlen("--monitor_ratio=");
					out->monitor_ratio = atof(cp);
					if(out->monitor_ratio <= 0.0)
					{
						out->monitor_ratio = 0.25;
					}
				}
				else if(strncmp(argv[outer], "--monitor_only", strlen("--monitor_only")) == 0)
				{
					out->monitor_flag = 2;
				}
				else if(strncmp(argv[outer], "--monitor_source", strlen("--monitor_source")) == 0)
				{
					vs->monitor_flag = 1;
					start_source_monitor(vs);
				}
				else if(strncmp(argv[outer], "--monitor", strlen("--monitor")) == 0)
				{
					out->monitor_flag = 1;
				}
				else if(strncmp(argv[outer], "--width=", strlen("--width=")) == 0)
				{
					char *cp = argv[outer] + strlen("--width=");
					width = atoi(cp);
					if(width > 0)
					{
						if(vs->cap != NULL)
						{
							vs->cap->set(cv::CAP_PROP_FRAME_WIDTH, width);
						}
						vs->output_width = width;
						if(vs == first_vs)
						{
							out->width = width;
						}
						if((vs->mode == MODE_IN_TEXT)
						|| (vs->mode == MODE_IN_TIMESTAMP)
						|| (vs->mode == MODE_IN_BACKGROUND))
						{
							vs->input_width = width;
						}
					}
					else
					{
						fprintf(stderr, "\nError: output width must be greater than zero.\n");
						rr = -1;
					}
				}
				else if(strncmp(argv[outer], "--height=", strlen("--height=")) == 0)
				{
					char *cp = argv[outer] + strlen("--height=");
					height = atoi(cp);
					if(height > 0)
					{
						if(vs->cap != NULL)
						{
							vs->cap->set(cv::CAP_PROP_FRAME_HEIGHT, height);
						}
						vs->output_height = height;
						if(vs == first_vs)
						{
							out->height = height;
						}
						if((vs->mode == MODE_IN_TEXT)
						|| (vs->mode == MODE_IN_TIMESTAMP)
						|| (vs->mode == MODE_IN_BACKGROUND))
						{
							vs->input_height = height;
						}
					}
					else
					{
						fprintf(stderr, "\nError: output height must be greater than zero.\n");
						rr = -1;
					}
				}
				else if(strncmp(argv[outer], "--snapshot=", strlen("--snapshot=")) == 0)
				{
					char *cp = argv[outer] + strlen("--snapshot=");
					out->snapshot_filename = cp;
					out->mode |= MODE_OUT_SNAPSHOT;
				}
				else if(strncmp(argv[outer], "--snapshot_interval=", strlen("--snapshot_interval=")) == 0)
				{
					char *cp = argv[outer] + strlen("--snapshot_interval=");
					out->snapshot_interval = atof(cp);
					out->mode |= MODE_OUT_SNAPSHOT;
				}
				else if(strncmp(argv[outer], "--audio_delay=", strlen("--audio_delay=")) == 0)
				{
					char *cp = argv[outer] + strlen("--audio_delay=");
					out->audio_delay = atoi(cp);
				}
				else if(strncmp(argv[outer], "--background=", strlen("--background=")) == 0)
				{
					char *cp = argv[outer] + strlen("--background=");
					int red = vs->background.red;
					int green = vs->background.green;
					int blue = vs->background.blue;
					int alpha = vs->background.alpha;
					resolve_four_numbers(cp, red, green, blue, alpha);
					vs->background.red = red;
					vs->background.green = green;
					vs->background.blue = blue;
					vs->background.alpha = alpha;
				}
				else if(strncmp(argv[outer], "--foreground=", strlen("--foreground=")) == 0)
				{
					char *cp = argv[outer] + strlen("--foreground=");
					int red = vs->foreground.red;
					int green = vs->foreground.green;
					int blue = vs->foreground.blue;
					int alpha = vs->foreground.alpha;
					resolve_four_numbers(cp, red, green, blue, alpha);
					vs->foreground.red = red;
					vs->foreground.green = green;
					vs->foreground.blue = blue;
					vs->foreground.alpha = alpha;
				}
				else if(strncmp(argv[outer], "--frame=", strlen("--frame=")) == 0)
				{
					char *cp = argv[outer] + strlen("--frame=");
					int thickness = atoi(cp);
					vs->frame_thickness = thickness;
				}
				else if(strncmp(argv[outer], "--wait=", strlen("--wait=")) == 0)
				{
					char *cp = argv[outer] + strlen("--wait=");
					wait = atof(cp);
					if(wait <= 0.0)
					{
						fprintf(stderr, "\nError: Wait time must be greater than zero.\n");
						rr = -1;
					}
				}
				else if(strncmp(argv[outer], "--record_time=", strlen("--record_time=")) == 0)
				{
					char *cp = argv[outer] + strlen("--record_time=");
					record_time = atof(cp);
					if(record_time <= 0.0)
					{
						fprintf(stderr, "\nError: Record time must be greater than zero.\n");
						rr = -1;
					}
				}
				else if(strncmp(argv[outer], "--wait_on_key", strlen("--wait_on_key")) == 0)
				{
					wait_on_key = 1;
				}
				else if(strncmp(argv[outer], "--select_existing", strlen("--select_existing")) == 0)
				{
					char *requested_window = select_window_by_name(display);
					rr = vs->Source(requested_window, 1);
				}
				else if(strncmp(argv[outer], "--format_code=", strlen("--format_code=")) == 0)
				{
					char *cp = argv[outer] + strlen("--format_code=");
					char *format_code = cp;
					if(last_vs->cap != NULL)
					{
						last_vs->FormatCode(format_code);
					}
				}
				else if(strncmp(argv[outer], "--scale=", strlen("--scale=")) == 0)
				{
					char *cp = argv[outer] + strlen("--scale=");
					double scale = atof(cp);
					if(scale > 0.0)
					{
						last_vs->Scale(scale);
						if(last_vs == first_vs)
						{
							out->scale = scale;
						}
					}
					else
					{
						fprintf(stderr, "\nError: Scale time must be greater than zero.\n");
						rr = -1;
					}
				}
				else if(strncmp(argv[outer], "--video_codec=", strlen("--video_codec=")) == 0)
				{
					char *cp = argv[outer] + strlen("--video_codec=");
					char *name = cp;
					AVCodecID id = codec_by_name(name);
					if(id == 0)
					{
						fprintf(stderr, "\nError: Video codec \"%s\" is not recognized.\n", name);
						rr = -1;
					}
					else
					{
						out->video_codec = id;
					}
				}
				else if(strncmp(argv[outer], "--audio_codec=", strlen("--audio_codec=")) == 0)
				{
					char *cp = argv[outer] + strlen("--audio_codec=");
					char *name = cp;
					AVCodecID id = codec_by_name(name);
					if(id == 0)
					{
						fprintf(stderr, "\nError: Audio codec \"%s\" is not recognized.\n", name);
						rr = -1;
					}
					else
					{
						out->audio_codec = id;
					}
				}
				else if(strncmp(argv[outer], "--x=", strlen("--x=")) == 0)
				{
					char *cp = argv[outer] + strlen("--x=");
					int xpos = atoi(cp);
					vs->output_x = xpos;
				}
				else if(strncmp(argv[outer], "--y=", strlen("--y=")) == 0)
				{
					char *cp = argv[outer] + strlen("--y=");
					int ypos = atoi(cp);
					vs->output_y = ypos;
				}
				else if(strncmp(argv[outer], "--font=", strlen("--font=")) == 0)
				{
					char *cp = argv[outer] + strlen("--font=");
					vs->font = cp;
				}
				else if(strncmp(argv[outer], "--bold", strlen("--bold")) == 0)
				{
					vs->font_bold = 1;
				}
				else if(strncmp(argv[outer], "--italic", strlen("--italic")) == 0)
				{
					vs->font_italic = 1;
				}
				else if(strncmp(argv[outer], "--font_size=", strlen("--font_size=")) == 0)
				{
					char *cp = argv[outer] + strlen("--font_size=");
					int sz = atoi(cp);
					if((sz > 0) && (sz < 256))
					{
						vs->font_sz = sz;
					}
					else
					{
						fprintf(stderr, "Error: font size must be larger than 0 and less than 256.\n");
					}
				}
				else if(strncmp(argv[outer], "--record_mouse", strlen("--record_mouse")) == 0)
				{
					vs->request_mouse = 1;
				}
				else if(strncmp(argv[outer], "--start=", strlen("--start=")) == 0)
				{
					char *cp = argv[outer] + strlen("--start=");
					int start = atof(cp);
					vs->start_time = start;
				}
				else if(strncmp(argv[outer], "--stop=", strlen("--stop=")) == 0)
				{
					char *cp = argv[outer] + strlen("--stop=");
					int stop = atof(cp);
					vs->stop_time = stop;
				}
				else if(strncmp(argv[outer], "--audio_device_list", strlen("--audio_device_list")) == 0)
				{
					scan_pulse(1);
					rr = -1;
				}
				else if(strncmp(argv[outer], "--audio_device=", strlen("--audio_device=")) == 0)
				{
					char *cp = argv[outer] + strlen("--audio_device=");
					if(out->audio_device_cnt < 128)
					{
						if(strlen(cp) < 3)
						{
							char buf[256];
							strcpy(buf, "");
							int nn = atoi(cp);
							scan_pulse_for_index(1, nn, buf);
							out->audio_device[out->audio_device_cnt] = strdup(buf);
						}
						else
						{
							out->audio_device[out->audio_device_cnt] = strdup(cp);
						}
						out->audio_device_cnt++;
					}
				}
				else if(strncmp(argv[outer], "--volume=", strlen("--volume=")) == 0)
				{
					char *cp = argv[outer] + strlen("--volume=");
					double vol = atof(cp);
					if(out->audio_device_cnt > 0)
					{
						out->audio_volume[out->audio_device_cnt - 1] = vol;
					}
				}
				else if(strncmp(argv[outer], "--frequency=", strlen("--frequency=")) == 0)
				{
					char *cp = argv[outer] + strlen("--frequency=");
					int freq = atoi(cp);
					out->frequency = freq;
				}
				else if(strncmp(argv[outer], "--channels=", strlen("--channels=")) == 0)
				{
					char *cp = argv[outer] + strlen("--channels=");
					int ch = atoi(cp);
					out->channels = ch;
				}
				else if(strncmp(argv[outer], "--launch=", strlen("--launch=")) == 0)
				{
					char *cp = argv[outer] + strlen("--launch=");
					char buffer[4100];
					strcpy(buffer, cp);
					if(strlen(buffer) > 0)
					{
						strcat(buffer, " &");
						system(buffer);
					}
				}
				else if(strncmp(argv[outer], "--help", strlen("--help")) == 0)
				{
					print_help();
					rr = -1;
				}
				else
				{
					fprintf(stderr, "Error: Unrecognized argument \"%s\".\n", argv[outer]);
				}
			}
			if((rr == 0) && (out->stopped == 0))
			{
				rr = vs->Source((char *)NULL, instance);
			}
		}
		else
		{
			rr = vs->Source((char *)NULL, instance);
		}
		if(rr == 1)
		{
			if((width == 0) || (height == 0))
			{
				if((first_vs->crop_w > 0) && (first_vs->crop_h > 0))
				{
					out->SetSize(first_vs->crop_w, first_vs->crop_h);
				}
				else
				{
					out->SetSize(first_vs->input_width, first_vs->input_height);
				}
			}
			else
			{
				out->SetSize(width, height);
			}
			out->Record(wait_on_key, wait, record_time);
		}
		delete out;
		XCloseDisplay(display);
	}
	return(0);
}
