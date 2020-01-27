// The MIT License (MIT)
//
// Copyright (c) 2013 Dan Ginsburg, Budirijanto Purnomo
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

//
// Book:      OpenGL(R) ES 3.0 Programming Guide, 2nd Edition
// Authors:   Dan Ginsburg, Budirijanto Purnomo, Dave Shreiner, Aaftab Munshi
// ISBN-10:   0-321-93388-5
// ISBN-13:   978-0-321-93388-1
// Publisher: Addison-Wesley Professional
// URLs:      http://www.opengles-book.com
//            http://my.safaribooksonline.com/book/animation-and-3d/9780133440133
//
// ESUtil.c
//
//    A utility library for OpenGL ES.  This library provides a
//    basic common framework for the example applications in the
//    OpenGL ES 3.0 Programming Guide.
//

///
//  Includes
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "esUtil.h"

typedef FILE esFile;

///
//  Macros
//
#define INVERTED_BIT            (1 << 5)

///
//  Types
//
#ifndef __APPLE__
#pragma pack(push,x1)                            // Byte alignment (8-bit)
#pragma pack(1)
#endif

typedef struct
#ifdef __APPLE__
__attribute__ ( ( packed ) )
#endif
{
   unsigned char  IdSize,
            MapType,
            ImageType;
   unsigned short PaletteStart,
            PaletteSize;
   unsigned char  PaletteEntryDepth;
   unsigned short X,
            Y,
            Width,
            Height;
   unsigned char  ColorDepth,
            Descriptor;

} TGA_HEADER;

#ifndef __APPLE__
#pragma pack(pop,x1)
#endif

#ifndef __APPLE__

///
// GetContextRenderableType()
//
//    Check whether EGL_KHR_create_context extension is supported.  If so,
//    return EGL_OPENGL_ES3_BIT_KHR instead of EGL_OPENGL_ES2_BIT
//
EGLint GetContextRenderableType ( EGLDisplay eglDisplay )
{
#ifdef EGL_KHR_create_context
   const char *extensions = eglQueryString ( eglDisplay, EGL_EXTENSIONS );

   // check whether EGL_KHR_create_context is in the extension string
   if ( extensions != NULL && strstr( extensions, "EGL_KHR_create_context" ) )
   {
      // extension is supported
      return EGL_OPENGL_ES3_BIT_KHR;
   }
#endif
   // extension is not supported
   return EGL_OPENGL_ES2_BIT;
}
#endif

//////////////////////////////////////////////////////////////////
//
//  Public Functions
//
//

///
//  esRegisterDrawFunc()
//
void ESUTIL_API esRegisterDrawFunc ( ESContext *esContext, void ( ESCALLBACK *drawFunc ) ( ESContext * ) )
{
   esContext->drawFunc = drawFunc;
}

///
//  esRegisterShutdownFunc()
//
void ESUTIL_API esRegisterShutdownFunc ( ESContext *esContext, void ( ESCALLBACK *shutdownFunc ) ( ESContext * ) )
{
   esContext->shutdownFunc = shutdownFunc;
}

///
//  esRegisterUpdateFunc()
//
void ESUTIL_API esRegisterUpdateFunc ( ESContext *esContext, void ( ESCALLBACK *updateFunc ) ( ESContext *, float ) )
{
   esContext->updateFunc = updateFunc;
}


///
//  esRegisterKeyFunc()
//
void ESUTIL_API esRegisterKeyFunc ( ESContext *esContext,
                                    void ( ESCALLBACK *keyFunc ) ( ESContext *, unsigned char, int, int ) )
{
   esContext->keyFunc = keyFunc;
}


///
// esLogMessage()
//
//    Log an error message to the debug output for the platform
//
void ESUTIL_API esLogMessage ( const char *formatStr, ... )
{
   va_list params;
   char buf[BUFSIZ];

   va_start ( params, formatStr );
   vsprintf ( buf, formatStr, params );

#ifdef ANDROID
   __android_log_print ( ANDROID_LOG_INFO, "esUtil" , "%s", buf );
#else
   printf ( "%s", buf );
#endif

   va_end ( params );
}

///
// esFileRead()
//
//    Wrapper for platform specific File open
//
static esFile *esFileOpen ( void *ioContext, const char *fileName )
{
   esFile *pFile = NULL;

#ifdef ANDROID

   if ( ioContext != NULL )
   {
      AAssetManager *assetManager = ( AAssetManager * ) ioContext;
      pFile = AAssetManager_open ( assetManager, fileName, AASSET_MODE_BUFFER );
   }

#else
#ifdef __APPLE__
   // iOS: Remap the filename to a path that can be opened from the bundle.
   fileName = GetBundleFileName ( fileName );
#endif

   pFile = fopen ( fileName, "rb" );
#endif

   return pFile;
}

///
// esFileRead()
//
//    Wrapper for platform specific File close
//
static void esFileClose ( esFile *pFile )
{
   if ( pFile != NULL )
   {
#ifdef ANDROID
      AAsset_close ( pFile );
#else
      fclose ( pFile );
      pFile = NULL;
#endif
   }
}

///
// esFileRead()
//
//    Wrapper for platform specific File read
//
static int esFileRead ( esFile *pFile, int bytesToRead, void *buffer )
{
   int bytesRead = 0;

   if ( pFile == NULL )
   {
      return bytesRead;
   }

#ifdef ANDROID
   bytesRead = AAsset_read ( pFile, buffer, bytesToRead );
#else
   bytesRead = fread ( buffer, bytesToRead, 1, pFile );
#endif

   return bytesRead;
}

///
// esLoadTGA()
//
//    Loads a 8-bit, 24-bit or 32-bit TGA image from a file
//
char *ESUTIL_API esLoadTGA ( void *ioContext, const char *fileName, int *width, int *height )
{
   char        *buffer;
   esFile      *fp;
   TGA_HEADER   Header;

   // Open the file for reading
   fp = esFileOpen ( ioContext, fileName );

   if ( fp == NULL )
   {
      // Log error as 'error in opening the input file from apk'
      esLogMessage ( "esLoadTGA FAILED to load : { %s }\n", fileName );
      return NULL;
   }

   esFileRead ( fp, sizeof ( TGA_HEADER ), &Header );

   *width = Header.Width;
   *height = Header.Height;

   if ( Header.ColorDepth == 8 ||
         Header.ColorDepth == 24 || Header.ColorDepth == 32 )
   {
      int bytesToRead = sizeof ( char ) * ( *width ) * ( *height ) * Header.ColorDepth / 8;

      // Allocate the image data buffer
      buffer = ( char * ) malloc ( bytesToRead );

      if ( buffer )
      {
         esFileRead ( fp, bytesToRead, buffer );
         esFileClose ( fp );

         return ( buffer );
      }
   }

   return ( NULL );
}
