// Copyright 2019 Qwant Research. Licensed under the terms of the Apache 2.0
// license. See LICENSE in the project root.

#ifndef __CLASSIFIER_CONTROLLER_H
#define __CLASSIFIER_CONTROLLER_H

#include <fasttext/fasttext.h>
#include <sstream>

#include "yaml-cpp/yaml.h"

#include "classifier.h"

class ClassifierController {
public:
  ClassifierController(std::string& classif_config);
  ~ClassifierController();
  std::vector<Classifier *> GetListClassifs();
  std::vector<std::pair<fasttext::real, std::string>> AskClassification(std::string& text, 
                                                                      std::string& tokenized,
                                                                      std::string& domain,
                                                                      int count,
                                                                      float threshold);

private:
  std::vector<Classifier *> _list_classifs;
  void ProcessConfigFile(std::string& classif_config, std::vector<Classifier *>& list_classifs);
};

#endif // __CLASSIFIER_CONTROLLER_H
