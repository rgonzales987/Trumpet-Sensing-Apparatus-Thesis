######################################
# This program was developed with help and code from the following sources:
# 
# - https://learn.sparkfun.com/tutorials/raspberry-gpio/all, for working with GPIO pins
# - https://www.geeksforgeeks.org/python/python-os-mkdir-method/, for creating a directory
# - https://www.geeksforgeeks.org/python/python-subprocess-module/, for running commands in a terminal
# - https://www.raspberrypi.com/documentation/computers/camera_software.html#create-a-time-lapse-video, for working with rpicam-apps
# - https://medium.com/@martin.hodges/setting-up-a-mems-i2s-microphone-on-a-raspberry-pi-306248961043, for setting up the ICS-43434
# - https://learn.adafruit.com/adafruit-i2s-mems-microphone-breakout/raspberry-pi-wiring-test, for setting up the ICS-43434
# - https://www.geeksforgeeks.org/python/reading-and-writing-lists-to-a-file-in-python/, for writing to a file
#
######################################


# import all necessary libraries
import RPi.GPIO as GPIO
import os
import serial
import subprocess
import time

# setup GPIO pins
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

ON_LED = 17
STATUS_LED = 22
RECORD_LED = 27
RECORD_BUTTON = 24
TARE_BUTTON = 23
RESET_PIN = 25

GPIO.setup(ON_LED, GPIO.OUT)
GPIO.setup(STATUS_LED, GPIO.OUT)
GPIO.setup(RECORD_LED, GPIO.OUT)
GPIO.setup(RECORD_BUTTON, GPIO.IN)
GPIO.setup(TARE_BUTTON, GPIO.IN)
GPIO.setup(RESET_PIN, GPIO.OUT)

######################################
# FUNCTION DEFINITIONS

# Places a zero in front of the provided time reading, if the time reading is less than 10.
# Ex: 9 --> 09, 22 --> 22, 0 --> 00
#
# @param value: The time reading to evaluate; always a non-negative value
# @return The time reading as is, or with a leading 0 if less than 10
#
def appendZero(value):
	if (value < 10): return '0' + str(value)
	else: return str(value)

# Reads the five values at each data collection time point.
# In order: differential presssure; gauge pressure; strain gauges 1, 2, 3
#
# @param ser_obj: The serial object used to communicate with the Arduino
# @return A list of size 5 with the data values, or an empty list if an error occured
#
def read_inputs(ser_obj):
	try:
		time = float(ser_obj.readline().decode('utf-8').rstrip())
		p_diff = float(ser_obj.readline().decode('utf-8').rstrip())
		p_gauge = float(ser_obj.readline().decode('utf-8').rstrip())
		strain_1 = float(ser_obj.readline().decode('utf-8').rstrip())
		strain_2 = float(ser_obj.readline().decode('utf-8').rstrip())
		strain_3 = float(ser_obj.readline().decode('utf-8').rstrip())

		# return [time, p_diff, p_gauge]
		return [time, p_diff, p_gauge, strain_1, strain_2, strain_3]
	except:
		print("Invalid data, ignoring measurement line")
		return []

# Saves the pressure and force data to a provided file.
#
# @param data: The array containing all recorded values from this collection process
# @param path: The filepath to save the data to
#
def save_inputs(data, path):
    with open(path, 'w+') as file:
        for i in range(0, len(data)):
            data_line = str(i + 1) + ' '
            for item in data[i]:
                data_line += str(item) + ' '
            file.write(data_line + '\n')
        
        print("Finished writing data to file")
    file.close()

######################################

# create directory name using current time
CURRENT_TIME = time.gmtime()
YEAR = appendZero(CURRENT_TIME[0])
MONTH = appendZero(CURRENT_TIME[1])
DATE = appendZero(CURRENT_TIME[2])
HOUR = appendZero(CURRENT_TIME[3])
MIN = appendZero(CURRENT_TIME[4])
SEC = appendZero(CURRENT_TIME[5])

PARENT_DIR = '/home/rg26/DataCollect'
TIME_STRING = YEAR + MONTH + DATE + '_' + HOUR + MIN + SEC
SESSION_DIR = os.path.join(PARENT_DIR, TIME_STRING)
os.mkdir(SESSION_DIR)

# other constant variables
BAUD_RATE = 115200	# for serial communication with Arduino
GAIN = 2 			# units: decibels (dB)


######################################
# MAIN FUNCTIONALITY

if __name__ == '__main__':
	try:
		# Turn on indicator LED, make sure others are off
		GPIO.output(ON_LED, GPIO.HIGH)
		GPIO.output(STATUS_LED, GPIO.LOW)
		GPIO.output(RECORD_LED, GPIO.LOW)
		
		# Ask the user to input the recording time for the whole session
		print('Ensure mouthpiece is inserted, and remove all weight from it')
		data_duration = input("Insert recording time in seconds: ")
		valid = False

		# Removes all invalid inputs (non-numbers and non-positive values)
		while(valid == False):
			try:
				data_duration = int(data_duration)
				if (data_duration > 0): 
					valid = True
				else:
					data_duration = input("Time must be a non-negative integer. Insert recording time in seconds: ")
			except:
				data_duration = input("Invalid input, insert recording time in seconds: ")
		
		# For the RPi4B, to allow all serial data to get through
		LOOP_DURATION = data_duration + 1

		# The Arduino needs to be reset for this code to function properly
		print('Wait for Arduino to come online')
		GPIO.output(RESET_PIN, GPIO.LOW)
		time.sleep(0.5)
		GPIO.output(RESET_PIN, GPIO.HIGH)
		time.sleep(3)

		# Connect to the Arduino using declared baud rate
		ser = serial.Serial('/dev/serial0', BAUD_RATE, timeout=0.1)

		# Clear any input already made by the Arduino
		ser.reset_input_buffer()

		# Wait for Arduino to be ready to take data
		start_line = ""
		while(start_line != "Online"):
			if ser.in_waiting > 0:
				start_line = ser.readline().decode('utf-8').rstrip()
		print('Arudino is online')

		# Send collection duration to Arduino, and receive confirmation back
		duration_line = str(data_duration) + '\n'
		ser.write(duration_line.encode("utf-8"))
		while(ser.in_waiting <= 0):
			time.sleep(0.1)
		print('Ready to begin recording')
		print('Confirmed recording time: ' + ser.readline().decode('utf-8').rstrip() + ' seconds')

		# Turn on indicator LED
		GPIO.output(STATUS_LED, GPIO.HIGH)

		# global changing variables
		data_array = []
		recording = False
		startTime = time.time()
		counter = 1

		# set mic gain to 2 dB
		gain_command = 'amixer -D hw:1 sset \'Mic\' ' + str(GAIN) + 'dB'
		subprocess.Popen(gain_command, shell = True)

		while(True):
			# if record button is pressed, or if it was pressed earlier and data is being recorded
			if (GPIO.input(RECORD_BUTTON) or recording == True):
				current_time = time.time()

				# if data wasn't being recorded yet
				if (recording == False):
					# remove any input sent before by the Arduino
					ser.reset_input_buffer()

					# prepare for data collection
					print('Starting data collection')
					GPIO.output(STATUS_LED, GPIO.LOW)
					GPIO.output(RECORD_LED, GPIO.HIGH)
					startTime = time.time()

					# make a new directory for this specific recording instance
					recording_dir = os.path.join(SESSION_DIR, 'Recording_' + str(counter))
					os.mkdir(recording_dir)

					# camera recording command
					cam_path = os.path.join(recording_dir, 'camera.mov')
					cam_command = 'rpicam-vid --level 4.2 --framerate 120 --width 1280 --height 720 --denoise cdn_off -n -t ' + str(LOOP_DURATION) + 's -o ' + cam_path 

					# microphone recording command
					mic_path = os.path.join(recording_dir, 'mic.wav')
					mic_command = 'arecord -D mic_sv -f S32_LE -r 48000 -c 2 ' + mic_path + ' -d ' + str(LOOP_DURATION)

					# execute the recording commands
					subprocess.Popen(cam_command, shell = True)
					subprocess.Popen(mic_command, shell = True)

					# tell Arduino to start sending serial data
					ser.write(b'Start\n')

					# update necessary variables
					recording = True
					counter += 1

				# if the recording is over
				elif (current_time - startTime >= LOOP_DURATION):
					GPIO.output(RECORD_LED, GPIO.LOW)
					print('Ending data collection')

					# save serial data to the file at serial_path
					recording_dir = os.path.join(SESSION_DIR, 'Recording_' + str(counter - 1))
					serial_path = os.path.join(recording_dir, 'pressure_strain.txt')
					save_inputs(data_array, serial_path)
					
					# update necessary variables
					data_array = []
					recording = False

					# let the microphone and camera commands fully close out
					print('Wait for camera and microphone to reset')
					time.sleep(2)
					print('Ready for next recording')

					GPIO.output(STATUS_LED, GPIO.HIGH)

				# if the recording is in progress
				else:
					# read the next line if it exists
					# if it starts with "Data", it's good data; collect it
					# if it starts with "Ending", the Arduino is done sending data
					if ser.in_waiting > 0:
						line = ser.readline().decode('utf-8').rstrip()
						if (line.startswith("Data")):
							data_array.append(read_inputs(ser))
						elif (line.startswith("Ending")):
							print("Serial data finished sending")

			# if tare button is pressed, reset the strain gauges
			elif (GPIO.input(TARE_BUTTON)):
				# tell Arduino to reset gauges
				ser.write(b'Zero\n')
				GPIO.output(STATUS_LED, GPIO.LOW)

				# wait for Arduino to send back a finished message
				print('Wait for tare process to finish')
				line = ""
				while(line != "Finished"):
					if ser.in_waiting > 0:
						line = ser.readline().decode('utf-8').rstrip()
						time.sleep(0.1)

				# allow for recording again
				print('Tare process finished, ready for next recording')
				GPIO.output(STATUS_LED, GPIO.HIGH)
				
	# hit CTRL+C to end the program
	except KeyboardInterrupt:
		print('Stopping data collection')
		GPIO.cleanup()
		ser.close()

	# if any other error occurs
	except:
		print('Error occured, shutting down data collection')
		GPIO.cleanup()
		ser.close()