import sys
import pyaudio
import numpy as np
import time
import smbus
import serial
from datetime import datetime
from threading import Thread, Event
from pathlib import Path
import pickle
from scipy.signal import butter, lfilter
import noisereduce as nr

np.set_printoptions(threshold=sys.maxsize)
ser = serial.Serial('/dev/ttyUSB0',9600,timeout=1)
ser.flush()
CHUNK = 512
FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 32000
RECORD_SECONDS = 6
CSV_OUTPUT_FILENAME = "/home/creampi88/unripe_4.csv"
lowcut=200
highcut=10000



#ADS1115 address
ADS1115_ADDRESS = 0x48

#Register address
ADS1115_REG_POINTER_CONVERT = 0x00
ADS1115_REG_POINTER_CONFIG = 0x01


#Configure the ADS1115
def configure_ads1115():
    #Get I2C bus
    bus = smbus.SMBus(1) #Use 0 for older Ras
    
    #Configure the ADC
    #Set the gain (PGA) to 2.048V and single-shot mode
    config = [0x84, 0x83]
    bus.write_i2c_block_data(ADS1115_ADDRESS,ADS1115_REG_POINTER_CONFIG,config)

#Read ADC value from ADS1115
def read_adc(result_list, rec_sec, event):
    #Get I2C bus
    bus = smbus.SMBus(1)
    
    #Read the conversion register
   
    start_time = time.time()
    vibrations=[]
    
    #Convert the data to a 16-bit signed integer
    while time.time() - start_time < rec_sec:
        data = bus.read_i2c_block_data(ADS1115_ADDRESS, ADS1115_REG_POINTER_CONVERT, 2)
        value = (data[0] << 8) + data[1]
        if value > 32767:
            value -= 65536
        result_list.append(value)
    event.set()


def serial_receive():
    while True:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8').rstrip()
            print(line)
            data = line.split(',')
            weight = data[0]
            diameter = data[1]
            ripeness = data[2]
            return True, weight, diameter, ripeness

def record_audio(result_list):
    p = pyaudio.PyAudio()
    stream = p.open(format=FORMAT,
                channels=CHANNELS,
                rate=RATE,
                input=True,
                frames_per_buffer=CHUNK)
    print("* recording")
    frames = []
    for i in range(0, int(RATE / CHUNK * RECORD_SECONDS)):
        data = stream.read(CHUNK)
        frames.append(data)
    print("* done recording")
    stream.stop_stream()
    stream.close()
    p.terminate()
    joined_frames = b''.join(frames)
    sig = np.frombuffer(joined_frames, dtype='<i2')
    result_list.extend(np.array(sig).flatten())

def save_data_to_csv(data, filename):
    file_path = Path(filename)
    
    if not file_path.is_file():
        np.savetxt(file_path, data, delimiter = ',', fmt='%s')
        return
    try:
        previous_data = np.genfromtxt(file_path, delimiter=',', dtype=str)
    except FileNotFoundError:
        previous_data = np.array([]).reshape(-1, 0)

    if previous_data.ndim == 1:
        previous_data = previous_data.reshape(-1, 1)
    
    data = np.pad(data.reshape(-1,1), ((0, previous_data.shape[0] - data.shape[0]), (0, 0)), mode='constant', constant_values='')

    data_to_save = np.concatenate((previous_data, data), axis=1)

    np.savetxt(filename, data_to_save, delimiter=',', fmt='%s')
    
    
def butter_bandpass(lowcut,highcut,fs, order=8):
    nyquist=0.5*fs
    low=lowcut/nyquist
    high=highcut/nyquist
    b, a = butter(order, [low,high], btype='band')
    return b,a

def butter_bandpass_filter(data,lowcut,highcut,fs, order):
    b, a = butter_bandpass(lowcut,highcut,fs, order=order)
    y=lfilter(b,a,data)
    return y

def main():
    configure_ads1115()
    with open('mlp_model.pkl','rb') as f:
        mlp = pickle.load(f)
    while True:
        received, weight, diameter, ripeness  = serial_receive()
        arduino_data = np.array([weight, diameter, ripeness]).reshape(-1,1)
        if received:
            result_list1 = []
            result_list2 = []
            data_collection_event = Event()
            audio_thread = Thread(target = record_audio,args = (result_list1,))
            vibrations_thread = Thread(target = read_adc,args = (result_list2, RECORD_SECONDS,data_collection_event))
            
            audio_thread.start()
            vibrations_thread.start()
            
            audio_thread.join()
            
            data_collection_event.wait()
            
            vibrations_thread.join()
            
            audio = np.array(result_list1)
            vibrations = np.array(result_list2)
            vibrations = vibrations[:6000]
            
            if ripeness == 'A':
                audio = nr.reduce_noise(y = butter_bandpass_filter(audio,lowcut,highcut,fs=RATE, order=8),sr=RATE,stationary=True,prop_decrease=0.85)
                X_real = np.concatenate((audio,vibrations), axis=0).reshape(1,-1)
                predictions = mlp.predict(X_real)
                print(predictions)
                ser.write(str(predictions[0]).encode('ascii'))
                print("done")

            else:
                # Concatenate data and normalized into a single 2D array
                data_to_save = np.concatenate((arduino_data, audio.reshape(-1, 1), vibrations.reshape(-1,1)), axis=0)
                # Save the data to the CSV file using save_data_to_csv function
                save_data_to_csv(data_to_save, CSV_OUTPUT_FILENAME)

if __name__=='__main__':
    main()


