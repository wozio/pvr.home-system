#undef _WINSOCKAPI_
#include "pvrclient.h"

#include "discovery.h"
#include "yamicontainer.h"

#include <boost/circular_buffer.hpp>
#include <boost/shared_array.hpp>
#include <memory>
#include <functional>

using namespace std;

#define BUFSIZE 18800
#define BUFMAX 1000
#define BUFMAXSIZE = BUFSIZE * BUFMAX

size_t tmp_len_(0);
boost::shared_array<BYTE> tmp_buf_;
boost::circular_buffer<boost::shared_array<BYTE> > buffer_(BUFMAX);
size_t buffer_len_(0);
size_t buffer_pos_(0);
std::mutex buffer_mutex_;
long long remote_buf_len_(0);
long long remote_buf_pos_(0);

int session_ = -1;
bool seeking_ = false;

home_system::yc_t _yc;
home_system::discovery_t _discovery;

namespace home_system
{
  pvr_client::pvr_client()
  {
    _yc = yami_container::create();
    _discovery = discovery::create();
    seeking_ = false;

    auto func = [&](yami::incoming_message& im)
    {
      if (im.get_message_name() == "stream_part")
      {
        if (session_ != -1)
        {
          // discard parts if in the middle of seeking
          if (seeking_)
          {
            return;
          }
          try
          {
            remote_buf_len_ = im.get_parameters().get_long_long("size");
            remote_buf_pos_ = im.get_parameters().get_long_long("position");
            size_t len = 0;
            BYTE* buf = (BYTE*)im.get_parameters().get_binary("payload", len);

            if (tmp_len_ == 0)
            {
              tmp_buf_.reset(new BYTE[BUFSIZE]);
            }

            // loop on arrived data until all gets copied into buffer
            // some may be left in temporary buffer until next portion of data arrives

            while (len != 0)
            {
              size_t available_in_tmp = BUFSIZE - tmp_len_;

              if (len >= available_in_tmp)
              {
                // if arrived data fills remaining space in temporary buffer
                // copy all it fits into temporary buffer
                memcpy(tmp_buf_.get() + tmp_len_, buf, available_in_tmp);

                len -= available_in_tmp;
                buf += available_in_tmp;

                // add it to main buffer
                lock_guard<mutex> lock(buffer_mutex_);
                buffer_.push_back(tmp_buf_);
                buffer_len_ += BUFSIZE;
                tmp_len_ = 0;
              }
              else
              {
                // copy into temporary buffer
                memcpy(tmp_buf_.get() + tmp_len_, buf, len);
                tmp_len_ += len;
                len = 0;
              }
            }

            // removing already read elements
            lock_guard<mutex> lock(buffer_mutex_);
            while (buffer_pos_ >= BUFSIZE)
            {
              buffer_.erase_begin(1);
              buffer_pos_ -= BUFSIZE;
              buffer_len_ -= BUFSIZE;
            }

            if (buffer_.size() > BUFMAX * 0.66)
            {

            }
          }
          catch (const std::exception& e)
          {
          }
        }
        //im.reply();
      }
      else if (im.get_message_name() == "session_deleted")
      {
        session_ = -1;
      }
    };

    AGENT.register_object("player", func);

    _discovery->subscribe([&](const std::string& service, bool available)
    {
      if (available)
      {
        if (service == "tv")
        {
          //m_CurStatus = ADDON_STATUS_OK;

          AGENT.send(DISCOVERY.get("tv"), "tv", "hello");
        }
      }
      else if (service == "tv")
      {
        //m_CurStatus = ADDON_STATUS_LOST_CONNECTION;
      }
    });
  }

  pvr_client::~pvr_client()
  {
    AGENT.unregister_object("player");

    _discovery.reset();
    _yc.reset();
    session_ = -1;
    seeking_ = false;
    lock_guard<mutex> lock(buffer_mutex_);
    tmp_len_ = 0;
    buffer_len_ = 0;
    buffer_pos_ = 0;
    remote_buf_len_ = 0;
    remote_buf_pos_ = 0;
    buffer_.clear();
  }

  int pvr_client::get_channels_num()
  {

    size_t c = 50; // 5 seconds
    while (c)
    {
      try
      {
        std::unique_ptr<yami::outgoing_message> message(AGENT.send(DISCOVERY.get("tv"), "tv", "get_channels"));
        message->wait_for_completion(1000);
        if (message->get_state() == yami::replied)
        {
          //XBMC->Log(LOG_DEBUG, "%s - Number of channels: %d", __FUNCTION__, message->get_reply().get_string_array_length("name"));
          return message->get_reply().get_string_array_length("name");
        }
      }
      catch (const home_system::service_not_found& e)
      {
        Sleep(100);
        c--;
      }
    }
    return -1;
  }

  void pvr_client::get_channels(std::function<void(int id, const std::string& name)> callback)
  {
    size_t c = 50; // 5 seconds
    while (c)
    {
      try
      {
        std::unique_ptr<yami::outgoing_message> message(AGENT.send(DISCOVERY.get("tv"), "tv", "get_channels"));
        message->wait_for_completion(1000);
        if (message->get_state() == yami::replied)
        {
          size_t s = message->get_reply().get_string_array_length("name");
          int* channels = message->get_reply().get_integer_array("channel", s);

          for (size_t i = 0; i < s; ++i)
          {
            callback(channels[i], message->get_reply().get_string_in_array("name", i));
          }
          return;
        }
      }
      catch (const home_system::service_not_found&)
      {
        c--;
        Sleep(100);
      }
    }
  }

  void pvr_client::get_epg(int channel_id, long long start_time, long long end_time, std::function<void(epg_entry&)> callback)
  {
    size_t c = 50; // 5 seconds
    while (c)
    {
      try
      {
        yami::parameters params;
        params.set_integer("channel", channel_id);
        params.set_long_long("start_time", start_time);
        std::unique_ptr<yami::outgoing_message> message(AGENT.send(DISCOVERY.get("tv"), "tv", "get_epg_data", params));
        message->wait_for_completion(1000);
        if (message->get_state() == yami::replied)
        {
          size_t s = message->get_reply().get_integer("event_num");
          if (s > 0)
          {
            int* ids = message->get_reply().get_integer_array("id", s);
            int* durations = message->get_reply().get_integer_array("duration", s);
            long long* start_times = message->get_reply().get_long_long_array("start_time", s);


            for (size_t i = 0; i < s; ++i)
            {
              if (start_times[i] > start_time && start_times[i] < end_time)
              {
                epg_entry entry;

                entry.id = ids[i];
                entry.title = message->get_reply().get_string_in_array("name", i);
                entry.plot = message->get_reply().get_string_in_array("plot", i);
                entry.channel_id = channel_id;
                entry.start = start_times[i];
                entry.end = start_times[i] + durations[i];

                callback(entry);
              }
            }
          }
        }
        return;
      }
      catch (const home_system::service_not_found& e)
      {
        c--;
        Sleep(100);
      }
    }
  }

  void pvr_client::create_session(int id)
  {
    yami::parameters params;

    params.set_integer("channel", id);
    params.set_string("destination", "player");
    params.set_string("endpoint", YC.endpoint());

    unique_ptr<yami::outgoing_message> message(AGENT.send(DISCOVERY.get("tv"), "tv", "create_session", params));

    message->wait_for_completion(1000);

    if (message->get_state() != yami::replied)
    {
      session_ = -1;
      throw runtime_error("No reply in time");
    }

    session_ = message->get_reply().get_integer("session");

    if (session_ == -1)
    {
      auto reason = message->get_reply().get_string("reason");
      throw runtime_error("Failed to create session because: " + reason);
    }
  }

  int pvr_client::read_data(unsigned char *inbuf, unsigned int buf_size)
  {
    int i = 0;
    while (buffer_len_ == 0 || buffer_pos_ == buffer_len_)
      Sleep(0);
    size_t size = buf_size;

    BYTE* buf = inbuf;

    lock_guard<mutex> lock(buffer_mutex_);

    if (size > buffer_len_ - buffer_pos_)
    {
      size = buffer_len_ - buffer_pos_;
    }

    size_t read_size = size;

    while (read_size != 0)
    {
      size_t buffer_index = buffer_pos_ / BUFSIZE;
      size_t local_buf_pos = buffer_pos_ - buffer_index * BUFSIZE;
      size_t to_read;
      if (read_size >= BUFSIZE)
      {
        to_read = BUFSIZE - local_buf_pos;
      }
      else
      {
        to_read = read_size;
      }
      //XBMC->Log(LOG_DEBUG, "Read: read_size: %d, buffer_index: %d, buffer pos=%d, to_read=%d, buffer size=%d, buffer_len=%d", read_size, buffer_index, buffer_pos_, to_read, buffer_.size(), buffer_len_);
      memcpy(buf, buffer_[buffer_index].get() + local_buf_pos, to_read);
      buffer_pos_ += to_read;
      read_size -= to_read;
      buf += to_read;
    }
    return size;
  }

  void pvr_client::play()
  {
    yami::parameters params;

    params.set_integer("session", session_);

    AGENT.send_one_way(DISCOVERY.get("tv"), "tv", "play_session", params);
  }

  void pvr_client::pause()
  {
    yami::parameters params;

    params.set_integer("session", session_);

    AGENT.send_one_way(DISCOVERY.get("tv"), "tv", "pause_session", params);
  }

  long long pvr_client::seek(long long pos)
  {
    seeking_ = true;

    lock_guard<mutex> lock(buffer_mutex_);

    yami::parameters params;

    params.set_integer("session", session_);
    params.set_long_long("position", pos);

    unique_ptr<yami::outgoing_message> message(AGENT.send(DISCOVERY.get("tv"), "tv", "seek_session", params));

    message->wait_for_completion(1000);

    if (message->get_state() != yami::replied)
    {
      throw runtime_error("No reply in time");
    }

    tmp_len_ = 0;
    buffer_len_ = 0;
    buffer_pos_ = 0;
    remote_buf_pos_ = 0;

    buffer_.clear();

    remote_buf_pos_ = message->get_reply().get_long_long("position");

    seeking_ = false;

    return remote_buf_pos_;
  }

  void pvr_client::destroy_session()
  {
    if (session_ != -1)
    {
      lock_guard<mutex> lock(buffer_mutex_);

      yami::parameters params;

      params.set_integer("session", session_);

      AGENT.send(DISCOVERY.get("tv"), "tv", "delete_session", params);

      session_ = -1;

      tmp_len_ = 0;
      buffer_len_ = 0;
      buffer_pos_ = 0;
      remote_buf_len_ = 0;
      remote_buf_pos_ = 0;

      buffer_.clear();
    }
  }

  long long pvr_client::get_buffer_length()
  {
    return remote_buf_len_;
  }

  long long pvr_client::get_buffer_position()
  {
    return remote_buf_pos_ - buffer_len_ + buffer_pos_;
  }
}
