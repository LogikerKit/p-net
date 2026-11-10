/* SPDX-License-Identifier: GPL-3.0-or-later
 * Provisional interpretation of IEC 61158-6-10:2019 Table 242 rows 7/8/16,
 * Table 243, Table 409 row 5 and Table 886 row 13. See EXPERT_REVIEW.md.
 * Faults occur after a complete TACK reaches the PNAL send mock, NOT before
 * allocation/request creation. No standards approval follows from this test. */
#include "alarmack_fixture.h"
#include "recovery_cases.h"
#include <cstdio>

using namespace AlarmAckTest;

class AlarmAckRecovery : public Fixture,
                         public ::testing::WithParamInterface<RecoveryCase>
{
};

TEST_P (AlarmAckRecovery, CheckedCompletionContract)
{
   const RecoveryCase & tc = GetParam ();
   begin_notification (tc.separate);
   ASSERT_FALSE (HasFatalFailure ());
   const Status status = {{tc.status[0], tc.status[1], tc.status[2], tc.status[3]}};
   const Bytes frame = alarm_ack (status);
   arm_send_fault (tc.send_result);
   inject (frame);
   ASSERT_FALSE (HasFatalFailure ());
   ASSERT_EQ (attempts.size (), 1U);
   const unsigned first_callbacks = callbacks;
   const uint16_t first_sequence = ar->apmx[1].exp_seq_count;
   const int first_state = ar->alpmx[1].alpmi_state;
   const int first_result = attempts[0].result;
   EXPECT_EQ (first_callbacks, tc.first_callbacks);
   EXPECT_EQ (first_sequence, tc.next_sequence);
   EXPECT_EQ (first_state, PF_ALPMI_STATE_W_ALARM);
   EXPECT_EQ (first_result, tc.send_result == 1 ?
      static_cast<int> (attempts[0].frame.size ()) : tc.send_result);
   EXPECT_EQ (callback_status, status);
   inject (frame);
   ASSERT_FALSE (HasFatalFailure ());
   const unsigned retry_callbacks = callbacks;
   inject (frame);
   ASSERT_FALSE (HasFatalFailure ());
   ASSERT_EQ (attempts.size (), 3U);
   EXPECT_EQ (retry_callbacks, tc.retry_callbacks);
   EXPECT_EQ (callbacks, tc.retry_callbacks);
   EXPECT_EQ (ar->apmx[1].exp_seq_count, tc.next_sequence);
   EXPECT_EQ (attempts[0].frame, attempts[1].frame);
   EXPECT_EQ (attempts[1].frame, attempts[2].frame);
   EXPECT_GT (attempts[1].result, 0);
   EXPECT_GT (attempts[2].result, 0);
   request_next_notification ();
   ASSERT_FALSE (HasFatalFailure ());
   EXPECT_TRUE (next_sent);
   std::printf (
      "ALARM_MODEL_JSON {\"scope\":\"recovery\",\"id\":%u,"
      "\"first_callbacks\":%u,\"retry_callbacks\":%u,\"callbacks\":%u,"
      "\"first_sequence\":%u,\"first_send_result\":%d,"
      "\"attempted_tacks\":%zu,\"retry_send_results\":[%d,%d],"
      "\"status\":[%u,%u,%u,%u],\"next_sent\":%s,"
      "\"block_offset\":%zu,\"frame_hex\":\"%s\",\"tack_hex\":\"%s\"}\n",
      tc.id, first_callbacks, retry_callbacks, callbacks, first_sequence,
      first_result, attempts.size (), attempts[1].result, attempts[2].result,
      callback_status[0], callback_status[1], callback_status[2], callback_status[3],
      next_sent ? "true" : "false", notification.block, hex (frame).c_str (),
      hex (attempts[0].frame).c_str ());
}

INSTANTIATE_TEST_SUITE_P (
   LeanCorpus, AlarmAckRecovery, ::testing::ValuesIn (recovery_cases));
