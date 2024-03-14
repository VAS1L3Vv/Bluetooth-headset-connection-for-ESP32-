#define BTSTACK_FILE__ "hfp_ag.c"
#include "hfp_ag_core.h"

static bool connection_mode;
static int cycle_number = 0;
static int data_bytes = 0;
static bool audio_streaming = 0;

static void set_bt_mode(bool mode) {
    connection_mode = mode;
}

static uint8_t record_bt_microphone(hci_con_handle_t acl_handle)
{
    if(audio_streaming){
        printf("Audio stream already in progress. \nWait for it to end.\n ");
        return (uint8_t)0x00;}
    data_bytes = 0;
    set_bt_mode(RECORDING);
    uint8_t stat = hfp_ag_establish_audio_connection(acl_handle);
    return stat;
}

static uint8_t playback_bt_speaker(hci_con_handle_t acl_handle)
{
    if(audio_streaming){
        printf("Audio stream already in progress. \nWait for it to end.\n ");
        return (uint8_t)0x00;}
    if(cycle_number == 0) {
        printf("Nothing recorded yet. Press 4 to record voice audio. \n");
        return NULL; }
    data_bytes = 0;
    set_bt_mode(PLAYBACK);
    uint8_t stat = hfp_ag_establish_audio_connection(acl_handle);
    return stat;
}

static void toggle_codec2() { // вкл-выкл обработки буффера с кодек2
    if(codec2_enabled()) {
        set_codec2_state(OFF); // выключает кодек2 если прежде был включен
        printf("Codec2 processing has been disabled \n ");
        return;
    }
    else {
        set_codec2_state(ON); // включает кодек2 если прежде был выключен
        printf("Codec2 processing has been enabled \n ");
        return; }
}

static bool current_bt_mode(){
    return connection_mode;
}

static void show_usage(void){ 
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth HFP Audiogateway (AG) unit Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("\n");
    printf("1 - scan nearby HF units\n");
    printf("2 - establish HFP connection %s\n", bd_addr_to_str(device_addr));
    printf("3 - release HFP connection\n");
    printf("4 - record headset microphone for 10 seconds\n");
    printf("5 - playback recorded audio\n");
    printf("6 - toggle codec2 processing\n");
    printf("7 - abort audio.\n");
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
        case '9':
            log_info("USER:\'%c\'", cmd);
            printf("Establish Audio connection %s...\n", bd_addr_to_str(device_addr));
            status = hfp_ag_establish_audio_connection(acl_handle);
            break;
        case '4':
            log_info("USER:\'%c\'", cmd);
            status = record_bt_microphone(acl_handle);
            break;
        case '5':
            log_info("USER:\'%c\'", cmd);
            status = playback_bt_speaker(acl_handle);
            break;
        case '6':
            log_info("USER:\'%c\'", cmd);
            toggle_codec2();
            break;
        case '7':
            log_info("USER:\'%c\'", cmd);
            printf("Abort Audio.\n");
            status = hfp_ag_release_audio_connection(acl_handle);
            break;
        case 'a':
            log_info("USER:\'%c\'", cmd);
            printf("Report AG failure\n");
            status = hfp_ag_report_extended_audio_gateway_error_result_code(acl_handle, HFP_CME_ERROR_AG_FAILURE);
            break;
        case 'b':
            printf("Deleting all link keys\n");
            gap_delete_all_link_keys();
            break;
        case 'c':
            log_info("USER:\'%c\'", cmd);
            printf("Set signal strength to 0\n");
            status = hfp_ag_set_signal_strength(0);
            break;
        case 'C':
            log_info("USER:\'%c\'", cmd);
            printf("Set signal strength to 5\n");
            status = hfp_ag_set_signal_strength(5);
            break;
        case 'd':
            log_info("USER:\'%c\'", cmd);
            printf("Set battery level to 3\n");
            status = hfp_ag_set_battery_level(3);
            break;
        case 'D':
            log_info("USER:\'%c\'", cmd);
            printf("Set battery level to 5\n");
            status = hfp_ag_set_battery_level(5);
            break;
        case 'e':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 0 (minimum)\n");
            status = hfp_ag_set_speaker_gain(acl_handle, 0);
            break;
        case 'E':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 9 (default)\n");
            status = hfp_ag_set_speaker_gain(acl_handle, 9);
            break;
        case 'f':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 12 (higher)\n");
            status = hfp_ag_set_speaker_gain(acl_handle, 12);
            break;
        case 'F':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 15 (maximum)\n");
            status = hfp_ag_set_speaker_gain(acl_handle, 15);
            break;
        case 'g':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 0\n");
            status = hfp_ag_set_microphone_gain(acl_handle, 0);
            break;
        case 'G':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 9\n");
            status = hfp_ag_set_microphone_gain(acl_handle, 9);
            break;
        case 'h':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 12\n");
            status = hfp_ag_set_microphone_gain(acl_handle, 12);
            break;
        case 'H':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 15\n");
            status = hfp_ag_set_microphone_gain(acl_handle, 15);
            break;
        case 'T':
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

static void report_status(uint8_t status, const char * message){
    if (status != ERROR_CODE_SUCCESS){
        printf("%s command failed, status 0x%02x\n", message, status);
    } else {
        printf("%s command successful\n", message);
    }
}

void sco_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * event, uint16_t event_size)
{
        if(data_bytes >= SECONDS_TO_BYTES) {
        hfp_ag_release_audio_connection(acl_handle);
        printf("\n\n microphone playback completed.  \n Press 4 to listen to mic again.");
    }
    UNUSED(channel);
    switch(packet_type)
    {
        case HCI_EVENT_PACKET:
            if(connection_mode == PLAYBACK) {
            switch(hci_event_packet_get_type(event))
            {
                case HCI_EVENT_SCO_CAN_SEND_NOW:
                    sco_send(sco_handle);
                    data_bytes += SCO_PACKET_SIZE;
                    break;
                default:
                    break;
            }
        }

        case HCI_SCO_DATA_PACKET:
            if(connection_mode == RECORDING) {
            if (READ_SCO_CONNECTION_HANDLE(event) != sco_handle) break;
            sco_receive(event, event_size); 
            data_bytes += SCO_PACKET_SIZE;
            }
            break;
        default:
            break;
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
                default:
                    break;
            }

            if (hci_event_packet_get_type(event) != HCI_EVENT_HFP_META) return;

            switch (hci_event_hfp_meta_get_subevent_code(event)) 
            {
                case HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED:
                    if (hfp_subevent_audio_connection_established_get_status(event) != ERROR_CODE_SUCCESS){
                        printf("Audio connection establishment failed with status 0x%02x\n", hfp_subevent_audio_connection_established_get_status(event));
                    } else {
                        sco_handle = hfp_subevent_audio_connection_established_get_sco_handle(event);
                        printf("Audio connection established with SCO handle 0x%04x.\n", sco_handle);
                        negotiated_codec = hfp_subevent_audio_connection_established_get_negotiated_codec(event);

    sco_set_codec(negotiated_codec, connection_mode);
    if(connection_mode == PLAYBACK) {
        hci_request_sco_can_send_now_event();}
    audio_streaming = 1;
                    }
                    break;
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

                case HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED:
                    printf("Audio connection released\n");
                    sco_handle = HCI_CON_HANDLE_INVALID;
                    sco_close();
                    data_bytes = 0;
                    audio_streaming = 0;
                    cycle_number++;
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

    // SDP Server
    sdp_init();
    memset(hfp_service_buffer, 0, sizeof(hfp_service_buffer));
    hfp_ag_create_sdp_record( hfp_service_buffer, 0x10001, rfcomm_channel_nr, hfp_ag_service_name, 0, supported_features, wide_band_speech);
    printf("SDP service record size: %u\n", de_get_len( hfp_service_buffer));
    sdp_register_service(hfp_service_buffer);
    
    // register for HCI events and SCO packets
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    hci_register_sco_packet_handler(&sco_packet_handler);
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
