#define F_CPU 1000000
#include <mega16.h>
#include <delay.h>
#include <inttypes.h>  // uint8_t
#include <stdio.h> // printf
#include <interrupt.h>
#include <alcd.h> // Alphanumeric LCD

#define ADC_VREF_TYPE ((0<<REFS1) | (0<<REFS0) | (0<<ADLAR))

#define SIM900_TX_PORT      PORTD
#define SIM900_TX_PIN       PD1
#define SIM900_RX_PORT      PORTD
#define SIM900_RX_PIN       PD0


volatile uint8_t smsFlag = 3; // Initialize flag to 3 initially
volatile uint8_t StatusSMSTimerFlag = 0; // Initialize flag to 0 initially

// BuadRate
unsigned int BuadRate = 9600;

// void GSM_SendSMS(char* number,char* message);
void GSM_SendSMS(char* phone,char* message, uint8_t Temp, uint8_t RH);

char TEMP_str[5];
char D_TEMP_str[5];
char RH_str[5];
char D_RH_str[5];
uint8_t c=0,I_RH,D_RH,I_TEMP,D_TEMP; // I for int , D for float

// Golobal char
char Phone[] = "+989123456789"; // Replace with the recipient's phone number
char TempMessage[] = "Alert Temperature is Bad";
char RHMessage[] = "Alert Humidity is Bad";
char StatMessage[] = "Status of Sensors";

// Request to Sensor to get data
void Request(){
    DDRD |= (1<<6);
    PORTD |= (1<<6);
    delay_ms(100);
    PORTD &= ~(1<<6);
    delay_ms(100);
    PORTD |= (1<<6);
}


void Response(){
    DDRD &= ~(1<<6);
    while(PIND & (1<<6));  //if PORTD.6 = 1 Stay in loop (for check)
    while((PIND & (1<<6)) == 0); //if PORTD.6 = 0 Stay in loop (for check)
    while(PIND & (1<<6));  //if PORTD.6 = 1 Stay in loop (for check)
}


// Start ADC conversion
unsigned int read_adc(unsigned char adc_input)
{
// | == OR
ADMUX=adc_input | ADC_VREF_TYPE;
delay_us(10);
ADCSRA|=(1<<ADSC);
// Wait for conversion to complete
while ((ADCSRA & (1<<ADIF))==0);
ADCSRA|=(1<<ADIF);
// ADCW is a 16-bit register combining ADCL and ADCH
return ADCW;
}


uint8_t Receive(){
    int i;
    for (i=0; i<8; i++){
        while((PIND & (1<<6)) == 0);
        delay_us(30);
        if (PIND & (1<<6)){
            c = (c<<1)|(0x01);    
        }
        else {
            c = (c<<1);
        }
        while (PIND & (1<<6));
    }
    return c;
}

// SMS Sender
void GSM_SendSMS(char* phone, char* message, uint8_t Temp, uint8_t RH) {
    
    lcd_clear();
    lcd_gotoxy(4, 0);
    lcd_putsf("Set GSM");
   // Send AT command to set SMS mode
    printf("AT+CMGF=1\r\n");
    delay_ms(500);

    // Send AT command to set recipient phone number
    lcd_clear();
    lcd_gotoxy(0, 0);
    lcd_putsf("Phone: ");
    lcd_gotoxy(0,1);
    lcd_puts(phone);
    printf("AT+CMGS=\"%s\"\r\n", phone);
    delay_ms(1000);
    
    // Send the SMS message
    lcd_clear();
    lcd_gotoxy(4, 0);
    lcd_putsf("Send SMS");
    printf("%s", message);
    delay_ms(500);
    
    // Send Temp
    lcd_clear();
    lcd_gotoxy(4, 0);
    lcd_putsf("Send Temp");
    printf("\nTemp is %" PRIu8 "\n", Temp);
    delay_ms(500);
    
    // Humidity
    lcd_clear();
    lcd_gotoxy(4, 0);
    lcd_putsf("Send RH");
    printf("\nHumidity is %" PRIu8 "\n", RH);
    delay_ms(500);
    
    // Send Ctrl+Z to indicate the end of the message
    lcd_clear();
    lcd_gotoxy(4, 0);
    lcd_putsf("DONE");
    printf("%c", 0x1A);
    delay_ms(500);
    
    lcd_gotoxy(3,1);
    lcd_putsf("Exit GSM");
    delay_ms(500);
    lcd_clear();
    smsFlag = 0; // Reset the flag after sending SMS 
}


// USART Initialize
void USART_Init(unsigned int baud) {
    // Set baud rate
    UBRRH = (unsigned char)(baud >> 8);
    UBRRL = (unsigned char)baud;

    // Clear the USART Control and Status registers
    UCSRA = 0;
    UCSRB = 0;
    UCSRC = 0;

    // Enable transmitter
    UCSRB |= (1 << TXEN);
    
    // Set frame format: 8 data bits, 1 stop bit, no parity
    UCSRC |= (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}

// Timer1 Initialize
void timer1_init() {
    // Set Timer1 in normal mode with prescaler 1024
    TCCR1A = 0;
    TCCR1B = (1 << CS12) | (1 << CS10);
    // Enable Timer1 overflow interrupt
    TIMSK |= (1 << TOIE1);
    // Set Timer1 value for a 3-minute interval
    TCNT1 = 65535 - (10 * 60 * (F_CPU / 1024)); // Adjust based on your MCU frequency
}

// Interrupt for Enable GSM to Send SMS
interrupt [TIM1_OVF] void timer1_overflow_isr(void) {
    StatusSMSTimerFlag = StatusSMSTimerFlag + 1; // Set flag when timer overflows
    smsFlag = smsFlag + 1;
    TCNT1 = 65535 - (1 * 60 * (F_CPU / 1024)); // Reset Timer1 value for another 1-minute interval
}


void TEMP_LCD(int I, int D)
{
    lcd_clear();
    lcd_gotoxy(0,0);
    lcd_putsf("Temp is Bad");
    lcd_gotoxy(0,1);
    lcd_putsf("Temp: ");
    lcd_gotoxy(6,1);
    sprintf(TEMP_str, "%d", I);
    lcd_puts(TEMP_str);
    lcd_gotoxy(8,1);
    lcd_putsf(".");
    lcd_gotoxy(9,1);
    sprintf(D_TEMP_str, "%d", D);
    lcd_puts(D_TEMP_str);
    lcd_gotoxy(10,1);
    lcd_putsf("C");
}

void RH_LCD(int I, int D)
{
    lcd_clear();
    lcd_gotoxy(0,0);
    lcd_putsf("Humidity is Bad");
    lcd_gotoxy(0,1);
    lcd_putsf("RH: ");
    lcd_gotoxy(3,1);
    sprintf(RH_str, "%d", I);
    lcd_puts(RH_str);
    lcd_gotoxy(5,1);
    lcd_putsf(".");
    lcd_gotoxy(6,1);
    sprintf(D_RH_str, "%d", D);
    lcd_puts(D_RH_str);
    lcd_gotoxy(7,1);
    lcd_putsf("%");
}

void STAT_LCD(int I_T, int D_T, int I_R, int D_R)
{
    // TEMP
    lcd_clear();
    lcd_gotoxy(0,0);
    lcd_putsf("Temp: ");
    lcd_gotoxy(6,0);
    sprintf(TEMP_str, "%d", I_T);
    lcd_puts(TEMP_str);
    lcd_gotoxy(8,0);
    lcd_putsf(".");
    lcd_gotoxy(9,0);
    sprintf(D_TEMP_str, "%d", D_T);
    lcd_puts(D_TEMP_str);
    lcd_gotoxy(10,0);
    lcd_putsf("C");
    // Humidity
    lcd_gotoxy(0,1);
    lcd_putsf("RH: ");
    lcd_gotoxy(3,1);
    sprintf(RH_str, "%d", I_R);
    lcd_puts(RH_str);
    lcd_gotoxy(5,1);
    lcd_putsf(".");
    lcd_gotoxy(6,1);
    sprintf(D_RH_str, "%d", D_R);
    lcd_puts(D_RH_str);
    lcd_gotoxy(7,1);
    lcd_putsf("%");
}



void main(void)
{

// for ADC
unsigned int A=0;
// PinD.5 is OUT
DDRD |= (1<<5);
// Relay is OFF
PORTD &= ~(1<<5);

// LED
DDRA = (1<<1) | (1<<2) | (1<<3);

// Set ADMUX
ADMUX=ADC_VREF_TYPE;
// Set ADCSRA --> ADC Enabled , Division Factor = 2
ADCSRA=(1<<ADEN) | (0<<ADSC) | (0<<ADATE) | (0<<ADIF) | (0<<ADIE) | (0<<ADPS2) | (0<<ADPS1) | (1<<ADPS0);
// SFIOR = Special Function IO Register
// ADTS = ADC Trigger Select
SFIOR=(0<<ADTS2) | (0<<ADTS1) | (0<<ADTS0);

// Set USART
UCSRA=(0<<RXC) | (0<<TXC) | (0<<UDRE) | (0<<FE) | (0<<DOR) | (0<<UPE) | (0<<U2X) | (0<<MPCM);
UCSRB=(0<<RXCIE) | (0<<TXCIE) | (0<<UDRIE) | (0<<RXEN) | (1<<TXEN) | (0<<UCSZ2) | (0<<RXB8) | (0<<TXB8);
// UCSZ1 & UCSZ0 == we use 8-bit
UCSRC=(1<<URSEL) | (0<<UMSEL) | (0<<UPM1) | (0<<UPM0) | (0<<USBS) | (1<<UCSZ1) | (1<<UCSZ0) | (0<<UCPOL);
UBRRH=0x00;
UBRRL=0x06;


USART_Init(BuadRate);
timer1_init();
sei();
StatusSMSTimerFlag = 0;
// for 2x16 LCD
lcd_init(16);

// Welcome
lcd_clear();
lcd_gotoxy(0, 0);
lcd_putsf("Welcome");
delay_ms(3000);
lcd_clear();

while (1)
      {   
          Request();
          Response();
          I_RH = Receive(); // int for humidity
          D_RH = Receive(); // float for humidity
          I_TEMP = Receive(); // int for temperature
          D_TEMP = Receive(); // float for temperature
          STAT_LCD(I_TEMP, D_TEMP, I_RH, D_RH);
          
          // Temperature SMS Sender
          if ( (I_TEMP > 40 || I_TEMP < 26) && smsFlag == 3){
            PORTA |= (1<<1);
            delay_ms(500);
            GSM_SendSMS(Phone, TempMessage, I_TEMP, I_RH);
            delay_ms(50);
          }
          else{
            PORTA &= ~(1<<1);
          }
          
          // Temperature LED
          if ( I_TEMP > 40 || I_TEMP < 26){
            PORTA |= (1<<1);
            TEMP_LCD(I_TEMP, D_TEMP);
            delay_ms(700);
            PORTA &= ~(1<<1);
            lcd_clear();
            delay_ms(10);
          }
          else{
            PORTA &= ~(1<<1);
          }
          
          // Humidity SMS Sender
          if ( (I_RH > 70 || I_RH < 40) && smsFlag == 3){
            PORTA |= (1<<2);
            delay_ms(500);
            GSM_SendSMS(Phone, RHMessage, I_TEMP, I_RH);
            delay_ms(50);
          }
          else{
            PORTA &= ~(1<<2);
          }
          
          // Humidity LED
          if (I_RH > 70 || I_RH < 40){
            PORTA |= (1<<2);
            RH_LCD(I_RH, D_RH);
            delay_ms(700);
            PORTA &= ~(1<<2);
            delay_ms(70);
            lcd_clear();
          }
          else{
            PORTA &= ~(1<<2);
          }  
          
          // Every 10 min send Status SMS
          if (StatusSMSTimerFlag == 10) {
            lcd_clear();
            lcd_gotoxy(0,0);
            lcd_putsf("Send Status SMS");
            delay_ms(500);
            PORTA |= (1<<3);
            STAT_LCD(I_TEMP, D_TEMP, I_RH, D_RH);
            delay_ms(2000);
            GSM_SendSMS(Phone, StatMessage, I_TEMP, I_RH);
            StatusSMSTimerFlag = 0; // Reset The Status flag
            lcd_clear();
          }
          else{
            PORTA &= ~(1<<3);
          }
          
          // read analoge
          A=read_adc(0);
          if(A>500) PORTD.5=0;   
          if(A<500) PORTD.5=1;
           
          delay_ms(500);
      } 
}
