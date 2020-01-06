#include "portal.h"
#include <stdio.h>

void local_state_engine(int button, this_gun_struct& this_gun, other_gun_struct& other_gun){	

	//check for expiration of other gun
	if (this_gun.clock - other_gun.last_seen > GUN_EXPIRE) {
		if (this_gun.connected != false){
			this_gun.connected = false;
			other_gun.clock = 0;
			printf("MAIN Gun Expired\n");	
		}
		other_gun.state = 0;
	}
	else {
		if(this_gun.connected == false){
			this_gun.connected = true;
			printf("MAIN Gun Connected\n");
		}
	}
	
	//button event transitions
	if(button == BUTTON_RESET){
		this_gun.state_solo = 0; //reset self state
		this_gun.state_duo = 0; //reset local state
		this_gun.skip_states = false; 
		this_gun.mode = MODE_DUO; //reset to duo mode for common use case
	}
	else if(button == BUTTON_MODE_TOGGLE){
		if (this_gun.state_solo == 0 && this_gun.state_duo == 0){//only allow mode toggle when completely off
			if (this_gun.mode == MODE_DUO){
				this_gun.mode = MODE_SOLO;
			}else if (this_gun.mode == MODE_SOLO){
				this_gun.mode = MODE_DUO;
			}
		}
	}//TODO: add LONG PRESS from V2?
	else if (button == BUTTON_PRIMARY_FIRE){
		if (this_gun.mode == MODE_DUO){
			//projector modes
			if(this_gun.state_duo == 0){
				this_gun.state_duo = 1;
			}else if(this_gun.state_duo == 1){
				this_gun.state_duo = 2;
			}else if((this_gun.state_duo == 2 || this_gun.state_duo == 3) && (other_gun.state <= -3 || this_gun.skip_states)){ 	
			    this_gun.state_duo = 4; //answer an incoming call immediately and open portal on button press
			}else if(this_gun.state_duo == 5){ //playlist advance with closed portal
				this_gun.state_duo = 4; //open the portal
			}else if(this_gun.state_duo == 2 && other_gun.state > -3){ 
				this_gun.state_duo = 3; //wait at  mode 3 for other gun
			}
			//camera modes
			else if (this_gun.state_duo == -1){
				this_gun.state_duo = -2;
			}else if((this_gun.state_duo == -2 || this_gun.state_duo == -3) && other_gun.state >= 3 ){ 	
			   this_gun.state_duo = -4;
			}else if (this_gun.state_duo == -2){
				this_gun.state_duo = -3;//wait at -3 for the other gun
			}
		}else if (this_gun.mode == MODE_SOLO){
			if(this_gun.state_solo >= 0 && this_gun.state_solo < 4){
				this_gun.state_solo++;
			}else if(this_gun.state_solo == 4){
				this_gun.state_solo = 3; //possibly move this to alt fire for consistency?  But then solo color change needs to be moved
			}else if(this_gun.state_solo < 0 && this_gun.state_solo > -4){
				this_gun.state_solo--;
			}else if(this_gun.state_solo == -4){
				this_gun.state_solo = -3; //possibly move this to alt fire for consistency? But then solo color change needs to be moved
			}		
		}
	}
	else if (button == BUTTON_ALT_FIRE){
		if (this_gun.mode == MODE_DUO){
			if (this_gun.state_duo == 0){ //start duo camera mode
				this_gun.state_duo = -1;
			}else if(this_gun.state_duo == 4){ //go back to closed portal for effect change 
				this_gun.state_duo = 5;  //moved to alt fire from primary to prevent mode overrun when quickly tapping fire
			}else if(this_gun.state_duo == -4){ //quick swap function
				this_gun.state_duo = 4;
			}
		}else if (this_gun.mode == MODE_SOLO){
			if(this_gun.state_solo == 0){ //blue solo portal open
				this_gun.state_solo = -1;
			}else if(this_gun.state_solo == 3 || this_gun.state_solo == 4){//solo portal color change
				this_gun.state_solo = -3;
			}else if(this_gun.state_solo == -3 || this_gun.state_solo == -4){//solo portal color change
				this_gun.state_solo = 3;
			}			
		}
	}

	//other gun transitions
	if (other_gun.state_previous != other_gun.state){
		if (this_gun.mode == MODE_DUO){
			if (other_gun.state == 0){
				this_gun.state_duo = 0;
				this_gun.skip_states = false; 
			}else if ((other_gun.state == -2 || other_gun.state == -3) && this_gun.state_duo < 2){
				this_gun.state_duo = 2;
				this_gun.skip_states = true; 
			}else if ((other_gun.state == 2 || other_gun.state == 3) && this_gun.state_duo > -2){
				this_gun.state_duo = -2;
			}else if (other_gun.state <= -4 && this_gun.state_duo == 3){ //special case
				this_gun.state_duo = 4; //only bump into state 4 from 3, since 3 & 4 turn on the projector, below 3 the portal is off
			}else if (other_gun.state >= 4 && this_gun.state_duo > -4){
				this_gun.state_duo = -4;
			}
		}else if (this_gun.mode == MODE_SOLO){  //code to pull out of solo states
			if (other_gun.state <= -3){
				if (this_gun.state_solo <= -3 || this_gun.state_solo >= 3){
					this_gun.state_duo = 4;  //if a portal is open, go direct to projecting
				}else{
					this_gun.state_duo = 2; //if a portal is closed, don't blind anyone and just go to duo mode 2
				}			
				this_gun.mode = MODE_DUO;
				this_gun.state_solo = 0;
			}
		}
	}
	
	//reset playlists to the start
	//continually reload playlist if in state 0 to catch updates
	if (this_gun.state_duo == 0){
		this_gun.effect_duo = this_gun.playlist_duo[0];
		if (this_gun.effect_duo <= -1) this_gun.effect_duo = GST_VIDEOTESTSRC;
		this_gun.playlist_duo_index = 1;
		//special case of playlist 1 item long
		if (this_gun.playlist_duo[this_gun.playlist_duo_index] <= -1) this_gun.playlist_duo_index = 0;
	}
	
	if (this_gun.state_solo == 0){
		this_gun.effect_solo = this_gun.playlist_solo[0];
		if (this_gun.effect_solo <= -1) this_gun.effect_solo = GST_VIDEOTESTSRC;
		this_gun.playlist_solo_index = 1;	
		//special case of playlist 1 item long
		if (this_gun.playlist_solo[this_gun.playlist_solo_index] <= -1) this_gun.playlist_solo_index = 0;
	}
	
	//load next shared playlist item
	if (this_gun.state_duo == 5 && this_gun.state_duo_previous == 4){ 
		this_gun.effect_duo = this_gun.playlist_duo[this_gun.playlist_duo_index];
		this_gun.playlist_duo_index++;
		if (this_gun.playlist_duo[this_gun.playlist_duo_index] <= -1) this_gun.playlist_duo_index = 0;
		if (this_gun.playlist_duo_index >= playlist_duo_SIZE) this_gun.playlist_duo_index = 0;
	}
	
	//load next private playlist item
	if ((this_gun.state_solo == 3 && (this_gun.state_solo_previous == 4 || this_gun.state_solo_previous == -4 )) || (this_gun.state_solo == -3 && (this_gun.state_solo_previous == -4 || this_gun.state_solo_previous == 4 ))){ 
		this_gun.effect_solo = this_gun.playlist_solo[this_gun.playlist_solo_index];
		this_gun.playlist_solo_index++;
		if (this_gun.playlist_solo[this_gun.playlist_solo_index] <= -1) this_gun.playlist_solo_index = 0;
		if (this_gun.playlist_solo_index >= playlist_solo_SIZE) this_gun.playlist_solo_index = 0;
	}
}
