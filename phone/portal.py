import pprint
import time
from datetime import datetime
import requests
from http.server import BaseHTTPRequestHandler, HTTPServer
import _thread
import re

hostName = "localhost"
hostPort = 9000
req_to_do = 0

auth = ('', '')
url_upload = 'https://swapfile.duckdns.org/portalguns/add.php'
url_gordon_data ='http://192.168.1.20/tmp/portal.txt'
url_chell_data ='http://192.168.1.21/tmp/portal.txt'
url_gordon_image = 'http://192.168.1.20/tmp/snapshot.jpg'
url_chell_image = 'http://192.168.1.21/tmp/snapshot.jpg'

gordon_packet_id_last = -1
chell_packet_id_last = -1
gordon_last_seen_time = -1
chell_last_seen_time = -1

session_gordon = requests.Session()
session_chell = requests.Session()
session_upload = requests.Session()

cycle = 0

class MyServer(BaseHTTPRequestHandler):
	def do_GET(self):
		global req_to_do
		temp = -2
		lock.acquire()
		try:
			if self.path == "/set":
				req_to_do = 1
			temp = req_to_do	
		finally:
			lock.release()
			
		self.send_response(200)
		self.send_header("Content-type", "text/plain")
		self.send_header("Access-Control-Allow-Origin", "*")
		self.end_headers()
		self.wfile.write(bytes("%s" % temp, "utf-8"))

def start_server():
	myServer = HTTPServer((hostName, hostPort), MyServer)
	print(time.asctime(), "Server Starts - %s:%s" % (hostName, hostPort))
	myServer.serve_forever()
	  

def upload_data(keyframe):
	global cycle
	global session_gordon
	global session_chell
	global session_upload
	global gordon_packet_id_last
	global chell_packet_id_last
	global gordon_last_seen_time
	global chell_last_seen_time

	
	payload = {}
	payload['keyframe'] = keyframe
	
	retries = 0
	while retries < 2:
		try:
			retries = retries + 1
			#get file from gun 1
			gordon_text = session_gordon.get(url_gordon_data,timeout=1).content.split( )

			gordon_packet_counter = int(gordon_text[30])
			if (gordon_packet_counter != gordon_packet_id_last and gordon_packet_id_last != -1):
				payload['g_state'] = int(gordon_text[1])
				gordon_syned = int(gordon_text[2])
				payload['g_volts'] = float(gordon_text[26])
				payload['g_temp1'] = float(gordon_text[27])
				payload['g_temp2'] = float(gordon_text[28])
				payload['g_lag'] = float(gordon_text[29])
				payload['g_live'] = 1
			else:
				payload['g_state'] = 0
				gordon_syned = 0
				payload['g_volts'] = 0
				payload['g_temp1'] = 0
				payload['g_temp2'] = 0
				payload['g_lag'] = 0
				payload['g_live'] = 0
			gordon_packet_id_last = gordon_packet_counter	
			retries = 99;
		except:
			print ("Gordon Failed")
			payload['g_state'] = 0
			gordon_syned = 0
			payload['g_volts'] = 0
			payload['g_temp1'] = 0
			payload['g_temp2'] = 0
			payload['g_lag'] = 0
			payload['g_live'] = 0
		
	retries = 0
	while retries < 2:
		try:
			retries = retries + 1	
			#get file from gun 2
			chell_text = session_chell.get(url_chell_data,timeout=1).content.split( )
			
			chell_packet_counter = int(chell_text[30])
			if (chell_packet_counter != chell_packet_id_last and chell_packet_id_last != -1):
				payload['c_state'] = int(chell_text[1])
				chell_synced = int(chell_text[2])
				payload['c_volts'] = float(chell_text[26])
				payload['c_temp1'] = float(chell_text[27])
				payload['c_temp2'] = float(chell_text[28])
				payload['c_lag'] = float(chell_text[29])
				payload['c_live'] = 1
			else:
				payload['c_state'] = 0
				chell_synced = 0
				payload['c_volts'] = 0
				payload['c_temp1'] = 0
				payload['c_temp2'] = 0
				payload['c_lag'] = 0
				payload['c_live'] = 0
			chell_packet_id_last = chell_packet_counter
			retries = 99;
		except:
			print ("Chell Failed")
			payload['c_state'] = 0
			chell_synced = 0
			payload['c_volts'] = 0
			payload['c_temp1'] = 0
			payload['c_temp2'] = 0
			payload['c_lag'] = 0
			payload['c_live'] = 0
		
	if (chell_synced == 1 and gordon_syned == 1):
		payload['synced'] = 1
	else:
		payload['synced'] = 0
	
	retries = 0
	while retries < 2:
		try:
			retries = retries + 1
			#determine which gun if any to get image from
			files = {};
			if payload['c_state'] < -1:
				files['img'] = ('i.jpg', session_chell.get(url_chell_image,timeout=1).content)
					
			if payload['g_state'] < -1:
				files['img'] = ('i.jpg', session_gordon.get(url_gordon_image,timeout=1).content)
			retries = 99;
		except:
			print("Img get Failed")
		
	#supress warmup cycle
	if cycle > 0:
		try:
			upload_result = session_upload.post(url_upload,data=payload, auth=auth,files=files, verify=False)
			upload_result = str( upload_result.content, encoding=upload_result.encoding ) 
		except:
			print ("Upload Failed")
	else:
		print("Warming...")
		
	cycle = cycle + 1
	
requests.packages.urllib3.disable_warnings()	
ip = ""

try:
	requests.get(url_gordon_data,timeout=.1)
	requests.get(url_chell_data,timeout=.1)
except:
	print ("BT Scan Active")
	while (ip == ""):
		fullfile = open('/proc/net/arp', 'r').read()
		results = re.findall( r'[0-9]+(?:\.[0-9]+){3}', fullfile )

		for item in results:
			print ("Trying Gordon at " + item)
			try:
				requests.get("http://" + item + ":8020/tmp/portal.txt",timeout=1)
			except:
				print ("Gordon Failed")
			else:
				print ("Found Gordon")
				ip = item
				break
				
			print ("Trying Chell at " + item)
			try:
				requests.get("http://" + item + ":8021/tmp/portal.txt",timeout=1)
			except:
				print ("Chell Failed")
			else:
				print ("Found Chell")
				ip = item
				break
		time.sleep(10)
else:
	print ("LAN Mode Active")
	print ('Gordon: http://192.168.1.20')
	print ('Chell: http://192.168.1.21')
	
	

if (ip != ""):
	print ("Building URLs...")		
	url_gordon_data ='http://' + ip + ':8020/tmp/portal.txt'
	url_chell_data ='http://' + ip + ':8021/tmp/portal.txt'
	url_gordon_image = 'http://' + ip + ':8020/tmp/snapshot.jpg'
	url_chell_image = 'http://' + ip + ':8021/tmp/snapshot.jpg'
	print ('Gordon: http://' + ip +':8020')
	print ('Chell: http://' + ip +':8021')

print("Warming")
upload_data(0)
time.sleep(2)
upload_data(0)
print("Warmup Complete")

lock = _thread.allocate_lock()
_thread.start_new_thread(start_server,())

frames = 0
start_time = time.time()
fps_time = time.time()
while 1:
	#check if work to do flag is set
	work_to_do = 0
	lock.acquire()
	try:
		if req_to_do > 0:
			work_to_do = 1
	finally:
		lock.release()	
		
	#rush datapoint
	if (work_to_do > 0):
		print("Keyframe!")
		upload_data(1)
		work_to_do = 0
		#set work as being done
		lock.acquire()
		try:
			req_to_do = 0
		finally:
			lock.release()	
		
	#fps (minutes) display
	if (time.time() - fps_time > 60):
		print ("FPM: " + str(frames))
		frames = 0
		fps_time = time.time()
		
	#normal datapoint
	 
	if (time.time() >= start_time + 3):
		#do a frame
		upload_data(0)
		frames += 1
		#upload every 3 seconds
		start_time = start_time + 3;
		#make sure next deadline is possible
		if (start_time + 3 <= time.time()):
			print("Skipping Delay")
			start_time = time.time()
	else:
		time.sleep(.1)
