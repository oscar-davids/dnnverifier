#include "calcmatrix.h"
#ifdef _WIN32
#include "wininclude/pthread.h"
#else
#include "pthread.h"
#endif

#ifdef _DEBUG
#include "bmpio.h"
#endif

using namespace cv;

#define MAX_NUM_THREADS 11	//master + rendition 10 at same time
#define MAX_NUM_SAMPLES 50	//sample count for compare

int open_context(LPDecContext* lpcontext)
{
	if (lpcontext == NULL || strlen(lpcontext->path) <= 0) return LP_FAIL;
	int ret;

	if (avformat_open_input(&lpcontext->input_ctx, lpcontext->path, NULL, NULL) != 0) {
		fprintf(stderr, "Cannot open input file '%s'\n", lpcontext->path);
		return LP_FAIL;
	}

	if (avformat_find_stream_info(lpcontext->input_ctx, NULL) < 0) {
		fprintf(stderr, "Cannot find input stream information.\n");
		return LP_FAIL;
	}

	/* find the video stream information */
	ret = av_find_best_stream(lpcontext->input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &lpcontext->vcodec, 0);

	if (ret >= 0) {
		lpcontext->alivevideo = 1;

		lpcontext->video_stream = ret;
		lpcontext->vstream = lpcontext->input_ctx->streams[lpcontext->video_stream];
		lpcontext->vcontext = avcodec_alloc_context3(lpcontext->vcodec);

		lpcontext->vcontext->thread_count = 16;

		if (lpcontext->vcontext) {
			avcodec_parameters_to_context(lpcontext->vcontext, lpcontext->vstream->codecpar);			
			if (avcodec_open2(lpcontext->vcontext, lpcontext->vcodec, NULL) >= 0) {

				lpcontext->fps = 1.0;
				if (lpcontext->vstream->r_frame_rate.den > 0.0) {
					lpcontext->fps = av_q2d(lpcontext->vstream->r_frame_rate);
				}
				else {
					lpcontext->fps = 1.0 / av_q2d(lpcontext->vstream->time_base);
				}

				lpcontext->width = lpcontext->vcontext->width;
				lpcontext->height = lpcontext->vcontext->height;

				lpcontext->duration = (double)lpcontext->input_ctx->duration / (double)AV_TIME_BASE;
				if (lpcontext->duration <= 0.000001) {
					lpcontext->duration = (double)lpcontext->vstream->duration * av_q2d(lpcontext->vstream->time_base);
				}

				lpcontext->framecount = lpcontext->vstream->nb_frames;
				if (lpcontext->framecount == 0) {
					lpcontext->framecount = (int)(lpcontext->duration * lpcontext->fps + 0.5);
				}
			}			
		}
	}
	else {
		fprintf(stderr, "Cannot find a video stream in the input file\n");		
	}

	/* find the audio stream information */
	ret = av_find_best_stream(lpcontext->input_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &lpcontext->acodec, 0);

	if (ret >= 0) {		
			lpcontext->aliveaudio = 1;
			lpcontext->audio_stream = ret;
			lpcontext->astream = lpcontext->input_ctx->streams[lpcontext->audio_stream];
			lpcontext->acontext = avcodec_alloc_context3(lpcontext->acodec);
			if (lpcontext->acontext) {
				avcodec_parameters_to_context(lpcontext->acontext, lpcontext->astream->codecpar);				
				if (avcodec_open2(lpcontext->acontext, lpcontext->acodec, NULL) >= 0) {
					lpcontext->samplerate = lpcontext->acontext->sample_rate;
					lpcontext->channels = lpcontext->acontext->channels;
				}
			}
	} else {
		fprintf(stderr, "Cannot find a audio stream in the input file\n");
	}
	lpcontext->normalw = 480;
	lpcontext->normalh = 270;

	return LP_OK;
}
void release_context(LPDecContext* lpcontext)
{
	//release timestamp array
	if (lpcontext->frameindexs)
		free(lpcontext->frameindexs);
	if (lpcontext->framestamps)
		free(lpcontext->framestamps);
	if (lpcontext->framediffps)
		free(lpcontext->framediffps);
	if (lpcontext->audiodiffps)
		free(lpcontext->audiodiffps);

	//release frame and audio array
	for (int i = 0; i < lpcontext->samplecount; i++)
	{

		if (lpcontext->alivevideo && lpcontext->listfrmame[i])
			av_frame_free(&lpcontext->listfrmame[i]);

		if (lpcontext->aliveaudio && lpcontext->listaudio[i])
			av_packet_unref(lpcontext->listaudio[i]);
	}
	if (lpcontext->listfrmame) free(lpcontext->listfrmame);
	if (lpcontext->listaudio) free(lpcontext->listaudio);
	if (lpcontext->ftmatrix) free(lpcontext->ftmatrix);
	
	//release media context
	if (lpcontext->sws_rgb_scale)
		sws_freeContext(lpcontext->sws_rgb_scale);

	if (lpcontext->vcontext)
		avcodec_free_context(&lpcontext->vcontext);

	if (lpcontext->acontext)
		avcodec_free_context(&lpcontext->acontext);

	avformat_close_input(&lpcontext->input_ctx);
}

void* decode_frames(void* parg)
{
	LPDecContext* context = (LPDecContext*)parg;

	if (context == NULL || (context->vcontext == NULL && context->acontext == NULL)) 
		return NULL;
	
	int ret = 0;	
	int i, got_frame, index;
	double tolerence = 1.0 / (context->fps * 2.0);
	double diffpts = 0.0;
	AVPacket packet;
	AVFrame *ptmpframe = NULL;
	
	context->readframe = av_frame_alloc();
	if (context->alivevideo) {
		context->listfrmame = (AVFrame**)malloc(sizeof(AVFrame*)*context->samplecount);
		memset(context->listfrmame, 0x00, sizeof(AVFrame*)*context->samplecount);
	}
	if (context->aliveaudio) {
		context->listaudio = (AVPacket**)malloc(sizeof(AVPacket*)*context->samplecount);
		memset(context->listaudio, 0x00, sizeof(AVPacket*)*context->samplecount);
	}


	while (ret >= 0) {

		if ((ret = av_read_frame(context->input_ctx, &packet)) < 0)
			break;

		//video decoding
		if (context->video_stream == packet.stream_index)
		{
			got_frame = 0;
		
			ret = avcodec_send_packet(context->vcontext, &packet);
			if (ret < 0 && ret != AVERROR_EOF)
				return NULL;
		
			while (ret >= 0) {
				ret = avcodec_receive_frame(context->vcontext, context->readframe);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					ret = 0;
					break;
				} else if (ret < 0) {
					fprintf(stderr, "Error while receiving a frame from the decoder\n");					
					return NULL;
				}
				if (context->readframe->pict_type == AV_PICTURE_TYPE_I) {					
				}				
				context->framenum++;

				if (context->sws_rgb_scale == NULL) {
					context->sws_rgb_scale = sws_getContext(context->readframe->width, context->readframe->height, 
						(AVPixelFormat)context->readframe->format, context->normalw, context->normalh, AV_PIX_FMT_BGR24,
						SWS_BILINEAR, NULL, NULL, NULL);					
				}
				//check timestamp and add frame buffer
				index = -1;
				
				for ( i = 0; i < context->samplecount; i++){
					diffpts = fabs(context->framestamps[i] - packet.pts * av_q2d(context->vstream->time_base));

					if (diffpts < tolerence) {
						index = i; break;
					}
				}
				//add frame in list
				if (i >= 0 &&  i < context->samplecount && context->framediffps[i] > diffpts) {
					context->framediffps[i] = diffpts;

					if (context->listfrmame[i] != NULL) av_frame_free(&context->listfrmame[i]);
					ptmpframe = av_frame_alloc();
					if (!ptmpframe)
						return NULL;

					ptmpframe->format = AV_PIX_FMT_RGB24;
					ptmpframe->width = context->normalw;
					ptmpframe->height = context->normalh;
					ret = av_frame_get_buffer(ptmpframe, 0);
					if (ret < 0) {
						av_frame_free(&ptmpframe);
						return NULL;
					}
					sws_scale(context->sws_rgb_scale, (const uint8_t **)context->readframe->data, context->readframe->linesize,
						0, context->readframe->height, (uint8_t * const*)(&ptmpframe->data),
						ptmpframe->linesize);

					context->listfrmame[i] = ptmpframe;					
				}				
			}

			av_frame_unref(context->readframe);
		}

		//audio process
		if (context->audio_stream == packet.stream_index) {

			//check timestamp and add audio packet
			index = -1;
			for (i = 0; i < context->samplecount; i++) {
				diffpts = fabs(context->audiodiffps[i] - packet.pts * av_q2d(context->astream->time_base));
				if (diffpts < tolerence) {
					index = i; break;
				}
			}
			//add frame in list
			if (i >= 0 && i < context->samplecount && context->audiodiffps[i] > diffpts) {
				context->audiodiffps[i] = diffpts;

				if (context->listaudio[i] != NULL) av_packet_free(&context->listaudio[i]);

				context->listaudio[i] = av_packet_clone(&packet);
				if (context->listaudio[i])
					return NULL;
			}
		}

		av_packet_unref(&packet);
	}

	//clean frame
	if (context->readframe)
		av_frame_free(&context->readframe);

	return NULL;
}

int grab_allframes(LPDecContext* pcontext, int ncount)
{
	if (pcontext == NULL || pcontext->alivevideo == 0 || ncount <= 1 || pcontext->samplecount <= 0)
		return LP_FAIL;
	int i , j, tmp;
	LPDecContext* tmpcontext;
	time_t t;
	pthread_t threads[MAX_NUM_THREADS];

	pcontext->frameindexs = (int*)malloc(sizeof(int) * pcontext->samplecount);
	pcontext->framestamps = (double*)malloc(sizeof(double) * pcontext->samplecount);
	pcontext->framediffps = (double*)malloc(sizeof(double) * pcontext->samplecount);
	pcontext->audiodiffps = (double*)malloc(sizeof(double) * pcontext->samplecount);
	
	//intializes random number generator and generate randomize frame indexs
	srand((unsigned)time(&t));	
	for (i = 0; i < pcontext->samplecount; i++) {
		pcontext->frameindexs[i] = rand() % (pcontext->framecount - 2);
	}
	//sort indexs
	for (i = 0; i < pcontext->samplecount-1; i++) {
		for ( j = i+1; j < pcontext->samplecount; j++){
			if (pcontext->frameindexs[i] > pcontext->frameindexs[j]) {
				tmp = pcontext->frameindexs[i]; 
				pcontext->frameindexs[i] = pcontext->frameindexs[j]; 
				pcontext->frameindexs[j] = tmp;
			}
		}
	}	
	for (i = 0; i < pcontext->samplecount; i++) {
		pcontext->framestamps[i] = pcontext->frameindexs[i] / pcontext->fps;
		pcontext->framediffps[i] = 10000;
		pcontext->audiodiffps[i] = 10000;
	}
	for (i = 1; i < ncount; i++) {
		tmpcontext = pcontext + i;
		tmpcontext->frameindexs = (int*)malloc(sizeof(int) * pcontext->samplecount);
		tmpcontext->framestamps = (double*)malloc(sizeof(double) * pcontext->samplecount);
		tmpcontext->framediffps = (double*)malloc(sizeof(double) * pcontext->samplecount);
		tmpcontext->audiodiffps = (double*)malloc(sizeof(double) * pcontext->samplecount);
		memcpy(tmpcontext->frameindexs, pcontext->frameindexs, sizeof(int) * pcontext->samplecount);
		memcpy(tmpcontext->framestamps, pcontext->framestamps, sizeof(double) * pcontext->samplecount);
		memcpy(tmpcontext->framediffps, pcontext->framediffps, sizeof(double) * pcontext->samplecount);
		memcpy(tmpcontext->audiodiffps, pcontext->audiodiffps, sizeof(double) * pcontext->samplecount);
	}
	
	//run grabber thread , here grabber all frames based on time
#if 0
	for (i = 0; i < ncount; i++) {
		decode_frames(&pcontext[i]);
	}
#else
	for (i = 0; i < ncount; i++) {
		if (pthread_create(&threads[i], NULL, decode_frames, (void *)&pcontext[i])) {
			fprintf(stderr, "Error create thread id %d\n", i);
		}
	}

	for (i = 0; i < ncount; i++) {
		if (pthread_join(threads[i], NULL)) {
			fprintf(stderr, "Error joining thread id %d\n", i);
		}
	}
#endif

	return LP_OK;
}
int pre_verify(LPDecContext* pcontext, int renditions)
{
	if (pcontext == NULL || renditions<=0)
		return LP_FAIL;

	int i;
	LPDecContext* prendition;
	for (i = 0; i < renditions; i++) {
		prendition = pcontext + i + 1;
		if (pcontext->alivevideo != prendition->alivevideo ||
			pcontext->aliveaudio != prendition->aliveaudio ||
			pcontext->samplerate != prendition->samplerate)
			prendition->tamper = 1;
	}
	return LP_OK;
}

int calc_framediff(LPDecContext* pctxmaster, LPDecContext* pctxrendition, int index)
{
	if (pctxmaster == NULL || pctxrendition == NULL /*|| featurelist == NULL*/)
		return LP_ERROR_NULL_POINT;
	int x, y;
	//LP_FT_DCT, LP_FT_GAUSSIAN_MSE, LP_FT_GAUSSIAN_DIFF, LP_FT_GAUSSIAN_TH_DIFF, LP_FT_HISTOGRAM_DISTANCE
	Mat reference_frame, rendition_frame, next_reference_frame, next_rendition_frame;
	Mat reference_frame_v, rendition_frame_v, next_reference_frame_v, next_rendition_frame_v;
	Mat reference_frame_float, rendition_frame_float, reference_dct, rendition_dct;
	double dmin, dmax;
	Mat gauss_reference_frame, gauss_rendition_frame, difference_frame, threshold_frame, temporal_difference;
	//sigma = 4
	//gauss_reference_frame = gaussian(reference_frame_v, sigma = sigma)
	//gauss_rendition_frame = gaussian(rendition_frame_v, sigma = sigma)
	double dsum, difference, dmse , dabssum;
	int width, height;
	Scalar mean, stddev, ssum;
	MatND hist_a, hist_b;
	int channels[] = { 0, 1, 2 };
	int bins[3] = { 8, 8, 8 };
	int histSize[] = { 256, 256, 256 };
	float h_ranges[] = { 0, 256 };
	float s_ranges[] = { 0, 256 };
	float v_ranges[] = { 0, 256 };
	const float* ranges[] = { h_ranges, s_ranges, v_ranges };	
	width = pctxmaster->normalw;
	height = pctxmaster->normalh;

	if (pctxmaster->listfrmame[index] == NULL || pctxrendition->listfrmame[index] == NULL)
		return -1;

	reference_frame = Mat(height, width, CV_8UC3, pctxmaster->listfrmame[index]->data[0]);
	rendition_frame = Mat(height, width, CV_8UC3, pctxrendition->listfrmame[index]->data[0]);
	next_reference_frame = Mat(height, width, CV_8UC3, pctxmaster->listfrmame[index+1]->data[0]);
	next_rendition_frame = Mat(height, width, CV_8UC3, pctxrendition->listfrmame[index+1]->data[0]);
	
	cvtColor(reference_frame, reference_frame_v, COLOR_BGR2HSV);
	cvtColor(rendition_frame, rendition_frame_v, COLOR_BGR2HSV);
	cvtColor(next_reference_frame, next_reference_frame_v, COLOR_BGR2HSV);
	cvtColor(next_rendition_frame, next_rendition_frame_v, COLOR_BGR2HSV);

	extractChannel(reference_frame_v, reference_frame_v, 2);
	extractChannel(rendition_frame_v, rendition_frame_v, 2);
	extractChannel(next_reference_frame_v, next_reference_frame_v, 2);
	extractChannel(next_rendition_frame_v, next_rendition_frame_v, 2);

	GaussianBlur(reference_frame_v, gauss_reference_frame, Size(3, 3), 4);
	GaussianBlur(rendition_frame_v, gauss_rendition_frame, Size(3, 3), 4);

	
	dsum = dabssum = 0.0;
	double* pout = pctxrendition->ftmatrix + (int)LP_FT_FEATURE_MAX * index;

	absdiff(gauss_reference_frame, gauss_rendition_frame, difference_frame);
	
	for (int i = 0; i < LP_FT_FEATURE_MAX; i++)
	{
		switch (i)
		{
		case LP_FT_DCT:			
			reference_frame_v.convertTo(reference_frame_float, CV_32FC3, 1 / 255.0);
			rendition_frame_v.convertTo(rendition_frame_float, CV_32FC3, 1 / 255.0);
			dct(reference_frame_float, reference_dct);
			dct(rendition_frame_float, rendition_dct);
			minMaxIdx(reference_dct - rendition_dct,&dmin , &dmax);
			*(pout + i) = dmax;
			break;
		case LP_FT_GAUSSIAN_MSE:			
			/*for (x = 0; x < width; ++x) {
				for (y = 0; y < height; ++y) {					
					difference = (double)gauss_reference_frame.at<float>(x, y) -gauss_rendition_frame.at<float>(x, y);
					dsum = dsum + difference * difference;
					dabssum = dabssum + abs(difference);
				}
			}
			dmse = dsum / (width*height);//PSNR*/
			dmse = sum(difference_frame ^ 2).val[0] / (width*height);			
			*(pout + i) = dmse;
			break;
		case LP_FT_GAUSSIAN_DIFF:
			*(pout + i) = sum(difference_frame).val[0];
			break;
		case LP_FT_GAUSSIAN_TH_DIFF:
			temporal_difference = (next_reference_frame_v / 255.0) - (rendition_frame_v / 255.0);						
			meanStdDev(temporal_difference, mean, stddev);
			threshold(difference_frame, threshold_frame, stddev.val[0], 1, THRESH_BINARY);
			ssum = sum(threshold_frame);
			*(pout + i) = ssum.val[0];
			break;
		case LP_FT_HISTOGRAM_DISTANCE:
				/*def histogram_distance(reference_frame, rendition_frame, eps = 1e-10)
				bins = [8, 8, 8]
				hist_a = cv2.calcHist([reference_frame], [0, 1, 2],
					None, bins, [0, 256, 0, 256, 0, 256])
				hist_a = cv2.normalize(hist_a, hist_a)
				hist_b = cv2.calcHist([rendition_frame], [0, 1, 2],
					None, bins, [0, 256, 0, 256, 0, 256])
				hist_b = cv2.normalize(hist_b, hist_b)
				# return out 3D histogram as a flattened array
				hist_a = hist_a.flatten()
				hist_b = hist_b.flatten()
				# Return the chi squared distance of the histograms
				chi_dist = 0.5 * np.sum([((a - b) ** 2) / (a + b + eps) for (a, b) in zip(hist_a, hist_b)])
				return chi_dist
				*/
			calcHist(&reference_frame, 1, channels, Mat(), hist_a, 2, histSize, ranges, true, false);
			normalize(hist_a, hist_a, 0, 1, NORM_MINMAX, -1, Mat());

			calcHist(&rendition_frame, 1, channels, Mat(), hist_b, 2, histSize, ranges, true, false);
			normalize(hist_b, hist_b, 0, 1, NORM_MINMAX, -1, Mat());
			*(pout + i) =  compareHist(hist_a, hist_b, HISTCMP_CHISQR);
			
			break;		
		default:
			break;
		}
	}

	return LP_OK;
}
int calc_featurematrix(LPDecContext* pctxmaster, LPDecContext* pctxrendition)
{
	//make reature matrix(feature * samplecount)
	pctxrendition->ftmatrix = (double*)malloc(sizeof(double) * 5 * pctxrendition->samplecount);
	for (int i = 0; i < pctxrendition->samplecount-1; i++)
	{	
		calc_framediff(pctxmaster, pctxrendition, i);
	}
	return LP_OK;
}

int calc_featurediff(char* srcpath, char* renditions, char* featurelist, int samplenum)
{
	if (srcpath == NULL || renditions == NULL /*|| featurelist == NULL*/)
		return LP_ERROR_NULL_POINT;
	if(samplenum <= 0 || samplenum >= MAX_NUM_SAMPLES)
		return LP_ERROR_INVALID_PARAM;

	
	int ret, i, j, nvideonum, featureconut;
	
	LPDecContext* pcontext = NULL;
	LPDecContext* ptmpcontext = NULL;

	//parser renditions url & featurelist
	nvideonum = 2;
	featureconut = 5;

	//create context and read	
	pcontext = (LPDecContext*)malloc(sizeof(LPDecContext) * nvideonum);
	memset(pcontext, 0x00, sizeof(LPDecContext) * nvideonum);

	strcpy(pcontext[0].path, srcpath);
	strcpy(pcontext[1].path, renditions);

	for (i = 0; i < nvideonum; i++) {
		ptmpcontext = pcontext + i;
		ptmpcontext->audio_stream = -1;
		ptmpcontext->video_stream = -1;
		ptmpcontext->samplecount = samplenum;

		ret = open_context(ptmpcontext);		
		if (ret != LP_OK) break;

	}	
	//pre verify with metadata
	pre_verify(pcontext, nvideonum - 1);
	//grab all frames for compare
	grab_allframes(pcontext, nvideonum);

#ifdef 	_DEBUG
	for (i = 0; i < nvideonum; i++) {
		LPDecContext* ptmpcontext = pcontext + i;
		char tmppath[MAX_PATH] = { 0, };
		uint8_t *pbuffer = (uint8_t*)malloc(sizeof(uint8_t) * ptmpcontext->normalh * ptmpcontext->normalw* 3);
		for ( int j = 0; j < ptmpcontext->samplecount;  j++)
		{
			if (ptmpcontext->alivevideo && ptmpcontext->listfrmame[j] != NULL) {
				
				sprintf(tmppath, "D:/tmp/%02d_%02d.bmp", i, j);
#if 0 //def __OPENCV_
				//imwrite(tmppath, *((Mat*)ptmpcontext->listfrmame[j]));
#else
				//get rgb bugffer
				uint8_t *src = (uint8_t*)ptmpcontext->listfrmame[j]->data[0];
				memcpy(pbuffer, src, ptmpcontext->normalh * ptmpcontext->normalw * 3 * sizeof(uint8_t));				
				WriteColorBmp(tmppath, ptmpcontext->normalw, ptmpcontext->normalh, pbuffer);
#endif
			}
			if (ptmpcontext->aliveaudio) {

			}
		}
		if (pbuffer) free(pbuffer);
	}

#endif

	//calculate features and matrix
	//matirx column is [tamper, audiodiff, dct, gaussdiff, ... ] 2+featurecount
	// create matrix for calculation
	//first compare audio buffer
	if (pcontext->aliveaudio) {
		for (i = 1; i < nvideonum; i++) {
			ptmpcontext = pcontext + i;
			for (j = 0; j < pcontext->samplecount; j++) {
				memcmp(pcontext->listaudio[i]->data, ptmpcontext->listaudio[i]->data, pcontext->listaudio[i]->size);
				//set matrix value
			}
		}
	}
	//calculate vframe matrix
	if (pcontext->alivevideo) {
		for (i = 1; i < nvideonum; i++) {
			ptmpcontext = pcontext + i;
			calc_featurematrix(pcontext, ptmpcontext);
		}		
	}

	//aggregate matrix
	
	//make and return float matrix
	

	for (i = 0; i < nvideonum; i++) {
		release_context(pcontext + i);
	}
	if (pcontext) free(pcontext);

	return LP_OK;
}