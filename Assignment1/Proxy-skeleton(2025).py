# Include the libraries for socket and system calls
import socket
import sys
import os
import argparse
import re

# 1MB buffer size
BUFFER_SIZE = 1000000

# Get the IP address and Port number to use for this web proxy server
parser = argparse.ArgumentParser()
parser.add_argument('hostname', help='the IP Address Of Proxy Server')
parser.add_argument('port', help='the port number of the proxy server')
args = parser.parse_args()
proxyHost = args.hostname
proxyPort = int(args.port)

# Create a server socket, bind it to a port and start listening

try:
    # Create a socket using IPv4 and TCP (Stream) connection
    serverSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print('Created socket')
except:
    # Handle the error if socket creation fails
    print('Failed to create socket')
    sys.exit()

try:
    # Bind the socket to the specified host and port
    serverSocket.bind((proxyHost, proxyPort))
    print('Port is bound')
except:
    # Handle the error if the port is already in use
    print('Port is already in use')
    sys.exit()

try:
    # Start listening for incoming connections with a backlog of up to 5 connections
    serverSocket.listen(5)
    print('Listening to socket')
except:
    # Handle the error if the socket fails to listen
    print('Failed to listen')
    sys.exit()

# continuously accept connections
while True:
    print('Waiting for connection...')
    clientSocket = None

    # Accept connection from client and store in the clientSocket
    try:
        # ~~~~ INSERT CODE ~~~~
        clientSocket, addr = serverSocket.accept()
        # ~~~~ END CODE INSERT ~~~~
        print('Received a connection')
    except:
        print('Failed to accept connection')
        sys.exit()

    # Get HTTP request from client
    # and store it in the variable: message_bytes
    # ~~~~ INSERT CODE ~~~~
    message_bytes = clientSocket.recv(BUFFER_SIZE)
    # ~~~~ END CODE INSERT ~~~~
    message = message_bytes.decode('utf-8')
    print('Received request:')
    print('< ' + message)

    # Extract the method, URI and version of the HTTP client request 
    requestParts = message.split()
    method = requestParts[0]
    URI = requestParts[1]
    version = requestParts[2]

    print('Method:\t\t' + method)
    print('URI:\t\t' + URI)
    print('Version:\t' + version)
    print('')

    # Get the requested resource from URI
    # Remove http protocol from the URI
    URI = re.sub('^(/?)http(s?)://', '', URI, count=1)

    # Remove parent directory changes - security
    URI = URI.replace('/..', '')

    # Split hostname from resource name
    resourceParts = URI.split('/', 1)
    hostname = resourceParts[0]
    resource = '/'

    if len(resourceParts) == 2:
        # Resource is absolute URI with hostname and resource
        resource = resource + resourceParts[1]

    print('Requested Resource:\t' + resource)

    # Check if resource is in cache
    try:
        cacheLocation = './' + hostname + resource
        if cacheLocation.endswith('/'):
            cacheLocation = cacheLocation + 'default'

        print('Cache location:\t\t' + cacheLocation)

        fileExists = os.path.isfile(cacheLocation)
        
        # Check wether the file is currently in the cache
        cacheFile = open(cacheLocation, "r")
        cacheData = cacheFile.readlines()

        print('Cache hit! Loading from cache file: ' + cacheLocation)
        # ProxyServer finds a cache hit
        # Send back response to client 
        # ~~~~ INSERT CODE ~~~~
        for i in range(0, len(cacheData)):
            clientSocket.send(cacheData[i].encode())
        # ~~~~ END CODE INSERT ~~~~
        cacheFile.close()
        print('Sent to the client:')
        print('> ' + str(cacheData))
    except:
        # cache miss.  Get resource from origin server
        originServerSocket = None
        # Create a socket to connect to origin server
        # and store in originServerSocket
        # ~~~~ INSERT CODE ~~~~
        originServerSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # ~~~~ END CODE INSERT ~~~~

        print('Connecting to:\t\t' + hostname + '\n')
        try:
            # Get the IP address for a hostname
            address = socket.gethostbyname(hostname)
            # Connect to the origin server
            # ~~~~ INSERT CODE ~~~~
            originServerSocket.connect((address, 80))
            # ~~~~ END CODE INSERT ~~~~
            print('Connected to origin Server')

            originServerRequest = ''
            originServerRequestHeader = ''
            # Create origin server request line and headers to send
            # and store in originServerRequestHeader and originServerRequest
            # originServerRequest is the first line in the request and
            # originServerRequestHeader is the second line in the request
            # ~~~~ INSERT CODE ~~~~
            originServerRequest = method + " " + resource + " " + version
            originServerRequestHeader = "Host: " + hostname
            # ~~~~ END CODE INSERT ~~~~

            # Construct the request to send to the origin server
            request = originServerRequest + '\r\n' + originServerRequestHeader + '\r\n\r\n'

            # Request the web resource from origin server
            print('Forwarding request to origin server:')
            for line in request.split('\r\n'):
                print('> ' + line)

            try:
                originServerSocket.sendall(request.encode())
            except socket.error:
                print('Forward request to origin failed')
                sys.exit()

            print('Request sent to origin server\n')

            # Get the response from the origin server
            # ~~~~ INSERT CODE ~~~~
            origin_response = b''
            while True:
                data = originServerSocket.recv(BUFFER_SIZE)
                if not data:
                    break
                origin_response += data
            # ~~~~ END CODE INSERT ~~~~

            # Send the response to the client
            # ~~~~ INSERT CODE ~~~~
            clientSocket.sendall(origin_response)
            # ~~~~ END CODE INSERT ~~~~

            # Create a new file in the cache for the requested file.
            cacheDir, file = os.path.split(cacheLocation)
            print('cached directory ' + cacheDir)
            if not os.path.exists(cacheDir):
                os.makedirs(cacheDir)
            cacheFile = open(cacheLocation, 'wb')

            # Save origin server response in the cache file
            # ~~~~ INSERT CODE ~~~~
            cacheFile.write(origin_response)
            # ~~~~ END CODE INSERT ~~~~
            cacheFile.close()
            print('cache file closed')

            # finished communicating with origin server - shutdown socket writes
            print('origin response received. Closing sockets')
            originServerSocket.close()
            
            clientSocket.shutdown(socket.SHUT_WR)
            print('client socket shutdown for writing')
        except OSError as err:
            print('origin server request failed. ' + err.strerror)

    try:
        clientSocket.close()
    except:
        print('Failed to close client socket')