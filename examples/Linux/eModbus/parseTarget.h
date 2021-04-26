// Copyright 2021 Michael Harwerth - miq1 AT gmx DOT de
#ifndef _PARSETARGET_H
#define _PARSETARGET_H

#include <regex>
#include "Logging.h"
#include "Client.h"
#include "IPAddress.h"

using std::regex;
using std::regex_match;
using std::cmatch;

int parseTarget(const char *source, IPAddress& ip, uint16_t& port, uint8_t& serverID);
#endif

