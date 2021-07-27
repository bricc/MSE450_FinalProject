/*******************************************************************************
 * FILENAME :       GuitarInvaders.c, Release 2.0 
 *
 * DESCRIPTION :
 *      This is the main file for the project for MSE 450
 *
 * PUBLIC FUNCTIONS :
 *      
 *      
 * 
 * NOTES : 
 *      n/a
 * 
 * AUTHORS :
 *      Written by Ricardo Bravo <rebravo@sfu.ca>
 *      Written by Joshua Fritsch
 *      Written by Aidan Hunter
 *
 *******************************************************************************/ 
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "Nokia5110.h"
#include "Random.h"
#include "ADCSWTrigger.h"
#include "PLL.h"
#include "TExaS.h"
#include "GI_BitMaps.h"
#include "UART.h"

/* MACROS USED FOR BITMAPPING */ 
#define CIRCLEW    ((unsigned char)circleEmpty[18])
#define CIRCLEH    ((unsigned char)circleEmpty[22])
#define INDICATORW ((unsigned char)indicator[18])
#define INDICATORH ((unsigned char)indicator[22])
	
/* DEVICE RELATED REGISTERS */ 
#define GPIO_PORTF_DATA_R       (*((volatile unsigned long *)0x400253FC))
#define GPIO_PORTF_DIR_R        (*((volatile unsigned long *)0x40025400))
#define GPIO_PORTF_AFSEL_R      (*((volatile unsigned long *)0x40025420))
#define GPIO_PORTF_PUR_R        (*((volatile unsigned long *)0x40025510))
#define GPIO_PORTF_DEN_R        (*((volatile unsigned long *)0x4002551C))
#define GPIO_PORTF_LOCK_R       (*((volatile unsigned long *)0x40025520))
#define GPIO_PORTF_CR_R         (*((volatile unsigned long *)0x40025524))
#define GPIO_PORTF_AMSEL_R      (*((volatile unsigned long *)0x40025528))
#define GPIO_PORTF_PCTL_R       (*((volatile unsigned long *)0x4002552C))
#define SYSCTL_RCGC2_R          (*((volatile unsigned long *)0x400FE108))
#define GPIO_PORTF_IS_R         (*((volatile unsigned long *)0x40025404))
#define GPIO_PORTF_IBE_R        (*((volatile unsigned long *)0x40025408))
#define GPIO_PORTF_IEV_R        (*((volatile unsigned long *)0x4002540C))
#define GPIO_PORTF_IM_R         (*((volatile unsigned long *)0x40025410))
#define GPIO_PORTF_ICR_R        (*((volatile unsigned long *)0x4002541C))
#define NVIC_EN0_R              (*((volatile unsigned long *)0xE000E100))  // IRQ 0 to 31 Set Enable Register
#define NVIC_PRI7_R             (*((volatile unsigned long *)0xE000E41C))  // IRQ 28 to 31 Priority Register


/*******************************************************************************
* Function Declarations
*******************************************************************************/

/* INTERRUPT RELATED FUNCTIONS */ 
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts

/* TIMER RELATED FUNCTIONS */
long StartCritical (void);    // previous I bit, disable interrupts
void Timer2_Init(unsigned long period);
void Delay100ms(unsigned long count); // time delay in 0.1 seconds
void SysTick_Init(unsigned long milliseconds);


/* MISC FUNCTIONS */
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
void PortF_Init(void);
void EdgeCounter_Init(void);


/* GAME SPECIFIC FUNCTIONS */
void Game_Menu(long volatile delay); // TITLE MENU (R1.0)
void TitleScreen(long volatile delay);
void SongList(long volatile delay);
void PortF_Init_2(void);
void Delay100ms(unsigned long count);

/* INITIALIZING GAME COMPONENTS */
void GameInit(void);
void HUDBuffer(void);
void songInit(int song[], int songDuration);
void Move(int songDuration);
void noteBuffer(int songDuration);

/* PERIPHERAL RELATED FUNCTIONS */ 
void ResetLEDs(void);
/*******************************************************************************
* Structs 
- STATE: For the state of the notes 
*******************************************************************************/

/* NOTE STRUCT */ 
struct State {
  signed long x;      // x coordinate
  signed long y;      // y coordinate
  const unsigned char *image; // ptr->image
  int visible;            // 0=dead, 1=alive
	int strikeable;         // If location is ready to be hit
	int songLevel;
}; 

/* PLAYER  STRUCT */
typedef struct Player {
	unsigned char x; 
	unsigned char y;
	unsigned char currLives; 
	unsigned char pt; 
	unsigned char width;
	unsigned char height;
	
	// need? 
  const unsigned char *image; // ptr->image
  int visible;            // 0=dead, 1=alive
	int strikeable;         // If location is ready to be hit
} element;



element Player_Init(void);
element CurrentPlayer;

// Initialize array for Notes. 
typedef struct State STyp;
STyp Note[20];

/* ARRAY THAT CONTAINS THE SONG */
//int song[] = {1,2,3,3,2,1,0,2,3,1,2,3,3,0,3,2,1,2,3,0}; // Zero is rest note
int song[] = {2,2,1,1,3,2,3,1,1,1,1,1,1,1,1,1,1,1,1};
long tempNote;
	
/*******************************************************************************
* Global Variables
*******************************************************************************/
unsigned long TimerCount;
unsigned long PotValue = 0;
unsigned long Semaphore;
volatile unsigned long ADCvalue;
unsigned long SW1,SW2;
volatile unsigned int msTicks = 0;
volatile unsigned long FallingEdges = 0;
unsigned long volatile delay;

// Guitar Invaders - Global Variables
unsigned int PlayerLives = 0; 
unsigned int GameLevel = 0; // Difficulty of the game (i.e. the speed of the notes?) 
unsigned int SongChoice = 0 ;
unsigned char screenRefreshFlag;
unsigned int isGameOver = 0;
unsigned int PlayerScore = 0;

unsigned char GameCleared; // indicate if current level is cleared. 

unsigned long GameScore;
unsigned char screenRefreshFlag;
unsigned char enemyMove; 
unsigned char timer;
unsigned char lvlCleared = 0;
// Note related global variables
unsigned int NOTETHRESHOLD = 0;
unsigned int STRUCK = 0;
unsigned int SONGDURATION = 20;

//timer related global variable 
unsigned long noteSpeed; //depending on the level

unsigned int CurrentNote = 0;
int deleteMeTemp;

void NoteSpeedFnc(){
// How `enemySpeed` changes after every level
// Higher value = enemies move faster
	if(GameLevel == 1)
		noteSpeed = 100;
	else
		noteSpeed -= 200 * GameLevel;
}

/***********************************************************************
* main() - Main function
***********************************************************************/
 int main(void){

	/* delay */
	unsigned long volatile delay;
	
	/* INTITIALIZE COMPONENTS IN THE CODE */
	TExaS_Init(SSI0_Real_Nokia5110_Scope);  // set system clock to 80 MHz
	Control_Init();
	Random_Init(1);
	Nokia5110_Init();
	EnableInterrupts(); 
	EdgeCounter_Init();
	PLL_Init();                           // 80 MHz
  PortF_Init_2();												// Initialize Port F 
  ADC0_InitSWTriggerSeq3_Ch1();         // ADC initialization PE2/AIN1
	 
	 
	
	while(1){
		void UART_Init(void);
		/* */ 
		lvlCleared = 1;
		PlayerLives = 3;
		PlayerScore = 0;
		/* UART INIT */ 
		
		/* INITIALIZE GAME */ 
		GameInit();
		

		
		/* DISPLAY TITLE SCREEN */
		TitleScreen(delay);
		Delay100ms(1);
		
		/* DISPLAY SONG LIST SCREN*/
		SongList(delay);
		
		/** REFRESEH SCREEN */ 
		Nokia5110_ClearBuffer();
		screenRefreshFlag = 1;
		
		/* MAIN GAME LOOP */
		while(PlayerLives > 0){ 
			while(screenRefreshFlag == 0){}  

			/* LOCK */ 
			if(lvlCleared){
				NoteSpeedFnc(); // Used to determine speed of the note
				SysTick_Init(4000); 
				lvlCleared = 0; // some sort of lock
			}
			
			
			/* CLEAR PREVIOUS SCREEN BUFFER */
			Nokia5110_ClearBuffer();
			
			/* MAIN GAME FUNCTIONS */
			HUDBuffer();
			noteBuffer(20);
			
			/* DISPLAY NEW STATE */
			Nokia5110_DisplayBuffer();
			
		}//END MAIN LOOP
		
		isGameOver = 0;
		ResetLEDs();
	} 
	
}


/*******************************************************************************
* Reset LED States
*******************************************************************************/

void ResetLEDs(void){
	GPIO_PORTF_DATA_R = 0x00;
}

void GameInit(void){
// Intialise player's ship & bunkers & print them
// PlayerSpr will be at bottom-centre
// BunkerSprs[] will be evenly spaced across screen 
	//unsigned char x_coord, y_coord; // coordinates 		
	//unsigned char i; // "for" loops
	//unsigned char gap; // gap b/w bunkers
	//element tmp; // to get .width of any sprite
				/* CLEAR PREVIOUS SCREEN BUFFER */
			Nokia5110_ClearBuffer();
	// Reset global variables
	GameLevel = 0;
	GameScore = 0;
	enemyMove = 0;
	timer = 0;
	CurrentPlayer = Player_Init();
	
	songInit(song, 20);
	/*
	// Destroy any other sprites
	playerBullet.hp = 0;
	UFOSpr.hp = 0 ;
	for(i = 0; i < ENEMYNUM; i++){
		enemySprs[i].hp = 0;
		enemyBullets[i].hp = 0;
	}*/
	
	
	// Note hitting init
	/*
	tmp = Bunker_Init(0,0);
	y_coord = MAXSCREENY - PLAYERH;
	gap = (EFFSCREENW - BUNKERNUM * tmp.width) / (BUNKERNUM - 1);
	for(i = 0; i < BUNKERNUM; i++){ // For each bunker
		x_coord = MINSCREENX + i * (tmp.width + gap);
		bunkerSprs[i] = Bunker_Init(x_coord, y_coord);
	}*/
}
/*******************************************************************************
* iNITIALIZE PLAYER 
- for release 1.0 
*******************************************************************************/
element Player_Init(void){
// Intialise player's ship sprite center-bottom of screen
// Returns: sprite of player's ship
	element player;
	player.x = 0; // test
	player.y = 0; // test, to be removed
	player.currLives = 3;
	player.pt = 0; //current point? 
	
	return player;
}

/*******************************************************************************
* SCREEN 1 - Title Screen (First screen the users will see)
*******************************************************************************/
void TitleScreen(long volatile delay){
	// Title Screen
	Nokia5110_Clear();
	while(!START){
		Nokia5110_SetCursor(1,0);
		Nokia5110_OutString("GTR INVDRS"); 
		Nokia5110_SetCursor(0,1);
		Nokia5110_OutString("Select Level");
		Nokia5110_SetCursor(1,4);
		Nokia5110_OutString("");
		Nokia5110_OutString("1   2   3");
		GPIO_PORTF_DATA_R |= 0x04;          // profile
		ADCvalue = Slide_Potentiometer();
		GPIO_PORTF_DATA_R &= ~0x04;
		for(delay=0; delay<100000; delay++){};
		Nokia5110_Clear();
		if(PotValue == 1){	
			Nokia5110_SetCursor(0, 0);
			Nokia5110_OutString("I");;
		}
		if(ADCvalue<1024){
			Nokia5110_SetCursor(1, 3);
			GameLevel = 1;
		}
		else{
			if(ADCvalue<3072){
				Nokia5110_SetCursor(5, 3);
				GameLevel = 2;
			}
			else{
				Nokia5110_SetCursor(9, 3);
				GameLevel = 3;
		}
		}
		Nokia5110_OutString("V");
	}
	Nokia5110_Clear();
}
/*******************************************************************************
* SCREEN 2 - Song Choice 
*******************************************************************************/
void SongList(long volatile delay){
	Nokia5110_Clear();
	while(!START){
		Nokia5110_SetCursor(0,0);
		Nokia5110_OutString("Choose Song:");
		Nokia5110_SetCursor(2,1);
		Nokia5110_OutString("Song 1");
		Nokia5110_SetCursor(2,3);
		Nokia5110_OutString("Song 2"); 
		Nokia5110_SetCursor(2,5);
		Nokia5110_OutString("Song 3"); 

		/*GPIO_PORTF_DATA_R |= 0x04;*/         // profile
		ADCvalue = Slide_Potentiometer();
		/*GPIO_PORTF_DATA_R &= ~0x04;*/
		
		// delay
		for(delay=0; delay<100000; delay++){};
		Nokia5110_Clear();
		if(PotValue == 1){	
			Nokia5110_SetCursor(0, 0);
			Nokia5110_OutString("I");;
		}
		if(ADCvalue<1024){
			Nokia5110_SetCursor(0, 1);
			SongChoice = 1;
		}
		else{
			if(ADCvalue<3072){ // 3rd option
				Nokia5110_SetCursor(0, 3);
				SongChoice = 2;
			}
			else{ // second option
				Nokia5110_SetCursor(0, 5);
				SongChoice = 3;
		}
		}
		Nokia5110_OutString(">");
	}	
	Nokia5110_Clear();
}


/*******************************************************************************
* SCREEN 3 - Actual Game 
*******************************************************************************/

/*******************************************************************************
* TIMERS - INIT AND HANDLERS
*******************************************************************************/
void SysTick_Init(unsigned long milliseconds){
// Initalise systick at `freq`Hz (assuming 80Mhz PLL)
	NVIC_ST_CTRL_R = 0; 
	NVIC_ST_RELOAD_R = milliseconds * 80000 - 1;
  NVIC_ST_CURRENT_R = 0;
  NVIC_SYS_PRI3_R |= 0x01 << 29; // priority 1
	NVIC_ST_CTRL_R = 0x07;
} 

// Handles the main movement in the game.
void SysTick_Handler(void){
	int i, songDuration = 20;
	int collided = 0; 
	signed long temp = (84-INDICATORW-CIRCLEW);
	
	//GPIO_PORTF_DATA_R ^= 0x04;       // toggle PF2

	Move(20);
	/**************************************/
	// Collision Check 
	for(i=0;i<songDuration;i++){
		deleteMeTemp = Note[i].songLevel;
		if(Note[i].x == temp){
			
			if(CurrentNote != deleteMeTemp){
				PlayerLives -= 1;
			}else{
				PlayerScore += 1;
			}
			//GPIO_PORTF_DATA_R ^= 0x0E;
			//collided = 1;
		}
	}
	
	/* UPDATE LIVES ON PORT F */ 
	if(PlayerLives == 1){
		GPIO_PORTF_DATA_R = 0x02;
	}else if(PlayerLives == 2){
		GPIO_PORTF_DATA_R = 0x06;
	}else if(PlayerLives == 3){
		GPIO_PORTF_DATA_R = 0x0E;
	}else{
		GPIO_PORTF_DATA_R = 0x00;
	}
	
}

void Timer2A_Handler(void){ 
  TIMER2_ICR_R = 0x00000001;   // acknowledge timer2A timeout
  TimerCount++;
  Semaphore = 1; // trigger
}

void Timer1_Init(unsigned char freq){ 
// Initalise Timer1 to trigger every specified seconds(assuming 880Mhz PLL)
  unsigned long volatile delay;
  SYSCTL_RCGCTIMER_R |= 0x02;
  delay = SYSCTL_RCGCTIMER_R;
  TIMER1_CTL_R = 0x00000000;
  TIMER1_CFG_R = 0x00000000; // 32-bit mode
  TIMER1_TAMR_R = 0x00000002; // periodic mode, default down-count
  TIMER1_TAILR_R = 80000000 / freq -1;
  TIMER1_TAPR_R = 0; // bus clock resolution
  TIMER1_ICR_R = 0x00000001;
  TIMER1_IMR_R = 0x00000001;
  NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF0FFF)|(0x02 <<13); // priority 2
	NVIC_EN0_R |= 1<<21;
	TIMER1_CTL_R = 0x00000001; 
}

void Timer2_Init(unsigned long seconds){ 
// Initalise Timer2 to trigger every few seconds(assuming 880Mhz PLL)
  unsigned long volatile delay;
  SYSCTL_RCGCTIMER_R |= 0x04;
  delay = SYSCTL_RCGCTIMER_R;
  TIMER2_CTL_R = 0x00000000;
  TIMER2_CFG_R = 0x00000000; // 32-bit mode
  TIMER2_TAMR_R = 0x00000002; // periodic mode, default down-caount
  TIMER2_TAILR_R = seconds * 80000000 - 1; 
  TIMER2_TAPR_R = 0; // bus clock resolution
  TIMER2_ICR_R = 0x00000001;
  TIMER2_IMR_R = 0x00000001;
  NVIC_PRI5_R = (NVIC_PRI5_R&0x0FFFFFFF)|(0x03 << 29); // priority 3
	NVIC_EN0_R |= 1<<23;
	TIMER2_CTL_R = 0x00000001; 
}

/*******************************************************************************
* Song Choice 
*******************************************************************************/
void EdgeCounter_Init(void){
	SYSCTL_RCGC2_R |= 0x00000020; // (a) activate clock for port F
  FallingEdges = 0;             // (b) initialize counter
  GPIO_PORTF_DIR_R &= ~0x10;    // (c) make PF4 in (built-in button)
  GPIO_PORTF_AFSEL_R &= ~0x10;  //     disable alt funct on PF4
  GPIO_PORTF_DEN_R |= 0x10;     //     enable digital I/O on PF4   
  GPIO_PORTF_PCTL_R &= ~0x000F0000; // configure PF4 as GPIO
  GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
  GPIO_PORTF_PUR_R |= 0x10;     //     enable weak pull-up on PF4
  GPIO_PORTF_IS_R &= ~0x10;     // (d) PF4 is edge-sensitive
  GPIO_PORTF_IBE_R &= ~0x10;    //     PF4 is not both edges
  GPIO_PORTF_IEV_R &= ~0x10;    //     PF4 falling edge event
  GPIO_PORTF_ICR_R = 0x10;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|0x00A00000; // (g) priority 5
  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
  EnableInterrupts();           // (i) Clears the I bit
}

/*******************************************************************************
* Port F Interrupt Handler
*******************************************************************************/
void GPIOPortF_Handler(void){
	GPIO_PORTF_ICR_R = 0x10;      // acknowledge flag4
		FallingEdges = FallingEdges + 1;
    ADCvalue = ADC0_InSeq3();
		if(ADCvalue<1024){
			PotValue = 1;
		}
		else{
				if(ADCvalue<3072){
					PotValue = 2;
				}
				else{
					PotValue = 3;
			}
		}

}

/*******************************************************************************
* Initialize Port F - Traditional, Unused
*******************************************************************************/
void PortF_Init(void){ 
  SYSCTL_RCGC2_R |= 0x00000020;     // 1) F clock
  GPIO_PORTF_LOCK_R = 0x4C4F434B;   // 2) unlock PortF PF0  
  GPIO_PORTF_CR_R = 0x1F;           // allow changes to PF4-0       
  GPIO_PORTF_AMSEL_R = 0x00;        // 3) disable analog function
  GPIO_PORTF_PCTL_R = 0x00000000;   // 4) GPIO clear bit PCTL  
  GPIO_PORTF_DIR_R = 0x0E;          // 5) PF4,PF0 input, PF3,PF2,PF1 output   
  GPIO_PORTF_AFSEL_R = 0x00;        // 6) no alternate function
  GPIO_PORTF_PUR_R = 0x11;          // enable pullup resistors on PF4,PF0       
  GPIO_PORTF_DEN_R = 0x1F;          // 7) enable digital pins PF4-PF0        
}


/*******************************************************************************
* Initialize Port F - Variation, used for Project
*******************************************************************************/
void PortF_Init_2(void){	
	// Port F Init? 
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOF; // activate port F
  delay = SYSCTL_RCGC2_R;
	
	GPIO_PORTF_CR_R = 0x1F;           // allow changes to PF4-0  
	GPIO_PORTF_AMSEL_R = 0x00;        // 3) disable analog function
	GPIO_PORTF_DIR_R = 0x0E;          // 5) PF4,PF0 input, PF3,PF2,PF1 output 
	
  //GPIO_PORTF_DIR_R |= 0x04;             // make PF2 out (built-in LED)
  GPIO_PORTF_AFSEL_R &= ~0x04;          // disable alt funct on PF2
  GPIO_PORTF_DEN_R |= 0x1F;             // enable digital I/O on PF2
                                        // configure PF2 as GPIO
  GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFFF0FF)+0x00000000;
  GPIO_PORTF_AMSEL_R = 0;               // disable analog functionality on PF 
}

/*******************************************************************************
* 100 ms delay function
*******************************************************************************/
void Delay100ms(unsigned long count){
// Delay by count*100ms (excluding interrupts)
	unsigned long volatile time;
  while(count>0){
    time = 727240;  // 0.1sec at 80 MHz
    while(time){
	  	time--;
    }
    count--;
  }
}

/*******************************************************************************
* MAIN GAME FUNCTIONS
- This subsection contains the functions needed for the main portion of the game

> HUD Buffer: Function that takes care of the 3 note hitters
> 
*******************************************************************************/
void HUDBuffer(void){
	
	unsigned long tempPotValue = 0; 
	ADCvalue = Slide_Potentiometer();
	if (ADCvalue<1024){ tempPotValue = 1; }
	else if (ADCvalue > 1024 & ADCvalue < 3072){ tempPotValue = 2; }
	else {tempPotValue = 3;}
		
	//Nokia5110_PrintBMP(

	// Display HUD of 3 Notes, if Button is pressed, change corresponding Note to filled to indicate it was pressed
	if (SW1 == 0x00){
		if (ADCvalue<1024){
			CurrentNote = 1;
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 32, circleEmpty, 0); 
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 48, circleEmpty, 0); 
		}
		else if (ADCvalue > 1024 & ADCvalue < 3072){
			CurrentNote = 2;
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 16, circleEmpty, 0); 
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 48, circleEmpty, 0); 
		}
		else {
			CurrentNote = 3;
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 16, circleEmpty, 0); 
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 32, circleEmpty, 0);  
		}
		// Fill Circle Corresponding to PotValue
		Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, tempPotValue*16, circleFilled, 0); 
	}	else {
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 16, circleEmpty, 0); 
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 32, circleEmpty, 0); 
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 48, circleEmpty, 0); 
	}
	// Display Indicator at Location based on PotValue *FOR FUTURE RELEASE*
	//Nokia5110_PrintBMP(84-INDICATORW, 16*(PotValue*((2*PotValue+1)%2)+1), indicator, 0);  // Logic in Y Value is due to screen having top of screen start at 0
	
	//Update HUD if note should be in HUD
	if (NOTETHRESHOLD != 0) {
		if (NOTETHRESHOLD == 16){
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 32, circleEmpty, 0); 
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 48, circleEmpty, 0); 
		}
		else if (NOTETHRESHOLD == 32){
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 32, circleNote, 0); 
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 48, circleEmpty, 0); 
		}
		else {
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 16, circleEmpty, 0); 
			Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, 32, circleEmpty, 0); 
		}
		Nokia5110_PrintBMP(84-INDICATORW-CIRCLEW, NOTETHRESHOLD, circleNote, 0);
	}
}

/*****/ 
void songInit(int song[], int songDuration){ 
	int i = 0;
	int j = 0;
	
	for( j = 0; j < songDuration; j++){
		Note[i].x = 0;
	}
  for(i=0;i<songDuration;i++){
    Note[i].x = -16-16*i; // Put Buffer In progressively Negative Values
    //Note[i].x = 1;
		//Note[i].y = (song[i]*((2*song[i]+1)%2)+1); // Flip Bits due to y = 0 at top of screen
		Note[i].y = song[i]*16-6;
    Note[i].image = note;
		Note[i].strikeable = 0;
		
		Note[i].songLevel = song[i];
		
		// Set all 0 Notes to non-visible
		if (song[i] == 0){
			Note[i].visible = 0;
		}else{
			Note[i].visible = 1;
		}
   }
}
// Move all Notes to right, Make notes going into HUD not visible to prepare for separate animation.
void Move(int songDuration){ 
	int i;
  for(i=0;i<songDuration;i++){
		Note[i].x += 1;
    if((Note[i].x < 64 & Note[i].x > 0) & (Note[i].y > 0)){
      // Do nothing
			Note[i].visible = 1;
    }else{
      Note[i].visible = 0;
    }
  }
}

// Load all Notes in Nokia Buffer (without moving them i.e just taking 'Note[i]'
void noteBuffer(int songDuration){ 
	int i;
  //Nokia5110_ClearBuffer();
  for(i=0;i<songDuration;i++){
    if(Note[i].visible > 0){
     Nokia5110_PrintBMP(Note[i].x, Note[i].y, Note[i].image, 0);
    }
  }
  //Nokia5110_DisplayBuffer();      // draw buffer
}

// Checks if Note is about to enter HUD
void strikable(int songDuration) {
	int i;
	for(i=0;i<songDuration;i++){
		if (Note[i].x == 64){
			NOTETHRESHOLD = Note[i].y; // set global variable to heigh of note
			Note[i].strikeable = 1; // set struct entry to strikeable
		}
	}
}

// Checks if there is a note that is now off the screen that was strikeable i.e. wasnt struck
void missDetection(int song[], int songDuration) {
	int i;
	for(i=0;i<songDuration;i++){
		if (Note[i].x > 84 && Note[i].strikeable == 1){
			//Signal Missed Note
			Note[i].strikeable = 0; // Clear flag
		}
	}
}

// Function for Updating UI without Moving Other points // RB This goes on the interupt of changes to button/slider
void updateAfterHUD (void) {
	Nokia5110_ClearBuffer();
	HUDBuffer();
	noteBuffer(SONGDURATION);
	Nokia5110_DisplayBuffer();
}

void checkNoteStrike (int songDuration) {  // RB PLACE THIS IN INTERRUPT RESPONSE OF BUTTON INTERUPT
	//if button is pressed 
	int i;
	for(i=0;i<songDuration;i++){
		if (Note[i].strikeable == 1 && Note[i].y == 16*(PotValue*((2*PotValue+1)%2)+1)){
			Note[i].strikeable = 0; // Clear flag
		}
	}
}
