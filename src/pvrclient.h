#pragma once

#include <functional>

void create();
void destroy();

int get_channels_num();
void get_channels(std::function<void(int id, const std::string& name)> callback);
