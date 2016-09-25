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

  struct epg_entry
  {
    int channel_id;
    int id;
    std::string title;
    std::string plot;
    long long start;
    long long end;
  };

  class pvr_client
  {
  public:
    pvr_client();
    ~pvr_client();

    int get_channels_num();
    void get_channels(std::function<void(int id, const std::string& name)> callback);
    void get_epg(int channel_id, long long start_time, long long end_time, std::function<void(epg_entry&)> callback);

    void create_session(int channel_id);
    int read_data(unsigned char *inbuf, unsigned int buf_size);
    void destroy_session();

    void play();
    void pause();

    long long seek(long long pos);

    long long get_buffer_length();
    long long get_buffer_position();
  private:
  };
}
