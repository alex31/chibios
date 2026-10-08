/*
    ChibiOS - Copyright (C) 2006-2026 Giovanni Di Sirio.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "hal.h"
#include "usbaudio.h"

#define AUDIO_PHASE_STEP                  39370534U
#define AUDIO_FRAME_MASK                  0x07FFU

volatile audio_stats_t audio_stats;

static USBInEndpointState audio_in_state;
static uint8_t audio_packet[AUDIO_PACKET_SIZE];
static uint8_t audio_alt;
static bool audio_pending;
static bool audio_have_frame;
static uint16_t audio_last_frame;
static uint32_t audio_phase;

/* 256 points, peak 8192 (-12 dBFS), linearly interpolated by the synthesizer.
   440 Hz DDS at 48 kHz, without floating-point work in the USB ISR. */
static const int16_t sine_table[256] = {
      0,   201,   402,   603,   803,  1003,  1202,  1401,
   1598,  1795,  1990,  2185,  2378,  2570,  2760,  2948,
   3135,  3320,  3503,  3683,  3862,  4038,  4212,  4383,
   4551,  4717,  4880,  5040,  5197,  5351,  5501,  5649,
   5793,  5933,  6070,  6203,  6333,  6458,  6580,  6698,
   6811,  6921,  7027,  7128,  7225,  7317,  7405,  7489,
   7568,  7643,  7713,  7779,  7839,  7895,  7946,  7993,
   8035,  8071,  8103,  8130,  8153,  8170,  8182,  8190,
   8192,  8190,  8182,  8170,  8153,  8130,  8103,  8071,
   8035,  7993,  7946,  7895,  7839,  7779,  7713,  7643,
   7568,  7489,  7405,  7317,  7225,  7128,  7027,  6921,
   6811,  6698,  6580,  6458,  6333,  6203,  6070,  5933,
   5793,  5649,  5501,  5351,  5197,  5040,  4880,  4717,
   4551,  4383,  4212,  4038,  3862,  3683,  3503,  3320,
   3135,  2948,  2760,  2570,  2378,  2185,  1990,  1795,
   1598,  1401,  1202,  1003,   803,   603,   402,   201,
      0,  -201,  -402,  -603,  -803, -1003, -1202, -1401,
  -1598, -1795, -1990, -2185, -2378, -2570, -2760, -2948,
  -3135, -3320, -3503, -3683, -3862, -4038, -4212, -4383,
  -4551, -4717, -4880, -5040, -5197, -5351, -5501, -5649,
  -5793, -5933, -6070, -6203, -6333, -6458, -6580, -6698,
  -6811, -6921, -7027, -7128, -7225, -7317, -7405, -7489,
  -7568, -7643, -7713, -7779, -7839, -7895, -7946, -7993,
  -8035, -8071, -8103, -8130, -8153, -8170, -8182, -8190,
  -8192, -8190, -8182, -8170, -8153, -8130, -8103, -8071,
  -8035, -7993, -7946, -7895, -7839, -7779, -7713, -7643,
  -7568, -7489, -7405, -7317, -7225, -7128, -7027, -6921,
  -6811, -6698, -6580, -6458, -6333, -6203, -6070, -5933,
  -5793, -5649, -5501, -5351, -5197, -5040, -4880, -4717,
  -4551, -4383, -4212, -4038, -3862, -3683, -3503, -3320,
  -3135, -2948, -2760, -2570, -2378, -2185, -1990, -1795,
  -1598, -1401, -1202, -1003,  -803,  -603,  -402,  -201
};

static void audio_queue_i(USBDriver *usbp);

/* Endpoint callbacks are invoked from the USB ISR, outside critical zones.*/
static void audio_in_cb(USBDriver *usbp, usbep_t ep) {

  (void)ep;
  osalSysLockFromISR();
  audio_stats.callbacks++;
  audio_pending = false;
  audio_queue_i(usbp);
  osalSysUnlockFromISR();
}

static const USBEndpointConfig audio_ep_config = {
  USB_EP_MODE_TYPE_ISOC,
  NULL,
  audio_in_cb,
  NULL,
  AUDIO_PACKET_SIZE,
  0x0000,
  &audio_in_state,
  NULL,
  1,
  NULL
};

/* Called locked, either from SOF or after the previous packet completes.
   OTG arms isochronous transfers for the NEXT frame. Completion immediately
   queues that frame's packet; SOF bootstraps/restarts a stream. SOF-only
   queuing would leave alternate frames empty if the previous transfer is
   still busy when SOF is dispatched. Never queue twice in the same frame. */
static void audio_queue_i(USBDriver *usbp) {
  uint16_t frame;
  unsigned i;

  if ((audio_alt != 1U) || audio_pending ||
      (usbGetDriverStateI(usbp) != USB_ACTIVE)) {
    return;
  }

  frame = usbGetFrameNumberX(usbp) & AUDIO_FRAME_MASK;
  if (audio_have_frame) {
    unsigned elapsed = (frame - audio_last_frame) & AUDIO_FRAME_MASK;

    if (elapsed == 0U) {
      return;
    }
    if (elapsed > 1U) {
      audio_stats.skipped_frames += elapsed - 1U;
      audio_phase += AUDIO_PHASE_STEP * AUDIO_SAMPLES_PER_FRAME *
                     (elapsed - 1U);
    }
  }
  audio_last_frame = frame;
  audio_have_frame = true;

  for (i = 0U; i < AUDIO_SAMPLES_PER_FRAME; i++) {
    unsigned index = audio_phase >> 24U;
    int32_t fraction = (audio_phase >> 16U) & 255U;
    int32_t a = sine_table[index];
    int32_t b = sine_table[(index + 1U) & 255U];
    uint16_t sample = (uint16_t)(a + (b - a) * fraction / 256);

    audio_packet[i * 2U]      = (uint8_t)sample;
    audio_packet[i * 2U + 1U] = (uint8_t)(sample >> 8U);
    audio_phase += AUDIO_PHASE_STEP;
  }

  audio_pending = true;
  audio_stats.packets_queued++;
  usbStartTransmitI(usbp, AUDIO_IN_EP, audio_packet, sizeof audio_packet);
}

static void audio_clear(void) {

  audio_alt = 0U;
  audio_pending = false;
  audio_have_frame = false;
  audio_phase = 0U;
}

/* Bus reset, I-class.*/
void audioResetHookI(USBDriver *usbp) {

  (void)usbp;
  audio_stats.resets++;
  audio_clear();
}

/* Configuration selected or removed, both select alternate setting 0.*/
void audioConfigureHookI(USBDriver *usbp) {

  (void)usbp;
  audio_clear();
}

/* The driver has cancelled the transfer; SOF restarts the stream on resume.*/
void audioSuspendHookI(USBDriver *usbp) {

  (void)usbp;
  audio_stats.suspends++;
  audio_pending = false;
  audio_have_frame = false;
}

void audioSOFHookI(USBDriver *usbp) {

  if (audio_alt == 1U) {
    audio_stats.sofs++;
    audio_queue_i(usbp);
  }
}

/* GET_INTERFACE and SET_INTERFACE, not served by the default handler.
   Invoked from the USB ISR, outside critical zones. Other requests are left
   to the default handler, invalid ones stall EP0.*/
bool audioRequestsHook(USBDriver *usbp) {
  static uint8_t alt;
  uint8_t iface = usbp->setup[4];

  if (((usbp->setup[0] & (USB_RTYPE_TYPE_MASK | USB_RTYPE_RECIPIENT_MASK)) !=
       (USB_RTYPE_TYPE_STD | USB_RTYPE_RECIPIENT_INTERFACE)) ||
      ((usbp->setup[1] != USB_REQ_GET_INTERFACE) &&
       (usbp->setup[1] != USB_REQ_SET_INTERFACE))) {
    return false;
  }
  if ((usbGetDriverStateI(usbp) != USB_ACTIVE) ||
      (usbp->setup[3] != 0U) || (iface > AUDIO_STREAMING_INTERFACE) ||
      (usbp->setup[5] != 0U) || (usbp->setup[7] != 0U)) {
    return false;
  }

  if (usbp->setup[1] == USB_REQ_GET_INTERFACE) {
    if ((usbp->setup[0] != 0x81U) || (usbp->setup[2] != 0U) ||
        (usbp->setup[6] != 1U)) {
      return false;
    }
    alt = iface == AUDIO_STREAMING_INTERFACE ? audio_alt : 0U;
    usbSetupTransfer(usbp, &alt, 1U, NULL);
    return true;
  }

  if ((usbp->setup[0] != 0x01U) || (usbp->setup[6] != 0U) ||
      (usbp->setup[2] > (iface == AUDIO_STREAMING_INTERFACE ? 1U : 0U))) {
    return false;
  }
  if (iface == AUDIO_STREAMING_INTERFACE) {
    osalSysLockFromISR();
    if (audio_alt == 1U) {
      audio_stats.stops++;
    }
    audio_clear();
    /* Safe only for this single-function test: this API disables ALL
       non-control endpoints, not just the selected streaming interface.
       The IN disable is requested, not awaited, see
       usb_lld_disable_endpoints().*/
    usbDisableEndpointsI(usbp);
    if (usbp->setup[2] == 1U) {
      usbInitEndpointI(usbp, AUDIO_IN_EP, &audio_ep_config);
      audio_alt = 1U;
      audio_stats.starts++;
    }
    osalSysUnlockFromISR();
  }
  usbSetupTransfer(usbp, NULL, 0U, NULL);
  return true;
}
