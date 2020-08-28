#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "calcmatrix.h"

#ifdef _WIN32
#include "wininclude/pthread.h"
#else
#include "pthread.h"
#endif

#ifndef _TEST_MODULE
#include <Python.h>
#include "numpy/arrayobject.h"
#else
#ifdef _DEBUG
#include "bmpio.h"
#endif
#endif

using namespace cv;

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

		lpcontext->vcontext->thread_count = 8;

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
void close_context(LPDecContext* lpcontext)
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
	double tolerance = 2.0 / context->fps;
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

					if (diffpts < tolerance) {
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
				if (diffpts < tolerance) {
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
#ifdef USE_MULTI_THREAD
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
#else
	for (i = 0; i < ncount; i++) {
		decode_frames(&pcontext[i]);
	}
#endif

	return LP_OK;
}
int pre_verify(LPDecContext* pcontext, int vcount)
{
	if (pcontext == NULL || vcount <=0)
		return LP_FAIL;

	int i;
	LPDecContext* prendition;
	for (i = 1; i < vcount; i++) {
		prendition = pcontext + i;
		if (pcontext->alivevideo != prendition->alivevideo ||
			pcontext->aliveaudio != prendition->aliveaudio ||
			pcontext->samplerate != prendition->samplerate)
			prendition->tamper = 1;
	}
	return LP_OK;
}

void* calc_framediff(void* pairinfo)
{
	if (pairinfo == NULL) return NULL;

	LPDecContext *pctxmaster, *pctxrendition;
	int index = ((LPPair*)pairinfo)->frameid;
	pctxmaster = ((LPPair*)pairinfo)->master;
	pctxrendition = ((LPPair*)pairinfo)->rendition;

	if (pctxmaster == NULL || pctxrendition == NULL /*|| featurelist == NULL*/)
		return NULL;
	int x, y;
	//LP_FT_DCT, LP_FT_GAUSSIAN_MSE, LP_FT_GAUSSIAN_DIFF, LP_FT_GAUSSIAN_TH_DIFF, LP_FT_HISTOGRAM_DISTANCE
	Mat reference_frame, rendition_frame, next_reference_frame, next_rendition_frame;
	Mat reference_frame_v, rendition_frame_v, next_reference_frame_v, next_rendition_frame_v;
	Mat reference_frame_float, rendition_frame_float, reference_dct, rendition_dct;
	double dmin, dmax, deps, chi_dist , dtmpe;
	Mat gauss_reference_frame, gauss_rendition_frame, difference_frame, threshold_frame, temporal_difference, difference_frame_p;
	//sigma = 4
	//gauss_reference_frame = gaussian(reference_frame_v, sigma = sigma)
	//gauss_rendition_frame = gaussian(rendition_frame_v, sigma = sigma)
	double dsum, difference, dmse , dabssum;
	int width, height, i, j;
	Scalar mean, stddev, ssum;
	MatND hist_a, hist_b;
	int channels[] = { 0, 1, 2 };
	int bins[3] = { 8, 8, 8 };
	int histSize[] = { 256, 256, 256 };
	float h_ranges[] = { 0, 256 };
	float s_ranges[] = { 0, 256 };
	float v_ranges[] = { 0, 256 };
	const float* ranges[] = { h_ranges, s_ranges, v_ranges };	
	float *phis_a, *phis_b;
	deps = 1e-10;
	width = pctxmaster->normalw;
	height = pctxmaster->normalh;

	if (pctxmaster->listfrmame[index] == NULL || pctxrendition->listfrmame[index] == NULL)
		return NULL;

#if 0 //def _DEBUG
	reference_frame = imread("d:/bmp/reference_frame.bmp");
	rendition_frame = imread("d:/bmp/rendition_frame.bmp");
	next_reference_frame = imread("d:/bmp/next_reference_frame.bmp");
	next_rendition_frame = imread("d:/bmp/next_rendition_frame.bmp");
#else

	reference_frame = Mat(height, width, CV_8UC3, pctxmaster->listfrmame[index]->data[0]);
	rendition_frame = Mat(height, width, CV_8UC3, pctxrendition->listfrmame[index]->data[0]);

	next_reference_frame = Mat(height, width, CV_8UC3, pctxmaster->listfrmame[index+1]->data[0]);
	//next_rendition_frame = Mat(height, width, CV_8UC3, pctxrendition->listfrmame[index+1]->data[0]);
#endif

#if 0 //def _DEBUG
	imwrite("d:/reference_frame.bmp", reference_frame);
	imwrite("d:/rendition_frame.bmp", rendition_frame);
	imwrite("d:/next_reference_frame.bmp", next_reference_frame);
	imwrite("d:/next_rendition_frame.bmp", next_rendition_frame);
#endif
	
	cvtColor(reference_frame, reference_frame_v, COLOR_BGR2HSV);
	cvtColor(rendition_frame, rendition_frame_v, COLOR_BGR2HSV);
	cvtColor(next_reference_frame, next_reference_frame_v, COLOR_BGR2HSV);
	//cvtColor(next_rendition_frame, next_rendition_frame_v, COLOR_BGR2HSV);

	extractChannel(reference_frame_v, reference_frame_v, 2);
	extractChannel(rendition_frame_v, rendition_frame_v, 2);
	extractChannel(next_reference_frame_v, next_reference_frame_v, 2);
	//extractChannel(next_rendition_frame_v, next_rendition_frame_v, 2);

	reference_frame_v.convertTo(reference_frame_float, CV_32FC1, 1.0 / 255.0);
	rendition_frame_v.convertTo(rendition_frame_float, CV_32FC1, 1.0 / 255.0);

	next_reference_frame_v.convertTo(next_reference_frame_v, CV_32FC1, 1.0 / 255.0);	

	GaussianBlur(reference_frame_float, gauss_reference_frame, Size(33, 33), 4, 4);
	GaussianBlur(rendition_frame_float, gauss_rendition_frame, Size(33, 33), 4, 4);	

#if 0 //def _DEBUG
	imwrite("d:/c_gauss_reference_frame.bmp", gauss_reference_frame);
	imwrite("d:/c_gauss_rendition_frame.bmp", gauss_rendition_frame);	
#endif
	
	dsum = dabssum = 0.0;
	double* pout = pctxrendition->ftmatrix + (int)LP_FT_FEATURE_MAX * index;

	absdiff(gauss_reference_frame, gauss_rendition_frame, difference_frame);	

	for (i = 0; i < LP_FT_FEATURE_MAX; i++)
	{
		switch (i)
		{
		case LP_FT_DCT:			
			dct(reference_frame_float, reference_dct);
			dct(rendition_frame_float, rendition_dct);
			minMaxIdx(reference_dct - rendition_dct, &dmin , &dmax);
			*(pout + i) = dmax;
			break;
		case LP_FT_GAUSSIAN_MSE:
			pow(difference_frame, 2.0, difference_frame_p);
			dmse = sum(difference_frame_p).val[0] / (width*height);
			*(pout + i) = dmse;
			break;
		case LP_FT_GAUSSIAN_DIFF:
			*(pout + i) = sum(difference_frame).val[0];
			break;
		case LP_FT_GAUSSIAN_TH_DIFF:
			absdiff(next_reference_frame_v, rendition_frame_float, temporal_difference);			
			meanStdDev(temporal_difference, mean, stddev);
			threshold(difference_frame, threshold_frame, stddev.val[0], 1, THRESH_BINARY);
			ssum = sum(threshold_frame);
			*(pout + i) = ssum.val[0];
			break;
		case LP_FT_HISTOGRAM_DISTANCE:				
			calcHist(&reference_frame, 1, channels, Mat(), hist_a, 3, bins, ranges, true, false);
			normalize(hist_a, hist_a); phis_a = (float*)hist_a.data;

			calcHist(&rendition_frame, 1, channels, Mat(), hist_b, 3, bins, ranges, true, false);
			normalize(hist_b, hist_b); phis_b = (float*)hist_b.data;
			chi_dist = 0.0;
			for ( j = 0; j < 512; j++){
				dtmpe = *phis_a - *phis_b;
				chi_dist += (0.5 * dtmpe * dtmpe / (*phis_a + *phis_b + deps));
				phis_a++; phis_b++;
			}			
			*(pout + i) = chi_dist;
			//*(pout + i) =  compareHist(hist_a, hist_b, HISTCMP_CHISQR);			
			break;		
		default:
			break;
		}
	}

	return NULL;
}
int calc_featurematrix(LPDecContext* pctxmaster, LPDecContext* pctxrendition)
{

	int i, ncount;
	pthread_t threads[MAX_NUM_THREADS];
	//make feature matrix(feature * samplecount)

	pctxrendition->ftmatrix = (double*)malloc(sizeof(double) * 5 * pctxrendition->samplecount);
	ncount = pctxrendition->samplecount - 1;
	LPPair* pairinfo = (LPPair*)malloc(sizeof(LPPair) * ncount);
	for ( i = 0; i < ncount; i++)
	{
		pairinfo[i].master = pctxmaster;
		pairinfo[i].rendition = pctxrendition;
		pairinfo[i].frameid = i;
	}
#ifdef USE_MULTI_THREAD
	for (i = 0; i < ncount; i++) {
		if (pthread_create(&threads[i], NULL, calc_framediff, (void *)&pairinfo[i])) {
			fprintf(stderr, "Error create thread id %d\n", i);
		}
	}
	for (i = 0; i < ncount; i++) {
		if (pthread_join(threads[i], NULL)) {
			fprintf(stderr, "Error joining thread id %d\n", i);
		}
	}
#else
	for (int i = 0; i < pctxrendition->samplecount - 1; i++)
	{
		calc_framediff((void *)&pairinfo[i]);
	}
#endif
	if(pairinfo)
		free(pairinfo);

	return LP_OK;
}
int aggregate_matrix(LPDecContext* pctxrendition)
{	
	double* pout = pctxrendition->ftmatrix;
	for (int j = 0; j < LP_FT_FEATURE_MAX; j++)
	{
		double* poutstart = pout + j;
		for (int i = 1; i < pctxrendition->samplecount - 1; i++)
		{
			*poutstart += *(poutstart + (int)LP_FT_FEATURE_MAX * i);
		}

		*poutstart = *poutstart / (pctxrendition->samplecount - 1);
		//up scale values
		*poutstart = *poutstart * pctxrendition->width * pctxrendition->height;		
	}
	return LP_OK;
}
void shift_frame(LPDecContext* pctxmaster, int index)
{
	if (pctxmaster == NULL || index < 0 || index >= pctxmaster->samplecount)
		return;
	if (pctxmaster->listfrmame[index]) av_frame_free(&pctxmaster->listfrmame[index]);

	for (int i = index; i < pctxmaster->samplecount-1; i++)
	{
		pctxmaster->listfrmame[i] = pctxmaster->listfrmame[i + 1];
	}
	pctxmaster->samplecount--;
}
void remove_nullframe(LPDecContext* pcontext, int nvideonum)
{
	int i, j, k;
	LPDecContext* ptmpcontext = NULL;
	//check skipped frame related FPS and Time stamp
	if (pcontext->alivevideo) {
		for (i = 0; i < nvideonum; i++) {
			ptmpcontext = pcontext + i;
			for (j = 0; j < ptmpcontext->samplecount; j++)
			{
				if (ptmpcontext->listfrmame[j] == NULL) {
					for (k = 0; k < nvideonum; k++) {
						shift_frame(pcontext + k, j);
					}
					j--;
				}
			}
		}
	}
}

#if 0
#ifdef Py_PYTHON_H
static PyObject *calc_featurediff(PyObject *self, PyObject *args)
#else
int calc_featurediff(char* srcpath, char* renditions, int samplenum)
#endif
{
#ifdef Py_PYTHON_H
	char* srcpath = NULL;
	char* renditions = NULL;
	int samplenum = 0;
	PyArrayObject *final_arr = NULL;
	if (!PyArg_ParseTuple(args, "ssi", &srcpath, &renditions, &samplenum)) return NULL;
#endif

	if (srcpath == NULL || renditions == NULL) {
#ifdef Py_PYTHON_H
		return NULL;
#else
		return LP_ERROR_NULL_POINT;
#endif
	}
		
	if (samplenum <= 0 || samplenum >= MAX_NUM_SAMPLES) {
#ifdef Py_PYTHON_H
		return NULL;
#else
		return LP_ERROR_INVALID_PARAM;
#endif
	}	

	int ret, i, j, k, adiff, nvideonum;
	struct stat fstatus;

	LPDecContext* pcontext = NULL;
	LPDecContext* ptmpcontext = NULL;

	//create context and read	
	pcontext = (LPDecContext*)malloc(sizeof(LPDecContext) * MAX_NUM_RENDITIONS);
	memset(pcontext, 0x00, sizeof(LPDecContext) * MAX_NUM_RENDITIONS);

	//set master video path
	strcpy(pcontext[0].path, srcpath);
	//parser renditions url
	int slen = strlen(renditions);
	int sstart, sfinddot;
	sstart = sfinddot = 0;	
	nvideonum = 1;
	for ( i = 0; i < slen; i++)
	{
		if (renditions[i] == '.') sfinddot = 1;
		if (renditions[i] == ',') {
			if (sfinddot && (i - sstart) > 2) {				
				strncpy(pcontext[nvideonum].path, renditions+ sstart, i - sstart);
				sstart = i + 1;
				sfinddot = 0;
				nvideonum++;
				if (nvideonum >= MAX_NUM_RENDITIONS) break;
			}
		}
	}
	if ((i - sstart) > 2 && sfinddot && nvideonum < MAX_NUM_RENDITIONS) {
		strncpy(pcontext[nvideonum].path, renditions + sstart, i - sstart);
		nvideonum++;
	}
	
	for (i = 0; i < nvideonum; i++) {
		ptmpcontext = pcontext + i;
		ptmpcontext->audio_stream = -1;
		ptmpcontext->video_stream = -1;
		ptmpcontext->samplecount = samplenum;
		ret = stat(pcontext[i].path, &fstatus);
		if (ret == 0) {
			ptmpcontext->filesize = fstatus.st_size;			
		}
		ret = open_context(ptmpcontext);
	}	
	//pre verify with metadata
	pre_verify(pcontext, nvideonum);
	//grab all frames for compare
	grab_allframes(pcontext, nvideonum);

	remove_nullframe(pcontext, nvideonum);

#ifdef 	_DEBUG
	//debug_printvframe(pcontext, nvideonum);
	//debug_saveimage(pcontext, nvideonum);
#endif

	//calculate features and matrix		
	//first compare audio buffer
	if (pcontext->aliveaudio) {
		for (i = 1; i < nvideonum; i++) {
			adiff = 0;
			ptmpcontext = pcontext + i;
			for (j = 0; j < pcontext->samplecount; j++) {
				if(pcontext->listaudio[i] && ptmpcontext->listaudio[i])
					adiff += memcmp(pcontext->listaudio[i]->data, ptmpcontext->listaudio[i]->data, pcontext->listaudio[i]->size);				
			}
			//set matrix value
			if (adiff)
				ptmpcontext->audiodiff = adiff;
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
	if (pcontext->alivevideo) {
		for (i = 1; i < nvideonum; i++) {
			ptmpcontext = pcontext + i;
			aggregate_matrix(ptmpcontext);
		}
	}

#ifdef 	_DEBUG
	debug_printmatrix(pcontext, nvideonum);
#endif	
	//make python matrix and return		
	//matirx column is [metatamper, videoalive, audioalive, fps, width, height, audiodiff, sizeratio, dctdiff, gaussiandiff, gaussiamse, gaussianthreshold, histogramdiff]
#ifdef Py_PYTHON_H
	npy_intp dims[2];
	dims[0] = nvideonum - 1;
	dims[1] = 13;
	final_arr = (PyArrayObject*)PyArray_ZEROS(2, dims, NPY_FLOAT32, 0);
		
	//memcpy(final_arr->data, fnormal, buffersize);
	//matrix setting
	float* pdata = (float*)final_arr->data;
	for (i = 1; i < nvideonum; i++){
		ptmpcontext = pcontext + i;
		pdata[0] = ptmpcontext->tamper; pdata[1] = ptmpcontext->alivevideo; pdata[2] = ptmpcontext->aliveaudio; pdata[3] = ptmpcontext->fps;
		pdata[4] = ptmpcontext->width; pdata[5] = ptmpcontext->height; pdata[6] = ptmpcontext->audiodiff; 
		pdata[7] = (float)ptmpcontext->filesize / (float)(ptmpcontext->width * ptmpcontext->height);
		for ( j = 0; j < LP_FT_FEATURE_MAX; j++){
			pdata[8 + j] = ptmpcontext->ftmatrix[j];
		}
#ifdef _DEBUG
		//for debug
		fprintf(stderr, "feature vid (%d) :", i - 1);
		for (j = 0; j < 13; j++) {
			fprintf(stderr, "%lf ", pdata[j]);
		}
		fprintf(stderr, "\n");
#endif
		pdata += 13;
	}
#endif

	for (i = 0; i < nvideonum; i++) {
		close_context(pcontext + i);
	}
	if (pcontext) free(pcontext);

#ifdef Py_PYTHON_H
	return PyArray_Return(final_arr);
#else
	return LP_OK;
#endif
}
#endif

#ifdef _DEBUG
void debug_saveimage(LPDecContext* lpcontext, int videonum)
{
	int i; 
	for (i = 0; i < videonum; i++) {
		LPDecContext* ptmpcontext = lpcontext + i;
		char tmppath[MAX_PATH] = { 0, };
		uint8_t *pbuffer = (uint8_t*)malloc(sizeof(uint8_t) * ptmpcontext->normalh * ptmpcontext->normalw * 3);
		for (int j = 0; j < ptmpcontext->samplecount; j++)
		{
			if (ptmpcontext->alivevideo && ptmpcontext->listfrmame[j] != NULL) {

				sprintf(tmppath, "D:/tmp/%02d_%02d.bmp", i, j);
				//get rgb bugffer
				uint8_t *src = (uint8_t*)ptmpcontext->listfrmame[j]->data[0];
				memcpy(pbuffer, src, ptmpcontext->normalh * ptmpcontext->normalw * 3 * sizeof(uint8_t));
				WriteColorBmp(tmppath, ptmpcontext->normalw, ptmpcontext->normalh, pbuffer);
			}
			if (ptmpcontext->aliveaudio) {

			}
		}
		if (pbuffer) free(pbuffer);
	}
}
void debug_printvframe(LPDecContext* lpcontext, int videonum)
{
	int i, j;
	//check buffer
	if (lpcontext->alivevideo) {
		fprintf(stderr, "video frame ids(%d): ", lpcontext->samplecount);
		for (int j = 0; j < lpcontext->samplecount; j++)
		{
			fprintf(stderr, "%d (%03d_%lf), ", j, lpcontext->frameindexs[j], lpcontext->framestamps[j]);
		}
		fprintf(stderr, "\n");

		for (i = 0; i < videonum; i++) {
			LPDecContext* ptmpcontext = lpcontext + i;
			fprintf(stderr, "video %d Skipped frame ids: ", i);
			for (int j = 0; j < ptmpcontext->samplecount; j++)
			{
				if (ptmpcontext->alivevideo && ptmpcontext->listfrmame[j] == NULL) {
					fprintf(stderr, "%d (%03d_%lf), ", j, lpcontext->frameindexs[j], ptmpcontext->framestamps[j]);
				}
			}
			fprintf(stderr, "\n");
		}
	}
}
void debug_printmatrix(LPDecContext* lpcontext, int videonum)
{
	int i;
	for (i = 1; i < videonum; i++) {
		LPDecContext* ptmpcontext = lpcontext + i;
		double* pout = ptmpcontext->ftmatrix;
		fprintf(stderr, "aggregate(%d) :", ptmpcontext->samplecount - 1);
		for (int j = 0; j < LP_FT_FEATURE_MAX; j++)
		{
			fprintf(stderr, "%lf ", *(pout + j));
		}
		fprintf(stderr, "\n");
	}	
}
#endif