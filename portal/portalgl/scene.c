#include "../shared.h"
#include "portalgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h> //sqrt
#include <gst/gst.h>
#include <gst/gl/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include "png.h"

extern struct gun_struct *this_gun;

static int last_acceleration[2] = {0,0};

//textures
static GLuint orange_1,blue_n,orange_n,orange_0,blue_0,blue_1,texture_orange,texture_blue,circle64;

//vertex lists
static GLuint video_quad_vertex_list,event_horizon_vertex_list,event_horizon2_vertex_list,portal_vertex_list,shutter_vertex_list;

static float portal_spin = 0;
static float event_horizon_spin = 0;

static float event_horizon_vertex_list_shimmer = 0;
static bool event_horizon_vertex_list_shimmer_direction = true;

static float event_horizon_transparency_level = 0;

static float global_zoom = 0;

static float torus_offset[360];
static float running_magnitude;
static float angle_target;
static float angle_target_delayed;

//texture scrolling
static GLfloat donut_texture_scrolling = 0.0;

/* Borrowed from glut, adapted */
void draw_torus_vbo(){

	#define r 1.4
	#define R 9.8
	#define nsides 30
	#define rings 60
	
	int texcount=0,vertcount=0;
	
	GLfloat points[(nsides + 1) * rings * 2 *6];
	GLfloat tex[(nsides + 1) * rings * 2 * 4];

	GLfloat ringDelta = 2.0 * M_PI / rings;  //x and y 
	GLfloat sideDelta = 2.0 * M_PI / nsides; //z

	GLfloat theta = 0.0;
	GLfloat cosTheta = 1.0;
	GLfloat sinTheta = 0.0;
	
	for (int i = rings - 1; i >= 0; i--) {
		GLfloat theta1 = theta + ringDelta; //from 0 to 6.28
		
		GLfloat cosTheta1 = cos(theta1);
		GLfloat sinTheta1 = sin(theta1);

		GLfloat phi = 0.0;
		
		int index_deg = (360 - portal_spin + ( theta1 * 360  / (2*M_PI)));
		
		while (index_deg >= 360) index_deg -=360;
		
		GLfloat r_using = r + torus_offset[index_deg];
		GLfloat R_using = R - torus_offset[index_deg];
		
		for (int j = nsides; j >= 0; j--) {

			phi += sideDelta;
			
			GLfloat cosPhi = cos(phi);
			GLfloat sinPhi = sin(phi);
			
			GLfloat dist = R_using + r_using * cosPhi;
			
			GLfloat s0 = 20.0 * theta / (2.0 * M_PI);
			GLfloat s1 = 20.0 * theta1 / (2.0 * M_PI);
			GLfloat t = 2.0 * phi / (2.0 * M_PI);  //this seems to control texture wrap around the nut

			//glNormal3f(cosTheta1 * cosPhi, -sinTheta1 * cosPhi, sinPhi);
			tex[texcount++] = s0;
			tex[texcount++] = t - donut_texture_scrolling;
			points[vertcount++] =cosTheta1 * dist;
			points[vertcount++] =-sinTheta1 * dist;
			points[vertcount++] =r_using * sinPhi;
		
			//glNormal3f(cosTheta * cosPhi, -sinTheta * cosPhi, sinPhi);
			tex[texcount++] = s1;
			tex[texcount++] = t - donut_texture_scrolling;
			points[vertcount++] =cosTheta * dist;
			points[vertcount++] =-sinTheta * dist;
			points[vertcount++] =r_using * sinPhi;
		}
		
		theta = theta1;
		cosTheta = cosTheta1;
		sinTheta = sinTheta1;
	}
	
	glTexCoordPointer(2, GL_FLOAT, 0, tex);
	glVertexPointer(3, GL_FLOAT, 0, points);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, (nsides + 1) * rings * 2);	//3720 vectors
}

//trail of points
#define TRAIL_QTY    6
#define TRAIL_LENGTH 20
#define TRAIL_OD     9.2
#define TRAIL_ID     2
static float cardinal_offset = 0.0;
static float helical_offset = 0.0;
static GLfloat point_vertexes[TRAIL_QTY * 3 * TRAIL_LENGTH];
static GLfloat point_colors[TRAIL_QTY * 4 * TRAIL_LENGTH];
static int head_offset = 0;

static void draw_point_sprite_vertexes(void){
	
	//first decay ALL trail points, alpha is stored in 4th positions of color array
	for (int i = 3; i < TRAIL_QTY * 4 * TRAIL_LENGTH; i += 4) point_colors[i] *= 0.9;
	
	//overwrite the oldest point in each tail with the new head, render order doesnt matter
	head_offset = (head_offset + 1) % TRAIL_LENGTH; 
	float helical_index = 0;
	//do each head once
	for (int trail_head = 0; trail_head < TRAIL_QTY; trail_head++){
		
		//find the start of each trail, add the offset to get to the new head
		int vertex_index = trail_head * 3 * TRAIL_LENGTH + head_offset * 3;
		int color_index  = trail_head * 4 * TRAIL_LENGTH + head_offset * 4;	
		
		//find how far around the circle this head is
		float cardinal_index = (float)trail_head * (( 2 * M_PI )/((float)3));  //evenly space all heads
		
		//offset the position to get movement in a circle (controls cardinal speed)
		float cardinal_position = cardinal_index + cardinal_offset; 
			
		//how far to spin the helix
		helical_index =	helical_index + (( 2 * M_PI )*60.0/360.0);

		//offset the position to get helical movement (controls helical speed)
		float helical_position = helical_index + helical_offset; //position in radian
		
		//calculate helix coords
		float z_helix = cos(helical_position) * TRAIL_ID ; 
		float radius_helix = sin(helical_position) * TRAIL_ID; 
				
		///calculate the x y position of the head (trace a circle)
		float ypoint = sin(cardinal_position) * (TRAIL_OD + radius_helix); 
		float xpoint = cos(cardinal_position) * (TRAIL_OD + radius_helix); 
		
		point_vertexes[vertex_index+0] = xpoint;
		point_vertexes[vertex_index+1] = ypoint;
		point_vertexes[vertex_index+2] = z_helix;
		
		//clip at Z of 0
		point_colors[color_index+3] = (z_helix > 0) ? 1.0 : 0.0;
		
	}
	cardinal_offset -= .03;
	helical_offset -= .1;
	//draw the items
	
	glColorPointer(4, GL_FLOAT, 0, point_colors);
	glVertexPointer(3, GL_FLOAT, 0, point_vertexes);
	glDrawArrays(GL_POINTS,0,TRAIL_LENGTH * TRAIL_QTY);	
}


	
void scene_animate(int acceleration[], int frame){
	
	static int frame_previous = -1;
	static int running_acceleration[2] = {0,0};
	
	//decay the torus offset
	for (int i = 0; i <  360; i ++){
		torus_offset[i] *= .9;
	}
	
	int relative_acceleration[2];

	float alpha = .5;
	
	//filter acceleration
	for (int i = 0; i < 2; i++){
		
		relative_acceleration[i] = (last_acceleration[i] - acceleration[i]);

		//decay values
		//running_acceleration[i] *= .95;
		
		//ignore small disturbances
		if (abs(relative_acceleration[i] ) < 2 && abs(running_acceleration[i]) < 2  ){
			relative_acceleration[i] = 0;
		}
		
		//if ((relative_acceleration[i] >= 0 && running_acceleration[i] >= 0 )|| (relative_acceleration[0] <= 0 && running_acceleration[i]<= 0 )){
		//	running_acceleration[i] = running_acceleration[i] * alpha + (1-alpha)*relative_acceleration[i];
		//}
		running_acceleration[i] = running_acceleration[i] * alpha + (1-alpha)*relative_acceleration[i];
	}

	//scale magnitude to 0 to 1
	running_magnitude *= .95;
	float temp_mag = sqrt( running_acceleration[0]  * running_acceleration[0]  + running_acceleration[1] * running_acceleration[1]);
	if (temp_mag > running_magnitude){
		running_magnitude = running_magnitude * .5 + .5 *temp_mag;
		
	}
	
	angle_target =  atan2(-running_acceleration[1],-running_acceleration[0] ) ;

	angle_target_delayed = angle_target_delayed * .5 + angle_target * .5;
	
	//printf( "%f %f\n", running_magnitude,angle_target);
	
	//spins the portal rim (uncontrolled)
	portal_spin -= .50;
	if (portal_spin < 0) portal_spin += 360;	
	
	//slowly spins the portal background (uncontrolled)
	event_horizon_spin -= .01;
	if (event_horizon_spin < 360) event_horizon_spin += 360;

	//slowly shimmers the portal background (uncontrolled)
	if (event_horizon_vertex_list_shimmer > .99) event_horizon_vertex_list_shimmer_direction = false;
	if (event_horizon_vertex_list_shimmer < .25) event_horizon_vertex_list_shimmer_direction = true;
	if (event_horizon_vertex_list_shimmer_direction)	event_horizon_vertex_list_shimmer += .01;
	else												event_horizon_vertex_list_shimmer -= .01;

	if (frame == PORTAL_OPEN_BLUE || frame == PORTAL_CLOSED_BLUE){
		if (frame_previous != PORTAL_OPEN_BLUE && frame_previous != PORTAL_CLOSED_BLUE ){
			global_zoom = 0.0;
			event_horizon_transparency_level = 1.0; //close portal for a moment on rapid color change
		} 
	}else if (frame == PORTAL_OPEN_ORANGE || frame == PORTAL_CLOSED_ORANGE){
		if (frame_previous != PORTAL_OPEN_ORANGE  && frame_previous != PORTAL_CLOSED_ORANGE ){
			global_zoom = 0.0;
			event_horizon_transparency_level = 1.0;  //close portal for a moment on rapid color change
		} 
	}

	//this controls global zoom (0 is blanked)
	if (frame == PORTAL_CLOSED){
		//turn off display
		global_zoom = 0.0;
	}else{
		//turn on display
		if (global_zoom == 0.0){
			global_zoom = 0.01; //bump global_zoom from it's safe spot
		}
	}
	
	frame_previous = frame;
	//let blank fader fall to normal size
	if( global_zoom  > 0 && global_zoom < 1.0){
		global_zoom += 0.05 ;
		if ( global_zoom > 1.0 ) global_zoom = 1.0;
	}
	
	//this controls making the portal fade into the backgroud video
	if (frame == PORTAL_CLOSED_ORANGE || frame == PORTAL_CLOSED_BLUE){
		event_horizon_transparency_level = 1.0;
	}else{
		//wait for zoom to finish before fading to background
		if (event_horizon_transparency_level == 1.0 && global_zoom >= 0.5){ //wait for portal to be open a bit before unfading
			event_horizon_transparency_level = .99; //bump the transparency and let it fall the rest of the way
		}
	}
	
	//let transparency fall to maximum
	if (event_horizon_transparency_level > 0.0 && event_horizon_transparency_level < 1.0 ){
		event_horizon_transparency_level = event_horizon_transparency_level - 0.01;
		if (event_horizon_transparency_level < 0) event_horizon_transparency_level = 0;
	}
	

	last_acceleration[0] = acceleration[0];
	last_acceleration[1] = acceleration[1];
}

void scene_init(void)
{
	//set all points to be white, let the texture through
	for (uint16_t i = 0; i < (sizeof(point_colors)/sizeof(point_colors[0])); i++) point_colors[i] = 1.0;

	//initial fill to snap back
	for (int i = 0; i < 360; i ++)	torus_offset[i] = 3;
	
	orange_n = png_load("/home/pi/portal/gstvideo/assets/orange_n.png", NULL, NULL);
	//override default of clamp
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	
	blue_n = png_load("/home/pi/portal/gstvideo/assets/blue_n.png", NULL, NULL);
	//override default of clamp
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	
	orange_0 = png_load("/home/pi/portal/gstvideo/assets/orange_0.png", NULL, NULL);
	orange_1 = png_load("/home/pi/portal/gstvideo/assets/orange_1.png", NULL, NULL);

	blue_0 = png_load("/home/pi/portal/gstvideo/assets/blue_0.png", NULL, NULL);
	blue_1 = png_load("/home/pi/portal/gstvideo/assets/blue_1.png", NULL, NULL);
	
	texture_orange = png_load("/home/pi/portal/gstvideo/assets/orange_portal.png", NULL, NULL);
	texture_blue   = png_load("/home/pi/portal/gstvideo/assets/blue_portal.png",   NULL, NULL);
	
	circle64 = png_load("/home/pi/portal/gstvideo/assets/circle64.png", NULL, NULL);
	
	if (orange_n == 0 || blue_n == 0 || texture_orange == 0 || texture_blue == 0 || orange_0 == 0 || orange_1 == 0 || blue_0 == 0 || blue_1 == 0 || circle64 == 0)
	{
		printf("Loading textures failed.\n");
		exit(1);
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND); 
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
	
	//enable texturing
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE0);	
	glActiveTexture(GL_TEXTURE0);
	
	//setup point sprites
	glTexEnvi(GL_POINT_SPRITE_ARB, GL_COORD_REPLACE_ARB, GL_TRUE);
	
	//check min and max point size	
	//GLfloat sizes[2];
	//glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, sizes);
	//glPointParameterfARB( GL_POINT_SIZE_MAX_ARB, sizes[1]);
	//glPointParameterfARB( GL_POINT_SIZE_MIN_ARB, sizes[0]);
	//printf("Min point size: %f  Max Point size:  %f \n ",sizes[0] ,sizes[1] );

	//setup automatic point size changing based on Z distance(not used currently)
	//float quadratic[] = { 1.0f, 0.0f, 0.01f };
	//glPointParameterfvARB( GL_POINT_DISTANCE_ATTENUATION_ARB, quadratic );
	
	video_quad_vertex_list = glGenLists( 1 );
	glNewList( video_quad_vertex_list, GL_COMPILE );
	#define VIDEO_DEPTH -0.003
	#define VIDEO_SCALE 13.8  
	glBegin( GL_QUADS ); // 1.4 is slightly over 4/3 4:3 ratio 640/480 
	glTexCoord2f( 0.0f, 0.0f );	glVertex3f(  VIDEO_SCALE * 1.4,-VIDEO_SCALE,VIDEO_DEPTH);//top left
	glTexCoord2f( 0.0f, 1.0f ); glVertex3f(  VIDEO_SCALE * 1.4, VIDEO_SCALE,VIDEO_DEPTH);//bottom left
	glTexCoord2f( 1.0f, 1.0f );	glVertex3f( -VIDEO_SCALE * 1.4, VIDEO_SCALE,VIDEO_DEPTH);//bottom right
	glTexCoord2f( 1.0f, 0.0f );	glVertex3f( -VIDEO_SCALE * 1.4,-VIDEO_SCALE,VIDEO_DEPTH);//top right
	
	glEnd();
	glEndList();

	event_horizon_vertex_list = glGenLists( 1 );
	glNewList( event_horizon_vertex_list, GL_COMPILE );
	#define EVENT_HORIZON_DEPTH -0.002
	#define EVENT_HORIZON_SCALE 11
	glBegin( GL_QUADS );
	glTexCoord2f( 0.0f, 0.0f );	glVertex3f( -EVENT_HORIZON_SCALE, EVENT_HORIZON_SCALE,EVENT_HORIZON_DEPTH);//top left
	glTexCoord2f( 0.0f, 1.0f ); glVertex3f( -EVENT_HORIZON_SCALE,-EVENT_HORIZON_SCALE,EVENT_HORIZON_DEPTH);//bottom left
	glTexCoord2f( 1.0f, 1.0f );	glVertex3f(  EVENT_HORIZON_SCALE,-EVENT_HORIZON_SCALE,EVENT_HORIZON_DEPTH);//bottom right
	glTexCoord2f( 1.0f, 0.0f );	glVertex3f(  EVENT_HORIZON_SCALE, EVENT_HORIZON_SCALE,EVENT_HORIZON_DEPTH);//top right
	glEnd( );
	glEndList();
	
	event_horizon2_vertex_list = glGenLists( 1 );
	glNewList( event_horizon2_vertex_list, GL_COMPILE );
	#define EVENT_HORIZON2_DEPTH -0.001
	#define EVENT_HORIZON2_SCALE 11
	glBegin( GL_QUADS );
	glTexCoord2f( 0.0f, 0.0f );	glVertex3f( -EVENT_HORIZON2_SCALE, EVENT_HORIZON2_SCALE,EVENT_HORIZON2_DEPTH);//top left
	glTexCoord2f( 0.0f, 1.0f ); glVertex3f( -EVENT_HORIZON2_SCALE,-EVENT_HORIZON2_SCALE,EVENT_HORIZON2_DEPTH);//bottom left
	glTexCoord2f( 1.0f, 1.0f );	glVertex3f(  EVENT_HORIZON2_SCALE,-EVENT_HORIZON2_SCALE,EVENT_HORIZON2_DEPTH);//bottom right
	glTexCoord2f( 1.0f, 0.0f );	glVertex3f(  EVENT_HORIZON2_SCALE, EVENT_HORIZON2_SCALE,EVENT_HORIZON2_DEPTH);//top right
	glEnd( );
	glEndList();
	
	portal_vertex_list = glGenLists( 1 );
	glNewList( portal_vertex_list, GL_COMPILE );
	#define PORTAL_DEPTH 0
	#define PORTAL_SCALE 28.5
	glBegin( GL_QUADS );
	glTexCoord2f( 0.0f, 0.0f );	glVertex3f( -PORTAL_SCALE, PORTAL_SCALE,PORTAL_DEPTH);//top left
	glTexCoord2f( 0.0f, 1.0f ); glVertex3f( -PORTAL_SCALE,-PORTAL_SCALE,PORTAL_DEPTH);//bottom left
	glTexCoord2f( 1.0f, 1.0f ); glVertex3f(  PORTAL_SCALE,-PORTAL_SCALE,PORTAL_DEPTH);//bottom right
	glTexCoord2f( 1.0f, 0.0f );	glVertex3f(  PORTAL_SCALE, PORTAL_SCALE,PORTAL_DEPTH);//top right
	glEnd( );
	glEndList();
	
	shutter_vertex_list = glGenLists( 1 );
	glNewList( shutter_vertex_list, GL_COMPILE );
	#define SHUTTER_DEPTH 28
	#define SHUTTER_SCALE 2.0
	glBegin( GL_QUADS );
	glVertex3f( -SHUTTER_SCALE, SHUTTER_SCALE,SHUTTER_DEPTH);//top left
	glVertex3f( -SHUTTER_SCALE,-SHUTTER_SCALE,SHUTTER_DEPTH);//bottom left
	glVertex3f(  SHUTTER_SCALE,-SHUTTER_SCALE,SHUTTER_DEPTH);//bottom right
	glVertex3f(  SHUTTER_SCALE, SHUTTER_SCALE,SHUTTER_DEPTH);//top right
	glEnd( );
	glEndList();
	
}

void scene_redraw(GLuint video_texture, int frame){	

	//RESTART - CHECK IF I NEED TO SET ALL OF THESE EACH CYCLE!
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	

	glPushMatrix();
	
	glTranslatef(-22.13, 0, -30);
	
	//global scale
	glScalef(global_zoom,global_zoom,1.0);   //maybe use a Z zoom?
	
	//VIDEO QUAD
	glBindTexture (GL_TEXTURE_2D, video_texture); //video frame from gstreamer
	glColor4f(1.0,1.0,1.0,1.0); //video is not transparent at all
	glCallList(video_quad_vertex_list);
	
	//EVENT HORIZON QUAD
	glPushMatrix(); //save positions pre-rotation
	
	glScalef(1.0*1366/768,1,1.0);  //stretch portal to an oval
	
	//base event horizon texture
	if      (frame == PORTAL_OPEN_BLUE   || frame == PORTAL_CLOSED_BLUE)   glBindTexture(GL_TEXTURE_2D, blue_0);
	else if (frame == PORTAL_OPEN_ORANGE || frame == PORTAL_CLOSED_ORANGE) glBindTexture(GL_TEXTURE_2D, orange_0);		
	glColor4f(1.0,1.0,1.0,MIN(1.0,event_horizon_transparency_level)); //not transparent until forced open
	glRotatef(event_horizon_spin, 0, 0,1.0); //rotate background	
	glCallList(event_horizon_vertex_list);

	//base event horizon texture
	if      (frame == PORTAL_OPEN_BLUE   || frame == PORTAL_CLOSED_BLUE)   glBindTexture(GL_TEXTURE_2D, blue_1);
	else if (frame == PORTAL_OPEN_ORANGE || frame == PORTAL_CLOSED_ORANGE) glBindTexture(GL_TEXTURE_2D, orange_1);	
	glColor4f(1.0,1.0,1.0, MIN( event_horizon_vertex_list_shimmer ,event_horizon_transparency_level)); //shimmer transparency until forced open
	glRotatef(event_horizon_spin, 0, 0,1.0); //rotate background more
	glCallList(event_horizon2_vertex_list);
	
	glPopMatrix(); //un-rotate and unstretch

	//PORTAL RIM QUAD
	glPushMatrix(); //save positions pre-rotation and scaling
	
	//portal texture
	if      (frame == PORTAL_OPEN_BLUE   || frame == PORTAL_CLOSED_BLUE)   glBindTexture(GL_TEXTURE_2D, texture_blue);
	else if (frame == PORTAL_OPEN_ORANGE || frame == PORTAL_CLOSED_ORANGE) glBindTexture(GL_TEXTURE_2D, texture_orange);

	glColor4f(1.0,1.0,1.0,1.0); //portal texture is not transparent
	glScalef(1.0*1366/768,1,1.0);  //stretch portal to an oval
	glRotatef(portal_spin, 0, 0, 1); //make it spin
	glCallList(portal_vertex_list);
	

	//POINT SPRITES
	glDisable(GL_DEPTH_TEST);  //manual depth check
	glBindTexture(GL_TEXTURE_2D, circle64);
	glPointSize(20.0f);
	glEnable(GL_POINT_SPRITE_ARB);
	
	glColor4f(1.0,1.0,1.0,1.0);
	
	glRotatef(-portal_spin*3, 0, 0, 1); //slow the spin
	glEnableClientState(GL_VERTEX_ARRAY);  
	glEnableClientState(GL_COLOR_ARRAY);
	draw_point_sprite_vertexes();
	//glDisableClientState(GL_VERTEX_ARRAY); //Leave on for Points!
	glDisableClientState(GL_COLOR_ARRAY);
	glDisable( GL_POINT_SPRITE_ARB );
	glEnable(GL_DEPTH_TEST);
	
	//TEXTURED TORUS 
	if      (frame == PORTAL_OPEN_BLUE   || frame == PORTAL_CLOSED_BLUE)   glBindTexture(GL_TEXTURE_2D, blue_n);
	else if (frame == PORTAL_OPEN_ORANGE || frame == PORTAL_CLOSED_ORANGE) glBindTexture(GL_TEXTURE_2D, orange_n);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	//draw_torus_vbo();
	donut_texture_scrolling+=.05;
	//glEnableClientState(GL_VERTEX_ARRAY);  
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	
	glPopMatrix(); //un-rotate and unscale the portal
	glPopMatrix(); //unscale the global
	
	//MAIN SHUTTER (for safety, ensures the lasers don't turn on) 
	glBindTexture(GL_TEXTURE_2D, 0); //no texture
	
	GLfloat shutter = 0.0; 
	if (frame == PORTAL_CLOSED)  shutter = 1.0;
	
	glColor4f(0.0,0.0,0.0,shutter); 
	glCallList(shutter_vertex_list);	
}
