// Copyright 2019 Qwant Research. Licensed under the terms of the Apache 2.0
// license. See LICENSE in the project root.

#include "rest_server.h"
#include "utils.h"

void rest_server::init(size_t thr) {
  Pistache::Port port(_num_port);
  Address addr(Ipv4::any(), port);
  httpEndpoint = std::make_shared<Http::Endpoint>(addr);

  ProcessCongifFile(_classif_config, _list_classifs);

  auto opts = Http::Endpoint::options().threads(thr).flags(
      Tcp::Options::InstallSignalHandler);
  httpEndpoint->init(opts);
  setupRoutes();
}

void rest_server::start() {
  httpEndpoint->setHandler(router.handler());
  httpEndpoint->serve();
  httpEndpoint->shutdown();
}

void rest_server::setupRoutes() {
  using namespace Rest;

  Routes::Post(router, "/intention/",
               Routes::bind(&rest_server::doClassificationPost, this));

  Routes::Post(router, "/intention_batch/",
              Routes::bind(&rest_server::doClassificationBatchPost, this));

  Routes::Get(router, "/intention/",
              Routes::bind(&rest_server::doClassificationGet, this));
}

void rest_server::doClassificationGet(const Rest::Request &request,
                                      Http::ResponseWriter response) {
  response.headers().add<Http::Header::AccessControlAllowHeaders>(
      "Content-Type");
  response.headers().add<Http::Header::AccessControlAllowMethods>(
      "GET, POST, DELETE, OPTIONS");
  response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
  response.headers().add<Http::Header::ContentType>(MIME(Application, Json));
  string response_string = "{\"domains\":[";
  for (int inc = 0; inc < (int)_list_classifs.size(); inc++) {
    if (inc > 0)
      response_string.append(",");
    response_string.append("\"");
    response_string.append(_list_classifs.at(inc)->getDomain());
    response_string.append("\"");
  }
  response_string.append("]}");
  if (_debug_mode != 0)
    cerr << "LOG: " << currentDateTime() << "\t" << response_string << endl;
  response.send(Pistache::Http::Code::Ok, response_string);
}

void rest_server::doClassificationPost(const Rest::Request &request,
                                       Http::ResponseWriter response) {
  response.headers().add<Http::Header::AccessControlAllowHeaders>(
      "Content-Type");
  response.headers().add<Http::Header::AccessControlAllowMethods>(
      "GET, POST, DELETE, OPTIONS");
  response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
  nlohmann::json j = nlohmann::json::parse(request.body());
  
  int count;
  float threshold;
  bool debugmode;
  string language, domain;
  
  try {
    rest_server::fetchParamWithDefault(j, domain, language, count, threshold, debugmode);
  } catch (std::runtime_error e) {
    response.headers().add<Http::Header::ContentType>(MIME(Application, Json));
    response.send(Http::Code::Bad_Request, e.what());
  }

  tokenizer l_tok(language, true);

  if (j.find("text") != j.end()) {
    string text = j["text"];
    string tokenized = l_tok.tokenize_str(text);
    j.push_back(nlohmann::json::object_t::value_type(
        string("tokenized"), tokenized));
    if (_debug_mode != 0)
      cerr << "LOG: " << currentDateTime() << "\t"
            << "ASK CLASS :\t" << j << endl;
    std::vector<std::pair<fasttext::real, std::string>> results;
    results = askClassification(tokenized, domain, count, threshold);
    j.push_back(
        nlohmann::json::object_t::value_type(string("intention"), results));
    std::string s = j.dump();
    if (_debug_mode != 0)
      cerr << "LOG: " << currentDateTime() << "\t" << s << endl;
    response.headers().add<Http::Header::ContentType>(
        MIME(Application, Json));
    response.send(Http::Code::Ok, std::string(s));
  } else {
    response.headers().add<Http::Header::ContentType>(MIME(Application, Json));
    response.send(Http::Code::Bad_Request,
                  std::string("The `text` value is required"));
  }
}

void rest_server::doClassificationBatchPost(const Rest::Request &request,
                                       Http::ResponseWriter response) {
  response.headers().add<Http::Header::AccessControlAllowHeaders>(
      "Content-Type");
  response.headers().add<Http::Header::AccessControlAllowMethods>(
      "GET, POST, DELETE, OPTIONS");
  response.headers().add<Http::Header::AccessControlAllowOrigin>("*");
  nlohmann::json j = nlohmann::json::parse(request.body());

  int count;
  float threshold;
  bool debugmode;
  string language;
  string domain;
  
  try {
    rest_server::fetchParamWithDefault(j, domain, language, count, threshold, debugmode);
  } catch (std::runtime_error e) {
    response.headers().add<Http::Header::ContentType>(MIME(Application, Json));
    response.send(Http::Code::Bad_Request, e.what());
  }
  
  tokenizer l_tok(language, true);
  
  if (j.find("batch_data") != j.end()) {
    for (auto& it: j["batch_data"]){
      if (it.find("text") != it.end()) {
        string text = it["text"];
        string tokenized = l_tok.tokenize_str(text);
        it.push_back(nlohmann::json::object_t::value_type(
            string("tokenized"), tokenized));
        if (_debug_mode != 0)
          cerr << "LOG: " << currentDateTime() << "\t"
              << "ASK CLASS :\t" << it << endl;
        auto results = askClassification(tokenized, domain, count, threshold);
        it.push_back(
            nlohmann::json::object_t::value_type(string("intention"), results));
      } else {
        response.headers().add<Http::Header::ContentType>(
            MIME(Application, Json));
        response.send(Http::Code::Bad_Request,
                      std::string("`text` value is required for each item in `batch_data` array"));
      }
    }
    std::string s = j.dump();
    if (_debug_mode != 0)
      cerr << "LOG: " << currentDateTime() << "\t" << s << endl;
    response.headers().add<Http::Header::ContentType>(
        MIME(Application, Json));
    response.send(Http::Code::Ok, std::string(s));
  } else {
    response.headers().add<Http::Header::ContentType>(MIME(Application, Json));
    response.send(Http::Code::Bad_Request,
                  std::string("`batch_data` value is required"));
  }
}

void rest_server::fetchParamWithDefault(const nlohmann::json& j, 
                            string& domain, 
                            string& language,
                            int& count,
                            float& threshold,
                            bool& debugmode){
  count = 10;
  threshold = 0.0;
  debugmode = false;

  if (j.find("count") != j.end()) {
    count = j["count"];
  }
  if (j.find("threshold") != j.end()) {
    threshold = j["threshold"];
  }
  if (j.find("debug") != j.end()) {
    debugmode = j["debug"];
  }
  if (j.find("language") != j.end()) {
    language = j["language"];
  } else {
    throw std::runtime_error("`language` value is null");
  }
  if (j.find("domain") != j.end()) {
    domain = j["domain"];
  } else {
    throw std::runtime_error("`domain` value is null");
  }
}

bool rest_server::process_localization(string &input, json &output) {
  string token(input.c_str());
  if (input.find("à ") == 0)
    token = input.substr(3);
  if (input.find("au dessus de ") == 0)
    token = input.substr(13);
  if (input.find("au ") == 0)
    token = input.substr(4);
  if (input.find("vers ") == 0)
    token = input.substr(5);
  output.push_back(
      nlohmann::json::object_t::value_type(string("label"), token));
}

void rest_server::doAuth(const Rest::Request &request,
                         Http::ResponseWriter response) {
  printCookies(request);
  response.cookies().add(Http::Cookie("lang", "fr-FR"));
  response.send(Http::Code::Ok);
}

void rest_server::shutdown() {
  httpEndpoint->shutdown(); 
}