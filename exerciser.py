import time
from urllib import request, parse
primary = parse.urlencode({'mode': '1 100'}).encode()
reset = parse.urlencode({'mode': '1 103'}).encode()
secondary = parse.urlencode({'mode': '1 101'}).encode()


gordon_primary_req = request.Request('http://192.168.1.20/index_post.php', data=primary)
gordon_secondary_req = request.Request('http://192.168.1.20/index_post.php', data=secondary)
gordon_reset_req = request.Request('http://192.168.1.20/index_post.php', data=reset)

chell_primary_req = request.Request('http://192.168.1.21/index_post.php', data=primary)
chell_secondary_req = request.Request('http://192.168.1.21/index_post.php', data=secondary)
chell_reset_req = request.Request('http://192.168.1.21/index_post.php', data=reset)

while 1:
    req = request.urlopen(gordon_reset_req)
    print(req.read)
    time.sleep(10)
    req = request.urlopen(gordon_primary_req)
    print(req.read)
    time.sleep(10)
    req = request.urlopen(gordon_primary_req)
    print(req.read)
    time.sleep(10)
    req = request.urlopen(chell_primary_req)
    print(req.read)
    time.sleep(10)
    
    for x in range(1, 20):
        req = request.urlopen(chell_secondary_req)
        print(req.read)
        time.sleep(10)
        req = request.urlopen(gordon_secondary_req)
        print(req.read)
        time.sleep(10)
        