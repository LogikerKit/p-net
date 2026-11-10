/* SPDX-License-Identifier: GPL-3.0-or-later
 * IEC 61158-6-10:2019, 5.1.2/Table 411; Table 242 sequence/duplicate handling.
 * parser_cases.h is emitted by the checked Lean model, not hand-maintained. */
#include "alarmack_fixture.h"
#include "parser_cases.h"
#include <cstdio>

using namespace AlarmAckTest;

class AlarmAckParser : public Fixture,
                       public ::testing::WithParamInterface<ParserCase>
{
};

TEST_P (AlarmAckParser, CheckedWireContract)
{
   const ParserCase & tc = GetParam ();
   begin_notification (tc.separate);
   ASSERT_FALSE (HasFatalFailure ());
   const Status status = {{tc.status[0], tc.status[1], tc.status[2], tc.status[3]}};
   const Bytes valid = alarm_ack (status);
   Bytes received = valid;
   const size_t block = notification.block;
   /* Mutation IDs are tied to Parser.lean. The bridge independently checks
    * these exact received bytes against the model-emitted block afterward. */
   if (tc.mutation >= 1 && tc.mutation <= 16)
      received.resize (received.size () - tc.mutation);
   else if (tc.mutation == 17)
      write16 (received, block + 2, 17);
   else if (tc.mutation == 18)
      write16 (received, block + 2, 19);
   else if (tc.mutation == 19)
      received[block + 4] = 0;
   else if (tc.mutation == 20)
      received[block + 5] = 1;
   else if (tc.mutation == 21)
   {
      received.push_back (0xaa);
      write16 (received, block - 2, 23);
   }
   else if (tc.mutation == 22)
      write16 (received, block - 2, 21);
   else if (tc.mutation == 23)
      write16 (received, block - 2, 23);

   const unsigned before = mock_os_data.eth_send_count;
   const auto sender = ar->apmx[1].apms_state;
   inject (received);
   ASSERT_FALSE (HasFatalFailure ());
   const unsigned first_callbacks = callbacks;
   const uint16_t first_sequence = ar->apmx[1].exp_seq_count;
   const unsigned first_sends = mock_os_data.eth_send_count - before;
   bool rejected_without_effects = true;
   if (!tc.accepted)
   {
      rejected_without_effects = callbacks == 0 && first_sends == 0 &&
         first_sequence == peer_sequence && ar->apmx[1].apms_state == sender &&
         ar->apmx[1].apmr_state == PF_APMR_STATE_OPEN &&
         ar->alpmx[1].alpmi_state == PF_ALPMI_STATE_W_ACK;
      EXPECT_TRUE (rejected_without_effects);
      inject (valid);
      ASSERT_FALSE (HasFatalFailure ());
   }
   EXPECT_EQ (callbacks, 1U);
   EXPECT_EQ (callback_status, status);
   EXPECT_EQ (callback_arep, appdata.main_arep);
   EXPECT_EQ (ar->apmx[1].exp_seq_count, (peer_sequence + 1) & 0x7fff);
   const unsigned delivered = callbacks;
   inject (valid);
   ASSERT_FALSE (HasFatalFailure ());
   EXPECT_EQ (callbacks, delivered);
   const bool duplicate_suppressed = callbacks == delivered;
   request_next_notification ();
   ASSERT_FALSE (HasFatalFailure ());
   EXPECT_TRUE (next_sent);
   std::printf (
      "ALARM_MODEL_JSON {\"scope\":\"parser\",\"id\":%u,"
      "\"first_callbacks\":%u,\"first_sequence\":%u,\"first_tacks\":%u,"
      "\"callbacks\":%u,\"status\":[%u,%u,%u,%u],"
      "\"rejected_without_effects\":%s,\"duplicate_suppressed\":%s,"
      "\"next_sent\":%s,\"block_offset\":%zu,\"frame_hex\":\"%s\","
      "\"valid_frame_hex\":\"%s\"}\n",
      tc.id, first_callbacks, first_sequence, first_sends, callbacks,
      callback_status[0], callback_status[1], callback_status[2], callback_status[3],
      rejected_without_effects ? "true" : "false",
      duplicate_suppressed ? "true" : "false", next_sent ? "true" : "false",
      block, hex (received).c_str (), hex (valid).c_str ());
}

INSTANTIATE_TEST_SUITE_P (
   LeanCorpus, AlarmAckParser, ::testing::ValuesIn (parser_cases));
