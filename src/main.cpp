#include <iostream>
#include <string>
#include <stdlib.h>
#include <fstream>
#include <filesystem>
#include <typeinfo>
#include <cmath>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXUserAgent.h>

#include "Logger.h"

std::string const cpath = std::filesystem::current_path();
std::string const fname = cpath + "/../data/data_";
std::string const extension = ".csv";
std::string const header = "bid,bid_qty,ask,ask_qty,last,volume,vwap,low,high,change,change_pct\n";

std::string const log_path = cpath + "/../logs/log.txt";

std::array<std::string, 11> fields = {"bid","bid_qty","ask","ask_qty","last","volume","vwap","low","high","change","change_pct"};

//TODO : add time for tickers


std::string subscribe_msg(std::vector<std::string> const &symbols){
    rapidjson::Document sb_web_socket;
    sb_web_socket.SetObject();

    sb_web_socket.AddMember("method", "subscribe", sb_web_socket.GetAllocator());
    //sb_web_socket.AddMember("req_id", req_id, sb_web_socket.GetAllocator());

    rapidjson::Document params;
    params.SetObject();
    params.AddMember("channel", "ticker", params.GetAllocator());
    params.AddMember("snapshot", true, params.GetAllocator());

    // Create an array for the "symbol" field
    rapidjson::Value symbolArray(rapidjson::kArrayType);
    for (const auto &symb : symbols){
        rapidjson::Value value;
        value.SetString(symb.c_str(), symb.length());
        symbolArray.PushBack(value, params.GetAllocator());
    }
    params.AddMember("symbol", symbolArray, params.GetAllocator());

    sb_web_socket.AddMember("params", params, sb_web_socket.GetAllocator());


    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    sb_web_socket.Accept(writer);
    return buffer.GetString();

}

std::string json_to_csv(const rapidjson::Value &data){
    //Rules for .csv extension filesystem : since we only store numerical values, we won't have formatting issues.
    // comma separated values and '\n'' to delimitate end of row

    std::string content="";
    //std::cout << "Iterating through data to put it in csv" << std::endl;
    for (std::string field : fields) content += std::to_string(data[field.c_str()].GetDouble())  + ",";
    content.pop_back();
    std::cout << content << std::endl;
    return content+"\n";

}

void appendToFile(const std::string& filename, const std::string& content) {
    //std::cout << filename << std::endl;
    bool new_file = std::filesystem::exists(filename) ? false: true;
    std::fstream outfile(filename,  std::ios_base::out | std::ios_base::app); // Open file in append mode

    if (outfile.is_open()) {
        if (new_file) outfile << header;
        outfile << content; // Append content to file
        outfile.close(); // Close the file
    } else {
        std::cerr << "Unable to open file for appending." << std::endl;
    }
}



void record_data(rapidjson::Value &data){
    std::string symbol = data[0]["symbol"].GetString();
    std::replace(symbol.begin(), symbol.end(),'/','-');
    std::string filename = fname + symbol + extension;

    data[0].RemoveMember("symbol");
    std::string content = json_to_csv(data[0]);
    //std::cout << content << std::endl;
    appendToFile(filename, content);
}

int main() {
    std::string const uri = "wss://ws.kraken.com/v2";
    const int ping_delay = 60;
    std::vector<std::string> symbols = {"NANO/EUR", "BTC/EUR", "ETH/EUR", "XRP/EUR", "XMR/EUR", "SOL/EUR", "LTC/EUR"}; //Fill in with the desired pairs you want to track
    Logger logger = Logger(log_path);

    // Our websocket object
    ix::WebSocket webSocket;
    ix::SocketTLSOptions tlsOptions;
    tlsOptions.tls = true;
    webSocket.setTLSOptions(tlsOptions);

    webSocket.setUrl(uri);
    webSocket.setPingInterval(ping_delay);

    //Never used so far, could crash:
    auto back_off_reconnect = [&webSocket, &symbols](){
        webSocket.start();
        unsigned int backoff = 1;

        while(webSocket.getReadyState() != ix::ReadyState::Open){
            if (webSocket.getReadyState() != ix::ReadyState::Closed){
                sleep(std::pow(2,backoff));     //Exponential Backoff to prevent getting IP banned
                backoff++;
                webSocket.start();
            }
            else sleep(2);
        }
        webSocket.send(subscribe_msg(symbols));

    };

    std::cout << "Connecting to " << uri << std::endl;
    bool quit = false;

    // Setup a callback to be fired (in a background thread, watch out for race conditions !)
    // when a message or an event (open, close, error) is received
    webSocket.setOnMessageCallback([&back_off_reconnect, &logger](const ix::WebSocketMessagePtr& msg)
        {
            if (msg->type == ix::WebSocketMessageType::Message)
            {

                rapidjson::Document resp;
                resp.Parse(msg->str.c_str());

                // Check if parsing was successful
                if (!resp.IsObject()) {
                    std::cerr << "Failed to parse JSON string" << std::endl;
                    return;
                }

                if (resp.HasMember("channel") && not ((std::string)resp["channel"].GetString()).compare("ticker")) {
                    std::cout << "Channel: " << msg->str << std::endl;
                    record_data(resp["data"]);
                }
            }

            else if (msg->type == ix::WebSocketMessageType::Open)
            {
                std::cout << "Connection established" << std::endl;
            }


            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                std::cout << "Connection error: " << msg->errorInfo.reason << std::endl;
                logger.log(msg->errorInfo.reason, LOG::TYPE::ERROR);
            }

            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                std::cout << "Closing the web socket." << std::endl;
                if (msg->closeInfo.code != 1000){
                    std::cout <<"Unexpected closure, will try to reconnect." << std::endl;
                    back_off_reconnect();
                }
            }
        }
    );

    // Now that our callback is setup, we can start our background thread and receive messages
    webSocket.start();

    while(webSocket.getReadyState() != ix::ReadyState::Open){
        std::cout << "Waiting for connection to be established..." << std::endl;
        sleep(2);
    }

    //std::string const sb_web_socket_str = subscribe_msg(symbols);
    //std::cout << sb_web_socket_str << std::endl;
    webSocket.send(subscribe_msg(symbols));


    std::cout << "Enter a character ('q' to quit): ";
    do {
        char input;
        std::cin >> input;
        if (input == 'q') {
            quit = true;
        }
    } while (!quit);


    webSocket.close();

    return 0;
}
