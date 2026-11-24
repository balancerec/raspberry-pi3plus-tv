#pragma once
#include <cstdlib>
#include <iostream>
inline void check(bool ok,const char* label) {if(!ok){std::cerr<<"FAIL: "<<label<<'\n';std::exit(EXIT_FAILURE);}}
