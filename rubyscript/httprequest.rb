require 'socket'
require 'base64'

# Use http proxy
proxyhost = "test.proxy.com"
proxyport = 8080
socket = TCPSocket.open(proxyhost, proxyport)   # connect to server.
socket.setsockopt(Socket::IPPROTO_TCP, Socket::TCP_NODELAY, 1)

# Encrypt username and password.
code = Base64.strict_encode64('name:password')
#source = Base64.strict_decode64(code)
#print code

# http connect, support https(443) only.
webhost = 'www.baidu.com'
webport = 443
connect = "CONNECT https://#{webhost}:#{webport} HTTP/1.1\r\n" +
          "Host: #{webhost}:#webport\r\n" +
          "Proxy-Connection: keep-alive\r\n" +
          "Proxy-Authorization: Basic #{code}\r\n" +
          "\r\n"
#          "Content-Length: 0\r\n\r\n"
socket.print(connect)                           # use http connect.
#response = socket.read                          # read response
response = socket.recv(1024)
print response

# http request.
webhost = 'plugindoc.mozdev.org'                # default web port is 80.
webpath = "/testpages/test.pdf"
request = "HEAD #{webhost}#{webpath} HTTP/1.1\r\n" +
#request = "GET #{webpath} HTTP/1.1\r\n" +
          "Host: #{webhost}\r\n" +
          "Proxy-Connection: keep-alive\r\n" +
          "Proxy-Authorization: Basic #{code}\r\n" +
          "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n" +
          "Upgrade-Insecure-Requests: 1\r\n" +
          "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.107 Safari/537.36\r\n" +
          "Accept-Encoding: gzip, deflate, sdch\r\n" +
          "Accept-Language: zh-CN,zh;q=0.8,de;q=0.6\r\n\r\n"
socket.print(request)                           # use http connect.
response = socket.gets                          # read response
print response

# Split response at first blank line into headers and body
#headers,body = response.split("\r\n\r\n", 2)
#print response

socket.close