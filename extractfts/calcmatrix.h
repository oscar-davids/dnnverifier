#ifndef _CALC_MATRIX_H
#define _CALC_MATRIX_H

#ifdef __cplusplus

#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/gpu/gpu.hpp"
//#include "opencv2/imgcodecs.hpp"
//#include "opencv2/core/utility.hpp"
//#include "opencv2/core/cuda.hpp"

#define  __OPENCV_

extern "C" {
#endif

#include <libavutil/motion_vector.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>

#ifdef __cplusplus
	}
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#ifndef _DEBUG
#define USE_MULTI_THREAD
#endif

//#define USE_OPENCV_GPU

#define _TEST_MODULE

#ifndef _TEST_MODULE
#include <Python.h>
#include "numpy/arrayobject.h"
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

#define MAX_NUM_RENDITIONS	11	//max count for rendition video(10) + master
#define MAX_NUM_THREADS		16	//multithread count at same time
#define MAX_NUM_SAMPLES		50	//sample count for compare


typedef enum errorType {
	LP_ERROR_NOFILE_NAME = -200,
	LP_ERROR_NOTFIND_DECODE,
	LP_ERROR_NOTFIND_CODEC,
	LP_ERROR_NULL_POINT = -100,
	LP_ERROR_INVALID_PARAM,
	LP_ERROR_MAX,
} errorType;

typedef enum featureType {
	LP_FT_DCT = 0,
	LP_FT_GAUSSIAN_MSE,
	LP_FT_GAUSSIAN_DIFF,	
	LP_FT_GAUSSIAN_TH_DIFF,
	LP_FT_HISTOGRAM_DISTANCE,
	LP_FT_FEATURE_MAX,
} featureType;


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
	int64_t				framecount;
	double				duration;
	double				fps;
	float				bitrate;
	//simple metadata for audio
	int					aliveaudio;
	int					channels;
	int					samplerate;
	//file info
	int					filesize;
	int					audiodiff;	

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
	//void				**listfrmame;
	AVPacket			**listaudio;
	double				*ftmatrix;

} LPDecContext;

typedef struct LPPair {
	LPDecContext*	master;
	LPDecContext*	rendition;
	int				frameid;
} LPPair;


int open_context(LPDecContext* lpcontext);
void close_context(LPDecContext* lpcontext);
int pre_verify(LPDecContext* pcontext, int vcount);
int grab_allframes(LPDecContext* pcontext, int ncount);
void remove_nullframe(LPDecContext* pcontext, int nvideonum);
int calc_featurematrix(LPDecContext* pctxmaster, LPDecContext* pctxrendition);
int aggregate_matrix(LPDecContext* pctxrendition);

#ifdef _DEBUG
void debug_saveimage(LPDecContext* lpcontext, int videonum);
void debug_printvframe(LPDecContext* lpcontext, int videonum);
void debug_printmatrix(LPDecContext* lpcontext, int videonum);
#endif

#endif //_CALC_MATRIX_H

