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
typedef enum returnType {
	LP_FAIL = -1,
	LP_OK,
} returnType;

typedef enum errorType {
	LP_ERROR_NOFILE_NAME = -200,
	LP_ERROR_NOTFIND_DECODE,
	LP_ERROR_NOTFIND_CODEC,
	LP_ERROR_NULL_POINT = -100,
	LP_ERROR_MAX,
} errorType;

// define structure
typedef struct LPDecContext {
	//Video decode
	AVFormatContext 	*input_ctx;
	int 				video_stream;
	AVCodecContext 		*decoder_ctx;
	AVCodec 			*decoder;
	AVStream			*video;
	struct SwsContext   *sws_rgb_scale;
	struct AVFrame		*readframe;
	struct AVFrame      *swscaleframe;
	int                 framenum;

	int					master;
	char				path[MAX_PATH];
	int					featurenum;
	int					*frameindex;
	float				*framepts;
	uint8_t				**frmamelist;
	uint8_t				**audiolist;
} LPDecContext;

int calc_featurediff(char* srcpath, char* renditions, char* featurelist, int samplenum);

#endif

