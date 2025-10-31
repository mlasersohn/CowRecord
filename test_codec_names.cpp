#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
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

#include	"muxer.h"

long int precise_time()
{
struct timeval tv;
	
	gettimeofday(&tv, NULL);
	long int ts = (tv.tv_sec * 1000000) + tv.tv_usec;
	return(ts);
}

void	strip_lf(char *cp)
{
	while(*cp != '\0')
	{
		if((*cp == 10) || (*cp == 13))
		{
			*cp = '\0';
		}
		cp++;
	}
}

int	main()
{
char	buf[256];

	while(fgets(buf, 255, stdin))
	{
		strip_lf(buf);
		int nn = codec_by_name(buf);
		if(nn != 0)
		{
			printf("% 20s\t%d\n", buf, nn);
		}
	}
}
