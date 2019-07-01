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
The server has to read Sec-WebSocket-Key, concatinate the  string ```258EAFA5-E914-47DA-95CA-C5AB0DC85B11``` to it, take the SHA1 of it, and return the base64 encoded result to the client For acknowledgement as Handshake.


##### Tasks completed:
1. UART communication with BMS hardware
2. Transmiting the data from UART to CAN 

##### Tasks remaining:
1. Sending Data to CANBUS  (Testing)
2. Sending Data to Webpage  
3. Merging CAN and WiFi

##### Folder structure
1. [rs232](https://github.com/AdityaGawali/Magnesmotors/tree/master/rs232) : Basic UART communication.
3. [rs232withcan](https://github.com/AdityaGawali/Magnesmotors/tree/master/rs232withcan) : UART with CAN bus communication.
4. [serverwithrs232withcan](https://github.com/AdityaGawali/Magnesmotors/tree/master/serverwithrs232withcan) : UART communication along with transmission to CAN bus and Wifi client.
5. [serverwithsocket](https://github.com/AdityaGawali/Magnesmotors/tree/master/serverwithsocket) : Wifi Server setup with Websocket communication with client.
6. [canwithdac](https://github.com/AdityaGawali/Magnesmotors/tree/master/canwithdac) : CAN bus communication for charger to control charging modes.
7. [Hardware](https://github.com/AdityaGawali/Magnesmotors/tree/master/Hardware) : Contains PCB and gerber files along with pin usage and components of the board.
