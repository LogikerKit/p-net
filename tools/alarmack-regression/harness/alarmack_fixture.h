/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ALARMACK_FIXTURE_H
#define ALARMACK_FIXTURE_H

#include <gtest/gtest.h>
#include "utils_for_testing.h"
#include "mocks.h"
#include "pf_includes.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace AlarmAckTest
{
using Bytes = std::vector<uint8_t>;
using Status = std::array<uint8_t, 4>;

struct WireAlarm
{
   size_t fixed = 0;
   size_t block = 0;
   uint16_t frame_id = 0;
   uint16_t destination = 0;
   uint16_t source = 0;
   uint8_t type = 0;
   bool tack = false;
   uint16_t sequence = 0;
   uint16_t ack_sequence = 0;
   uint16_t length = 0;
   uint16_t alarm_type = 0;
   uint32_t api = 0;
   uint16_t slot = 0;
   uint16_t subslot = 0;
   uint16_t specifier = 0;
};

struct Attempt
{
   Bytes frame;
   int result;
};

uint16_t read16 (const Bytes & frame, size_t offset);
void write16 (Bytes & frame, size_t offset, uint16_t value);
std::string hex (const Bytes & frame);
bool decode (const Bytes & frame, WireAlarm & result, bool notification);

/* IEC 61158-6-10:2019 Table 411 and Tables 238/242/886.
 * Public startup, notification API, pf_eth_recv and pnet_handle_periodic drive
 * the state machines. Only configuration, input bytes and PNAL return values
 * are controlled; protocol states and counters are never assigned by tests. */
class Fixture : public PnetIntegrationTest
{
 public:
   unsigned callbacks = 0;
   Status callback_status = {{0, 0, 0, 0}};
   uint32_t callback_arep = 0;
   pf_ar_t * ar = nullptr;
   uint16_t peer_sequence = 0;
   Bytes notification_frame;
   WireAlarm notification;
   std::vector<Attempt> attempts;
   bool next_sent = false;

   void begin_notification (bool separate_ack);
   Bytes alarm_ack (const Status & status) const;
   void inject (const Bytes & frame);
   void arm_send_fault (int result);
   int observe_send (const uint8_t * payload, uint16_t length);
   void request_next_notification ();

 protected:
   void SetUp () override;
   void cfg_init () override;
   void TearDown () override;

 private:
   bool fault_armed = false;
   int first_send_result = 1;
   void startup ();
   Bytes peer_header (bool data, uint16_t length) const;
   Bytes last_send () const;
};
} // namespace AlarmAckTest
#endif
