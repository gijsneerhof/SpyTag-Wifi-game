//Copyright 2015 <>< Charles Lohr, see LICENSE file.
//Copyright 2019 <>< Aaron Angert, see LICENSE file.
//Work in Progress

#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "uart.h"
#include "osapi.h"
#include "espconn.h"
#include "esp82xxutil.h"
#include "ws2812.h"
#include "commonservices.h"
#include "vars.h"
#include "gpio.h"
#include "hash.c"
#include <mdns.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "gpio_buttons.h"

#define procTaskPrio 0
#define procTaskInit 1
#define procTaskopts 2

#define procTaskQueueLen 3

#define HUMAN 0
#define ZOMBIE 1
#define SUPERZOMBIE 2
#define ENDGAME 3

#define REDTEAM 4
#define GREENTEAM 5
#define BLUETEAM 6

#define NOTEAM 7

//B button = GPIO 12
//A button = GPIO 4

#define NOBUTTONS 216
#define ABUTTON 220 //post button
#define BBUTTON 218 //zombie button
#define BOTHBUTTONS 222 //endgame buttons

static volatile os_timer_t game_timer;
static volatile os_timer_t end_timer;
static volatile os_timer_t begin_timer;

static volatile os_timer_t game_timer;
static volatile os_timer_t end_timer;

static volatile os_timer_t score_display_timer;

static struct espconn *pUdpServer;
usr_conf_t *UsrCfg = (usr_conf_t *)(SETTINGS.UserData);
//uint8_t last_leds[40*3] = {0};
uint8_t leds[16 * 3] = {0};
// const int N = 300;
hashtable_t *hashtable;

//status of the beacon, and last three recorded rssi values
struct beacon_stat
{
	char *type;
	int rssi[3];
};

int state;
int prev_state;

char states[][32] = {"Human", "Zombie", "SuperZombie", "EndGame", "RedTeam", "GreenTeam", "BlueTeam", "NoTeam"};


int normal_radar[15] = {-300, -90, -85, -80, -75, -70, -65, -62 -60, -58, -55, -52, -50, -46, -42}; 

//int normal_radar[10] = {-100, -95, -90, -85, -80, -75, -70, -65, -60, -55, -50,};

int colors[12 * 3] = {0x00, 0x01, 0x00, //human
					  0x01, 0x00, 0x00, //Zombie
					  0x00, 0x04, 0x00, //Superzombie
					  0x01, 0x00, 0x00, //Dead
					  0x01, 0x00, 0x00, //BlueTeam
					  0x00, 0x01, 0x00, //RedTeam
					  0x00, 0x00, 0x01, //GreenTeam
					  0x01, 0x01, 0x01};//NoTeam

bool end_game = false;

bool button_pressed = false;

//9400
//4800

int light_level = 0x60;

bool show_time = false;
bool show_score = false;

int score = 0;

unsigned long score_cooldown = 0;

int timer_index = 4; //set 30 minutes
int times[8] = {5, 10, 15, 20, 30, 40, 50, 60};

int sensitivity_index = 3; //set  -42
float sensitivities[7] = {-30.0, -34.0, -38.0, -42.0, -46.0, -50.0, -54.0};

int begin_time = 15;

void ICACHE_FLASH_ATTR user_scan(void);


void user_rf_pre_init(void)
{ /*nothing*/
}

char *strcat(char *dest, char *src)
{
	return strcat(dest, src);
}

//Tasks that happen all the time.
os_event_t procTaskQueue[procTaskQueueLen];

//Timer event.
int count_down_time = 16;
static void ICACHE_FLASH_ATTR gameTimer(void *arg)
{
	if (count_down_time <= 1)
	{
		os_timer_disarm(&game_timer);
		end_game = true;
	}
	count_down_time -= 1;
}


//this function sets the begin grace period and counts down. When te timer ticks, this function will be called. 
// When done, user_scan will be called
static void ICACHE_FLASH_ATTR begin_game_func(void *arg)
{
	
	make_radar_full(leds, colors[BLUETEAM*3], colors[BLUETEAM *3+1], colors[BLUETEAM*3+2], begin_time);

	WS2812OutBuffer(leds, sizeof(leds), light_level);
	if (begin_time == 0)
	{
		os_timer_disarm(&begin_timer);
		os_timer_disarm(&game_timer);
		os_timer_setfn(&game_timer, (os_timer_func_t *)gameTimer, NULL);
		os_timer_arm(&game_timer, (times[timer_index] * 60 * 1000) / 16, 1);
		system_os_task(user_scan, procTaskPrio, procTaskQueue, procTaskQueueLen);
		system_os_post(procTaskPrio, 0, 0);
	}
	begin_time -= 1;
}


int end_state = 0;
//this function creates the endstate animation
static void ICACHE_FLASH_ATTR end_game_func(void *arg)
{
	//show score!
	if (end_state % 2 == 0)
	{
		make_lights(leds, 15, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 14, 0, 0, 0);
		make_lights(leds, 13, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 12, 0, 0, 0);
		make_lights(leds, 11, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 10, 0, 0, 0);
		make_lights(leds, 9, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 8, 0, 0, 0);

		make_lights(leds, 7, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 6, 0, 0, 0);
		make_lights(leds, 5, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 4, 0, 0, 0);
		make_lights(leds, 3, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 2, 0, 0, 0);
		make_lights(leds, 1, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 0, 0, 0, 0);
	}
	else
	{
		make_lights(leds, 15, 0, 0, 0);
		make_lights(leds, 14, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 13, 0, 0, 0);
		make_lights(leds, 12, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 11, 0, 0, 0);
		make_lights(leds, 10, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 9, 0, 0, 0);
		make_lights(leds, 8, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);

		make_lights(leds, 7, 0, 0, 0);
		make_lights(leds, 6, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 5, 0, 0, 0);
		make_lights(leds, 4, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 3, 0, 0, 0);
		make_lights(leds, 2, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
		make_lights(leds, 1, 0, 0, 0);
		make_lights(leds, 0, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
	}
	int i = 0;
	for(i = 0; i < score;i++){
		make_lights(leds, 14-i, colors[NOTEAM *3], colors[NOTEAM *3 +1], colors[NOTEAM*3 + 2]);
	}

	WS2812OutBuffer(leds, sizeof(leds), light_level);
	end_state = (end_state + 1) % 2;
}

//Called when new packet comes in.
static void ICACHE_FLASH_ATTR
udpserver_recv(void *arg, char *pusrdata, unsigned short len)
{
	struct espconn *pespconn = (struct espconn *)arg;
	uart0_sendStr("X");
}

void ICACHE_FLASH_ATTR charrx(uint8_t c)
{
	//Called from UART.
}

//sets single led (r,g,b)
void make_lights(char light_array[], int light_num, int r, int g, int b)
{
	light_array[light_num * 3] = g;
	light_array[light_num * 3 + 1] = r;
	light_array[light_num * 3 + 2] = b;
}

//callback for wifi scan done
void scan_done(void *arg, STATUS status);

//changes state, edits wifi ssid
void ICACHE_FLASH_ATTR change_state(void)
{
	printf("changing state, state is now %d\n", state);
	prev_state = state;
	struct softap_config softapconfig;
	wifi_softap_get_config(&softapconfig);

	memset(&softapconfig.ssid, 0, sizeof(softapconfig.ssid));
	os_memcpy(&softapconfig.ssid, states[state], sizeof(states[state]));
	softapconfig.channel = 1;

	wifi_softap_set_config_current(&softapconfig);
	wifi_set_opmode(STATIONAP_MODE);
	wifi_softap_set_config_current(&softapconfig);

	make_radar_full(leds, colors[ZOMBIE *3], colors[ZOMBIE *3+1], colors[ZOMBIE*3+2], 16);

}


//called after user_init
void ICACHE_FLASH_ATTR
game_options(void)
{
	system_os_task(game_options, procTaskopts, procTaskQueue, procTaskQueueLen);
	int buttons = GetButtons();

	state = HUMAN;
	if(buttons == BBUTTON){
		state = ZOMBIE;
		change_state();
	}
	else if(buttons == ABUTTON){
		state = NOTEAM;
		change_state();
		make_radar_full(leds, colors[NOTEAM*3], colors[NOTEAM*3 + 1], colors[NOTEAM*3 +2], 16);
		WS2812OutBuffer(leds, sizeof(leds), light_level);
		return;
	}
	else if(buttons == BOTHBUTTONS){
		state = ENDGAME;
		change_state();
		make_radar_full(leds, colors[ENDGAME*3], colors[ENDGAME*3 + 1], colors[ENDGAME*3 +2], 16);
		WS2812OutBuffer(leds, sizeof(leds), light_level);
		return;
	}

	os_timer_disarm(&begin_timer);
	os_timer_setfn(&begin_timer, (os_timer_func_t *)begin_game_func, NULL);
	//begin timer settings, should be 20000
	os_timer_arm(&begin_timer, 2000, 1);
	begin_game_func(1);
	make_radar_full(leds, colors[BLUETEAM *3], colors[BLUETEAM *3+1], colors[BLUETEAM*3+2], begin_time);

	WS2812OutBuffer(leds, sizeof(leds), light_level);
}

//scans for wifi 
void ICACHE_FLASH_ATTR
user_scan(void)
{
	if (end_game)
	{
		os_timer_disarm(&end_timer);
		os_timer_setfn(&end_timer, (os_timer_func_t *)end_game_func, NULL);
		os_timer_arm(&end_timer, 500, 1);
	}
	else
	{
		int buttons = GetButtons();
		//208, no buttons
		//221, A button
		//210, B button
		//223, both buttons

		if (buttons == BBUTTON)
		{
			button_pressed = true;
			show_time = true;
		}

		if (prev_state != state)
		{
			printf("WOOP PREV STATE %d is not state %d changing\n", prev_state, state);
			change_state();
		}

		if (button_pressed && buttons == NOBUTTONS)
		{
			if (show_time)
			{
				show_time = false;
				make_radar_full(leds, 1, 0, 0, 0, 16);
			}
			button_pressed = false;
		}

		struct scan_config config;

		memset(&config, 0, sizeof(config));
		//config.ssid	=	"AI-THINKER_CC47D5";
		config.channel = 1;

		wifi_station_scan(&config, scan_done);
	}
}

//give all leds up to num the color (r,g,b)
void make_radar_full(char leds[], int r, int g, int b, int num)
{
		int a = 0;
		for (a = 0; a < 16; a++)
		{
			if (a <= num)
			{
				make_lights(leds, a, r, g, b);
			}
			else
			{
				make_lights(leds, a, 0, 0, 0);
			}
		}

}

int get_radar_value(int values[], float distance)
{
	int a = 0;
	for (a = 0; a < 14; a++)
	{
		if (distance > values[a])
		{
			continue;
		}
		else
		{

			return a;
		}
	}
	return 14;
}

char macmap[15];

static void ICACHE_FLASH_ATTR end_score_display(void *arg){
	os_timer_disarm(&end_timer);
	show_score = false;
}


void scan_done(void *arg, STATUS status)
{
	system_os_task(user_scan, procTaskPrio, procTaskQueue, procTaskQueueLen);
	uint8 ssid[33];
	char temp[128];
	float closest_zombie = -300.0;

	//This is a human that is not nearby
	float closest_out_human = -300.0;
	float closest_human = -300.0;

	//closest noteam (Scorepoint)
	float closest_out_noteam = -300.0;
	float closest_noteam = -300.0;

	if (status == OK)
	{
		struct bss_info *bss_link = (struct bss_info *)arg;

		while (bss_link != NULL)
		{

			memset(ssid, 0, 33);
			if (strlen(bss_link->ssid) <= 32)
				memcpy(ssid, bss_link->ssid, strlen(bss_link->ssid));
			else
				memcpy(ssid, bss_link->ssid, 32);

			ets_sprintf(macmap, MACSTR, MAC2STR(bss_link->bssid));
			struct beacon_stat *beacon = hasht_get(hashtable, macmap);
			if (!beacon)
			{
				int first_rssi[3] = {bss_link->rssi, bss_link->rssi, bss_link->rssi};

				struct beacon_stat b;
				b.type = ssid;
				b.rssi[0] = bss_link->rssi;
				b.rssi[1] = bss_link->rssi;
				b.rssi[2] = bss_link->rssi;

				ht_set(hashtable, macmap, &b);
				beacon = &b;
			}
			beacon->rssi[0] = beacon->rssi[1];
			beacon->rssi[1] = beacon->rssi[2];
			beacon->rssi[2] = bss_link->rssi;
			//running average of 3 samples
			float average = (beacon->rssi[0] + beacon->rssi[1] + beacon->rssi[2]) / 3.0;

			if (strcmp(ssid, states[ZOMBIE]) == 0 || strcmp(ssid, states[SUPERZOMBIE]) == 0)
			{
				if (average > closest_zombie)
				{
					closest_zombie = average;
				}
			}

			else if (strcmp(ssid, states[HUMAN]) == 0)
			{
				if (average > closest_out_human && average < -50)
				{
					closest_out_human = average;
				}
				if (average > closest_human)
				{
					closest_human = average;
				}
			}
			else if (strcmp(ssid, states[NOTEAM]) == 0)
			{
				if (average > closest_out_noteam && average < -50)
				{
					closest_out_noteam = average;
				}
				if (average > closest_noteam)
				{
					closest_noteam = average;
				}
			}
			else if (strcmp(ssid, states[ENDGAME]) == 0)
			{
				os_timer_disarm(&game_timer);
				end_game = true;
			}

			bss_link = bss_link->next.stqe_next;
		}
	}
	else
	{
		printf("scan	fail	!!!\r\n");
	}
	if (state == HUMAN)
	{

		if (closest_zombie > sensitivities[sensitivity_index])
		{
			state = ZOMBIE;
		}
		if(closest_noteam > sensitivities[sensitivity_index])
		{
			if(!show_score){
				score++;
				show_score = true;
				if(score >=16){
					score = 0;
				}
				//set timer to stop displaying score
					os_timer_disarm(&score_display_timer);
					os_timer_setfn(&score_display_timer, (os_timer_func_t *)end_score_display, NULL);
					os_timer_arm(&score_display_timer, 30000, 0);
			}
		}

		int zombie_num = get_radar_value(normal_radar, closest_zombie);
		make_radar_full(leds, colors[ZOMBIE * 3], colors[ZOMBIE * 3 + 1], colors[ZOMBIE * 3 + 2], zombie_num);

	}
	else if (state == ZOMBIE)
	{

		int human_num = get_radar_value(normal_radar, closest_human);
		make_radar_full(leds, colors[HUMAN * 3], colors[HUMAN * 3 + 1], colors[HUMAN * 3 + 2], human_num);		
	}
	//set colors of own team
	make_lights(leds, 0, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);
	make_lights(leds, 15, colors[state * 3], colors[state * 3 + 1], colors[state * 3 + 2]);

	if (show_time == true)
	{
		make_radar_full(leds, colors[BLUETEAM * 3], colors[BLUETEAM * 3 + 1], colors[BLUETEAM * 3 + 2], count_down_time);
		WS2812OutBuffer(leds, sizeof(leds), light_level);
	}
	if(show_score == true)
	{
		int i = 0;
		for(i = 0; i < score;i++){
			make_lights(leds, 14-i, colors[NOTEAM *3], colors[NOTEAM *3 +1], colors[NOTEAM*3 + 2]);
		}
	}

	WS2812OutBuffer(leds, sizeof(leds), light_level);

	system_os_post(procTaskPrio, 0, 0);
}



static void ICACHE_FLASH_ATTR procTask(os_event_t *events)
{
}

//THIS DOESN"T WORK
void HandleButtonEvent(uint8_t stat, int btn, int down)
{
	system_os_post(0, 0, 0);
}

void user_init(void)
{

	state = 0;
	prev_state = 0;
	printf("start state, state is now %d\n", state);
	printf("SDK	version:%s\n", system_get_sdk_version());
	printf("ESP8266	chip	ID:0x%x\n", system_get_chip_id());

	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	uart0_sendStr("\r\nesp82XX Web-GUI\r\n" VERSSTR "\b");

	char nameo[32] = "Human";
	struct softap_config softapconfig;
	wifi_softap_get_config(&softapconfig);

	memset(&softapconfig.ssid, 0, sizeof(softapconfig.ssid));

	os_memcpy(&softapconfig.ssid, nameo, sizeof(nameo));
	softapconfig.channel = 1;

	wifi_softap_set_config_current(&softapconfig);
	wifi_set_opmode(STATIONAP_MODE);
	wifi_softap_set_config_current(&softapconfig);

	SetupGPIO();

	int buttons = GetButtons();

	hashtable = hasht_create(300);
	// wifi scan has to after system init done.

	gpio_init();
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	GPIO_OUTPUT_SET(GPIO_ID_PIN(2), 0);
	printf("ESP8266	chip ID:0x%x\n", system_get_chip_id());
	printf("makeleds\n");
	make_radar_full(leds, 0,0,0,16);
	printf("made leds\n");

	system_init_done_cb(game_options);
	//Timer example
	//os_timer_disarm(&some_timer);
	//os_timer_setfn(&some_timer, (os_timer_func_t *)myTimer, NULL);
	//os_timer_arm(&some_timer, 10000, 0);


}

//There is no code in this project that will cause reboots if interrupts are disabled.
void EnterCritical() {}

void ExitCritical() {}
