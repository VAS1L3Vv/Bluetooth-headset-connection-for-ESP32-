#define BTSTACK_FILE__ "hfp_ag.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "audio_control.h"
#include "btstack_config.h"
#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

#define MAX_BUFFER_SIZE_BYTES 960000
#define INQUIRY_INTERVAL 10

uint8_t hfp_service_buffer[150];
const uint8_t    rfcomm_channel_nr = 1;
const char hfp_ag_service_name[] = "HFP AG Demo";

static bd_addr_t device_addr;
static const char * device_addr_string = "A1:2A:A9:B0:AD:31";

static uint8_t codecs[] = {HFP_CODEC_CVSD};
static uint8_t negotiated_codec = HFP_CODEC_CVSD;
static hci_con_handle_t acl_handle = HCI_CON_HANDLE_INVALID;
static hci_con_handle_t sco_handle = HCI_CON_HANDLE_INVALID;
static btstack_packet_callback_registration_t hci_event_callback_registration;

static btstack_timer_source_t hfp_outgoing_call_ringing_timer;

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

static hfp_phone_number_t subscriber_number = { 129, "225577"};
static hfp_voice_recognition_message_t msg = {
    0xABCD, HFP_TEXT_TYPE_MESSAGE_FROM_AG, HFP_TEXT_OPERATION_REPLACE, "The temperature in Munich is 30 degrees."
};

enum STATE {INIT, W4_INQUIRY_MODE_COMPLETE, ACTIVE} ;
enum STATE state = INIT;

#ifdef HAVE_BTSTACK_STDIN
// Testig User Interface 
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth HFP Audiogateway (AG) unit Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("\n");
    printf("1 - scan nearby HF units\n");
    // соъздадим базу даннных макс. из 10 устройств для подключения 
    printf("2 - establish HFP connection %s\n", bd_addr_to_str(device_addr));
    printf("3 - release HFP connection\n");
    printf("4 - establish audio connection\n");
    printf("5 - release audio connection\n");
    printf("6 - record headset microphone\n");
    printf("7 - playback recorded audio\n");
    printf("8 - playback codec2 audio\n");
    printf("9 - abort voice n clear buffer\n\n\n");
    printf("a - report AG failure\n");
    printf("b - delete all link keys\n");
    printf("c - Set signal strength to 0            | C - Set signal strength to 5\n");
    printf("d - Set battery level to 3              | D - Set battery level to 5\n");
    printf("e - Set speaker volume to 0  (minimum)  | E - Set speaker volume to 9  (default)\n");
    printf("f - Set speaker volume to 12 (higher)   | F - Set speaker volume to 15 (maximum)\n");
    printf("g - Set microphone gain to 0  (minimum) | G - Set microphone gain to 9  (default)\n");
    printf("h - Set microphone gain to 12 (higher)  | H - Set microphone gain to 15 (maximum)\n");
    printf("T - terminate connection\n");
    printf("---\n");
}

void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;

    switch (cmd){
        case '1':
            printf("Start scanning...\n");
            gap_inquiry_start(INQUIRY_INTERVAL);
            break;
        case '2':
            log_info("USER:\'%c\'", cmd);
            printf("Establish HFP service level connection to %s...\n", bd_addr_to_str(device_addr));
            status = hfp_ag_establish_service_level_connection(device_addr);
            break;
        case '3':
            log_info("USER:\'%c\'", cmd);
            printf("Release HFP service level connection.\n");
            status = hfp_ag_release_service_level_connection(acl_handle);
            break;
        case '4':
            log_info("USER:\'%c\'", cmd);
            printf("Establish Audio connection %s...\n", bd_addr_to_str(device_addr));
            status = hfp_ag_establish_audio_connection(acl_handle);
            break;
        case '5':
            log_info("USER:\'%c\'", cmd);
            printf("Release Audio connection.\n");
            status = hfp_ag_release_audio_connection(acl_handle);
            break;
        // case '6':
        //     log_info("USER:\'%c\'", cmd);
        //     bytes_read = record_bt_mic(audio_buff, secs);
        //     break;
        case 'a':
            log_info("USER:\'%c\'", cmd);
            printf("Report AG failure\n");
            status = hfp_ag_report_extended_audio_gateway_error_result_code(acl_handle, HFP_CME_ERROR_AG_FAILURE);
            break;
        case 'b':
            printf("Deleting all link keys\n");
            gap_delete_all_link_keys();
            break;
        case 'g':
            log_info("USER:\'%c\'", cmd);
            printf("Set signal strength to 0\n");
            status = hfp_ag_set_signal_strength(0);
            break;
        case 'G':
            log_info("USER:\'%c\'", cmd);
            printf("Set signal strength to 5\n");
            status = hfp_ag_set_signal_strength(5);
            break;
        case 'i':
            log_info("USER:\'%c\'", cmd);
            printf("Set battery level to 3\n");
            status = hfp_ag_set_battery_level(3);
            break;
        case 'I':
            log_info("USER:\'%c\'", cmd);
            printf("Set battery level to 5\n");
            status = hfp_ag_set_battery_level(5);
            break;
        case 'o':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 0 (minimum)\n");
            status = hfp_ag_set_speaker_gain(acl_handle, 0);
            break;
        case 'O':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 9 (default)\n");
            status = hfp_ag_set_speaker_gain(acl_handle, 9);
            break;
        case 'p':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 12 (higher)\n");
            status = hfp_ag_set_speaker_gain(acl_handle, 12);
            break;
        case 'P':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 15 (maximum)\n");
            status = hfp_ag_set_speaker_gain(acl_handle, 15);
            break;
        case 'q':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 0\n");
            status = hfp_ag_set_microphone_gain(acl_handle, 0);
            break;
        case 'Q':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 9\n");
            status = hfp_ag_set_microphone_gain(acl_handle, 9);
            break;
        case 's':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 12\n");
            status = hfp_ag_set_microphone_gain(acl_handle, 12);
            break;
        case 'S':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 15\n");
            status = hfp_ag_set_microphone_gain(acl_handle, 15);
            break;
        case 't':
            log_info("USER:\'%c\'", cmd);
            printf("Terminate HCI connection. 0x%2x\n", acl_handle);
            gap_disconnect(acl_handle);
            break;
        default:
            show_usage();
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf("Could not perform command, status 0x%02x\n", status);
    }
}

#endif

static void report_status(uint8_t status, const char * message){
    if (status != ERROR_CODE_SUCCESS){
        printf("%s command failed, status 0x%02x\n", message, status);
    } else {
        printf("%s command successful\n", message);
    }
}

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * event, uint16_t event_size)
{
    UNUSED(channel);
    bd_addr_t addr;
    uint8_t status;
    switch (packet_type)
    {
        case HCI_EVENT_PACKET:
            switch(hci_event_packet_get_type(event))
            {
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(event) != HCI_STATE_WORKING) break;
                    break;
                case GAP_EVENT_INQUIRY_RESULT:
                    gap_event_inquiry_result_get_bd_addr(event, addr);
                    // print info
                    printf("Device found: %s ",  bd_addr_to_str(addr));
                    printf("with COD: 0x%06x, ", (unsigned int) gap_event_inquiry_result_get_class_of_device(event));
                    if (gap_event_inquiry_result_get_rssi_available(event)){
                        printf(", rssi %d dBm", (int8_t) gap_event_inquiry_result_get_rssi(event));
                    }
                    if (gap_event_inquiry_result_get_name_available(event)){
                        char name_buffer[240];
                        int name_len = gap_event_inquiry_result_get_name_len(event);
                        memcpy(name_buffer, gap_event_inquiry_result_get_name(event), name_len);
                        name_buffer[name_len] = 0;
                        printf(", name '%s'", name_buffer);
                    }
                    printf("\n");
                    break;
                case GAP_EVENT_INQUIRY_COMPLETE:
                    printf("Inquiry scan complete.\n");
                    break;
                case HCI_EVENT_SCO_CAN_SEND_NOW:
                    sco_send(sco_handle); 
                    break; 
                default:
                    break;
            }

            if (hci_event_packet_get_type(event) != HCI_EVENT_HFP_META) return;

            switch (hci_event_hfp_meta_get_subevent_code(event)) 
            {
                case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
                    status = hfp_subevent_service_level_connection_established_get_status(event);
                    if (status != ERROR_CODE_SUCCESS){
                        printf("Connection failed, status 0x%02x\n", status);
                        break;
                    }
                    acl_handle = hfp_subevent_service_level_connection_established_get_acl_handle(event);
                    hfp_subevent_service_level_connection_established_get_bd_addr(event, device_addr);
                    printf("Service level connection established to %s.\n", bd_addr_to_str(device_addr));
                    break;
                case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED:
                    printf("Service level connection released.\n");
                    acl_handle = HCI_CON_HANDLE_INVALID;
                    break;
                case HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED:
                    if (hfp_subevent_audio_connection_established_get_status(event) != ERROR_CODE_SUCCESS){
                        printf("Audio connection establishment failed with status 0x%02x\n", hfp_subevent_audio_connection_established_get_status(event));
                    } else {
                        sco_handle = hfp_subevent_audio_connection_established_get_sco_handle(event);
                        printf("Audio connection established with SCO handle 0x%04x.\n", sco_handle);
                        negotiated_codec = hfp_subevent_audio_connection_established_get_negotiated_codec(event);
                        switch (negotiated_codec){
                            case HFP_CODEC_CVSD:
                                printf("Using CVSD codec.\n");
                                break;
                            case HFP_CODEC_MSBC:
                                printf("Using mSBC codec.\n");
                                break;
                            case HFP_CODEC_LC3_SWB:
                                printf("Using LC3-SWB codec.\n");
                                break;
                            default:
                                printf("Using unknown codec 0x%02x.\n", negotiated_codec);
                                break;
                        }
                        sco_set_codec(negotiated_codec);
                        hci_request_sco_can_send_now_event();
                    }
                    break;
          
                case HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED:
                    printf("Audio connection released\n");
                    sco_handle = HCI_CON_HANDLE_INVALID;
                    sco_close();
                    break;

                case HFP_SUBEVENT_SPEAKER_VOLUME:
                    printf("Speaker volume: gain %u\n",
                           hfp_subevent_speaker_volume_get_gain(event));
                    break;
                case HFP_SUBEVENT_MICROPHONE_VOLUME:
                    printf("Microphone volume: gain %u\n",
                           hfp_subevent_microphone_volume_get_gain(event));
                    break;
               
                case HFP_SUBEVENT_TRANSMIT_DTMF_CODES:
                    printf("Send DTMF Codes: '%s'\n", hfp_subevent_transmit_dtmf_codes_get_dtmf(event));
                    hfp_ag_send_dtmf_code_done(acl_handle);
                    break;
               
                case HFP_SUBEVENT_VOICE_RECOGNITION_ACTIVATED:
                    status = hfp_subevent_voice_recognition_activated_get_status(event);
                    if (status != ERROR_CODE_SUCCESS){
                        printf("Voice Recognition Activate command failed\n");
                        break;
                    }

                    switch (hfp_subevent_voice_recognition_activated_get_enhanced(event)){
                        case 0:
                            printf("\nVoice recognition ACTVATED\n\n");
                            break;
                        default:
                            printf("\nEnhanced voice recognition ACTVATED\n\n");
                            break;
                    }
                    break;
                
                case HFP_SUBEVENT_VOICE_RECOGNITION_DEACTIVATED:
                    status = hfp_subevent_voice_recognition_deactivated_get_status(event);
                    if (status != ERROR_CODE_SUCCESS){
                        printf("Voice Recognition Deactivate command failed\n");
                        break;
                    }
                    printf("\nVoice Recognition DEACTIVATED\n\n");
                    break;

                case HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_HF_READY_FOR_AUDIO:
                    status = hfp_subevent_enhanced_voice_recognition_hf_ready_for_audio_get_status(event);
                    report_status(status, "Enhanced Voice recognition: READY FOR AUDIO");
                    break;

                case HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_READY_TO_ACCEPT_AUDIO_INPUT:
                    status = hfp_subevent_enhanced_voice_recognition_ag_ready_to_accept_audio_input_get_status(event);
                    report_status(status, "Enhanced Voice recognition: AG READY TO ACCEPT AUDIO INPUT");                   
                    break;

                case HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_IS_STARTING_SOUND:
                    status = hfp_subevent_enhanced_voice_recognition_ag_is_starting_sound_get_status(event);
                    report_status(status, "Enhanced Voice recognition: AG IS STARTING SOUND");          
                    break;

                case HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_IS_PROCESSING_AUDIO_INPUT:
                    status = hfp_subevent_enhanced_voice_recognition_ag_is_processing_audio_input_get_status(event);
                    report_status(status, "Enhanced Voice recognition: AG IS PROCESSING AUDIO INPUT");           
                    break;

                case HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_MESSAGE_SENT:
                    status = hfp_subevent_enhanced_voice_recognition_ag_message_sent_get_status(event);
                    report_status(status, "Enhanced Voice recognition: AG MESSAGE SENT");
                    printf("Enhanced Voice recognition: \'%s\'\n\n", msg.text);   
                    break;

                case HFP_SUBEVENT_ECHO_CANCELING_AND_NOISE_REDUCTION_DEACTIVATE:
                    status = hfp_subevent_echo_canceling_and_noise_reduction_deactivate_get_status(event);
                    report_status(status, "Echo Canceling and Noise Reduction Deactivate");
                    break;

                case HFP_SUBEVENT_HF_INDICATOR:
                    switch (hfp_subevent_hf_indicator_get_uuid(event)){
                        case HFP_HF_INDICATOR_UUID_ENHANCED_SAFETY:
                            printf("Received ENHANCED SAFETY indicator, value %d\n", hfp_subevent_hf_indicator_get_value(event));
                            break;
                        case HFP_HF_INDICATOR_UUID_BATTERY_LEVEL:
                            printf("Received BATTERY LEVEL indicator, value %d\n", hfp_subevent_hf_indicator_get_value(event));
                            break;
                        default:
                            printf("Received HF INDICATOR indicator, UUID 0x%4x, value %d\n", hfp_subevent_hf_indicator_get_uuid(event), hfp_subevent_hf_indicator_get_value(event));
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
            
        case HCI_SCO_DATA_PACKET:
            if (READ_SCO_CONNECTION_HANDLE(event) != sco_handle) break;
            sco_receive(event, event_size);
            break;
        default:
            break;
    }
}

// ГЛАВНЫЙ НАСТРОИЧНЫЙ КОД ПОДКЛЮЧЕНИЯ К HFP HF

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[])
{
    (void)argc;
    (void)argv;

    sco_init(); //инициализация служебных функций приема и передачи SCO пакетов.

    // Request role change on reconnecting headset to always use them in slave mode
    hci_set_master_slave_policy(0);
    gap_set_local_name("");
    gap_discoverable_control(0); // указываем, что наше устройство является не обнаруживаемым. 

    // L2CAP
    l2cap_init(); // init L2CAP

    uint16_t supported_features                   =
        (1<<HFP_AGSF_ESCO_S4)                     |
        (1<<HFP_AGSF_HF_INDICATORS)               |
        (1<<HFP_AGSF_CODEC_NEGOTIATION)           |
        (1<<HFP_AGSF_EXTENDED_ERROR_RESULT_CODES) |
        (1<<HFP_AGSF_ENHANCED_CALL_CONTROL)       |
        (1<<HFP_AGSF_ENHANCED_CALL_STATUS)        |
        (1<<HFP_AGSF_ABILITY_TO_REJECT_A_CALL)    |
        (1<<HFP_AGSF_IN_BAND_RING_TONE)           |
        (1<<HFP_AGSF_VOICE_RECOGNITION_FUNCTION)  |
        (1<<HFP_AGSF_ENHANCED_VOICE_RECOGNITION_STATUS) |
        (1<<HFP_AGSF_VOICE_RECOGNITION_TEXT) |
        (1<<HFP_AGSF_EC_NR_FUNCTION) |
        (1<<HFP_AGSF_THREE_WAY_CALLING);
    int wide_band_speech = 0;

    // HFP
    rfcomm_init();
    hfp_ag_init(rfcomm_channel_nr);
    hfp_ag_init_supported_features(supported_features);
    hfp_ag_init_codecs(sizeof(codecs), codecs);
    hfp_ag_init_ag_indicators(ag_indicators_nr, ag_indicators);
    hfp_ag_init_hf_indicators(hf_indicators_nr, hf_indicators); 
    hfp_ag_init_call_hold_services(call_hold_services_nr, call_hold_services);
    hfp_ag_set_subcriber_number_information(&subscriber_number, 1);

    // SDP Server
    sdp_init();
    memset(hfp_service_buffer, 0, sizeof(hfp_service_buffer));
    hfp_ag_create_sdp_record( hfp_service_buffer, 0x10001, rfcomm_channel_nr, hfp_ag_service_name, 0, supported_features, wide_band_speech);
    printf("SDP service record size: %u\n", de_get_len( hfp_service_buffer));
    sdp_register_service(hfp_service_buffer);
    
    // register for HCI events and SCO packets
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    hci_register_sco_packet_handler(&packet_handler);

    // register for HFP events
    hfp_ag_register_packet_handler(&packet_handler); 
    // функция пакет хандлер так же является управляющей SCO пакетами

    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif  
    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
