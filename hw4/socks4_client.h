//
// Created by ray on 18年12月21日.
//

#ifndef INC_0756536_NP_PROJECT4_SOCKS4_CLIENT_H
#define INC_0756536_NP_PROJECT4_SOCKS4_CLIENT_H

#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#define USER_NAME "Ray"

using namespace std;
using namespace boost::asio;

namespace socks4c {

    const unsigned char version = 0x04;

    // VN=0, CD=90(accepted) or 91(rejected or failed)
    class reply
    {
    public:
        enum status_type
        {
            request_granted = 0x5a,
            request_failed = 0x5b
        };

        reply()
                : version_(0),
                  status_()
        {}

        unsigned char status() const { return status_; }

        boost::array<mutable_buffer, 5> buffers()
        {
            boost::array<mutable_buffer, 5> bufs =
                    {
                            {
                                    buffer(&version_, 1),
                                    buffer(&status_, 1),
                                    buffer(&port_high_byte_, 1),
                                    buffer(&port_low_byte_, 1),
                                    buffer(address_)
                            }
                    };
            return bufs;
        }

        bool success() const
        {
            return version_ == 0 && status_ == request_granted;
        }

        ip::tcp::endpoint endpoint() const
        {
            unsigned short port = port_high_byte_;
            port = static_cast<unsigned short>((port << 8) & 0xff00);
            port = port | port_low_byte_;

            ip::address_v4 address(address_);

            return ip::tcp::endpoint(address, port);
        }

    private:
        unsigned char version_;
        unsigned char status_;
        unsigned char port_high_byte_;
        unsigned char port_low_byte_;
        ip::address_v4::bytes_type address_;
    };

    // VN=4, CD=1 or 2
    class request
    {
    public:
        enum command_type
        {
            connect = 0x01,
            bind = 0x02
        };

        request(command_type cmd, const boost::asio::ip::tcp::endpoint& endpoint)
                : version_(version),
                  command_(cmd),
                  u_id_(USER_NAME),
                  null_byte_(0)
        {
            // Only IPv4 is supported by the SOCKS 4 protocol.
            if (endpoint.protocol() != ip::tcp::v4())
            {
                throw boost::system::system_error(
                        error::address_family_not_supported);
            }

            // Convert port number to network byte order.
            unsigned short port = endpoint.port();
            port_high_byte_ = static_cast<unsigned char>((port >> 8) & 0xff);
            port_low_byte_ = static_cast<unsigned char>(port & 0xff);

            // Save IP address in network byte order.
            address_ = endpoint.address().to_v4().to_bytes();
        }

        boost::array<const_buffer, 7> buffers()
        {
            boost::array<const_buffer, 7> bufs =
                    {
                            {
                                    buffer(&version_, 1),
                                    buffer(&command_, 1),
                                    buffer(&port_high_byte_, 1),
                                    buffer(&port_low_byte_, 1),
                                    buffer(address_),
                                    //buffer(u_id_),   // send u_id (not use)
                                    buffer(&null_byte_, 1)
                            }
                    };
            return bufs;
        }

        unsigned char command() const { return command_; }

        string short_command() const {
            if (command_ == connect)
                return "c";
            else if (command_ == bind)
                return "b";
            else
                return "nothing";
        }

        string s_command() const {
            if (command_ == connect)
                return "connect";
            else if (command_ == bind)
                return "bind";
            else
                return "nothing";
        }

    private:
        unsigned char version_;
        unsigned char command_;
        unsigned char port_high_byte_;
        unsigned char port_low_byte_;
        ip::address_v4::bytes_type address_;
        string u_id_;
        unsigned char null_byte_;
    };
}

#endif //INC_0756536_NP_PROJECT4_SOCKS4_CLIENT_H
