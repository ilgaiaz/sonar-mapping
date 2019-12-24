#define F_CPU 8000000UL /* Define CPU Frequency 8MHz */
#include <avr/io.h> /* Include AVR std. library file */
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>

#define SONAR_Dir DDRB
#define SONAR_Trigger PINB6
#define SONAR_Port PORTB
#define LED_Pin PINB1

#define MOTOR_Dir DDRC
#define MOTOR_Port PORTC

#define USART_Dir DDRD

#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

/* Flags for count motor interrupts and sonar interrupts */
volatile unsigned int interrupt_counter=0, degree_multiplier=0;
volatile int activate_sonar = 0, last_step = 0;
/* Flag for coil activation */
volatile unsigned char i=0;
/* Flag for the LED */
volatile unsigned char t_flag=0;
/* Variable for SONAR. Count timer overflow */
volatile int TimerOverflow = 0;

/* --------------------------------------- SONAR ---------------------------------------- */
void SONAR_Init()
{
    SONAR_Port |= (1 << SONAR_Trigger) | (1 << LED_Pin);	/* Make trigger pin as output and activate LED pin*/
    TIMSK1 = (1 << TOIE1);	                                /* Enable Timer1 overflow interrupts */
	TCCR1A = 0;				                                /* Set all bit to zero Normal operation */
}

ISR(TIMER1_OVF_vect)
{
	TimerOverflow++;	/* Increment Timer Overflow count */
}

/* --------------------------------------- MOTOR ---------------------------------------- */
void MOTOR_Init()
{
    /* ------------------------- TIMER0 ----------------------- */
    /* Init PC0/1/2/3/4 */
    MOTOR_Dir = 0x0F;
    
    TCCR0A = (1<<WGM01); //modo CTC
    TCCR0B = 0;//prescaler 256
    
    OCR0A=155;//5ms=>155; 1ms=>30  
    OCR0B=155;//5ms
	
    TIMSK0 |= (1<<OCIE0A); // enable Timer interrupt on compare
}

/* --------------------------------------- USART ---------------------------------------- */

void USART_Init()
{
	UCSR0B |= (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);  /* Turn on transmission and reception and enable receve interrupt */
	UCSR0C |= (1 << UCSZ01) | (1 << UCSZ00);                /* Use 8-bit character sizes */
	UBRR0L = BAUD_PRESCALE;		                            /* Load lower 8-bits of the baud rate value */
	UBRR0H = (BAUD_PRESCALE >> 8);	                        /* Load upper 8-bits */
}

unsigned char USART_RxChar()
{
	while ((UCSR0A & (1 << RXC0)) == 0);    /* Wait till data is received */
	return(UDR0);			                /* Return the byte */
}

void USART_TxChar(char ch)
{
	while (! (UCSR0A & (1 << UDRE0)));	    /* Wait for empty transmit buffer */
	UDR0 = ch ;
}

void USART_SendString(char *str)
{
	unsigned char j=0;
	
	while (str[j]!=0)		                /* Send string till null */
	{
		USART_TxChar(str[j]);	
		j++;
	}
}

/* ------------------------------------- USART Interrupt recever ----------------------- */

ISR(USART_RX_vect)
{
    /* When it receve something check save the char in Data_in */
    char Data_in;
	Data_in = UDR0;
    
    if(Data_in =='A')
    {
        /* If receve the activate command enable the interrupt */
        interrupt_counter = 0;
        last_step = 0;
        TCCR0B= (1 << CS02);
    }
}

/* --------------------------------------- USART sender -------------------------------- */
void Send_Data(double degree, double distance)
{
    char dst_string[10]="", degr_string[10]="", sonarData[30]="";
    /* Convert value from float to string */
    dtostrf(degree, 2, 2, degr_string);
    dtostrf(distance, 2, 2, dst_string);
    /* Concatenate the value and create the formatted string */
    strcat(sonarData, degr_string);
    strcat(sonarData, ";");
    strcat(sonarData, dst_string);
    strcat(sonarData, "\n");
    /* Send the final string via USART */
    USART_SendString(sonarData);

    memset(degr_string,0,sizeof(degr_string));
    memset(dst_string,0,sizeof(dst_string));
    memset(sonarData,0,sizeof(sonarData));
}

/* ------------------------------------- SONAR measurement -------------------------------------- */
double Distance_Measurement()
{
    long count = 0;
    double distance = 0;

    SONAR_Port |= (1 << SONAR_Trigger);
    _delay_us(10);
    SONAR_Port &= (~(1 << SONAR_Trigger));
    
    TCNT1 = 0;				/* Clear Timer counter */
    TCCR1B = 0x41;			/* Capture on rising edge, No prescaler*/
    TIFR1 = (1 << ICF1);	/* Clear ICP flag (Input Capture flag) */
    TIFR1 = (1 << TOV1);	/* Clear Timer Overflow flag */
    
    /*Calculate width of Echo by Input Capture (ICP) */
    while ((TIFR1 & (1 << ICF1)) == 0);	/* Wait for rising edge */
    TCNT1 = 0;							/* Clear Timer counter. From this point the distance is misured */
    TCCR1B = 0x01;						/* Capture on falling edge, No prescaler */
    TIFR1 = (1 << ICF1);				/* Clear ICP flag (Input Capture flag) */
    TIFR1 = (1 << TOV1);				/* Clear Timer Overflow flag */
    TimerOverflow = 0;					/* Clear Timer overflow count */

    /* ICP (Input Capture Pin) */ 
    /* When a change occurs on the ICP the value of TCNT1 is written to the ICR1 */
    while ((TIFR1 & (1 << ICF1)) == 0);		/* Wait for falling edge */
    if(TimerOverflow > 3)
        count = (long) 300*464;
    else
        count = ICR1 + (65535 * TimerOverflow);	/* Take count */
    
    /*
    Timer freq = 8MHz, sound speed = 343 m/s = 34300 cm/s  
    Distance = (SoundSpeed * TIMER)/2 [cm]
             = 17150 * TIMER 
             = TIMERval / 464
    */

    distance = (double)count / 464;    

    return distance;
}



/* ------------- Interrupt for anticlockwise rotation and measurements ------------ */

ISR(TIMER0_COMPA_vect)
{
    switch(i)
    {
        case 0: {PORTC = 0x01;break;}
        case 1: {PORTC = 0x03;break;}
        case 2: {PORTC = 0x02;break;}
        case 3: {PORTC = 0x06;break;}
        case 4: {PORTC = 0x04;break;}
        case 5: {PORTC = 0x0C;break;}
        case 6: {PORTC = 0x08;break;}
        case 7: {PORTC = 0x09;break;}
    }

     /* Every 5,625 degree activate Sonar */
    if ( interrupt_counter%64 == 0 )
    {
        activate_sonar = 1;
    }
	

    t_flag++;
	if (t_flag == 100)
	{
		PORTB ^= (1 << PORTB1);  // toggles the led
		t_flag = 0;
	}

    interrupt_counter++;
    if(i==7) i=0;
    else i++;

    if(interrupt_counter == 4097)
    {
        PORTC = 0x01;
        TIMSK0 = (0 << OCIE0A) | (1 << OCIE0B);
        degree_multiplier = 0;
        USART_SendString("E\n");
    }
}

/* ------------- Interrupt for clockwise rotation positioning at 0 degree ------------ */

ISR(TIMER0_COMPB_vect)
{	
    switch(i)
    {
        case 0: {PORTC = 0x09;break;}
        case 1: {PORTC = 0x08;break;}
        case 2: {PORTC = 0x0C;break;}
        case 3: {PORTC = 0x04;break;}
        case 4: {PORTC = 0x06;break;}
        case 5: {PORTC = 0x02;break;}
        case 6: {PORTC = 0x03;break;}
        case 7: {PORTC = 0x01;break;}
    }
	
    /* -------- LED TOGGLE ------- */
    t_flag++;
	if (t_flag == 100)
	{
		PORTB ^= (1 << PORTB1);  // toggles the led
		t_flag = 0;
	}
    /* -------- LED TOGGLE ------- */

    interrupt_counter--; 
    if(i==7) i=0;
    else i++;

    if(interrupt_counter == 0)
	{
		PORTC = 0x09;
        TIMSK0 = (1 << OCIE0A) | (0 << OCIE0B);
        /* Disable interrupt when the acquisition ended */
        last_step = 1;
	}
}

void init()//initialization
{
    /* -------------------------- MOTOR ----------------------- */
    MOTOR_Init();

    /* -------------------------- USART ----------------------- */
	USART_Init();	

    /* -------------------------- SONAR ----------------------- */
    SONAR_Init();

    sei();
}

int main(void)
{
    char dst_string[10], degr_string[10], sonarData[30];
    double distance = 0, degree = 0;
    init();
    while (1)
    {
        if(activate_sonar == 1 && last_step == 0)
        {   
            TCCR0B= 0;

            degree = (5.625 * degree_multiplier);
            distance = Distance_Measurement();

            Send_Data(degree, distance);

            degree_multiplier++;
            activate_sonar = 0;
            TCCR0B= (1 << CS02);
        } else if (last_step == 1) 
        {
            TCCR0B = 0;
        }
    }   
}