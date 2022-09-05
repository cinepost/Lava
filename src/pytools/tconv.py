import numpy as np
import cv2 as cv
import imageio as oiio
import sys, os

import argparse

def convert_oiio(in_imagepath, out_imagepath, out_format):
	if not os.path.isfile(exr_file):
	return False

	im = oiio.imread(in_imagefile)
	image = image[:,:,:3]

	data = image * 65535
	data[data>65535]=65535
	image = data.astype('uint16')

	oiio.imwrite(out_filename, image, format=out_format)
	return True

def convert_cv(in_imagepath, out_imagepath):
	if not os.path.isfile(in_imagepath):
	return False

	image = cv.imread(in_imagepath, -1)
	image = image * 65535
	image[image>65535] = 65535
	image = np.uint8(image)
	result = cv.imwrite(out_imagepath, image)

	return result