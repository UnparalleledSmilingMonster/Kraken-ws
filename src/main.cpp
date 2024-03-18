#include <iostream>
#include <string>
#include <stdlib.h>
#include <fstream>
#include <filesystem>
#include <typeinfo>
#include <cmath>
#include <chrono>

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
std::string const header = "bid,bid_qty,ask,ask_qty,last,volume,vwap,low,high,change,change_pct,date\n";

std::string const log_path = cpath + "/../logs/log.txt";
std::string const time_iso_format = "%Y-%m-%dT%H:%M:%S";

std::array<std::string, 11> fields = {"bid","bid_qty","ask","ask_qty","last","volume","vwap","low","high","change","change_pct"};

std::string utc(){
    std::time_t now_c = std::chrono::system_clock::to_time_t( std::chrono::system_clock::now());
    std::tm now_tm = *std::localtime(&now_c);

    char time_buffer[32];
    std::strftime(time_buffer, sizeof(time_buffer), time_iso_format.c_str(), &now_tm);
    //std::cout << time_buffer << std::endl;
    return std::string(time_buffer);
}

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

std::string json_to_csv(const rapidjson::Value &data, const std::string &time){
    //Rules for .csv extension filesystem : since we only store numerical values, we won't have formatting issues.
    // comma separated values and '\n'' to delimitate end of row

    std::string content="";
    //std::cout << "Iterating through data to put it in csv" << std::endl;
    for (std::string field : fields) content += std::to_string(data[field.c_str()].GetDouble())  + ",";
    //std::cout << content << std::endl;
    return content+time+"\n";

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



void record_data(rapidjson::Value &data, const std::string &time){
    std::string symbol = data[0]["symbol"].GetString();
    std::replace(symbol.begin(), symbol.end(),'/','-');
    std::string filename = fname + symbol + extension;

    data[0].RemoveMember("symbol");
    std::string content = json_to_csv(data[0], time);
    //std::cout << content << std::endl;
    appendToFile(filename, content);
}

int main() {
    std::string const uri = "wss://ws.kraken.com/v2";
    int counter = 0;
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
    auto back_off_reconnect = [&webSocket, &symbols, &logger](){
        logger.log("Launching the exponential backoff reconnection procedure.", LOG::TYPE::WARNING);
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

    logger.log("Connecting to "+ uri, LOG::TYPE::INFO);
    bool quit = false;

    // Setup a callback to be fired (in a background thread, watch out for race conditions !)
    // when a message or an event (open, close, error) is received
    webSocket.setOnMessageCallback([&back_off_reconnect, &logger, &counter](const ix::WebSocketMessagePtr& msg)
        {
            if (msg->type == ix::WebSocketMessageType::Message)
            {

                rapidjson::Document resp;
                resp.Parse(msg->str.c_str());

                // Check if parsing was successful
                if (!resp.IsObject()) {
                    logger.log("Failed to parse JSON string", LOG::TYPE::ERROR);
                    return;
                }

                if (resp.HasMember("channel") && not ((std::string)resp["channel"].GetString()).compare("ticker")) {
                    //std::cout << "Channel: " << msg->str << std::endl;
                    record_data(resp["data"], utc());
                    counter++;
                    if (counter %100 ==0) logger.log(std::to_string(counter) +" elements recorded so far.", LOG::TYPE::INFO);
                }
            }

            else if (msg->type == ix::WebSocketMessageType::Open)
            {
                logger.log("Connection established", LOG::TYPE::INFO);

            }


            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                logger.log(msg->errorInfo.reason, LOG::TYPE::ERROR);
            }

            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                logger.log("Closing the web socket.", LOG::TYPE::INFO);
                if (msg->closeInfo.code != 1000){
                    logger.log("Unexpected closure, will try to reconnect.", LOG::TYPE::INFO);
                    back_off_reconnect();
                }
            }
        }
    );

    // Now that our callback is setup, we can start our background thread and receive messages
    webSocket.start();

    while(webSocket.getReadyState() != ix::ReadyState::Open){
        logger.log("Waiting for connection to be established.", LOG::TYPE::INFO);
        sleep(3);
    }

    //std::string const sb_web_socket_str = subscribe_msg(symbols);
    //std::cout << sb_web_socket_str << std::endl;
    webSocket.send(subscribe_msg(symbols));

    logger.log("Enter 'q' to quit", LOG::TYPE::INFO);
    do {
        char input;
        std::cin >> input;
        if (input == 'q') {
            quit = true;
            logger.log("User action to stop.", LOG::TYPE::INFO);
        }
    } while (!quit);


    webSocket.close();

    return 0;
}
