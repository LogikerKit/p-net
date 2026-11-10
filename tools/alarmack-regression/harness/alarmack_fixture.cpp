/* SPDX-License-Identifier: GPL-3.0-or-later
 * Adapted from the earlier full-exchange harness. See PROVENANCE.md. */
#include "alarmack_fixture.h"
#include "startup_frames.h"

#include <cstdio>
#include <cstring>

namespace AlarmAckTest
{
static Fixture * active = nullptr;

static int confirmation (
   pnet_t *, void *, uint32_t arep, const pnet_pnio_status_t * status)
{
   if (active && status)
   {
      ++active->callbacks;
      active->callback_arep = arep;
      active->callback_status = {{status->error_code, status->error_decode,
                                 status->error_code_1, status->error_code_2}};
   }
   return 0;
}

uint16_t read16 (const Bytes & frame, size_t offset)
{
   return static_cast<uint16_t> (
      (static_cast<uint16_t> (frame.at (offset)) << 8) | frame.at (offset + 1));
}

void write16 (Bytes & frame, size_t offset, uint16_t value)
{
   frame.at (offset) = static_cast<uint8_t> (value >> 8);
   frame.at (offset + 1) = static_cast<uint8_t> (value);
}

static void put16 (Bytes & frame, uint16_t value)
{
   frame.push_back (static_cast<uint8_t> (value >> 8));
   frame.push_back (static_cast<uint8_t> (value));
}

static void put32 (Bytes & frame, uint32_t value)
{
   put16 (frame, static_cast<uint16_t> (value >> 16));
   put16 (frame, static_cast<uint16_t> (value));
}

std::string hex (const Bytes & frame)
{
   const char digits[] = "0123456789abcdef";
   std::string result;
   for (uint8_t byte : frame)
   {
      result.push_back (digits[byte >> 4]);
      result.push_back (digits[byte & 15]);
   }
   return result;
}

bool decode (const Bytes & frame, WireAlarm & result, bool notification)
{
   if (frame.size () < 14)
      return false;
   size_t ether = 12;
   while (read16 (frame, ether) == 0x8100)
   {
      ether += 4;
      if (frame.size () < ether + 2)
         return false;
   }
   if (read16 (frame, ether) != 0x8892 || frame.size () < ether + 16)
      return false;
   result.frame_id = read16 (frame, ether + 2);
   result.fixed = ether + 4;
   result.block = result.fixed + 12;
   result.destination = read16 (frame, result.fixed);
   result.source = read16 (frame, result.fixed + 2);
   result.type = frame[result.fixed + 4] & 15;
   result.tack = (frame[result.fixed + 5] & 0x10) != 0;
   result.sequence = read16 (frame, result.fixed + 6);
   result.ack_sequence = read16 (frame, result.fixed + 8);
   result.length = read16 (frame, result.fixed + 10);
   if (frame.size () < result.block + result.length)
      return false;
   if (notification)
   {
      if (result.length < 26)
         return false;
      result.alarm_type = read16 (frame, result.block + 6);
      result.api = (static_cast<uint32_t> (read16 (frame, result.block + 8)) << 16) |
                   read16 (frame, result.block + 10);
      result.slot = read16 (frame, result.block + 12);
      result.subslot = read16 (frame, result.block + 14);
      result.specifier = read16 (frame, result.block + 24);
   }
   return true;
}

void Fixture::cfg_init ()
{
   std::memset (&pnet_default_cfg, 0, sizeof (pnet_default_cfg));
   PnetIntegrationTestBase::cfg_init ();
}

void Fixture::SetUp ()
{
   mock_init ();
   cfg_init ();
   appdata_init ();
   available_modules_and_submodules_init ();
   callcounter_reset ();
   ASSERT_EQ (pnet_init_only (net, &pnet_default_cfg), 0);
   pf_pdport_update_eth_status (net);
   mock_clear ();
}

void Fixture::TearDown ()
{
   active = nullptr;
   PnetIntegrationTest::TearDown ();
}

void Fixture::startup ()
{
   mock_set_pnal_udp_recvfrom_buffer (connect_req, sizeof (connect_req));
   run_stack (TEST_UDP_DELAY);
   ASSERT_EQ (appdata.cmdev_state, PNET_EVENT_STARTUP);
   ASSERT_EQ (appdata.call_counters.connect_calls, 1);
   ASSERT_NE (appdata.main_arep, 0U);
   mock_set_pnal_udp_recvfrom_buffer (write_req, sizeof (write_req));
   run_stack (TEST_UDP_DELAY);
   ASSERT_EQ (appdata.call_counters.write_calls, 1);
   mock_set_pnal_udp_recvfrom_buffer (prm_end_req, sizeof (prm_end_req));
   run_stack (TEST_UDP_DELAY);
   ASSERT_EQ (appdata.cmdev_state, PNET_EVENT_PRMEND);
   ASSERT_EQ (pnet_application_ready (net, appdata.main_arep), 0);
   mock_set_pnal_udp_recvfrom_buffer (appl_rdy_rsp, sizeof (appl_rdy_rsp));
   run_stack (TEST_UDP_DELAY);
   ASSERT_EQ (appdata.cmdev_state, PNET_EVENT_APPLRDY);
   for (unsigned i = 0; i < 100; ++i)
   {
      send_data (data_packet, sizeof (data_packet));
      run_stack (TEST_DATA_DELAY);
   }
   ASSERT_EQ (appdata.cmdev_state, PNET_EVENT_DATA);
   ASSERT_EQ (appdata.call_counters.state_calls, 4);
}

Bytes Fixture::last_send () const
{
   return Bytes (mock_os_data.eth_send_copy,
                 mock_os_data.eth_send_copy + mock_os_data.eth_send_len);
}

void Fixture::begin_notification (bool separate_ack)
{
   active = this;
   startup ();
   ASSERT_FALSE (HasFatalFailure ());
   pnet_handle_periodic (net);
   net->fspm_cfg.alarm_cnf_cb = confirmation;
   ASSERT_EQ (pf_ar_find_by_arep (net, appdata.main_arep, &ar), 0);
   ASSERT_NE (ar, nullptr);
   ASSERT_TRUE (ar->in_use && ar->alarm_enable && net->global_alarm_enable);
   ASSERT_EQ (ar->apmx[1].apmr_state, PF_APMR_STATE_OPEN);
   ASSERT_EQ (ar->apmx[1].apms_state, PF_APMS_STATE_OPEN);
   ASSERT_EQ (ar->alpmx[1].alpmi_state, PF_ALPMI_STATE_W_ALARM);
   peer_sequence = ar->apmx[1].exp_seq_count;
   ASSERT_EQ (peer_sequence, 0xffff);
   const uint8_t payload[] = {0xaa, 0xbb};
   const unsigned sent = mock_os_data.eth_send_count;
   ASSERT_EQ (pnet_alarm_send_process_alarm (
      net, appdata.main_arep, TEST_API_IDENT, TEST_SLOT_IDENT,
      TEST_SUBSLOT_IDENT, 0x1234, sizeof (payload), payload), 0);
   ASSERT_EQ (mock_os_data.eth_send_count, sent);
   pnet_handle_periodic (net);
   ASSERT_EQ (mock_os_data.eth_send_count, sent + 1);
   notification_frame = last_send ();
   ASSERT_TRUE (decode (notification_frame, notification, true));
   ASSERT_EQ (notification.frame_id, 0xfc01);
   ASSERT_EQ (notification.type, 1);
   ASSERT_EQ (read16 (notification_frame, notification.block), 1);
   ASSERT_EQ (notification.alarm_type, 2);
   ASSERT_EQ (notification.length, 30);
   ASSERT_TRUE (notification.tack);
   ASSERT_EQ (ar->apmx[1].apms_state, PF_APMS_STATE_WTACK);
   ASSERT_EQ (ar->alpmx[1].alpmi_state, PF_ALPMI_STATE_W_ACK);
   if (separate_ack)
   {
      inject (peer_header (false, 0));
      ASSERT_FALSE (HasFatalFailure ());
      ASSERT_EQ (ar->apmx[1].apms_state, PF_APMS_STATE_OPEN);
      ASSERT_EQ (ar->alpmx[1].alpmi_state, PF_ALPMI_STATE_W_ACK);
      ASSERT_EQ (callbacks, 0U);
   }
}

Bytes Fixture::peer_header (bool data, uint16_t length) const
{
   Bytes frame;
   frame.insert (frame.end (), notification_frame.begin () + 6,
                 notification_frame.begin () + 12);
   frame.insert (frame.end (), notification_frame.begin (),
                 notification_frame.begin () + 6);
   put16 (frame, 0x8100);
   put16 (frame, 0xc000);
   put16 (frame, 0x8892);
   put16 (frame, 0xfc01);
   put16 (frame, notification.source);
   put16 (frame, notification.destination);
   frame.push_back (data ? 0x11 : 0x13);
   frame.push_back (data ? 0x11 : 0x01);
   put16 (frame, data ? peer_sequence : static_cast<uint16_t> (peer_sequence - 1));
   put16 (frame, notification.sequence);
   put16 (frame, length);
   return frame;
}

Bytes Fixture::alarm_ack (const Status & status) const
{
   Bytes frame = peer_header (true, 22);
   /* Independent encoder of IEC 61158-6-10:2019 Table 411, not pf_put_*.
    * Header 6, AlarmType/API/Slot/Subslot/AlarmSpecifier 12, PNIOStatus 4. */
   put16 (frame, 0x8001);
   put16 (frame, 18);
   frame.push_back (1);
   frame.push_back (0);
   put16 (frame, notification.alarm_type);
   put32 (frame, notification.api);
   put16 (frame, notification.slot);
   put16 (frame, notification.subslot);
   put16 (frame, notification.specifier);
   frame.insert (frame.end (), status.begin (), status.end ());
   return frame;
}

void Fixture::inject (const Bytes & frame)
{
   ASSERT_GE (frame.size (), 32U);
   ASSERT_LE (frame.size (), static_cast<size_t> (PF_FRAME_BUFFER_SIZE));
   pnal_buf_t * buffer = pnal_buf_alloc (PF_FRAME_BUFFER_SIZE);
   ASSERT_NE (buffer, nullptr);
   std::memcpy (buffer->payload, frame.data (), frame.size ());
   buffer->len = static_cast<uint16_t> (frame.size ());
   const int handled = pf_eth_recv (mock_os_data.eth_if_handle, net, buffer);
   if (handled == 0)
      pnal_buf_free (buffer);
   ASSERT_EQ (handled, 1);
   pnet_handle_periodic (net);
}

void Fixture::arm_send_fault (int result)
{
   fault_armed = true;
   first_send_result = result;
}

int Fixture::observe_send (const uint8_t * payload, uint16_t length)
{
   if (!fault_armed)
      return length;
   const Bytes frame (payload, payload + length);
   WireAlarm parsed;
   const bool selected = decode (frame, parsed, false) &&
      parsed.frame_id == 0xfc01 && parsed.type == 3 && !parsed.tack &&
      parsed.length == 0 && parsed.ack_sequence == peer_sequence &&
      parsed.source == notification.source &&
      parsed.destination == notification.destination &&
      Bytes (frame.begin (), frame.begin () + 12) ==
         Bytes (notification_frame.begin (), notification_frame.begin () + 12);
   if (!selected)
      return length;
   const int result = attempts.empty () && first_send_result != 1 ?
      first_send_result : length;
   attempts.push_back ({frame, result});
   return result;
}

void Fixture::request_next_notification ()
{
   fault_armed = false;
   const uint8_t payload[] = {0xaa, 0xbb};
   const unsigned sent = mock_os_data.eth_send_count;
   ASSERT_EQ (pnet_alarm_send_process_alarm (
      net, appdata.main_arep, TEST_API_IDENT, TEST_SLOT_IDENT,
      TEST_SUBSLOT_IDENT, 0x1234, sizeof (payload), payload), 0);
   pnet_handle_periodic (net);
   if (mock_os_data.eth_send_count == sent + 1)
   {
      WireAlarm next;
      ASSERT_TRUE (decode (last_send (), next, true));
      next_sent = next.type == 1 && next.frame_id == 0xfc01 &&
         (next.specifier & 0x7ff) == ((notification.specifier + 1) & 0x7ff) &&
         next.sequence == ((notification.sequence + 1) & 0x7fff) &&
         ar->alpmx[1].alpmi_state == PF_ALPMI_STATE_W_ACK &&
         ar->apmx[1].apms_state == PF_APMS_STATE_WTACK;
   }
}
} // namespace AlarmAckTest

extern "C" int alarmack_test_send_observe (const uint8_t * payload, uint16_t length)
{
   return AlarmAckTest::active ?
      AlarmAckTest::active->observe_send (payload, length) : length;
}
