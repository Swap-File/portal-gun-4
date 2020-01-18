#include "../shared.h"
#include "portalgl.h"
#include <GL/glx.h>
#include <stdio.h>
#include "font.h"

static GLuint text_vertex_list;
extern struct gun_struct *this_gun;

static void print_centered(char *input,int height)
{
	int offset = ((768/2) - font_GetWidth(input) )/2;
	font_PrintXY(input,offset,height);
}

static void slide_to(float r, float g, float b)
{
	static float r_target,g_target,b_target;
	if (r_target < r)		r_target += .02;
	else if (r_target > r)	r_target -= .02;
	if (g_target < g)		g_target += .02;
	else if (g_target > g)	g_target -= .02;
	if (b_target < b)		b_target += .02;
	else if (b_target > b)	b_target -= .02;
	glColor4f(r_target,g_target,b_target,1.0); 
}

void ui_redraw(void)
{	
	glDisable(GL_DEPTH_TEST);
	glDepthMask(false);

	// Setup projection
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	// Setup Modelview
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	//set size here
	glOrtho(0,1366,0,768/2,-1,1);

	glBindTexture(GL_TEXTURE_2D, 0); //no texture	
	
	
	if (this_gun->state_solo < 0 || this_gun->state_duo < 0) slide_to(0.0,0.5,0.5); 
	else if (this_gun->state_solo > 0 || this_gun->state_duo > 0) slide_to(0.8,0.4,0.0); 
	else slide_to(0.0,0.0,0.0); 
	
	
	glCallList(text_vertex_list);
	glTranslatef(1366 , 0, 0);
	//angle here
	glRotatef(90.0, 0.0, 0.0, 1.0);
	
	// Setup Texture, color and blend options
	glEnable(GL_TEXTURE_2D);

	font_Bind();
	font_SetBlend();

	char temp[200];

	if (this_gun->gordon)   sprintf(temp,"Gordon");	
	else  sprintf(temp,"Chell");	
	print_centered(temp,64* 9.25);
	
	sprintf(temp,"%.0f/%.0f\260F",this_gun->temperature_pretty ,this_gun->coretemp);	
	print_centered(temp,64* 8);
	
	
	if (this_gun->connected) sprintf(temp,"Synced");	
	else  sprintf(temp,"Sync Err");	
	print_centered(temp,64* 7);
	
	sprintf(temp,"Idle");
	if (this_gun->state_solo > 0 ||this_gun->state_solo < 0 || this_gun->state_duo > 1 )  sprintf(temp,"Emitting");	//collecting or countdown
	if (this_gun->state_duo < 0)  sprintf(temp,"Capturing");
	print_centered(temp,64* 6);
	
	int kbits = this_gun->kbytes_wlan;
	if (this_gun->kbytes_bnep > 0)  kbits += this_gun->kbytes_bnep;
	sprintf(temp,"%dKb/s",kbits);	
	print_centered(temp,64* 5);
	
	sprintf(temp,"%ddB %dMB/s",this_gun->dbm ,this_gun->tx_bitrate);	 
	print_centered(temp,64* 4);
	
	if (this_gun->latency > 99) sprintf(temp,"%.1fV %.0fms",this_gun->battery_level_pretty,this_gun->latency );	
	else if (this_gun->latency > 9) sprintf(temp,"%.1fV %.1fms",this_gun->battery_level_pretty,this_gun->latency );
	else  sprintf(temp,"%.1fV %.2fms",this_gun->battery_level_pretty,this_gun->latency );
	print_centered(temp, 64* 3);
	
	if (this_gun->mode == 2) sprintf(temp,"%s",effectnames[this_gun->effect_duo]);
	else					sprintf(temp,"%s",effectnames[this_gun->effect_solo]);		
	print_centered(temp,64* 2);
	
	uint32_t current_time;	
	if (this_gun->mode == 2)  {
		sprintf(temp,"Duo Mode");	
		if (this_gun->connected) current_time = (millis() + this_gun->other_gun_clock)/2;
		else current_time = millis();  
	}
	else   {  
		sprintf(temp,"Solo Mode");	
		current_time = millis();
	}
	print_centered(temp,64);

	int milliseconds = (current_time % 1000);
	int seconds      = (current_time / 1000) % 60;
	int minutes      = (current_time / (1000*60)) % 60;
	int hours        = (current_time / (1000*60*60)) % 24;
	sprintf(temp,"%.2d:%.2d:%.2d.%.3d",hours,minutes,seconds,milliseconds);	
	print_centered(temp,0);
	
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(true);
	glEnable(GL_BLEND); 
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
}

void ui_init(void)
{
	font_Load("/home/pi/portal/gstvideo/consolas.bff");
	
	text_vertex_list = glGenLists( 1 );
	glNewList( text_vertex_list, GL_COMPILE );
	glBegin( GL_QUADS );
	
	glVertex3f( 1366/2, 768/2,0);//top left
	glVertex3f( 1366/2,     0,0);//bottom left
	glVertex3f(   1366,     0,0);//bottom right	
	glVertex3f(   1366, 768/2,0);//top right
	glEnd();
	glEndList();
}