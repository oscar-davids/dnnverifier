from distutils.core import setup, Extension
import numpy as np

featuremaker_utils_module = Extension('extractfts',
		sources = ['extractfts.c'],
		include_dirs=[np.get_include(), './ffmpeg-4.2.1/include/'],
		extra_compile_args=['-DNDEBUG', '-O3'],
		extra_link_args=['avformat.lib', 'avfilter.lib', 'avcodec.lib', 'avutil.lib', 'swscale.lib', '-LIBPATH:"D:/work/vqwork/dnnverifier/extractfts/ffmpeg-4.2.1/lib/"']
)

setup ( name = 'extractfts',
	version = '0.1',
	description = 'Utils for training.',
	ext_modules = [ featuremaker_utils_module ]
)
