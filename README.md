# Smart Battery Charger for Magnes Motors

##### Requirement
1. [ESP-IDF](https://github.com/espressif/esp-idf)

##### Working
ESP_IDF has an excellent support for TCP/UDP sockets but Clients mainly Browsers can only communicate over Websockets.
**Websocets are not TCP sockets.**
Websockets are above the socket layer and are wrapped with  HTTP requests with meta information.

For browsers to communicate over websockets they must establish a *"Handshake"* with the server.
ESP32 acts as psuedo Websocket server which obtains Websocket connection and strips the data and handshakes.

##### Processing
Websocket request looks like this 
```
Host: 192.168.4.1:9998
Connection: Upgrade
Pragma: no-cache
Cache-Control: no-cache
Upgrade: websocket
Origin: file://
Sec-WebSocket-Version: 13
User-Agent: Mozilla/5.0 (Windows NT 6.1; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.87 Safari/537.36
Accept-Encoding: gzip, deflate, sdch
Accept-Language: de-DE,de;q=0.8,en-US;q=0.6,en;q=0.4
Sec-WebSocket-Key: Sb0llpkUl572foZxqBOxMw==
Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits
```
The server has to read Sec-WebSocket-Key, concatinate the magic string ```258EAFA5-E914-47DA-95CA-C5AB0DC85B11``` to it, take the SHA1 of it, and return the base64 encoded result to the client For acknowledgement as Handshake.


##### Tasks:
1. DAC on esp32
2. RS232 serial communication on esp32
3. CAN communication between multiple esp32