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
size_t remote_buf_len_(0);
size_t remote_buf_pos_(0);

int session_ = -1;

void create()
{
  _yc = home_system::yami_container::create();
  _discovery = home_system::discovery::create();

  auto func = [&](yami::incoming_message& im)
  {
    if (im.get_message_name() == "stream_part")
    {
      if (session_ != -1)
      {
        try
        {
          remote_buf_len_ = im.get_parameters().get_integer("buffer_size");
          size_t len = 0;
          BYTE* buf = (BYTE*)im.get_parameters().get_binary("payload", len);

          //if (XBMC)
            //XBMC->Log(LOG_DEBUG, "Receive: %d, In buffer: %d, in temp buffer: %d, buffer pos=%d", len, buffer_len_, tmp_len_, buffer_pos_);

          if (tmp_len_ == 0)
          {
            tmp_buf_.reset(new BYTE[BUFSIZE]);
          }

          // loop on arrived data until all gets copied into buffer
          // some may be left in temporary buffer until next portion of data arrives

          while (len != 0)
          {
            //LOG("len= " << len << " buffer_len_=" << buffer_len_ << " tmp_len_=" << tmp_len_ << " buffer_pos_=" << buffer_pos_);

            size_t available_in_tmp = BUFSIZE - tmp_len_;

            if (len >= available_in_tmp)
            {
              // if arrived data fills remaining space in temporary buffer
              // copy all it fits into temporary buffer
              memcpy(tmp_buf_.get() + tmp_len_, buf, available_in_tmp);

              len -= available_in_tmp;
              buf += available_in_tmp;

              lock_guard<mutex> lock(buffer_mutex_);

              // add it to main buffer
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
            //XBMC->Log(LOG_DEBUG, "In buffer: %d, in temp buffer: %d", buffer_len_, tmp_len_);
          }

          lock_guard<mutex> lock(buffer_mutex_);
          // removing already read elements
          while (buffer_pos_ >= BUFSIZE)
          {
            buffer_.erase_begin(1);
            buffer_pos_ -= BUFSIZE;
            buffer_len_ -= BUFSIZE;
          }
          //if (XBMC)
            //XBMC->Log(LOG_DEBUG, "After receive: In buffer: %d, in temp buffer: %d, buffer pos=%d", buffer_len_, tmp_len_, buffer_pos_);
          //LOG("buffer_len_=" << buffer_len_ << " tmp_len_=" << tmp_len_ << " buffer_pos_=" << buffer_pos_);
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
    //if (XBMC)
      //XBMC->Log(LOG_DEBUG, "Service %s available = %s", service.c_str(), available ? "true" : "false");

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

void destroy()
{
  delete m_data;
}