import numpy as np
import cv2 as cv
import imageio as iio
import sys, os

from enum import Enum

class NormalMapConversionType(Enum):
	G2B = 'gb'
	B2G = 'bg'
	
	def __str__(self):
		return self.value


def convert_oiio(input_file_name, output_file_name, conversion_mode, verbose_output):
	if not input_file_name: return False
	if not output_file_name: return False

	src_image = iio.imread(input_file_name)
	normal_data = src_image[:,:,:3].astype(float) / np.iinfo(src_image.dtype).max
	
	if src_image.dtype in [np.uint8, np.uint16, np.uint32]:
		normal_data = 2.0 * (normal_data - 0.5)


	if verbose_output:
		print("Converting normal map", input_file_name, "using", conversion_mode, "conversion mode.")
		print("Input data type: ", src_image.dtype)
		print("Input image shape: ", src_image.shape)

	if conversion_mode == NormalMapConversionType.B2G:
		normal_data = normal_data[:,:,:2] # only keep RG channels for XY normal components
	else:
		normal_data[:,:,2] = np.sqrt(1.0 - np.clip((np.square(normal_data[:,:,0]) + np.square(normal_data[:,:,1])), 0.0, 1.0 ))

	if src_image.dtype in [np.uint8, np.uint16, np.uint32]:
		normal_data = 0.5 * (normal_data + 1.0)

	normal_data = normal_data * np.iinfo(src_image.dtype).max
	dst_image = normal_data.astype(src_image.dtype) # Make destination dtype the same as source

	if verbose_output:
		print("Output data type: ", dst_image.dtype)
		print("Output image shape: ", dst_image.shape)

	iio.imwrite(output_file_name, dst_image)
	return True


if __name__ == '__main__':
	import argparse

	parser = argparse.ArgumentParser()
	parser.add_argument("-m", "--mode", action='store', type=NormalMapConversionType, dest='mode', choices=list(NormalMapConversionType), 
		default= NormalMapConversionType.G2B, help="Normal map conversion mode")
	parser.add_argument("-i", "--input", action='store', type=argparse.FileType('rb'), dest='input', required=True, help="Input normal map file path")
	parser.add_argument("-o", "--output", action='store', type=argparse.FileType('w'), dest='output', required=True, help="Output normal map file path")
	parser.add_argument("-v", action="store_true", dest='verbose_output', default=False, help="Verbose output")

	opts = parser.parse_args()
	if opts.verbose_output:
		if not opts.output: 
			print("Error! No output file name specified!")
			exit(False)

	convert_oiio(opts.input.name, opts.output.name, opts.mode, opts.verbose_output)