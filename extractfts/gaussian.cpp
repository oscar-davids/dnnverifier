/***************************************************************************
Gaussian blur filtering test
 ***************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <climits>

#define PI_F (3.14159265359f)
#define MAX_KERNEL_SIZE 4096

#define SWAPT(a, b) {\
  float* t = a;\
  a = b;\
  b = t;\
}

typedef struct gaussianimg {
	int depth;
	int height;
	int width;	
	int pixels;
	float* image_d;
	float* image;
	float* scratch;
	int cuda;
} gaussianimg;

int getKernelSizeForSigma(float sigma) {
  int size = int(ceilf(sigma * 6));
  if (size % 2 == 0) {
    ++size;
  }
  if (size < 3) {
    size = 3;
  }
  if (size >=  MAX_KERNEL_SIZE) {
    size = MAX_KERNEL_SIZE;
  }
  return size;
}

void fillGaussianBlurKernel1D(float sigma, int size, float* kernel) {
	float sigma2 = sigma * sigma;
	int middle = size / 2;
	for (int i = 0; i < size; ++i) {
		float distance = middle - i;
		float distance2 = distance * distance;
		float s = 1.0f / (sigma * sqrtf(2.0f*PI_F)) * expf(-distance2 / (2.0f * sigma2));
		kernel[i] = s;
	}
}

void fillGaussianBlurKernel2D(float sigma, int size, float* kernel) {
	float sigma2 = sigma * sigma;
	int middle = size / 2;
	
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			const int idx = y * size + x;
			float x_dist = middle - x;
			float y_dist = middle - y;				
			float distance2 = x_dist * x_dist + y_dist * y_dist ;
			float s = 1.0f / (sigma * sqrt(2.0f * PI_F)) * expf(-distance2 / (2.0f * sigma2));
			kernel[idx] = s;
		}
	}	
}

void Init_GaussianBlur(gaussianimg *gimg, float* img, int w, int h, int d, bool use_cuda) {  
	gimg->height = h;
	gimg->width = w;
	gimg->depth = d;
	gimg->image_d = NULL;
	gimg->image = NULL;
	gimg->scratch = NULL;  
	gimg->pixels = gimg->height * long(gimg->width) * long(gimg->depth);

	gimg->cuda = use_cuda;
#ifndef USE_CUDA
	gimg->cuda = false;
#endif

	gimg->image = new float[gimg->pixels];
#ifdef USE_CUDA
  if (gimg->cuda) {
	  gimg->image_d = (float*) alloc_cuda_array(gimg->pixels * sizeof(float));
	  gimg->scratch_d = (float*) alloc_cuda_array(gimg->pixels * sizeof(float));
    if (image_d == NULL || scratch_d == NULL) {
		gimg->cuda = false;
      free_cuda_array(gimg->image_d);
      free_cuda_array(gimg->scratch_d);
    } else {
      copy_array_to_gpu(gimg->image_d, img, gimg->pixels * sizeof(float));
      copy_array_to_gpu(gimg->scratch_d, img, gimg->pixels * sizeof(float));
    }
  } 
  if (!gimg->cuda)
#endif
  {
	  gimg->scratch = new float[gimg->pixels];
      memcpy(gimg->image, img, gimg->pixels * sizeof(float));
  }
}

void Exit_GaussianBlur(gaussianimg *gimg) {
	delete [] gimg->image;
#ifdef USE_CUDA
	if (cuda) {
		free_cuda_array(gimg->image_d);
		free_cuda_array(gimg->scratch_d);
	} else
#endif
    delete [] gimg->scratch;
}


#if defined(USE_CUDA)
void blur_cuda(gaussianimg *gimg, float sigma) {
  PROFILE_PUSH_RANGE("GaussianBlur<float>::blur_cuda()", 7);

  if (true) {  //TODO temp hack to test 3D gaussian kernel
    int size = getKernelSizeForSigma(sigma);
    float kernel[size];
    fillGaussianBlurKernel1D(sigma, size, kernel);
    set_gaussian_1D_kernel_cuda(kernel, size);

    gaussian1D_x_cuda(gimg->image_d, gimg->scratch_d, size, gimg->width, gimg->height, gimg->depth);
    SWAPT(image_d, scratch_d);

    gaussian1D_y_cuda(gimg->image_d, gimg->scratch_d, size, gimg->width, gimg->height, gimg->depth);
    SWAPT(image_d, scratch_d);

    gaussian1D_z_cuda(gimg->image_d, gimg->scratch_d, size, gimg->width, gimg->height, gimg->depth);
    SWAPT(gimg->image_d, gimg->scratch_d);
	        
  } else {
    int size = getKernelSizeForSigma(sigma);
    float kernel[size * size * size];
    fillGaussianBlurKernel3D(sigma, size, kernel);
  }

  PROFILE_POP_RANGE();
}
#endif

static inline void assign_float_to_dest(float val, float* dest) {  
  dest[0] = val;
}

void blur_cpu(gaussianimg *gimg, float sigma) {
  int x, y, z;
  int size = getKernelSizeForSigma(sigma);
  float *kernel = new float[size];
  fillGaussianBlurKernel1D(sigma, size, kernel);
  const int offset = size >> 1;
  SWAPT(gimg->scratch, gimg->image);

  /* X kernel */
#ifdef USE_OMP
#pragma omp parallel for schedule(static)
#endif
  for (z = 0; z < gimg->depth; ++z) {
    for (y = 0; y < gimg->height; ++y) {
      for (x = 0; x < gimg->width; ++x) {
        const long idx = z* gimg->height*gimg->width + y*long(gimg->width) + x;
        const int new_offset_neg = x - offset >= 0 ? -offset :  -x;
        const int new_offset_pos = x + offset < gimg->width ? offset : gimg->width - x - 1;
        float value = 0.0f;
        int i;
        for (i = -offset; i < new_offset_neg; ++i) {
          value += gimg->scratch[idx + new_offset_neg] * kernel[i+offset];
        }

        for (; i <= new_offset_pos; ++i) {
          value += gimg->scratch[idx + i] * kernel[i+offset];
        }

        for (; i <= offset; ++i) {
          value += gimg->scratch[idx + new_offset_pos] * kernel[i+offset];
        }

        assign_float_to_dest(value, &gimg->image[idx]);
      }
    }
  }
  SWAPT(gimg->scratch, gimg->image);

  /* Y kernel */
#ifdef USE_OMP
#pragma omp parallel for schedule(static)
#endif
  for (z = 0; z < gimg->depth; ++z) {
    for (y = 0; y < gimg->height; ++y) {
      const int new_offset_neg = y - offset >= 0 ? -offset :  -y;
      const int new_offset_pos = y + offset < gimg->height ? offset : gimg->height - y - 1;
      for (x = 0; x < gimg->width; ++x) {
        const long idx = z* gimg->height*gimg->width + y*long(gimg->width) + x;
        float value = 0.0f;
        int i;

        for (i = -offset; i < new_offset_neg; ++i) {
          value += gimg->scratch[idx + new_offset_neg* gimg->width] * kernel[i+offset];
        }

        for (; i <= new_offset_pos; ++i) {
          value += gimg->scratch[idx + i* gimg->width] * kernel[i+offset];
        }

        for (; i <= offset; ++i) {
          value += gimg->scratch[idx + new_offset_pos* gimg->width] * kernel[i+offset];
        }

        assign_float_to_dest(value, &gimg->image[idx]);
      }
    }
  }
  SWAPT(gimg->scratch, gimg->image);

  /* Z kernel */
#ifdef USE_OMP
#pragma omp parallel for schedule(static)
#endif
  for (z = 0; z < gimg->depth; ++z) {
    const int new_offset_neg = z - offset >= 0 ? -offset :  -z;
    const int new_offset_pos = z + offset < gimg->depth ? offset : gimg->depth - z - 1;
    for (y = 0; y < gimg->height; ++y) {
      for (x = 0; x < gimg->width; ++x) {
        const long idx = z* gimg->height*gimg->width + y*long(gimg->width) + x;
        float value = 0.0f;
        int i;

        for (i = -offset; i < new_offset_neg; ++i) {
          value += gimg->scratch[idx + new_offset_neg* gimg->height*gimg->width] * kernel[i+offset];
        }

        for (; i <= new_offset_pos; ++i) {
            value += gimg->scratch[idx + i* gimg->height*gimg->width] * kernel[i+offset];
        }

        for (; i <= offset; ++i) {
          value += gimg->scratch[idx + new_offset_pos* gimg->height*gimg->width] * kernel[i+offset];
        }

        assign_float_to_dest(value, &gimg->image[idx]);
      }
    }
  }

  delete [] kernel;
}

void Run_GaussianBlur(gaussianimg *gimg, float sigma) {
#ifdef USE_CUDA
	if (cuda) {
		blur_cuda(gimg, sigma);
	}
	else
#endif
		blur_cpu(gimg, sigma);
}
