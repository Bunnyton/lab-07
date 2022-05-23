#ifndef LAB_07_SUGGEST_HPP_
#define LAB_07_SUGGEST_HPP_

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <utility>
#include <iomanip>

using json = nlohmann::json;


class Suggest {
 private:
  json _storage;
  std::string _path;

 public:
  Suggest() : _path("../suggestions.json") { load(); }

  explicit Suggest(std::string path) : _path(std::move(path)) { load(); }

  void load() {
    try {
      std::ifstream f; f.open(_path);
      if (!f.is_open())
        throw std::logic_error("Can't find suggestions file. Empty suggestions");

      f >> _storage;
      sort();
    } catch (const std::exception& ec) {
      std::cout << ec.what() << std::endl;
    }
  }

  void sort(){
    try {
      std::sort(_storage.begin(), _storage.end(),
                [](const json& a, const json& b) -> bool {
                  if (a.at("cost") < b.at("cost")) return true;
                  return false;
                });
    } catch(std::exception &ec) { std::cout << ec.what() << std::endl; }
  }

  void update(){
        _storage.clear();
        load();
  };

  std::string suggest_request(const std::string &input){
    json res;
    for(unsigned i = 0, m = 0; i < _storage.size(); ++i) {
      if (_storage[i].at("id") == input){
        json sug;
        sug["text"] = _storage[i].at("name");
        sug["positions"] = m++;
        res["suggestions"].push_back(sug);
      }
    }
    if (res.empty()) return "{}";

    std::stringstream ss;
    ss << std::setw(4) << res;
    return ss.str();
  }
};

#endif  // LAB_07_SUGGEST_HPP_
