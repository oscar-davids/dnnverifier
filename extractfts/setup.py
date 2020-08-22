from distutils.core import setup, Extension
import numpy as np
import platform
import os.path

lipath = os.path.abspath(os.path.dirname(__file__))
if platform.system() == "Windows":
    featuremaker_utils_module = Extension('extractfts',
		sources = ['extractfts.cpp'],
		include_dirs=[np.get_include(), './ffmpeg-4.2.1/include/'],
		extra_compile_args=['-DNDEBUG', '-O3'],
		extra_link_args=['avformat.lib', 'avfilter.lib', 'avcodec.lib', 'avutil.lib', 'swscale.lib', '-LIBPATH:"./ffmpeg-4.2.1/lib/"']		
	)
else:
	featuremaker_utils_module = Extension('extractfts',
		sources = ['extractfts.cpp'],
		include_dirs=[np.get_include(), '/home/ubuntunik/libshare/include/'],
		extra_compile_args=['-DNDEBUG', '-O3'],		
		extra_link_args=[  '-lavformat', '-lavfilter', '-lavcodec', '-lavutil', '-lswscale', '-L/home/ubuntunik/libshare/lib/']
	)

setup ( name = 'extractfts',
	version = '0.1',
	description = 'Utils for training.',
	ext_modules = [ featuremaker_utils_module ]
)
