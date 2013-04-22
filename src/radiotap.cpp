/*
 * Copyright (c) 2012, Nasel
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <cstring>
#include <cassert>
#include <stdexcept>
#include "macros.h"
#ifndef WIN32
    #ifdef BSD
        #include <net/if_dl.h>
    #else
        #include <netpacket/packet.h>
    #endif
    #include <net/ethernet.h>
#endif
#include "radiotap.h"
#include "dot11.h"
#include "utils.h"
#include "packet_sender.h"
#include "exceptions.h"

namespace Tins {
void check_size(uint32_t total_sz, size_t field_size) {
    if(total_sz < field_size)
        throw malformed_packet();
}

template<typename T>
void read_field(const uint8_t* &buffer, uint32_t &total_sz, T& field) {
    check_size(total_sz, sizeof(field));
    memcpy(&field, buffer, sizeof(field));
    buffer += sizeof(field);
    total_sz -= sizeof(field);
}

RadioTap::RadioTap(PDU *child)
: PDU(child)
{
    std::memset(&_radio, 0, sizeof(_radio));
    init();
}

RadioTap::RadioTap(const uint8_t *buffer, uint32_t total_sz) 
{
    check_size(total_sz, sizeof(_radio));
    const uint8_t *buffer_start = buffer;
    std::memcpy(&_radio, buffer, sizeof(_radio));
    uint32_t radiotap_hdr_size = Endian::le_to_host(_radio.it_len);
    check_size(total_sz, radiotap_hdr_size);
    buffer += sizeof(_radio);
    radiotap_hdr_size -= sizeof(_radio);

    if(_radio.tsft) 
        read_field(buffer, radiotap_hdr_size, _tsft);
        
    if(_radio.flags) 
        read_field(buffer, radiotap_hdr_size, _flags);

    if(_radio.rate) 
        read_field(buffer, radiotap_hdr_size, _rate);
    
    if(_radio.channel) {
        if(((buffer - buffer_start) & 1) == 1) {
            buffer++;
            radiotap_hdr_size--;
        }
        read_field(buffer, radiotap_hdr_size, _channel_freq);
        uint16_t dummy;
        read_field(buffer, radiotap_hdr_size, dummy);
        _channel_type = dummy;
    }
    
    if(_radio.dbm_signal)
        read_field(buffer, radiotap_hdr_size, _dbm_signal);
    
    if(_radio.dbm_noise) 
        read_field(buffer, radiotap_hdr_size, _dbm_noise);
    
    if(_radio.antenna) 
        read_field(buffer, radiotap_hdr_size, _antenna);
    
    if(_radio.rx_flags) {
        if(((buffer - buffer_start) & 1) == 1) {
            buffer++;
            radiotap_hdr_size--;
        }
        read_field(buffer, radiotap_hdr_size, _rx_flags);
    }
    if(_radio.channel_plus) {
        uint32_t offset = ((buffer - buffer_start) % 4);
        if(offset) {
            offset = 4 - offset;
            buffer += offset;
            radiotap_hdr_size -= offset;
        }
        read_field(buffer, radiotap_hdr_size, _channel_type);
        read_field(buffer, radiotap_hdr_size, _channel_freq);
        read_field(buffer, radiotap_hdr_size, _channel);
        read_field(buffer, radiotap_hdr_size, _max_power);
    }

    total_sz -= Endian::le_to_host(_radio.it_len);
    buffer += radiotap_hdr_size;

    if((flags() & FCS) != 0) {
        check_size(total_sz, sizeof(uint32_t));
        total_sz -= sizeof(uint32_t);
    }

    if(total_sz) 
        inner_pdu(Dot11::from_bytes(buffer, total_sz));
}

void RadioTap::init() {
    channel(Utils::channel_to_mhz(1), 0xa0);
    flags(FCS);
    tsft(0);
    dbm_signal(0xce);
    rx_flags(0);
    antenna(0);
}

void RadioTap::version(uint8_t new_version) {
    _radio.it_version = new_version;
}
        
void RadioTap::padding(uint8_t new_padding) {
    _radio.it_pad = new_padding;
}

void RadioTap::length(uint16_t new_length) {
    _radio.it_len = Endian::host_to_le(new_length);
}

void RadioTap::tsft(uint64_t new_tsft) {
    _tsft = Endian::host_to_le(new_tsft);
    _radio.tsft = 1;
}

void RadioTap::flags(FrameFlags new_flags) {
    _flags = (uint8_t)new_flags;
    _radio.flags = 1;
}

void RadioTap::rate(uint8_t new_rate) {
    _rate = new_rate;
    _radio.rate = 1;
}

void RadioTap::channel(uint16_t new_freq, uint16_t new_type) {
    _channel_freq = Endian::host_to_le(new_freq);
    _channel_type = Endian::host_to_le<uint32_t>(new_type);
    _radio.channel = 1;
}
void RadioTap::dbm_signal(uint8_t new_dbm_signal) {
    _dbm_signal = new_dbm_signal;
    _radio.dbm_signal = 1;
}

void RadioTap::dbm_noise(uint8_t new_dbm_noise) {
    _dbm_noise = new_dbm_noise;
    _radio.dbm_noise = 1;
}

void RadioTap::antenna(uint8_t new_antenna) {
    _antenna = new_antenna;
    _radio.antenna = 1;
}

void RadioTap::rx_flags(uint16_t new_rx_flag) {
    _rx_flags = Endian::host_to_le(new_rx_flag);
    _radio.rx_flags = 1;
}

uint32_t RadioTap::header_size() const {
    uint32_t total_bytes = 0;
    if(_radio.tsft)
        total_bytes += sizeof(_tsft);
    if(_radio.flags)
        total_bytes += sizeof(_flags);
    if(_radio.rate)
        total_bytes += sizeof(_rate);
    if(_radio.channel) {
        total_bytes += (total_bytes & 1);
        total_bytes += sizeof(uint16_t) * 2;
    }
    if(_radio.dbm_signal)
        total_bytes += sizeof(_dbm_signal);
    if(_radio.dbm_noise)
        total_bytes += sizeof(_dbm_noise);
    if(_radio.antenna)
        total_bytes += sizeof(_antenna);
    if(_radio.rx_flags) {
        total_bytes += (total_bytes & 1);
        total_bytes += sizeof(_rx_flags);
    }
    if(_radio.channel_plus) {
        uint32_t offset = total_bytes % 4;
        if(offset)
            total_bytes += 4 - offset;
        total_bytes += 8;
    }
        
    return sizeof(_radio) + total_bytes;
}

uint32_t RadioTap::trailer_size() const {
    // will be sizeof(uint32_t) if the FCS-at-the-end bit is on.
    return ((_flags & 0x10) != 0) ? sizeof(uint32_t) : 0;
}

#ifndef WIN32
void RadioTap::send(PacketSender &sender, const NetworkInterface &iface) {
    if(!iface)
        throw invalid_interface();
    
    #ifndef BSD
        struct sockaddr_ll addr;

        memset(&addr, 0, sizeof(struct sockaddr_ll));

        addr.sll_family = Endian::host_to_be<uint16_t>(PF_PACKET);
        addr.sll_protocol = Endian::host_to_be<uint16_t>(ETH_P_ALL);
        addr.sll_halen = 6;
        addr.sll_ifindex = iface.id();
        
        Tins::Dot11 *wlan = dynamic_cast<Tins::Dot11*>(inner_pdu());
        if(wlan) {
            Tins::Dot11::address_type dot11_addr(wlan->addr1());
            std::copy(dot11_addr.begin(), dot11_addr.end(), addr.sll_addr);
        }

        sender.send_l2(*this, (struct sockaddr*)&addr, (uint32_t)sizeof(addr));
    #else
        sender.send_l2(*this, 0, 0, iface);
    #endif
}
#endif

bool RadioTap::matches_response(const uint8_t *ptr, uint32_t total_sz) const {
    if(sizeof(_radio) < total_sz)
        return false;
    const radiotap_hdr *radio_ptr = (const radiotap_hdr*)ptr;
    if(radio_ptr->it_len <= total_sz) {
        ptr += radio_ptr->it_len;
        total_sz -= radio_ptr->it_len;
        return inner_pdu() ? inner_pdu()->matches_response(ptr, total_sz) : true;
    }
    return false;
}

void RadioTap::write_serialization(uint8_t *buffer, uint32_t total_sz, const PDU *parent) {
    uint32_t sz = header_size();
    uint8_t *buffer_start = buffer;
    assert(total_sz >= sz);
    if(!_radio.it_len)
        _radio.it_len = Endian::host_to_le<uint16_t>(sz);
    memcpy(buffer, &_radio, sizeof(_radio));
    buffer += sizeof(_radio);
    if(_radio.tsft) {
        memcpy(buffer, &_tsft, sizeof(_tsft));
        buffer += sizeof(_tsft);
    }
    if(_radio.flags) {
        memcpy(buffer, &_flags, sizeof(_flags));
        buffer += sizeof(_flags);
    }
    if(_radio.rate) {
        memcpy(buffer, &_rate, sizeof(_rate));
        buffer += sizeof(_rate);
    }
    if(_radio.channel) {
        if(((buffer - buffer_start) & 1) == 1)
            *(buffer++) = 0;
        uint16_t dummy = _channel_type;
        memcpy(buffer, &_channel_freq, sizeof(_channel_freq));
        buffer += sizeof(_channel_freq);
        memcpy(buffer, &dummy, sizeof(dummy));
        buffer += sizeof(dummy);
    }
    if(_radio.dbm_signal) {
        memcpy(buffer, &_dbm_signal, sizeof(_dbm_signal));
        buffer += sizeof(_dbm_signal);
    }
    if(_radio.dbm_noise) {
        memcpy(buffer, &_dbm_noise, sizeof(_dbm_noise));
        buffer += sizeof(_dbm_noise);
    }
    if(_radio.antenna) {
        memcpy(buffer, &_antenna, sizeof(_antenna));
        buffer += sizeof(_antenna);
    }
    if(_radio.rx_flags) {
        if(((buffer - buffer_start) & 1) == 1)
            *(buffer++) = 0;
        memcpy(buffer, &_rx_flags, sizeof(_rx_flags));
        buffer += sizeof(_rx_flags);
    }
    if(_radio.channel_plus) {
        uint32_t offset = ((buffer - buffer_start) % 4);
        if(offset) {
            offset = 4 - offset;
            while(offset--) {
                *buffer++ = 0;
            }
        }
        memcpy(buffer, &_channel_type, sizeof(_channel_type));
        buffer += sizeof(_channel_type);
        memcpy(buffer, &_channel_freq, sizeof(_channel_freq));
        buffer += sizeof(_channel_freq);
        memcpy(buffer, &_channel, sizeof(_channel));
        buffer += sizeof(_channel);
        memcpy(buffer, &_max_power, sizeof(_max_power));
        buffer += sizeof(_max_power);
    }
    if((_flags & 0x10) != 0 && inner_pdu()) {
        *(uint32_t*)(buffer + inner_pdu()->size()) = Endian::host_to_le(
            Utils::crc32(buffer, inner_pdu()->size())
        );
    }
}
}
