#include "calcmatrix.h"

int decode_frames(LPDecContext* context)
{
	if (context == NULL || strlen(context->path) <= 0) return LP_ERROR_NULL_POINT;

	int ret = 0;	
	int got_frame = 0;
	AVPacket packet;

	/* open the input file */
	if (avformat_open_input(&context->input_ctx, context->path, NULL, NULL) != 0) {
		fprintf(stderr, "Cannot open input file '%s'\n", context->path);
		return LP_ERROR_NOFILE_NAME;
	}

	if (avformat_find_stream_info(context->input_ctx, NULL) < 0) {
		fprintf(stderr, "Cannot find input stream information.\n");
		return LP_ERROR_NOTFIND_DECODE;
	}

	/* find the video stream information */
	ret = av_find_best_stream(context->input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &context->decoder, 0);
	if (ret < 0) {
		fprintf(stderr, "Cannot find a video stream in the input file\n");
		return LP_ERROR_NOTFIND_CODEC;
	}
	context->video_stream = ret;

	if (!(context->decoder_ctx = avcodec_alloc_context3(context->decoder)))
		return AVERROR(ENOMEM);

	context->video = context->input_ctx->streams[context->video_stream];
	if (avcodec_parameters_to_context(context->decoder_ctx, context->video->codecpar) < 0)
		return -1;

	if ((ret = avcodec_open2(context->decoder_ctx, context->decoder, NULL)) < 0) {
		fprintf(stderr, "Failed to open codec for stream #%u\n", context->video_stream);
		return -1;
	}

	context->readframe = av_frame_alloc();

	while (ret >= 0) {

		if ((ret = av_read_frame(context->input_ctx, &packet)) < 0)
			break;

		//video decoding
		if (context->video_stream == packet.stream_index)
		{
			got_frame = 0;
		
			ret = avcodec_send_packet(context->decoder_ctx, &packet);
			if (ret < 0 && ret != AVERROR_EOF)
				return ret;
		
			while (ret >= 0) {
				ret = avcodec_receive_frame(context->decoder_ctx, context->readframe);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					ret = 0;
					break;
				} else if (ret < 0) {
					fprintf(stderr, "Error while receiving a frame from the decoder: %s\n", av_err2str(ret));
					return ret;
				}
				if (context->readframe->pict_type == AV_PICTURE_TYPE_I) {					
				}

				//cur_pos++;
				context->framenum++;
				if (context->sws_rgb_scale == NULL) {
					context->sws_rgb_scale = sws_getContext(context->readframe->width, context->readframe->height, 
						context->readframe->format,320, 270, AV_PIX_FMT_RGB24,
						SWS_BILINEAR, NULL, NULL, NULL);

					context->swscaleframe = av_frame_alloc();
					if (!context->swscaleframe)
						return AVERROR(ENOMEM);

					context->swscaleframe->format = AV_PIX_FMT_RGB24;
					context->swscaleframe->width = 320;
					context->swscaleframe->height = 270;
					ret = av_frame_get_buffer(context->swscaleframe, 0);
					if (ret < 0) {
						av_frame_free(&context->swscaleframe);
						return ret;
					}
				}
				//check timestamp and add frame buffer
				if (context->framenum % 10 == 0) {
					sws_scale(context->sws_rgb_scale, (const uint8_t **)context->readframe->data, context->readframe->linesize,
						0, context->readframe->height, (uint8_t * const*)(&context->swscaleframe->data),
						context->swscaleframe->linesize);
				}
			}

			av_frame_unref(context->readframe);
		}

		av_packet_unref(&packet);
	}

	//clean context
	if (context->readframe)
		av_frame_free(&context->readframe);
	avcodec_free_context(&context->decoder_ctx);
	avformat_close_input(&context->input_ctx);


	return LP_OK;
}

int calc_featurediff(char* srcpath, char* renditions, char* featurelist, int samplenum)
{
	if (srcpath == NULL || renditions == NULL || featurelist == NULL)
		return LP_ERROR_NULL_POINT;

	//parser renditions url & featurelist
	int nvideonum = 2;
	LPDecContext* pcontext = (LPDecContext*)malloc(sizeof(LPDecContext) * nvideonum);
	//run grabber thread , hele grabber all frames based on time

	//calculate features and matrix

	//aggregate matrix

	//make return float matrix
	
	return LP_OK;
}