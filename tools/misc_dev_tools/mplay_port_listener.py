#!/usr/bin/env python3

# This tool fakes MPlay presence so we can listen and debug acutual data sent by houdini

import os, sys, socket, time
import atexit

def createLockFile(filename, port_number):
	try:
		with open(filename, 'w') as f:
			f.write('00000 ' + str(port_number))
	except FileNotFoundError:
		print("Error creating lock file. Path not exist!")
		return False
	except FileExistsError:
		print("Error creating lock file. File already exist!")
		return False

	return True

def removeLockFile(filename):
	if os.path.isfile(filename):
		os.remove(filename)
		print("Listener lock file removed.")
	else:
		print("Error removing listener mplay lock file!")

	return

def main():
	buffer_read_size = 512
	default_mplay_port = 23456
	user_home_directory = os.path.expanduser( '~' )
	
	HOSTNAME = socket.gethostname()
	user_houdini_directory = os.environ.get('HIH')

	default_mplay_lock_filename = (user_houdini_directory or user_home_directory) + "/.mplay_lock"
	if HOSTNAME:
		default_mplay_lock_filename += '.' + HOSTNAME

	user_mplay_lock_filename = input("MPlay lock file name (" + default_mplay_lock_filename + "): ")
	user_mplay_port_string = input("MPlay port (" + str(default_mplay_port) + "): ")

	mplay_lock_filename = user_mplay_lock_filename or default_mplay_lock_filename

	if user_mplay_port_string:
		mplay_port = int(user_mplay_port_string)
	else:
		mplay_port = default_mplay_port
	
	print("Listening port " + str(mplay_port) + " using lock file: " + mplay_lock_filename)
	if not createLockFile(mplay_lock_filename, mplay_port):
		return

	atexit.register(removeLockFile, mplay_lock_filename)

	server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	
	try:
		server_socket.bind((HOSTNAME or 'localhost', mplay_port))

	except socket.error as msg:
		print('Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1])
		sys.exit()

	print('Socket bind complete')

	server_socket.connect((HOSTNAME or 'localhost', mplay_port))

	received_data_count = 0

	while True:
		time.sleep(5)
		data = server_socket.recv(buffer_read_size)
		received_data_count += buffer_read_size
		#if data.lower() == 'q':
		#	server_socket.close()
		#	break

		print("RECEIVED: %s" % data)
		data = input("SEND( TYPE q or Q to Quit):")
		#client_socket.send(data)
		
		if data.lower() == 'q':
			server_socket.close()
			break

if __name__ == "__main__":
	main()