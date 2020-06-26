/*

 * THE SOFTWARE.
 */

#include <libavutil/motion_vector.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>

//define part
#define FF_INPUT_BUFFER_PADDING_SIZE 32

#define GOTFM 1
#define GOTMB 2
#define GOTQP 4
#define GOTMV 8
#define GOTRD 16

//typedef part
typedef unsigned char uint8_t;


//variable part
static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL;
static AVStream *video_stream = NULL;
static const char *src_filename = NULL;

static int video_stream_idx = -1;
static AVFrame *frame = NULL;
static int video_frame_count = 0;

static const char *filename = NULL;

//function part
static int decode_packet(const AVPacket *pkt)
{
    int ret = avcodec_send_packet(video_dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error while sending a packet to the decoder: %s\n", av_err2str(ret));
        return ret;
    }

    while (ret >= 0)  {
        ret = avcodec_receive_frame(video_dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error while receiving a frame from the decoder: %s\n", av_err2str(ret));
            return ret;
        }

        if (ret >= 0) {
            int i;
            AVFrameSideData *sd;

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


void count_frames(int* gop_count, int* frame_count) {

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

	int array_idx;
	if (cur_pos == pos_target) {
		array_idx = 1;
	}
	else {
		array_idx = 0;
	}
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
						if (accumulate) {
							for (int c = 0; c < 2; ++c) {
								accu_src[p_dst_x * height * 2 + p_dst_y * 2 + c]
									= accu_src_old[p_src_x * height * 2 + p_src_y * 2 + c];
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
	if (accumulate) {
		memcpy(accu_src_old, accu_src, width * height * 2 * sizeof(int));
	}
	if (cur_pos > 0) {
		if (accumulate) {
			if (representation == GOTMV && cur_pos == pos_target) {
				for (int x = 0; x < width; ++x) {
					for (int y = 0; y < height; ++y) {
						//*((int32_t*)PyArray_GETPTR3(mv_arr, y, x, 0))
						//	= x - accu_src[x * height * 2 + y * 2];
						//*((int32_t*)PyArray_GETPTR3(mv_arr, y, x, 1))
						//	= y - accu_src[x * height * 2 + y * 2 + 1];
					}
				}
			}
		}
		if (representation == GOTRD && cur_pos == pos_target) {

			uint8_t *bgr_data = (uint8_t*)bgr_arr;
			int32_t *res_data = (int32_t*)res_arr;

			int stride_0 = height * width * 3;
			int stride_1 = width * 3;
			int stride_2 = 3;

			int y;

			for (y = 0; y < height; ++y) {
				int c, x, src_x, src_y, location, location2, location_src;
				int32_t tmp;
				for (x = 0; x < width; ++x) {
					tmp = x * height * 2 + y * 2;
					if (accumulate) {
						src_x = accu_src[tmp];
						src_y = accu_src[tmp + 1];
					}
					else {
						//src_x = x - (*((int32_t*)PyArray_GETPTR3(mv_arr, y, x, 0)));
						//src_y = y - (*((int32_t*)PyArray_GETPTR3(mv_arr, y, x, 1)));
					}
					location_src = src_y * stride_1 + src_x * stride_2;

					location = y * stride_1 + x * stride_2;
					for (c = 0; c < 3; ++c) {
						location2 = stride_0 + location;
						res_data[location] = (int32_t)bgr_data[location2]
							- (int32_t)bgr_data[location_src + c];
						location += 1;
					}
				}
			}
		}
	}
}

int decode_video(
	int gop_target,
	int pos_target,
	void** bgr_arr,
	void** mb_arr,
	void** qp_arr,
	void** mv_arr,
	void** res_arr,
	int representation,
	int accumulate) {

	AVCodec *pCodec;
	AVCodecContext *pCodecCtx = NULL;
	AVCodecParserContext *pCodecParserCtx = NULL;

	FILE *fp_in;
	AVFrame *pFrame;
	AVFrame *pFrameBGR;

	const int in_buffer_size = 4096;
	uint8_t *in_buffer = (uint8_t*)malloc(in_buffer_size + FF_INPUT_BUFFER_PADDING_SIZE);
	memset(in_buffer + in_buffer_size, 0, FF_INPUT_BUFFER_PADDING_SIZE);

	uint8_t *cur_ptr;
	int cur_size;
	int cur_gop = -1;
	AVPacket packet;
	int ret, got_picture;

	int mb_stride, mb_sum, mb_type, mb_width, mb_height;

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

	AVDictionary *opts = NULL;
	av_dict_set(&opts, "flags2", "+export_mvs", 0);
	if (avcodec_open2(pCodecCtx, pCodec, &opts) < 0) {
		printf("Could not open codec\n");
		return -1;
	}
	//Input File  
	fp_in = fopen(filename, "rb");
	if (!fp_in) {
		printf("Could not open input stream\n");
		return -1;
	}

	int cur_pos = 0;

	pFrame = av_frame_alloc();
	pFrameBGR = av_frame_alloc();

	uint8_t *buffer = NULL;

	av_init_packet(&packet);

	int *accu_src = NULL;
	int *accu_src_old = NULL;

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
				++cur_gop;
			}

			if (cur_gop == gop_target && cur_pos <= pos_target) {

				//ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, &packet);

				int ret = avcodec_send_packet(pCodecCtx, &packet);
				if (ret < 0) {
					fprintf(stderr, "Error while sending a packet to the decoder: %s\n", av_err2str(ret));
					return ret;
				}

				while (ret >= 0) {
					ret = avcodec_receive_frame(pCodecCtx, pFrame);
					if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
						break;
					}
					else if (ret < 0) {
						fprintf(stderr, "Error while receiving a frame from the decoder: %s\n", av_err2str(ret));
						return ret;
					}

					if (ret >= 0) {
						got_picture = 1;
						break;
					}
				}

				if (ret < 0) {
					printf("Decode Error.\n");
					return -1;
				}
				int h = pFrame->height;
				int w = pFrame->width;

				// Initialize arrays. 
				if ((representation & GOTFM) && !(*bgr_arr)) { // get rgb
					*bgr_arr = malloc(w * h * 3 * sizeof(uint8_t));
				}
				if ((representation & GOTMB) && !(*mb_arr)) { // get macroblock
					*mb_arr = malloc(w * h * sizeof(int));
				}

				if ((representation & GOTQP) && !(*qp_arr)) { // get qp
					*qp_arr = malloc(w * h * sizeof(int));
				}

				if ( (representation & GOTMV) && !(*mv_arr)) { // get motion vector
					*mv_arr = malloc(w * h * sizeof(int));
				}

				if (representation == GOTRD && !(*res_arr)) { // get residual 
					*res_arr = malloc(w * h * sizeof(int));
				}
				if ((representation & GOTMB) || (representation & GOTQP)) {
					mb_stride = w / 16 + 1;
					mb_sum = ((h + 15) >> 4)*(w / 16 + 1);
					//mb_type = (int *)pFrame->mb_type;
				}

		
				if (got_picture) {

					if ((cur_pos == 0 && accumulate  && representation == GOTRD) ||
						(cur_pos == pos_target - 1 && !accumulate && representation == GOTRD) ||
						cur_pos == pos_target) {
						create_and_load_bgr(pFrame, pFrameBGR, buffer, bgr_arr, cur_pos, pos_target);
					}

					if (representation == GOTMV ||
						representation == GOTRD) {
						AVFrameSideData *sd;
						sd = av_frame_get_side_data(pFrame, AV_FRAME_DATA_MOTION_VECTORS);
						if (sd) {
							if (accumulate || cur_pos == pos_target) {
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
						}
					}
					cur_pos++;
				}
			}
		}
	}

	//Flush Decoder  
	packet.data = NULL;
	packet.size = 0;
	while (1) {
		//ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, &packet);
		int ret = avcodec_send_packet(pCodecCtx, &packet);
		if (ret < 0) {
			fprintf(stderr, "Error while sending a packet to the decoder: %s\n", av_err2str(ret));
			return ret;
		}

		while (ret >= 0) {
			ret = avcodec_receive_frame(pCodecCtx, pFrame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			}
			else if (ret < 0) {
				fprintf(stderr, "Error while receiving a frame from the decoder: %s\n", av_err2str(ret));
				return ret;
			}

			if (ret >= 0) {
				got_picture = 1;
				break;
			}
		}
		if (ret < 0) {
			printf("Decode Error.\n");
			return -1;
		}
		if (!got_picture) {
			break;
		}
		else if (cur_gop == gop_target) {
			if ((cur_pos == 0 && accumulate) ||
				(cur_pos == pos_target - 1 && !accumulate) ||
				cur_pos == pos_target) {
				create_and_load_bgr(
					pFrame, pFrameBGR, buffer, bgr_arr, cur_pos, pos_target);
			}
		}
	}

	fclose(fp_in);

	av_parser_close(pCodecParserCtx);

	av_frame_free(&pFrame);
	av_frame_free(&pFrameBGR);
	avcodec_close(pCodecCtx);
	av_free(pCodecCtx);
	if ((representation == GOTMV ||
		representation == GOTRD) && accumulate) {
		if (accu_src) {
			free(accu_src);
		}
		if (accu_src_old) {
			free(accu_src_old);
		}
	}

	return 0;
}

static void load(char* fname, int gopidx, int framenum, int present, int acc)
{

	int gop_target, pos_target, representation, accumulate;

	gop_target = gopidx;
	pos_target = framenum;
	representation = present;
	accumulate = acc;

	uint8_t *bgr_arr = NULL;
	uint8_t *final_bgr_arr = NULL;
	uint8_t *mb_arr = NULL;
	uint8_t *qp_arr = NULL;
	uint8_t *mv_arr = NULL;
	uint8_t *res_arr = NULL;


	if (decode_video(gop_target, pos_target,
		&bgr_arr, &mb_arr, &qp_arr, &mv_arr, &res_arr,
		representation,
		accumulate) < 0) {
		printf("Decoding video failed.\n");
		
	}
	/*
	PyArrayObject *bgr_arr = NULL;
	PyArrayObject *final_bgr_arr = NULL;
	PyArrayObject *mv_arr = NULL;
	PyArrayObject *res_arr = NULL;

	if (decode_video(gop_target, pos_target,
		&bgr_arr, &mv_arr, &res_arr,
		representation,
		accumulate) < 0) {
		printf("Decoding video failed.\n");

		Py_XDECREF(bgr_arr);
		Py_XDECREF(mv_arr);
		Py_XDECREF(res_arr);
		return Py_None;
	}
	if (representation == MV) {
		Py_XDECREF(bgr_arr);
		Py_XDECREF(res_arr);
		return mv_arr;

	}
	else if (representation == RESIDUAL) {
		Py_XDECREF(bgr_arr);
		Py_XDECREF(mv_arr);
		return res_arr;

	}
	else {
		Py_XDECREF(mv_arr);
		Py_XDECREF(res_arr);

		npy_intp *dims_bgr = PyArray_SHAPE(bgr_arr);
		int h = dims_bgr[1];
		int w = dims_bgr[2];

		npy_intp dims[3];
		dims[0] = h;
		dims[1] = w;
		dims[2] = 3;
		PyArrayObject *final_bgr_arr = PyArray_ZEROS(3, dims, NPY_UINT8, 0);

		int size = h * w * 3 * sizeof(uint8_t);
		memcpy(final_bgr_arr->data, bgr_arr->data + size, size);

		Py_XDECREF(bgr_arr);
		return final_bgr_arr;
	}
	*/
}



int main(int argc, char **argv)
{
    int ret = 0;
    AVPacket pkt = { 0 };

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <video>\n", argv[0]);
        exit(1);
    }
    src_filename = argv[1];

    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        exit(1);
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }

    open_codec_context(fmt_ctx, AVMEDIA_TYPE_VIDEO);

    av_dump_format(fmt_ctx, 0, src_filename, 0);

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

    printf("framenum,source,blockw,blockh,srcx,srcy,dstx,dsty,flags\n");

    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == video_stream_idx)
            ret = decode_packet(&pkt);
        av_packet_unref(&pkt);
        if (ret < 0)
            break;
    }

    /* flush cached frames */
    decode_packet(NULL);

end:
    avcodec_free_context(&video_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    return ret < 0;
}
