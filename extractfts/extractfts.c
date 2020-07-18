/*

 * THE SOFTWARE.
 */

#include <libavutil/motion_vector.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include "extractfts.h"

#ifndef _DEBUG
#include <Python.h>
#include "numpy/arrayobject.h"
#endif

//define part
#define FF_INPUT_BUFFER_PADDING_SIZE 32

#define GOTFM 1
#define GOTMB 2
#define GOTQP 4
#define GOTMV 8
#define GOTRD 16

#define NORMALW		64
#define NORMALH		64
#define HALFNORMALH 32
//typedef part
typedef unsigned char uint8_t;

typedef struct sizeInfo {
	int w;
	int h;
	int mw;
	int mh;
	int repeat;
} sizeInfo;

//variable part
static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL;
static AVStream *video_stream = NULL;
static const char *src_filename = NULL;

static int video_stream_idx = -1;
static AVFrame *frame = NULL;
static int video_frame_count = 0;

static const char *filename = NULL;
#ifndef _DEBUG
static PyObject *FeatureError;
#endif

//function part

static int open_codec_context(AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret;
    AVStream *st;
    AVCodecContext *dec_ctx = NULL;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, &dec, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename);
        return ret;
    } else {
        int stream_idx = ret;
        st = fmt_ctx->streams[stream_idx];

        dec_ctx = avcodec_alloc_context3(dec);
        if (!dec_ctx) {
            fprintf(stderr, "Failed to allocate codec\n");
            return AVERROR(EINVAL);
        }

        ret = avcodec_parameters_to_context(dec_ctx, st->codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters to codec context\n");
            return ret;
        }

        /* Init the video decoder */
        av_dict_set(&opts, "flags2", "+export_mvs", 0);
        if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }

        video_stream_idx = stream_idx;
        video_stream = fmt_ctx->streams[video_stream_idx];
        video_dec_ctx = dec_ctx;
    }

    return 0;
}


void backcount_frames(int* gop_count, int* frame_count) {

	AVCodec *pCodec;
	AVCodecContext *pCodecCtx = NULL;
	AVCodecParserContext *pCodecParserCtx = NULL;

	FILE *fp_in;
	const int in_buffer_size = 4096;

	uint8_t *in_buffer = (uint8_t*)malloc(in_buffer_size + FF_INPUT_BUFFER_PADDING_SIZE);
	memset(in_buffer + in_buffer_size, 0, FF_INPUT_BUFFER_PADDING_SIZE);

	uint8_t *cur_ptr;
	int cur_size;
	AVPacket packet;

	//avcodec_register_all();

	pCodec = avcodec_find_decoder(AV_CODEC_ID_MPEG4);
	// pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);  
	if (!pCodec) {
		printf("Codec not found\n");
		return -1;
	}
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (!pCodecCtx) {
		printf("Could not allocate video codec context\n");
		return -1;
	}

	pCodecParserCtx = av_parser_init(AV_CODEC_ID_MPEG4);
	// pCodecParserCtx=av_parser_init(AV_CODEC_ID_H264);  
	if (!pCodecParserCtx) {
		printf("Could not allocate video parser context\n");
		return -1;
	}

	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		printf("Could not open codec\n");
		return -1;
	}

	//Input File  
	fp_in = fopen(filename, "rb");
	if (!fp_in) {
		printf("Could not open input stream\n");
		return -1;
	}

	*gop_count = 0;
	*frame_count = 0;

	av_init_packet(&packet);

	while (1) {

		cur_size = fread(in_buffer, 1, in_buffer_size, fp_in);
		if (cur_size == 0)
			break;
		cur_ptr = in_buffer;

		while (cur_size > 0) {

			int len = av_parser_parse2(
				pCodecParserCtx, pCodecCtx,
				&packet.data, &packet.size,
				cur_ptr, cur_size,
				AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);

			cur_ptr += len;
			cur_size -= len;

			if (packet.size == 0)
				continue;
			if (pCodecParserCtx->pict_type == AV_PICTURE_TYPE_I) {
				++(*gop_count);
			}
			++(*frame_count);
		}
	}

	fclose(fp_in);
	av_parser_close(pCodecParserCtx);

	avcodec_close(pCodecCtx);
	av_free(pCodecCtx);

	return 0;
}

int count_frames(int* gop_count, int* frame_count)
{
	
	if (filename == NULL) return -1;
	AVFormatContext 	*ic = NULL;
	AVCodec 			*decoder = NULL;
	AVCodecContext 		*dx = NULL;
	AVStream			*video = NULL;
	int 				ret, video_stream;
	AVPacket			pkt = { 0 };
	ret = -1;

	*gop_count = 0;
	*frame_count = 0;
	if (avformat_open_input(&ic, filename, NULL, NULL) != 0) {
		fprintf(stderr, "Cannot open input file '%s'\n", filename);
		return ret;
	}

	if (avformat_find_stream_info(ic, NULL) < 0) {
		fprintf(stderr, "Cannot find input stream information.\n");
		return ret;
	}

	/* find the video stream information */
	ret = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
	if (ret < 0) {
		fprintf(stderr, "Cannot find a video stream in the input file\n");
		return ret;
	}

	video_stream = ret;
	video = ic->streams[video_stream];

	if (!(dx = avcodec_alloc_context3(decoder))) return AVERROR(ENOMEM);
	if (avcodec_parameters_to_context(dx, video->codecpar) < 0) return ret;

	if ((ret = avcodec_open2(dx, decoder, NULL)) < 0) {
		fprintf(stderr, "Failed to open codec for stream #%u\n", video_stream);
		return -1;
	}
	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate frame\n");
		ret = AVERROR(ENOMEM);
		return -1;
	}

	int cur_gop = -1;
	int cur_pos = 0;

	while (av_read_frame(ic, &pkt) >= 0) {

		if (pkt.stream_index == video_stream)
		{
			//ret = decode_packet(&pkt);
			ret = avcodec_send_packet(dx, &pkt);
			if (ret < 0) {
				fprintf(stderr, "Error while sending a packet to the decoder: %s\n", av_err2str(ret));
				return ret;
			}

			while (ret >= 0) {
				ret = avcodec_receive_frame(dx, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					ret = 0;
					break;
				}
				else if (ret < 0) {
					fprintf(stderr, "Error while receiving a frame from the decoder: %s\n", av_err2str(ret));
					return ret;
				}
				if (frame->pict_type == AV_PICTURE_TYPE_I) {
					++cur_gop;
				}
				cur_pos++;
				av_frame_unref(frame);
			}
		}
	}

	*gop_count = cur_gop + 1;
	*frame_count = cur_pos;
	
	/*
	float vinfofps = 1.0;
	if (video->r_frame_rate.den > 0.0) {
		vinfofps = av_q2d(video->r_frame_rate);
	}
	else {
		vinfofps = 1.0 / av_q2d(video->time_base);
	}

	int vinfowidth = dx->width;
	int vinfoheight = dx->height;

	double vinfoduration = (double)ic->duration / (double)AV_TIME_BASE;
	if (vinfoduration <= 0.000001) {
		vinfoduration = (double)video->duration * av_q2d(video->time_base);
	}

	int vinfoframecount = video->nb_frames;
	if (vinfoframecount == 0) {
		vinfoframecount = (int)(vinfoduration * vinfofps + 0.5);
	}
	*/

	avcodec_free_context(&dx);
	avformat_close_input(&ic);
	av_frame_free(&frame);

	return 0;
}
void create_and_load_bgr(AVFrame *pFrame, AVFrame *pFrameBGR, uint8_t *buffer,
	void ** arr, int cur_pos, int pos_target) {

	//int numBytes = avpicture_get_size(AV_PIX_FMT_BGR24, pFrame->width, pFrame->height);	
	int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, pFrame->width, pFrame->height, 1);

	buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

	AVFrame *pic = *(AVFrame**)&pFrameBGR;

	//err = avpicture_fill( (AVPicture*)pic, NULL, pixFmt, w, h );	
	av_image_fill_arrays(pic->data, pic->linesize, buffer, AV_PIX_FMT_BGR24, pFrame->width, pFrame->height, 1);


	struct SwsContext *img_convert_ctx;
	img_convert_ctx = sws_getCachedContext(NULL,
		pFrame->width, pFrame->height, AV_PIX_FMT_YUV420P,
		pFrame->width, pFrame->height, AV_PIX_FMT_BGR24,
		SWS_BICUBIC, NULL, NULL, NULL);

	sws_scale(img_convert_ctx,
		pFrame->data,
		pFrame->linesize, 0, pFrame->height,
		pFrameBGR->data,
		pFrameBGR->linesize);
	sws_freeContext(img_convert_ctx);

	int linesize = pFrame->width * 3;
	int height = pFrame->height;

	int stride_0 = height * linesize;
	int stride_1 = linesize;
	int stride_2 = 3;

	uint8_t *src = (uint8_t*)pFrameBGR->data[0];
	uint8_t *dest = (uint8_t*)(*arr);

	int array_idx = 0;
	/*
	if (cur_pos == pos_target) {
		array_idx = 1;
	}
	else {
		array_idx = 0;
	}
	*/
	memcpy(dest + array_idx * stride_0, src, height * linesize * sizeof(uint8_t));
	av_free(buffer);
}

void create_and_load_mv_residual(
	AVFrameSideData *sd,
	void* bgr_arr,
	void* mv_arr,
	void* res_arr,
	int cur_pos,
	int accumulate,
	int representation,
	int *accu_src,
	int *accu_src_old,
	int width,
	int height,
	int pos_target) {

	int p_dst_x, p_dst_y, p_src_x, p_src_y, val_x, val_y;
	const AVMotionVector *mvs = (const AVMotionVector *)sd->data;
	int *pdest = NULL;

	for (int i = 0; i < sd->size / sizeof(*mvs); i++) {
		const AVMotionVector *mv = &mvs[i];
		//assert(mv->source == -1);

		if (mv->dst_x - mv->src_x != 0 || mv->dst_y - mv->src_y != 0) {

			val_x = mv->dst_x - mv->src_x;
			val_y = mv->dst_y - mv->src_y;

			for (int x_start = (-1 * mv->w / 2); x_start < mv->w / 2; ++x_start) {
				for (int y_start = (-1 * mv->h / 2); y_start < mv->h / 2; ++y_start) {
					p_dst_x = mv->dst_x + x_start;
					p_dst_y = mv->dst_y + y_start;

					p_src_x = mv->src_x + x_start;
					p_src_y = mv->src_y + y_start;

					if (p_dst_y >= 0 && p_dst_y < height &&
						p_dst_x >= 0 && p_dst_x < width &&
						p_src_y >= 0 && p_src_y < height &&
						p_src_x >= 0 && p_src_x < width) {

						// Write MV. 
						if (accumulate && accu_src && accu_src_old) {
							for (int c = 0; c < 2; ++c) {
								accu_src[p_dst_y * width * 2 + p_dst_x * 2 + c]
									= accu_src_old[p_src_y * width * 2 + p_src_x * 2 + c];
							}
						}
						else {
							//*((int32_t*)PyArray_GETPTR3(mv_arr, p_dst_y, p_dst_x, 0)) = val_x;
							//*((int32_t*)PyArray_GETPTR3(mv_arr, p_dst_y, p_dst_x, 1)) = val_y;
						}
					}
				}
			}
		}
	}
	if (accumulate && accu_src && accu_src_old) {
		memcpy(accu_src_old, accu_src, width * height * 2 * sizeof(int));
	}
	if (cur_pos > 0) {
		if (accumulate) {
			if (representation == GOTMV && cur_pos == pos_target) {
				pdest = (int*)mv_arr;
				for (int y = 0; y < height; ++y) {
					for (int x = 0; x < width; ++x) {
						//*((int32_t*)PyArray_GETPTR3(mv_arr, y, x, 0))
						//	= x - accu_src[x * height * 2 + y * 2];
						//*((int32_t*)PyArray_GETPTR3(mv_arr, y, x, 1))
						//	= y - accu_src[x * height * 2 + y * 2 + 1];
						val_x = x - accu_src[y * width * 2 + x * 2];
						val_y = y - accu_src[y * width * 2 + x * 2 + 1];
						pdest[width * y + x] = (int)sqrt((val_x * val_x) + (val_y*val_y));
					}
				}
			}
		}
		if (representation == GOTRD && cur_pos == pos_target && bgr_arr && res_arr) {

			uint8_t *bgr_data = (uint8_t*)bgr_arr;
			int32_t *res_data = (int32_t*)res_arr;

			int stride_0 = height * width;
			int stride_1 = width * 3;
			int stride_2 = 3;

			int y;

			for (y = 0; y < height; ++y) {
				int c, x, src_x, src_y, location, location2, location_src;
				int32_t tmp;
				for (x = 0; x < width; ++x) {
					tmp = y * height * 2 + x * 2;
					if (accumulate && accu_src) {
						src_x = accu_src[tmp];
						src_y = accu_src[tmp + 1];
					}
					else {
						//src_x = x - (*((int32_t*)PyArray_GETPTR3(mv_arr, y, x, 0)));
						//src_y = y - (*((int32_t*)PyArray_GETPTR3(mv_arr, y, x, 1)));
					}
					location_src = src_y * stride_1 + src_x * stride_2;

					//location = y * stride_1 + x * stride_2;
					location = y * width + x; //gray scale
					location2 = y * stride_1 + x * stride_2;
					for (c = 0; c < 3; ++c) {						
						res_data[location] += (int32_t)bgr_data[location2 + c]
							- (int32_t)bgr_data[location_src + c];
						//location += 1;
					}
				}
			}
		}
	}
}

int calc_bitrate_qp1(float *fbitrate, float* fqp1)
{
	if (filename == NULL) return -1;
	AVFormatContext 	*ic = NULL;
	AVCodec 			*decoder = NULL;
	AVCodecContext 		*dx = NULL;
	AVStream			*video = NULL;
	int 				ret, video_stream;
	AVPacket			pkt = { 0 };
	ret = -1;

	*fbitrate = 0.0;
	*fqp1 = 0.0;

	if (avformat_open_input(&ic, filename, NULL, NULL) != 0) {
		fprintf(stderr, "Cannot open input file '%s'\n", filename);
		return ret;
	}

	if (avformat_find_stream_info(ic, NULL) < 0) {
		fprintf(stderr, "Cannot find input stream information.\n");
		return ret;
	}

	/* find the video stream information */
	ret = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
	if (ret < 0) {
		fprintf(stderr, "Cannot find a video stream in the input file\n");
		return ret;
	}

	video_stream = ret;
	video = ic->streams[video_stream];

	if (!(dx = avcodec_alloc_context3(decoder))) return AVERROR(ENOMEM);
	if (avcodec_parameters_to_context(dx, video->codecpar) < 0) return ret;

	if ((ret = avcodec_open2(dx, decoder, NULL)) < 0) {
		fprintf(stderr, "Failed to open codec for stream #%u\n", video_stream);
		return -1;
	}
	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate frame\n");
		ret = AVERROR(ENOMEM);
		return -1;
	}


	int cur_gop = -1;
	int cur_pos = 0;

	while (av_read_frame(ic, &pkt) >= 0) {

		if (pkt.stream_index == video_stream)
		{
			//ret = decode_packet(&pkt);
			ret = avcodec_send_packet(dx, &pkt);
			if (ret < 0) {
				fprintf(stderr, "Error while sending a packet to the decoder: %s\n", av_err2str(ret));
				return ret;
			}

			while (ret >= 0) {
				ret = avcodec_receive_frame(dx, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					ret = 0;
					break;
				}
				else if (ret < 0) {
					fprintf(stderr, "Error while receiving a frame from the decoder: %s\n", av_err2str(ret));
					return ret;
				}
				if (frame->pict_type == AV_PICTURE_TYPE_I) {
					++cur_gop;

					AVFrameSideData *sd;
					H264Context *hcontext;
					hcontext = (H264Context*)dx->priv_data;
					H264Picture *pic = hcontext->next_output_pic;

					if (ret >= 0 && hcontext->mb_width * hcontext->mb_height > 0) {
						int i;


						int h = frame->height;
						int w = frame->width;


						int nsum = 0;

						if (pic && pic->qscale_table)
						{
							for (int j = 0; j < hcontext->mb_height; j++) {
								int8_t* prow = pic->qscale_table + j * hcontext->mb_stride;

								for (int i = 0; i < hcontext->mb_width; i++) {
									nsum += prow[i];
								}
							}
						}

						*fqp1 = (float)nsum / (float)(hcontext->mb_width * hcontext->mb_height);
						break;
					}
				}

				cur_pos++;
				av_frame_unref(frame);
			}

			if (*fqp1 > 0.0) break;
		}
	}

	*fbitrate = dx->bit_rate / (float)1000.0;


	avcodec_free_context(&dx);
	avformat_close_input(&ic);
	av_frame_free(&frame);

	return 0;
}
static int decode_packet(const AVPacket *pkt)
{
	int ret = avcodec_send_packet(video_dec_ctx, pkt);
	if (ret < 0) {
		fprintf(stderr, "Error while sending a packet to the decoder: %s\n", av_err2str(ret));
		return ret;
	}

	while (ret >= 0) {
		ret = avcodec_receive_frame(video_dec_ctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;
		}
		else if (ret < 0) {
			fprintf(stderr, "Error while receiving a frame from the decoder: %s\n", av_err2str(ret));
			return ret;
		}

		if (ret >= 0) {
			int i;
			AVFrameSideData *sd;
			H264Context *hcontext;

			//get macroblock
			//get QP table
			hcontext = (H264Context*)video_dec_ctx->priv_data;

			video_frame_count++;
			sd = av_frame_get_side_data(frame, AV_FRAME_DATA_MOTION_VECTORS);
			if (sd) {
				const AVMotionVector *mvs = (const AVMotionVector *)sd->data;
				for (i = 0; i < sd->size / sizeof(*mvs); i++) {
					const AVMotionVector *mv = &mvs[i];
					printf("%d,%2d,%2d,%2d,%4d,%4d,%4d,%4d,0x%"PRIx64"\n",
						video_frame_count, mv->source,
						mv->w, mv->h, mv->src_x, mv->src_y,
						mv->dst_x, mv->dst_y, mv->flags);
				}
			}
			av_frame_unref(frame);
		}
	}

	return 0;
}

int decode_videowithffmpeg(
	const char* fname, int gop_target, int pos_target,
	void** bgr_arr,	void** mb_arr,	void** qp_arr,
	void** mv_arr,	void** res_arr,
	int representation,	int accumulate, sizeInfo *szInfo) {


	int ret = 0;
	AVPacket pkt = { 0 };
	int got_picture;
	int cur_pos = 0;
	AVFrame *pFrameBGR = NULL;
	int *accu_src = NULL;
	int *accu_src_old = NULL;
	int nsuccess = 0;

	int mb_stride, mb_sum, mb_type, mb_width, mb_height;

	if (avformat_open_input(&fmt_ctx, fname, NULL, NULL) < 0) {
		fprintf(stderr, "Could not open source file %s\n", fname);
		return -1;
	}

	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Could not find stream information\n");
		return -1;
	}

	open_codec_context(fmt_ctx, AVMEDIA_TYPE_VIDEO);

	av_dump_format(fmt_ctx, 0, fname, 0);
	if (!video_stream) {
		fprintf(stderr, "Could not find video stream in the input, aborting\n");
		ret = 1;
		goto end;
	}

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate frame\n");
		ret = AVERROR(ENOMEM);
		goto end;
	}

	pFrameBGR = av_frame_alloc();
	if (!pFrameBGR) {
		fprintf(stderr, "Could not allocate frame\n");
		ret = AVERROR(ENOMEM);
		goto end;
	}
		
	int cur_gop = -1;

	/* read frames from the file */
	while (av_read_frame(fmt_ctx, &pkt) >= 0 && cur_pos <= pos_target) {

		if (pkt.stream_index == video_stream_idx)
		{
			//ret = decode_packet(&pkt);
			ret = avcodec_send_packet(video_dec_ctx, &pkt);
			if (ret < 0) {
				fprintf(stderr, "Error while sending a packet to the decoder: %s\n", av_err2str(ret));
				return ret;
			}

			while (ret >= 0) {
				ret = avcodec_receive_frame(video_dec_ctx, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					ret = 0;
					break;
				}
				else if (ret < 0) {
					fprintf(stderr, "Error while receiving a frame from the decoder: %s\n", av_err2str(ret));
					return ret;
				}
				if (frame->pict_type == AV_PICTURE_TYPE_I) {
					++cur_gop;
				}

				if (/*cur_gop == gop_target &&*/ cur_pos <= pos_target)
				{
					if (ret >= 0) {
						int i;
						AVFrameSideData *sd;
						H264Context *hcontext;

						int h = frame->height;
						int w = frame->width;

						
						if ((representation & GOTMB) && !(*mb_arr)) { // get macroblock
							*mb_arr = malloc(w * h * sizeof(int));
							memset(*mb_arr, 0x00, w * h * sizeof(int));
						}

						if ((representation & GOTQP) && !(*qp_arr)) { // get qp
							*qp_arr = malloc(w * h * sizeof(int));
							memset(*qp_arr, 0x00, w * h * sizeof(int));
						}

						if ((representation & GOTMV) && !(*mv_arr)) { // get motion vector
							*mv_arr = malloc(w * h * 2 * sizeof(int));
							memset(*mv_arr, 0x00, w * h * 2 * sizeof(int));
						}

						if (representation == GOTRD && !(*res_arr)) { // get residual 
							*res_arr = malloc(w * h * 3 * sizeof(int));
							memset(*res_arr, 0x00, w * h * 3 * sizeof(int));
						}
						if ((representation & GOTMB) || (representation & GOTQP)) {
							mb_stride = w / 16 + 1;
							mb_sum = ((h + 15) >> 4)*(w / 16 + 1);
							//mb_type = (int *)pFrame->mb_type;
						}
						if ((representation == GOTMV || representation == GOTRD) && accumulate && !accu_src && !accu_src_old) {
							accu_src = (int*)malloc(w * h * 2 * sizeof(int));
							accu_src_old = (int*)malloc(w * h * 2 * sizeof(int));

							for (size_t x = 0; x < w; ++x) {
								for (size_t y = 0; y < h; ++y) {
									accu_src_old[ w * y * 2 + x * 2] = x;
									accu_src_old[ w * y * 2 + x * 2 + 1] = y;
								}
							}
							memcpy(accu_src, accu_src_old, h * w * 2 * sizeof(int));
						}
						// Initialize arrays. 
						if (((representation & GOTFM) || (representation & GOTRD)) && !(*bgr_arr)) { // get rgb
							*bgr_arr = malloc(w * h * 3 * sizeof(uint8_t));
							memset(*bgr_arr, 0x00, w * h * 3 * sizeof(uint8_t));
						}

						szInfo->w = w;
						szInfo->h = h;

						hcontext = (H264Context*)video_dec_ctx->priv_data;
#ifdef CALA_NOREF_PPSNR
						H264SliceContext* slice =  hcontext->slice_ctx;
#endif

						//get macroblock
						if (hcontext && (representation & GOTMB)) {
							hcontext = (H264Context*)video_dec_ctx->priv_data;
							H264Picture *pic = hcontext->next_output_pic;
							int *d_arr = (int *)*mb_arr;

							if (pic && d_arr)
							{
								for (int j = 0; j < hcontext->mb_height; j++) {
									for (int i = 0; i < hcontext->mb_width; i++) {
										int num = j * hcontext->mb_stride + i;
										int mbtype = pic->mb_type[num];

										if (mbtype & MB_TYPE_INTRA4x4) {
											d_arr[hcontext->mb_width*j + i] += 10;
										}
										if (mbtype&MB_TYPE_INTRA16x16) {
											d_arr[hcontext->mb_width*j + i] += 9;
										}
										if (mbtype&MB_TYPE_INTRA_PCM) {
											d_arr[hcontext->mb_width*j + i] += 8;
										}
										if (mbtype&MB_TYPE_16x16) {
											d_arr[hcontext->mb_width*j + i] += 7;
										}
										if (mbtype&MB_TYPE_16x8) {
											d_arr[hcontext->mb_width*j + i] += 6;
										}
										if (mbtype&MB_TYPE_8x16) {
											d_arr[hcontext->mb_width*j + i] += 5;
										}
										if (mbtype&MB_TYPE_8x8) {
											d_arr[hcontext->mb_width*j + i] += 4;
										}
										if (mbtype&MB_TYPE_SKIP) {
											d_arr[hcontext->mb_width*j + i] += 3;
										}
										if (mbtype&MB_TYPE_L0) {
											d_arr[hcontext->mb_width*j + i] += 2;
										}
										if (mbtype&MB_TYPE_L1) {
											d_arr[hcontext->mb_width*j + i] += 1;
										}
										///don't often used
										if (mbtype&MB_TYPE_INTERLACED) {
										}
										if (mbtype&MB_TYPE_DIRECT2) {
										}
										if (mbtype&MB_TYPE_ACPRED) {
										}
										if (mbtype&MB_TYPE_GMC) {
										}
										if (mbtype&MB_TYPE_QUANT) {
										}
										if (mbtype&MB_TYPE_CBP) {
										}
									}
								}

								szInfo->mw = hcontext->mb_width;
								szInfo->mh = hcontext->mb_height;
								nsuccess = 1;
							}
						}
						//get QP table
						if (hcontext && (representation & GOTQP)) {

							H264Picture *pic = hcontext->next_output_pic;
							int *d_arr = (int *)*qp_arr;

							if (pic && pic->qscale_table && d_arr)
							{
								for (int j = 0; j < hcontext->mb_height; j++) {
									for (int i = 0; i < hcontext->mb_width; i++) {
										int num = j * hcontext->mb_stride + i;
										d_arr[hcontext->mb_width*j + i] += pic->qscale_table[num];
									}
								}
							}

							szInfo->mw = hcontext->mb_width;
							szInfo->mh = hcontext->mb_height;

							nsuccess = 1;
						}


						if (cur_pos == pos_target && representation == GOTRD
							/*|| (cur_pos == pos_target - 1 && !accumulate && representation == GOTRD) || cur_pos == pos_target*/) 
						{
							create_and_load_bgr(frame, pFrameBGR, NULL, bgr_arr, cur_pos, pos_target);
						}
						if ((representation & GOTMV) || (representation & GOTRD)) {
							AVFrameSideData *sd;
							sd = av_frame_get_side_data(frame, AV_FRAME_DATA_MOTION_VECTORS);
							if (sd) {
								if (accumulate && cur_pos == pos_target) {
									create_and_load_mv_residual(
										sd,
										*bgr_arr, *mv_arr, *res_arr,
										cur_pos,
										accumulate,
										representation,
										accu_src,
										accu_src_old,
										w,
										h,
										pos_target);
								}
								nsuccess = 1;
							}
						}
						
						cur_pos++;
						av_frame_unref(frame);
					}
				}
				
			}
		}

		av_packet_unref(&pkt);
		if (ret < 0)
			break;
	}

	szInfo->repeat = nsuccess;

	/* flush cached frames */
	//decode_packet(NULL);
end:
	avcodec_free_context(&video_dec_ctx);
	avformat_close_input(&fmt_ctx);
	av_frame_free(&frame);
	av_frame_free(&pFrameBGR);

	if ((representation == GOTMV || representation == GOTRD) && accumulate) {
		if (accu_src) {
			free(accu_src);
		}
		if (accu_src_old) {
			free(accu_src_old);
		}
	}

	return 0;
	
}

#define getByte(value, n) (value >> (n*8) & 0xFF)

uint32_t getpixel(int *image, int w, unsigned int x, unsigned int y) {
	return image[(y*w) + x];
}
float lerp(float s, float e, float t) { return s + (e - s)*t; }
float blerp(float c00, float c10, float c01, float c11, float tx, float ty) {
	return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
}

void normalscale(int *src, int srcw, int srch, float *dst,  int dsw, int dsh, int fnum) {


	float scalew = (float)srcw / dsw;
	float scaleh = (float)srch / dsh;
	int x, y;
	//linear interpolation
	if (1)
	{
		for (y = 0; y < dsh; y++) {
			for (x = 0; x < dsw; x++)
			{

				int gyi = (int)(y * scaleh); if (gyi >= srch) gyi = srch - 1;
				int gxi = (int)(x * scalew); if (gxi >= srcw) gxi = srcw - 1;
				dst[(y*dsw) + x] = src[srcw *gyi + gxi];
			}
		}
	}
	else
	{
		//Bilinear interpolation	
		for (x = 0, y = 0; y < dsh; x++) {
			if (x > dsw) {
				x = 0; y++;
			}
			float gx = x / (float)(dsw) * (srcw - 1);
			float gy = y / (float)(dsh) * (srch - 1);
			int gxi = (int)gx;
			int gyi = (int)gy;
			uint32_t result = 0;
			uint32_t c00 = getpixel(src, srcw, gxi, gyi);
			uint32_t c10 = getpixel(src, srcw, gxi + 1, gyi);
			uint32_t c01 = getpixel(src, srcw, gxi, gyi + 1);
			uint32_t c11 = getpixel(src, srcw, gxi + 1, gyi + 1);
			dst[(y*dsw) + x] = blerp(c00, c10, c01, c11, gx - gxi, gy - gyi);
		}
	}	
}

#ifdef _DEBUG
#include "bmpio.h"
#endif // DEBUG

#ifdef _DEBUG
static int loadft(const char* fname, int gopidx, int framenum, int present)
#else
static PyObject *loadft(PyObject *self, PyObject *args)
#endif
{

	int ret = -1;
	int gop_target, pos_target, representation, accumulate;

#ifdef _DEBUG
	char sname[MAX_PATH] = { 0, };
	filename = fname;
	gop_target = gopidx;
	pos_target = framenum;
	representation = present;	
#else
	PyArrayObject *final_arr = NULL;	
	if (!PyArg_ParseTuple(args, "siii", &filename,
		&gop_target, &pos_target, &representation)) return NULL;
#endif
	accumulate = 1;

	uint8_t *bgr_arr = NULL;	
	int *mb_arr = NULL;
	int *qp_arr = NULL;
	int *mv_arr = NULL;
	int *res_arr = NULL;

	sizeInfo szInfo = { 0, };

	if (decode_videowithffmpeg(filename, gop_target, pos_target,
		&bgr_arr, &mb_arr, &qp_arr, &mv_arr, &res_arr,
		representation, accumulate, &szInfo) < 0) {

		printf("Decoding video failed.\n");
#ifdef _DEBUG
		return ret;
#else
		return final_arr;
#endif
	}

	//normalize buffer
	if (szInfo.repeat > 0)
	{
#ifdef _DEBUG
		switch (representation)
		{
		case GOTMB:
			sprintf(sname, "D:/debugmb_%03d.bmp", framenum);
			break;
		case GOTQP:
			sprintf(sname, "D:/debugqp_%03d.bmp", framenum);
			break;
		case GOTMV:
			sprintf(sname, "D:/debugmv_%03d.bmp", framenum);
			break;
		case GOTRD:
			sprintf(sname, "D:/debugrd_%03d.bmp", framenum);
			break;
		case GOTMB ^ GOTQP:
			sprintf(sname, "D:/debugmbqp_%03d.bmp", framenum);
			break;
		case GOTMV ^ GOTRD:
			sprintf(sname, "D:/debugmvrd_%03d.bmp", framenum);
			break;
		default:
			break;
		}
#endif
		float* fnormal = NULL;
		fnormal = (float*)malloc(NORMALW * NORMALH * sizeof(float));
		memset(fnormal, 0x00, NORMALW * NORMALH * sizeof(float));

		if (representation == GOTMB) {
			normalscale(mb_arr, szInfo.mw, szInfo.mh, fnormal, NORMALW, NORMALH, szInfo.repeat);
		}
		if (representation == GOTQP) {
			normalscale(qp_arr, szInfo.mw, szInfo.mh, fnormal, NORMALW, NORMALH, szInfo.repeat);
		}
		if (representation == GOTMV) {
			normalscale(mv_arr, szInfo.w, szInfo.h, fnormal, NORMALW, NORMALH, szInfo.repeat);
		}
		if (representation == GOTRD) {
			normalscale(res_arr, szInfo.w, szInfo.h, fnormal, NORMALW, NORMALH, szInfo.repeat);
		}

		if (representation == (GOTMB ^ GOTQP)) // 6
		{
			normalscale(mb_arr, szInfo.mw, szInfo.mh, fnormal, NORMALW, HALFNORMALH, szInfo.repeat);
			normalscale(qp_arr, szInfo.mw, szInfo.mh, fnormal + NORMALW * HALFNORMALH, NORMALW, HALFNORMALH, szInfo.repeat);
		}
		if (representation == (GOTMV ^ GOTRD)) // 24
		{
			normalscale(mv_arr, szInfo.w, szInfo.h, fnormal, NORMALW, HALFNORMALH, szInfo.repeat);
			normalscale(res_arr, szInfo.w, szInfo.h, fnormal + NORMALW * HALFNORMALH, NORMALW, HALFNORMALH, szInfo.repeat);
		}

#ifdef _DEBUG
		WriteFloatBmp(sname, NORMALW, NORMALH, fnormal);
#endif

#ifdef Py_PYTHON_H
		npy_intp dims[2];
		dims[0] = NORMALH * NORMALW;
		dims[1] = 1;		
		final_arr = PyArray_ZEROS(1, dims, NPY_FLOAT32, 0);

		int size = NORMALH * NORMALW  * sizeof(float);
		memcpy(final_arr->data, fnormal, size);	
		
#endif
		if (fnormal != NULL) free(fnormal);
		ret = 0;
	}
	

	if (bgr_arr != NULL) free(bgr_arr);	
	if (mb_arr != NULL) free(mb_arr);
	if (qp_arr != NULL) free(qp_arr);
	if (mv_arr != NULL) free(mv_arr);
	if (res_arr != NULL) free(res_arr);

#ifdef _DEBUG	
	return ret;
#else
	return final_arr;
#endif	
}

static float getbitrate(const char* fname)
{
	
	if (fname == 0) return 0.0;
	filename = fname;

	if (filename == NULL) return -1;
	AVFormatContext 	*ic = NULL;
	AVCodec 			*decoder = NULL;
	AVCodecContext 		*dx = NULL;
	AVStream			*video = NULL;
	int 				ret, video_stream;	
	ret = -1;

	if (avformat_open_input(&ic, filename, NULL, NULL) != 0) {
		fprintf(stderr, "Cannot open input file '%s'\n", filename);
		return ret;
	}

	if (avformat_find_stream_info(ic, NULL) < 0) {
		fprintf(stderr, "Cannot find input stream information.\n");
		return ret;
	}

	/* find the video stream information */
	ret = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
	if (ret < 0) {
		fprintf(stderr, "Cannot find a video stream in the input file\n");
		return ret;
	}

	video_stream = ret;
	video = ic->streams[video_stream];

	if (!(dx = avcodec_alloc_context3(decoder))) return AVERROR(ENOMEM);
	if (avcodec_parameters_to_context(dx, video->codecpar) < 0) return ret;

	if ((ret = avcodec_open2(dx, decoder, NULL)) < 0) {
		fprintf(stderr, "Failed to open codec for stream #%u\n", video_stream);
		return -1;
	}

	float fbitrate = dx->bit_rate / (float)1000.0;

	avcodec_free_context(&dx);
	avformat_close_input(&ic);	

	return fbitrate;
}

static float getqp1(const char* fname)
{
	int ret = -1;
	if (fname == 0) return 0.0;
	filename = fname;

	float bitrate, qp1;
	bitrate = 0.0;
	qp1 = 0.0;

	calc_bitrate_qp1(&bitrate, &qp1);

	return qp1;
}

#ifdef _DEBUG
static int get_bitrate_qp1(const char* fname)
#else
static PyObject *get_bitrate_qp1(PyObject *self, PyObject *args)
#endif
{

#ifdef _DEBUG
	int ret = -1;
	if (fname == 0) return ret;
	filename = fname;
#else
	PyArrayObject *final_arr = NULL;
	if (!PyArg_ParseTuple(args, "s", &filename)) return NULL;
#endif

	float bitrate, qp1;
	bitrate = 0.0;
	qp1 = 0.0;

	calc_bitrate_qp1(&bitrate, &qp1);
	
#ifdef Py_PYTHON_H
	npy_intp dims = 2;
	final_arr = PyArray_ZEROS(1, &dims, PyArray_FLOAT, 0);
	final_arr->data[0] = bitrate;
	final_arr->data[1] = qp1;
#endif

#ifdef _DEBUG
	ret = 0;
	return ret;
#else
	return final_arr;
#endif
}

#ifdef _DEBUG	
int main(int argc, char **argv)
{
    int ret = 0;
    AVPacket pkt = { 0 };

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <video>\n", argv[0]);
        exit(1);
    }
    src_filename = argv[1];

#ifdef _DEBUG
	
	int gop_count, frame_count;
	filename = src_filename;

	float bitrate, qp1;

	//calc_bitrate_qp1(&bitrate, &qp1);
	bitrate = getbitrate(src_filename);
	qp1 = getqp1(src_filename);

	printf("bitrate = %lf qp1 = %lf .\n", bitrate, qp1);

	count_frames(&gop_count, &frame_count);

	printf("gopnum = %d frame = %d .\n", gop_count, frame_count);

	//loadft(src_filename, 0, 8, 6);
	/*
	for (size_t i = 1; i < 240; i++)
	{
		loadft(src_filename, 0, i, 16);
	}
	*/

	
#else

#endif

	return 0;
}
#else

static PyObject *get_num_gops(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, "s", &filename)) return NULL;

	int gop_count, frame_count;
	count_frames(&gop_count, &frame_count);
	return Py_BuildValue("i", gop_count);
}


static PyObject *get_num_frames(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, "s", &filename)) return NULL;

	int gop_count, frame_count;
	count_frames(&gop_count, &frame_count);
	return Py_BuildValue("i", frame_count);
}

static PyObject *get_bitrate(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, "s", &filename)) return NULL;

	float fbitrate = 0.0;
	fbitrate = getbitrate(filename);	
	return Py_BuildValue("f", fbitrate);
}


static PyObject *get_qp1(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, "s", &filename)) return NULL;

	float fqpi = 0.0;
	fqpi = getqp1(filename);
	return Py_BuildValue("f", fqpi);
}


static PyMethodDef FeatureMethods[] = {
	{"loadft",  loadft, METH_VARARGS, "Load a frames feature."},
	{"get_bitrate",  get_bitrate, METH_VARARGS, "Getting bitrate in a video.."},
	{"get_qp1",  get_qp1, METH_VARARGS, "Getting qpI in a video.."},
	{"get_bitrate_qp1",  get_bitrate_qp1, METH_VARARGS, "Getting bitrate and qp1 in a video.."},	
	{"get_num_gops",  get_num_gops, METH_VARARGS, "Getting number of GOPs in a video."},
	{"get_num_frames",  get_num_frames, METH_VARARGS, "Getting number of frames in a video."},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};


static struct PyModuleDef extractfts = {
	PyModuleDef_HEAD_INIT,
	"extractfts",   /* name of module */
	NULL,       /* module documentation, may be NULL */
	-1,         /* size of per-interpreter state of the module,
				 or -1 if the module keeps state in global variables. */
	FeatureMethods
};


PyMODINIT_FUNC PyInit_extractfts(void)
{
	PyObject *m;

	m = PyModule_Create(&extractfts);
	if (m == NULL)
		return NULL;

	/* IMPORTANT: this must be called */
	import_array();

	FeatureError = PyErr_NewException("extractfts.error", NULL, NULL);
	Py_INCREF(FeatureError);
	PyModule_AddObject(m, "error", FeatureError);
	return m;
}


int main(int argc, char *argv[])
{
	av_log_set_level(AV_LOG_QUIET);

	wchar_t *program = Py_DecodeLocale(argv[0], NULL);
	if (program == NULL) {
		fprintf(stderr, "Fatal error: cannot decode argv[0]\n");
		exit(1);
	}

	/* Add a built-in module, before Py_Initialize */
	PyImport_AppendInittab("extractfts", PyInit_extractfts);

	/* Pass argv[0] to the Python interpreter */
	Py_SetProgramName(program);

	/* Initialize the Python interpreter.  Required. */
	Py_Initialize();

	PyMem_RawFree(program);
	return 0;
}

#endif
