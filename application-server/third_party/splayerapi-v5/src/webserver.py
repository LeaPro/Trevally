from ctypes import *
import os
import sys
import hashlib as hash
import pprint
import SimpleHTTPServer
import SocketServer
import json
import socket
HOST = socket.gethostname() + ".local"
PORT = 8000

def get_library_version(filename):
    #print "\n\nAnalyzing file: {}\n".format(filename)
    lib = cdll.LoadLibrary(os.path.abspath(filename))
    splayer = c_char_p.in_dll(lib, "SPLAYER_VERSION")
    #print "SPLAYER_VERISON: '{}'".format(splayer.value)
    return splayer.value

def get_sha1(filename):
    BLOCKSIZE = 65536

    sha = hash.sha1()
    with open(filename, 'rb') as kali_file:
        file_buffer = kali_file.read(BLOCKSIZE)
        while len(file_buffer) > 0:
            sha.update(file_buffer)
            file_buffer = kali_file.read(BLOCKSIZE)
    digest = sha.hexdigest()
    #print digest
    return digest

libraries = []

if len(sys.argv) <= 1:
    print "usage: {} file1.so file2.so file3.so".format(sys.argv[0])
    sys.exit(1)

for n in sys.argv[1:]:
    l = {
        "version": get_library_version(n),
        "link": "http://{}:{}/{}".format(HOST, PORT, n),
        "checksum": get_sha1(n)
    }
    libraries.append(l)


pprint.pprint(libraries)


active_library = 0

class MyHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def do_GET(self):
        global active_library
        if self.path == '/latest.json':
            self.send_response(200, 'OK')
            self.send_header('Connection', 'close')
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(bytes(json.dumps(libraries[active_library])))
            self.wfile.close()
            return
        if self.path.startswith("/activate/"):
            active_library = int(self.path[10:])
            self.send_response(200, 'OK')
            self.send_header('Connection', 'close')
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write(bytes("Option {} activated. <a href='/'>back</a>".format(active_library)))
            self.wfile.close()
            return
        if self.path == '/':
            self.send_response(200, 'OK')
            self.send_header('Connection', 'close')
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            buf =  "<html>"
            buf += "<style>table, th, td {border: 1px solid black; border-collapse: collapse; padding: 4px;}</style>"
            buf += "<table><tr><td>#</td><td>File</td><td>Version</td><td>Checksum</td><td></td></tr>"
            for k,v in enumerate(libraries):
                buf += "<br><td>{}</td><td><a href='{}'>{}</a></td><td>{}</td><td>{}</td><td>".format(k, v["link"], v["link"], v["version"], v["checksum"])
                if k == active_library:
                    buf += "<b>ACTIVE</b>"
                else:
                    buf += "<a href='/activate/{}'>Activate</a>".format(k)
                buf += "</td></tr>"
            buf += "</table><a href='http://{}:{}/latest.json'>http://{}:{}/latest.json</a></html>".format(HOST, PORT, HOST, PORT)
            self.wfile.write(bytes(buf))
            self.wfile.close()
            return;
        return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

SocketServer.TCPServer.allow_reuse_address = True
MyHandler.protocol_vesrion = "HTTP/1.0"
httpd = SocketServer.TCPServer(("", PORT), MyHandler)

print "serving at port", PORT
httpd.serve_forever()

