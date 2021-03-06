#ifndef _PROJECTOR_H
#define _PROJECTOR_H

#include <stdint.h>

#define PORTAL_CLOSED			0b00000000  //DEC  0
#define PORTAL_OPEN_BLUE		0b00010011  //DEC 19
#define PORTAL_OPEN_ORANGE		0b00001011  //DEC 11
#define PORTAL_CLOSED_BLUE 		0b00010101  //DEC 21
#define PORTAL_CLOSED_ORANGE 	0b00001101  //DEC 13

#define PORTAL_OPEN_BIT			0b00000010
#define PORTAL_CLOSED_BIT		0b00000100
#define PORTAL_BLUE_BIT			0b00010000
#define PORTAL_ORANGE_BIT		0b00001000

#define PORTAL_FREE_BITS		0b11100000

#define GST_REPEAT -1

#define GST_FIRST 0

#define GST_BLANK 0
#define GST_VIDEOTESTSRC 1
#define GST_VIDEOTESTSRC_CUBED 2
#define GST_RPICAMSRC 3
#define GST_NORMAL 4

#define GST_LIBVISUAL_FIRST 7
#define GST_LIBVISUAL_WAVE 7
#define GST_LIBVISUAL_SYNAE 8
#define GST_LIBVISUAL_CORONA 9
#define GST_LIBVISUAL_JESS 10
#define GST_LIBVISUAL_INFINITE 11
#define GST_LIBVISUAL_JAKDAW 12
#define GST_LIBVISUAL_OINKSIE 13
#define GST_GOOM 14
#define GST_GOOM2K1 15
#define GST_LIBVISUAL_LAST 15

#define GST_FIRST_EFFECT 16
#define GST_FIRST_TV_EFFECT 16
#define GST_MARBLE 16
#define GST_KALEDIOSCOPE 17
#define GST_STREAKTV 18
#define GST_RADIOACTV 19
#define GST_REVTV 20
#define GST_AGINGTV 21
#define GST_DICETV 22
#define GST_WARPTV 23
#define GST_SHAGADELICTV 24
#define GST_VERTIGOTV 25
#define GST_AATV 26
#define GST_CACATV 27
#define GST_RIPPLETV 28
#define GST_EDGETV 29
#define GST_LAST_TV_EFFECT 29

#define GST_FIRST_GL_EFFECT 30
#define GST_GLCUBE 30
#define GST_GLMIRROR 31
#define GST_GLSQUEEZE 32
#define GST_GLSTRETCH 33
#define GST_GLTUNNEL 34
#define GST_GLTWIRL 35
#define GST_GLBULGE 36
#define GST_GLHEAT 37
#define GST_LAST_GL_EFFECT 37
#define GST_LAST_EFFECT 37

#define GST_MOVIE_FIRST 50
#define GST_MOVIE1 50
#define GST_MOVIE2 51
#define GST_MOVIE3 52
#define GST_MOVIE4 53
#define GST_MOVIE5 54
#define GST_MOVIE6 55
#define GST_MOVIE7 56
#define GST_MOVIE8 57
#define GST_MOVIE9 58
#define GST_MOVIE10 59
#define GST_MOVIE11 60
#define GST_MOVIE12 61
#define GST_MOVIE_LAST 61

#define GST_LAST 61

#define SERVO_BYPASS	0
#define SERVO_NORMAL	1
#define SERVO_FANCY		2

static char *const effectnames[GST_MOVIE_LAST+1] = {
    [GST_BLANK] = "Blank",
    [GST_VIDEOTESTSRC] = "Test",
    [GST_VIDEOTESTSRC_CUBED] = "3D Test",
    [GST_RPICAMSRC] = "Rpicam",
    [GST_NORMAL] = "Normal",

    [GST_LIBVISUAL_WAVE] = "Wave",
    [GST_LIBVISUAL_SYNAE] = "Synae",
    [GST_LIBVISUAL_CORONA] = "Corona",
    [GST_LIBVISUAL_JESS] = "Jess",
    [GST_LIBVISUAL_INFINITE] = "Infinite",
    [GST_LIBVISUAL_JAKDAW] = "Jakdaw",
    [GST_LIBVISUAL_OINKSIE] = "Oinksie",
    [GST_GOOM] = "Goom",
    [GST_GOOM2K1] = "Goom2k1",

    [GST_MARBLE] = "Marble",
    [GST_KALEDIOSCOPE] = "Kaledio",
    [GST_STREAKTV] = "Streak",
    [GST_RADIOACTV] = "Radioact",
    [GST_REVTV] = "Rev",
    [GST_AGINGTV] = "Aging",
    [GST_DICETV] = "Dice",
    [GST_WARPTV] = "Warp",
    [GST_SHAGADELICTV] = "Shag",
    [GST_VERTIGOTV] = "Vertigo",
    [GST_AATV] = "Ascii",
    [GST_CACATV] = "Caca",
    [GST_RIPPLETV] = "Ripple",
    [GST_EDGETV] = "Edge",

    [GST_GLCUBE] = "Cube",
    [GST_GLMIRROR] = "Mirror",
    [GST_GLSQUEEZE] = "Squeeze",
    [GST_GLSTRETCH] = "Stretch",
    [GST_GLTUNNEL] = "Tunnel",
    [GST_GLTWIRL] = "Twirl",
    [GST_GLBULGE] = "Buldge",
    [GST_GLHEAT] = "Heat",

    [GST_MOVIE1] = "Movie1",
    [GST_MOVIE2] = "Movie2",
    [GST_MOVIE3] = "Movie3",
    [GST_MOVIE4] = "Movie4",
    [GST_MOVIE5] = "Movie5",
    [GST_MOVIE6] = "Movie6",
    [GST_MOVIE7] = "Movie7",
    [GST_MOVIE8] = "Movie8",
    [GST_MOVIE9] = "Movie9",
    [GST_MOVIE10] = "Movie10",
    [GST_MOVIE11] = "Movie11",
    [GST_MOVIE12] = "Movie12"
};

#endif