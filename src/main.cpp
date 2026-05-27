#include <iostream>
#include <sstream>

#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

#include "curlpp/cURLpp.hpp"


int main() {
    curlpp::Cleanup cleanup;

    curlpp::Easy request;

    request.setOpt(new curlpp::options::Url("https://www.example.com"));

    std::ostringstream  response;
    request.setOpt(curlpp::options::WriteStream(&response));

    request.perform();

    std::cout << response.str() << std::endl;
    return 0;
}
