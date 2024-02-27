#ifndef HFP_AG_CORE_H
#define HFP_AG_CORE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "audio_control.h"
#include "btstack_config.h"
#include "btstack_stdin.h"

#define INQUIRY_INTERVAL 10

uint8_t hfp_service_buffer[150];
const uint8_t    rfcomm_channel_nr = 1;
const char hfp_ag_service_name[] = "HFP AG Demo";

static bd_addr_t device_addr;
static const char * device_addr_string = "A1:2A:A9:B0:AD:31";

static int16_t * audio_buff;

static uint8_t codecs[] = {HFP_CODEC_CVSD};
static uint8_t negotiated_codec = HFP_CODEC_CVSD;
static hci_con_handle_t acl_handle = HCI_CON_HANDLE_INVALID;
static hci_con_handle_t sco_handle = HCI_CON_HANDLE_INVALID;
static btstack_packet_callback_registration_t hci_event_callback_registration;

static int ag_indicators_nr = 7;
static hfp_ag_indicator_t ag_indicators[] = {
    // index, name, min range, max range, status, mandatory, enabled, status changed
    {1, "service",   0, 1, 1, 0, 0, 0},
    {2, "call",      0, 1, 0, 1, 1, 0},
    {3, "callsetup", 0, 3, 0, 1, 1, 0},
    {4, "battchg",   0, 5, 3, 0, 0, 0},
    {5, "signal",    0, 5, 5, 0, 1, 0},
    {6, "roam",      0, 1, 0, 0, 1, 0},
    {7, "callheld",  0, 2, 0, 1, 1, 0}
};

static int call_hold_services_nr = 5;
static const char* call_hold_services[] = {"1", "1x", "2", "2x", "3"};

static int hf_indicators_nr = 2;
static hfp_generic_status_indicator_t hf_indicators[] = {
    {1, 1},
    {2, 1},
};

static hfp_voice_recognition_message_t msg = {
    0xABCD, HFP_TEXT_TYPE_MESSAGE_FROM_AG, HFP_TEXT_OPERATION_REPLACE, "The temperature in Munich is 30 degrees."
};

enum STATE {INIT, W4_INQUIRY_MODE_COMPLETE, ACTIVE} ;
enum STATE state = INIT;

#endif