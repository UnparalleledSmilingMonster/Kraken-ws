#include <iostream>
#include <string>
#include <stdlib.h>
#include <fstream>
#include <filesystem>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXUserAgent.h>


std::string const cpath = std::filesystem::current_path();
std::string const fname = cpath + "/.." +"/data/data_";
std::string const extension = ".json";


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

void appendToFile(const std::string& filename, const std::string& content) {
    std::cout << filename << std::endl;

    /*
    if (!std::filesystem::exists(cpath + "/.." +"/data")) {
        std::cerr << "Current path :" << cpath  << std::endl;

    }*/

    std::fstream outfile(filename,  std::ios_base::out | std::ios_base::app); // Open file in append mode

    if (outfile.is_open()) {
        outfile << content+"\n"; // Append content to file
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
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    data[0].Accept(writer);
    std::string content = buffer.GetString();

    //Debug : std::cout << content << std::endl;
    appendToFile(filename, content);
}

int main() {
    std::string const uri = "wss://ws.kraken.com/v2";

    const int ping_delay = 60;
    std::vector<std::string> symbols = {"NANO/USD", "BTC/USD"}; //Fill in with the desired pairs you want to track


    // Our websocket object
    ix::WebSocket webSocket;
    ix::SocketTLSOptions tlsOptions;
    tlsOptions.tls = true;
    webSocket.setTLSOptions(tlsOptions);

    webSocket.setUrl(uri);
    webSocket.setPingInterval(ping_delay);

    std::cout << "Connecting to " << uri << std::endl;
    bool quit = false;

    // Setup a callback to be fired (in a background thread, watch out for race conditions !)
    // when a message or an event (open, close, error) is received
    webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg)
        {
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                std::cout << "Connection established" << std::endl;
            }

            else if (msg->type == ix::WebSocketMessageType::Message)
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

            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                // Maybe SSL is not configured properly
                std::cout << "Connection error: " << msg->errorInfo.reason << std::endl;
            }

            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                std::cout << "Closing the web socket." << std::endl;
            }
        }
    );

    // Now that our callback is setup, we can start our background thread and receive messages
    webSocket.start();

    while(webSocket.getReadyState() != ix::ReadyState::Open){
        std::cout << "Waiting for connection to be established..." << std::endl;
        sleep(2);
    }


    std::string const sb_web_socket_str = subscribe_msg(symbols);

    std::cout << sb_web_socket_str << std::endl;

    webSocket.send(sb_web_socket_str);


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
