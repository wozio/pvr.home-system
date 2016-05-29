#pragma once

#include <functional>

namespace home_system
{
  enum class pvr_client_status
  {
    unknown,
    not_connected,
    connected
  };

  class pvr_client
  {
  public:
    pvr_client();
    ~pvr_client();

    int get_channels_num();
    void get_channels(std::function<void(int id, const std::string& name)> callback);

    void create_session(int id);
    int read_data(unsigned char *inbuf, unsigned int buf_size);
    void destroy_session();
  };
}
