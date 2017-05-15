#include <project.h>
#define HIGH 1u
#define LOW 0u
#define FREQ_SCALE 4294967295/125000000
#include "stdio.h"
#define RX_LEN 20
#define ERR_MSG ": invalid input_str. Type 'help' to see available input_strs\r\n"
#define CMD_LEN 4
#define OPTION_LEN 4
#define NUM_LEN 15

const char help_message[] = 
    "\r\nHi! This is a PC controlled Antenna Analyzer.\r\n"
    "input_strs:\r\n"      
    "  'help' to see this help message.\r\n"
    "  'ant' to control the Antenna Analyzer.\r\n"
    ;

double dds_freq = (double) 14150e3;
uint amux_chan = 1u;

void dds_transfer_byte(uint8 data);
void dds_set_frequency(double frequency);
void dds_reset();
void extract_first_word(char * input_str, char * first_word, const uint word_len);
void send_help_msg();
void send_crlf();
void send_error_msg(char * input_str);
void send_uint(uint value);
void execute_input_str(char * input_str);


int main()
{
    ADC_Start();
    ADC_StartConvert();
    
    LED_Write(1u);
    
    // Enable serial mode on AD9850 DDS module
    Pin_DDS_Freq_Update_Write(HIGH);
    Pin_DDS_Freq_Update_Write(LOW);
    
    dds_set_frequency(dds_freq);
	LED_Write(0u);
    UART_Start();
    
    uint32 rx_data;
    char rx_str[RX_LEN];
    int rx_str_idx = 0;
    
    UART_SpiUartPutArray(help_message, strlen(help_message));
    UART_SpiUartWriteTxData('>');
    
    for(;;)
    {
        if (UART_SpiUartGetRxBufferSize()!=0){
            while(UART_SpiUartGetRxBufferSize() != 0){
                rx_data = UART_SpiUartReadRxData();
                if(rx_data=='\n'){
                    if(rx_str_idx > 1){
                        /* rx_str is more than just "\r" */
                        rx_str[rx_str_idx-1] = '\0';  // set trailing "\r" to null
                        execute_input_str(rx_str);
                    }
                    rx_str_idx = 0;
                    memset(rx_str, '\0', RX_LEN);
                    UART_SpiUartWriteTxData('>');
                }
                else{
                    rx_str[rx_str_idx]=rx_data;
                    rx_str_idx++;
                }
            }
        }
    }
}

void dds_transfer_byte(uint8 data) {
    // Transfer 1 byte of data to the AD9850 DDS module using the Data pin
    int i;
    for (i=0; i<8; i++) {
        Pin_DDS_Data_Write(data&0x01);
        // Toggle Word Clock pin to indicate that data bit is ready
        Pin_DDS_Word_Clock_Write(HIGH);
        Pin_DDS_Word_Clock_Write(LOW);
        data >>= 1;
    }
}
 
void dds_set_frequency(double frequency) {
    // Set desired frequency at the AD9850 DDS module
    // frequency calc from datasheet page 8 = <sys clock> * <frequency tuning word>/2^32
    // Based on Arduino sketch by Andrew Smallbone at www.rocketnumbernine.com
    int32 freq = frequency * FREQ_SCALE;  // note 125 MHz clock on 9850
    int b;
    for (b=0; b<4; b++) {
        dds_transfer_byte(freq & 0xFF);
        freq >>= 8;
    }
    // Final control byte, all 0 for 9850 chip
    dds_transfer_byte(0x00);   
    // Toggle Frequency Update pin to indicate that the data transfer is done
    Pin_DDS_Freq_Update_Write(HIGH);
    Pin_DDS_Freq_Update_Write(LOW);
}

void dds_reset(){
    Pin_DDS_Reset_Write(HIGH);
    Pin_DDS_Reset_Write(LOW);
}


void extract_first_word(char * input_str, char * first_word, const uint word_len){
    unsigned int i;
    for(i=0; i<strlen(input_str) && i<word_len; i++){
        if(input_str[i]==' ') break;
        else first_word[i] = input_str[i];
    }
}

void send_help_msg(){
    UART_SpiUartPutArray(help_message, strlen(help_message));
}

void send_crlf(){
    UART_SpiUartPutArray("\r\n", 2);
}

void send_error_msg(char * input_str){
    UART_SpiUartPutArray(input_str, strlen(input_str));
    UART_SpiUartPutArray(ERR_MSG, strlen(ERR_MSG));   
}

void send_uint(uint value){
    char value_str[NUM_LEN];
    sprintf(value_str, "%d", value);
    UART_SpiUartPutArray(value_str, strlen(value_str));
    send_crlf();
}

void execute_input_str(char * input_str){
    char cmd[CMD_LEN] = "";
    extract_first_word(input_str, cmd, CMD_LEN);

    /* help */
    if(strcmp(input_str, "help")==0) send_help_msg();

    /* dds */
    else if(strcmp(cmd, "ant")==0){
        const char cmd_format[] =  
        "ant format: dds <option> [property] [number]\r\n"
        "option:\r\n"
        "  set\r\n"
        "  get\r\n"
        "property:\r\n"
        "  f: frequency from AD850 DDS\r\n"
        "  s: switch resistor bridge input\r\n"
        "    1: ref\r\n"
        "    2: antenna\r\n"
        "  m: VMAG reading from AD8302 [get only]\r\n"
        "    multiply with (1.8*2/4096) to convert to Volts\r\n"
        "  p: VPHS reading from AD8302 [get only]\r\n"
        "    multiply with (1.8*2/4096) to convert to Volts\r\n"
        ;
        char option[OPTION_LEN] = "";
        extract_first_word(input_str+4, option, OPTION_LEN);
        char property = input_str[8];
        char number_str[NUM_LEN] = "";
        extract_first_word(input_str+10, number_str, NUM_LEN);
        uint number = 0;
        sscanf(number_str, "%u", &number);
        
        if(strcmp(option, "get")==0){
            if(property=='f') send_uint(AMuxControl_Read());
            else if(property=='s') send_uint(amux_chan);
            else if(property=='m') send_uint((uint)ADC_GetResult16(0u));
            else if(property=='p') send_uint((uint)ADC_GetResult16(1u));
            else UART_SpiUartPutArray(cmd_format, strlen(cmd_format));
        }
        else if(strcmp(option, "set")==0){
            if(property=='f'){
                dds_freq = number;
                dds_set_frequency(dds_freq);
                send_uint(dds_freq);
            }
            else if(property=='s'){
                AMuxControl_Write(number);
                send_uint(AMuxControl_Read());
            }
            else UART_SpiUartPutArray(cmd_format, strlen(cmd_format));
        }
        else UART_SpiUartPutArray(cmd_format, strlen(cmd_format));
    }
    /* no match */
    else send_error_msg(input_str);
}
