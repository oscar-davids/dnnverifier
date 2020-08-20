#ifndef _CALC_MATRIX_H
#define _CALC_MATRIX_H

#include <libavutil/motion_vector.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>

#ifndef NULL
#define NULL 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 256
#endif


// define enum type
/*
typedef enum returnType {
	LP_FAIL = -1,
	LP_OK,
} returnType;
*/
#define LP_FAIL		-1
#define LP_OK		 0


typedef enum errorType {
	LP_ERROR_NOFILE_NAME = -200,
	LP_ERROR_NOTFIND_DECODE,
	LP_ERROR_NOTFIND_CODEC,
	LP_ERROR_NULL_POINT = -100,
	LP_ERROR_INVALID_PARAM,
	LP_ERROR_MAX,
} errorType;

// define structure
typedef struct LPDecContext {
	//Video decode
	AVFormatContext 	*input_ctx;
	//video
	int 				video_stream;
	AVCodec 			*vcodec;
	AVCodecContext 		*vcontext;	
	AVStream			*vstream;
	//audio
	int 				audio_stream;
	AVCodec 			*acodec;
	AVCodecContext 		*acontext;	
	AVStream			*astream;

	struct SwsContext   *sws_rgb_scale;
	struct AVFrame		*readframe;	
	int                 framenum;

	//simple metadata for video
	int					alivevideo;
	int					width;
	int					height;
	int					framecount;
	float				duration;
	float				fps;
	float				bitrate;
	//simple metadata for audio
	int					aliveaudio;
	int					channels;
	float				samplerate;

	//normalize width & height
	int					normalw;
	int					normalh;

	// for calculate compare matrix
	int					master;
	int					tamper;
	int					samplecount;
	char				path[MAX_PATH];
	int					featurenum;
	int					*frameindexs;
	double				*framestamps;
	double				*framediffps;
	double				*audiodiffps;

	AVFrame				**listfrmame;
	AVPacket			**listaudio;

} LPDecContext;

int calc_featurediff(char* srcpath, char* renditions, char* featurelist, int samplenum);

#endif

