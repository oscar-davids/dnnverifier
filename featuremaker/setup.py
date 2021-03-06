from distutils.core import setup, Extension
import numpy as np

featuremaker_utils_module = Extension('featuremaker',
		sources = ['featuremaker.c'],
		include_dirs=[np.get_include(), './ffmpeg-4.2.1/include/'],
		extra_compile_args=['-DNDEBUG', '-O3'],
		extra_link_args=['-lavformat', '-lavfilter', '-lavcodec', '-lavutil', '-lswscale', '-L./ffmpeg-4.2.1/lib/']
)

setup ( name = 'featuremaker',
	version = '0.1',
	description = 'Utils for training.',
	ext_modules = [ featuremaker_utils_module ]
)
