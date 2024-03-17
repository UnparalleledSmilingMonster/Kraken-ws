# Kraken WebSocket Client

Mostly experimental and for my own usage, but if it can be of use to someone, be my guest.

## Dependencies

- IXWebSocket
- rapidjson

  (You will need zlib and maybe others)

## Build

Build with cmake.

## Run 

Type 'q' in console to close the websocket. You can modify the vector **symbols** to add in other tickers you would like to follow. Data is saved under *.json* files in **data** directory (one file per symbol followed). 
