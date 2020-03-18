#include "state.h"
#include "io.h"
#include "pipe.h"
#include "common/effects.h"
#include <stdio.h>
#include <stdlib.h>

#define GUN_EXPIRE 1000 //expire a gun if not seen for 1 second
#define SHUTTER_DELAY 100

void state_engine(int button,struct gun_struct *this_gun)
{
	/* force video mode to change if a video ends-> */
	if (this_gun->video_done){
		if (this_gun->state_solo == 5) this_gun->state_solo = 4;
		else if (this_gun->state_solo == -5) this_gun->state_solo = -4;
		this_gun->video_done = false;
	}

	/* check for expiration of other gun */
	if (this_gun->clock - this_gun->other_gun_last_seen > GUN_EXPIRE) {
		if (this_gun->connected != false) {
			this_gun->connected = false;
			this_gun->other_gun_clock = 0;
			printf("MAIN Gun Expired\n");
		}
		this_gun->other_gun_state = 0;
	} else {
		if(this_gun->connected == false) {
			this_gun->connected = true;
			printf("MAIN Gun Connected\n");
		}
	}

	if (button == BUTTON_RESET) {
		if (this_gun->state_solo == 0 && this_gun->state_duo == 0) pipe_laser_pwr(false,NULL);
		this_gun->state_solo = 0; //reset self state
		this_gun->state_duo = 0; //reset local state
		this_gun->mode = MODE_DUO; //reset to duo mode for common use case
	} else if (button == BUTTON_MODE_TOGGLE) {
		if (this_gun->state_solo == 0 && this_gun->state_duo == 0){//only allow mode toggle when completely off
			if (this_gun->mode == MODE_DUO) {
				this_gun->mode = MODE_SOLO;
			}else if (this_gun->mode == MODE_SOLO) {
				this_gun->mode = MODE_DUO;
			}
		} else {  //when not idle, control preview modes
			if (this_gun->ui_mode == UI_ADVANCED) {
				this_gun->ui_mode = UI_SIMPLE;
			} else if (this_gun->ui_mode == UI_SIMPLE) {
				this_gun->ui_mode = UI_ADVANCED;
			} else if (this_gun->ui_mode == UI_HIDDEN_SIMPLE) {
				this_gun->ui_mode = UI_SIMPLE;
			} else if (this_gun->ui_mode == UI_HIDDEN_ADVANCED) {
				this_gun->ui_mode = UI_ADVANCED;
			}
		}
	} else if (button == BUTTON_PRIMARY_FIRE) {
		if (this_gun->mode == MODE_DUO){
			/* projector modes */
			if (this_gun->state_duo == 0) {  //off
				this_gun->state_duo = 1;  //orange idle lights
			}else if (this_gun->state_duo == 2) {  //orange dialed lights
				this_gun->state_duo = 3;  //start projector
			} else if (this_gun->state_duo == 1) {  //orange idle lights
				this_gun->state_duo = 3;  //start projector
			} else if (this_gun->state_duo == 6) {  //duo closed portal
				this_gun->state_duo = 5;  //closed portal
			}
			/* camera modes */
			else if (this_gun->state_duo == -1) {  //blue idle lights
				this_gun->state_duo = -3;  //camera start
			} else if (this_gun->state_duo == -2) {  //blue dialed lights
				this_gun->state_duo = -4;  //camera start
			}
		} else if (this_gun->mode == MODE_SOLO) {
			if (this_gun->state_solo == 0) {  //off
				this_gun->state_solo = 1;  //orange solo idle lights
			} else if (this_gun->state_solo == 1) {  //orange solo idle lights
				this_gun->state_solo = 2;  //orange start projector
			} else if (this_gun->state_solo == 2) {  //orange solo idle lights
				this_gun->state_solo = 3;  //orange start projector
			} else if (this_gun->state_solo == 4) {  //orange closed portal
				this_gun->state_solo = 5;   //orange open portal
			} else if (this_gun->state_solo == 5) {  //orange open portal
				this_gun->state_solo = 4;   //orange closed portal
			} else if (this_gun->state_solo == -1) { //blue solo idle lights
				this_gun->state_solo = -2;  //blue start projector
			} else if (this_gun->state_solo == -2) { //blue solo idle lights
				this_gun->state_solo = -3;  //blue start projector
			} else if (this_gun->state_solo == -4) {  //blue closed portal
				this_gun->state_solo = -5;  //blue open portal
			} else if (this_gun->state_solo == -5) {  //blue open portal
				this_gun->state_solo = -4;  //blue closed portal
			}
		}
	}
	else if (button == BUTTON_ALT_FIRE) {
		if (this_gun->mode == MODE_DUO) {
			if (this_gun->state_duo == 0) {  //off
				this_gun->state_duo = -1;  //blue duo idle lights
			} else if(this_gun->state_duo == 5) { //orange open portal
				this_gun->state_duo = 6;  //duo closed portal
			} else if(this_gun->state_duo == -4) { //quick swap function
				this_gun->state_duo = 3;  //orange start projector
			}
		} else if (this_gun->mode == MODE_SOLO) {
			if (this_gun->state_solo == 0) { //off
				this_gun->state_solo = -1;  //blue solo idle
			} else if(this_gun->state_solo >= 3) {//solo portal color change
				this_gun->state_solo = -3; //should immediately transition to -4, could be set to -4
			} else if(this_gun->state_solo <= -3) {//solo portal color change
				this_gun->state_solo = 3;  //should immediately transition to 4, could be set to 4
			}
		}
	}

	/* other gun transition - fixed */
	if (this_gun->state_duo == 4 && this_gun->other_gun_state <= -3)  // must be <= -3 to catch quick swap direct to -4
	this_gun->state_duo = 5;

	/* other gun transitions - on change */
	if (this_gun->other_gun_state_previous != this_gun->other_gun_state){
		if (this_gun->mode == MODE_DUO){
			if (this_gun->other_gun_state == 0)
			this_gun->state_duo = 0;
			if (this_gun->other_gun_state == 3 || this_gun->other_gun_state == 4) {
				if (this_gun->state_duo == 0 || this_gun->state_duo == -1)
				this_gun->state_duo = -2;
			}
			if (this_gun->other_gun_state == -3) {
				if (this_gun->state_duo == 0 || this_gun->state_duo == 1)
				this_gun->state_duo = 2;
			}
			if (this_gun->other_gun_state == 3 || this_gun->other_gun_state == 4) {
				if (this_gun->state_duo == 5 || this_gun->state_duo == 6)
				this_gun->state_duo = -4;  //go directly to 4 to skip -3 sound
			}
			if (this_gun->other_gun_state == 5 || this_gun->other_gun_state == 6) {
				if (this_gun->state_duo == -3)
				this_gun->state_duo = -4;
			}
		} else if (this_gun->mode == MODE_SOLO) {  //code to pull out of solo states
			if (this_gun->other_gun_state == 3 || this_gun->other_gun_state == 4) {
				this_gun->state_solo = 0;
				this_gun->state_duo = -2;
				this_gun->mode = MODE_DUO;
			}
			if (this_gun->other_gun_state == -3) {
				if (this_gun->state_solo >= -2 && this_gun->state_solo <= 2) {
					this_gun->state_solo = 0;
					this_gun->state_duo = 2;
					this_gun->mode = MODE_DUO;
				}
				if (this_gun->state_solo < -2 || this_gun->state_solo > 2) {
					this_gun->state_solo = 0;
					this_gun->state_duo = 3;
					this_gun->mode = MODE_DUO;
				}
			}
		}
	}

	/* if the shutter is damaged, set use_servo_strat false to fall back to old method of waiting 5 seconds to toggle the laser */
	if (this_gun->servo_bypass){
		/* always keep servo open in bypass mode */
		this_gun->servo_open = true;
		/* laser power states */
		if ((abs(this_gun->state_solo) < 3 && this_gun->mode == MODE_SOLO) || (this_gun->state_duo < 3 && this_gun->mode == MODE_DUO)) {
			pipe_laser_pwr(false,this_gun);
		} else {
			pipe_laser_pwr(true,this_gun);
		}
	} else {
		/* laser power states */
		if ((abs(this_gun->state_solo) < 2 && this_gun->mode == MODE_SOLO) || (this_gun->state_duo < 1 && this_gun->mode == MODE_DUO)) {
			pipe_laser_pwr(false,this_gun);
		} else {
			pipe_laser_pwr(true,this_gun);
		}
	}

	static uint32_t shutter_start_time = 0;
	/* laser states */
	if (this_gun->state_solo == 3 || this_gun->state_solo == -3 || this_gun->state_duo == 3){

		if (this_gun->servo_open == false){
			shutter_start_time = millis();
			//open the shutter but delay the state change
			this_gun->servo_open = true;  
		}

		if (this_gun->laser_on && (shutter_start_time + SHUTTER_DELAY <= millis())) {
			if(this_gun->state_duo == 3)		this_gun->state_duo = 4;
			else if(this_gun->state_solo == 3)	this_gun->state_solo = 4;
			else if(this_gun->state_solo == -3)	this_gun->state_solo = -4;
		}
	} else {
	
		//dont try to move shutter if we are bypassing
		if (this_gun->servo_bypass == false){
			//keep shutter open on projecting states
			if ( abs(this_gun->state_solo) > 3 || this_gun->state_duo > 3)	this_gun->servo_open = true;
			//close shutter on lower value states other than solo 3 & -3 & duo 3
			//do we need a SHUTTER_DELAY here too?  probably not.
			//extravagant shutter flourish here
			//else if ( this_gun->state_solo == 0 && this_gun->state_duo == 0 && this_gun->laser_on == false)	this_gun->servo_open = true;
			else 															this_gun->servo_open = false;
		}
	
	}
	
	/* reset playlists to the start */
	/* continually reload playlist if in state 0 to catch updates */
	if (this_gun->state_duo == 0) {
		this_gun->effect_duo = this_gun->playlist_duo[0];
		if (this_gun->effect_duo <= -1) this_gun->effect_duo = GST_VIDEOTESTSRC;
		this_gun->playlist_duo_index = 1;
		/* special case of playlist 1 item long */
		if (this_gun->playlist_duo[this_gun->playlist_duo_index] <= -1) this_gun->playlist_duo_index = 0;
	}

	if (this_gun->state_solo == 0) {
		this_gun->effect_solo = this_gun->playlist_solo[0];
		if (this_gun->effect_solo <= -1) this_gun->effect_solo = GST_VIDEOTESTSRC;
		this_gun->playlist_solo_index = 1;
		/* special case of playlist 1 item long */
		if (this_gun->playlist_solo[this_gun->playlist_solo_index] <= -1) this_gun->playlist_solo_index = 0;
	}

	/* load next duo playlist item */
	if (this_gun->state_duo == 6 && this_gun->state_duo_previous == 5) {
		this_gun->effect_duo = this_gun->playlist_duo[this_gun->playlist_duo_index];
		this_gun->playlist_duo_index++;
		if (this_gun->playlist_duo[this_gun->playlist_duo_index] <= -1) this_gun->playlist_duo_index = 0;
		if (this_gun->playlist_duo_index >= PLAYLIST_SIZE) this_gun->playlist_duo_index = 0;
	}

	/* load next solo playlist item */
	if ((this_gun->state_solo == 4 && (this_gun->state_solo_previous == 5 || this_gun->state_solo_previous == -5 )) || (this_gun->state_solo == -4 && (this_gun->state_solo_previous == -5 || this_gun->state_solo_previous == 5))) {
		this_gun->effect_solo = this_gun->playlist_solo[this_gun->playlist_solo_index];
		this_gun->playlist_solo_index++;
		if (this_gun->playlist_solo[this_gun->playlist_solo_index] <= -1) this_gun->playlist_solo_index = 0;
		if (this_gun->playlist_solo_index >= PLAYLIST_SIZE) this_gun->playlist_solo_index = 0;
	}

	/* PORTALGL GSTREAMER */
	/* Reminder: These variables are shared and may be read at ANY time */
	/* camera preload */
	if (this_gun->state_duo <= -2)		this_gun->gst_state = GST_RPICAMSRC;
	/* project shared preload */
	else if (this_gun->state_duo >= 1)	this_gun->gst_state = this_gun->effect_duo;
	/* project private preload */
	else if (this_gun->state_solo != 0)	this_gun->gst_state = this_gun->effect_solo;
	else								this_gun->gst_state = GST_BLANK;

	//Auto change UI to aiming preview
	if (this_gun->state_duo < -2 && this_gun->state_duo_previous >= -2){
		if(this_gun->ui_mode == UI_SIMPLE) this_gun->ui_mode = UI_HIDDEN_SIMPLE;
		else if(this_gun->ui_mode == UI_ADVANCED) this_gun->ui_mode = UI_HIDDEN_ADVANCED;
	}

	/* PORTALGL ARPETURE */
	/* Reminder: These variables are shared and may be read at ANY time */
	/* for networked modes */
	if (this_gun->state_solo == 0) {
		if      (this_gun->state_duo == 4)   this_gun->portal_state = PORTAL_CLOSED_ORANGE;
		else if (this_gun->state_duo == 5)   this_gun->portal_state = PORTAL_OPEN_ORANGE;
		else if (this_gun->state_duo == 6)	 this_gun->portal_state = PORTAL_CLOSED_ORANGE; //blink shut on effect change
		else								 this_gun->portal_state = PORTAL_CLOSED;
	}
	/* for self modes */
	else if (this_gun->state_duo == 0) {
		if      (this_gun->state_solo ==  4) this_gun->portal_state = PORTAL_CLOSED_ORANGE;
		else if (this_gun->state_solo == -4) this_gun->portal_state = PORTAL_CLOSED_BLUE;
		else if (this_gun->state_solo <= -5) this_gun->portal_state = PORTAL_OPEN_BLUE;
		else if (this_gun->state_solo >=  5) this_gun->portal_state = PORTAL_OPEN_ORANGE;
		else								 this_gun->portal_state = PORTAL_CLOSED;
	} else {
		this_gun->portal_state = PORTAL_CLOSED;
	}

	//Restore UI on portal closing
	if (this_gun->portal_state == PORTAL_CLOSED){
		if (this_gun->ui_mode == UI_HIDDEN_SIMPLE) {
			this_gun->ui_mode = UI_SIMPLE;
		} else if (this_gun->ui_mode == UI_HIDDEN_ADVANCED) {
			this_gun->ui_mode = UI_ADVANCED;
		}
	}

	/* DUO 0 OR SOLO 0 CLOSE */
	if ((this_gun->state_duo_previous != 0 || this_gun->state_solo_previous != 0) && this_gun->state_duo == 0 && this_gun->state_solo == 0)
	pipe_audio("/home/pi/assets/portalgun/portal_close1.wav");
	/* DUO 3+ ENTRY FROM SOLO 3 OR -3 (must be high on list to override other entry sounds) */
	else if ((this_gun->state_solo_previous <= -3 || this_gun->state_solo_previous >= 3) && this_gun->state_duo >= 3)
	pipe_audio("/home/pi/assets/portalgun/portal_open2.wav");
	/* DUO -2 ENTRY FROM SOLO (must be high on list to override other entry sounds) */
	else if ((this_gun->state_solo_previous < 0 || this_gun->state_solo_previous > 0) && this_gun->state_duo == -2)
	pipe_audio("/home/pi/assets/portalgun/portal_fizzle2.wav");
	/* DUO 2 ENTRY FROM SOLO (must be high on list to override other entry sounds) */
	else if ((this_gun->state_solo_previous == -2 || this_gun->state_solo_previous == 2) && this_gun->state_duo == 2)
	pipe_audio("/home/pi/assets/portalgun/portal_fizzle2.wav");
	/* DUO 1 ENTRY */
	else if (this_gun->state_duo_previous != 1 && this_gun->state_duo == 1)
	pipe_audio("/home/pi/assets/physcannon/physcannon_charge1.wav");
	/* DUO -1 ENTRY */
	else if (this_gun->state_duo_previous != -1 && this_gun->state_duo == -1)
	pipe_audio("/home/pi/assets/physcannon/physcannon_charge1.wav");
	/* DUO 2 ENTRY */
	else if (this_gun->state_duo_previous != 2 && this_gun->state_duo == 2)
	pipe_audio("/home/pi/assets/physcannon/physcannon_charge2.wav");
	/* DUO -2 ENTRY */
	else if (this_gun->state_duo_previous != -2 && this_gun->state_duo == -2)
	pipe_audio("/home/pi/assets/physcannon/physcannon_charge2.wav");
	/* DUO -3 ENTRY */
	else if (this_gun->state_duo_previous != -3 && this_gun->state_duo == -3)
	pipe_audio("/home/pi/assets/physcannon/physcannon_charge3.wav");
	/* DUO 3 ENTRY */
	else if (this_gun->state_duo_previous != 3 && this_gun->state_duo == 3)
	pipe_audio("/home/pi/assets/physcannon/physcannon_charge3.wav");
	/* DUO 4 ENTRY */
	else if (this_gun->state_duo_previous != 4 && this_gun->state_duo == 4)
	pipe_audio("/home/pi/assets/portalgun/portalgun_shoot_blue1.wav");
	/* DUO 5 ENTRY FROM OFF */
	else if (this_gun->state_duo_previous < 5 && this_gun->state_duo == 5)
	pipe_audio("/home/pi/assets/portalgun/portal_open2.wav");
	/* DUO -4 ENTRY FOR QUICK SWAP */
	else if (this_gun->state_duo_previous >= 3 && this_gun->state_duo <= -4)
	pipe_audio("/home/pi/assets/portalgun/portal_fizzle2.wav");
	/* DUO 6 CLOSE PORTAL FOR PLAYLIST ADVANCE */
	else if (this_gun->state_duo_previous == 5 && this_gun->state_duo == 6)
	pipe_audio("/home/pi/assets/portalgun/portal_close1.wav");
	/* DUO 5 OPEN PORTAL FROM PLAYLIST ADVANCE */
	else if (this_gun->state_duo_previous == 6 && this_gun->state_duo == 5)
	pipe_audio("/home/pi/assets/portalgun/portal_open1.wav");
	/* SOLO 1 & -1 (Not used!) */
	else if ((this_gun->state_solo_previous != this_gun->state_solo) && (this_gun->state_solo == 1 || this_gun->state_solo == -2))
	pipe_audio("/home/pi/assets/physcannon/physcannon_charge1.wav");
	/* SOLO 2 & -2 */
	else if ((this_gun->state_solo_previous != this_gun->state_solo) && (this_gun->state_solo == 2 || this_gun->state_solo == -2))
	pipe_audio("/home/pi/assets/physcannon/physcannon_charge2.wav");
	/* SOLO 3 & -3 */
	else if ((this_gun->state_solo_previous != this_gun->state_solo) && (this_gun->state_solo == 3 || this_gun->state_solo == -3))
	pipe_audio("/home/pi/assets/physcannon/physcannon_charge3.wav");
	/* SOLO 4 & -4 - Closed Portal entry from off */
	else if ((this_gun->state_solo_previous < 4 && this_gun->state_solo_previous > -4) && (this_gun->state_solo == 4 || this_gun->state_solo == -4))
	pipe_audio("/home/pi/assets/portalgun/portalgun_shoot_blue1.wav");
	/* SOLO 4 & -4 - Closed Portal entry from swap */
	else if (( this_gun->state_solo_previous >= 4 && this_gun->state_solo == -4) || (this_gun->state_solo_previous <= -4 && this_gun->state_solo == 4))
	pipe_audio("/home/pi/assets/portalgun/portal_open2.wav");
	/* SOLO 4 & -4 - Closed Portal entry from playing */
	else if ((this_gun->state_solo_previous > 4 && this_gun->state_solo == 4) || (this_gun->state_solo_previous < -4 && this_gun->state_solo == -4))
	pipe_audio("/home/pi/assets/portalgun/portal_close1.wav");
	/* SOLO 5 & -5 - Entry to playing */
	else if ((this_gun->state_solo_previous != -5 && this_gun->state_solo == -5) || (this_gun->state_solo_previous != 5 && this_gun->state_solo == 5))
	pipe_audio("/home/pi/assets/portalgun/portal_open1.wav");

}
