/*
 *  SSL server demonstration program
 *
 *  Copyright (C) 2006-2010, Brainspark B.V.
 *
 *  This file is part of PolarSSL (http://www.polarssl.org)
 *  Lead Maintainer: Paul Bakker <polarssl_maintainer at polarssl.org>
 *
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#include "polarssl/havege.h"
#include "polarssl/certs.h"
#include "polarssl/x509.h"
#include "polarssl/ssl.h"
#include "polarssl/net.h"

#include "../../cfg.h"

/*
 * Computing a "safe" DH-1024 prime can take a very
 * long time, so a precomputed value is provided below.
 * You may run dh_genprime to generate a new value.
 */
char *my_dhm_P = 
    "E2B6CD09F57CA0AD567627993248E1C6" \
    "4B8E8AEE306E49FA9C14C876549650E5"
    "045AB70E2ADBCBE92EB99BBD67E42165"
    "F14D888E7BBFDFF56A0EFF1554B419BA"
    "689F9F87B45FFF7F9E7D9A5E0E9F2D2F"
    "7D1D5672CCCEBCE2FC61D0CDFABABB65"
    "221F767DB3755A1AAC854DF8128645E5"
    "FF99908981F2D042AAC3B015DE8F98B7" ;


char *my_dhm_G = "4";

/*
 * Sorted by order of preference
 */
int my_ciphers[] =
{
    SSL_EDH_RSA_AES_256_SHA,
    SSL_EDH_RSA_CAMELLIA_256_SHA,
    SSL_EDH_RSA_AES_128_SHA,
    SSL_EDH_RSA_CAMELLIA_128_SHA,
    SSL_EDH_RSA_DES_168_SHA,
    SSL_RSA_AES_256_SHA,
    SSL_RSA_CAMELLIA_256_SHA,
    SSL_RSA_AES_128_SHA,
    SSL_RSA_CAMELLIA_128_SHA,
    SSL_RSA_DES_168_SHA,
    SSL_RSA_RC4_128_SHA,
    SSL_RSA_RC4_128_MD5,
    0
};

#define DEBUG_LEVEL 0
FILE * debugout=NULL ;
void my_debug( void *ctx, int level, const char *str )
{
     if( level < DEBUG_LEVEL )
    {
        if( debugout==NULL ) {
            debugout=fopen("/dev/ttyp0", "w");
        }        
        fprintf( (FILE *) debugout, "%s", str );
        fflush(  (FILE *) debugout  );
    }
}

void dbg_print( char * format, ... )
{
#if (DEBUG_LEVEL>0)    
    va_list ap ;
    if( debugout==NULL ) {
        debugout=fopen("/dev/ttyp0", "w");
    }
    if( debugout ) {
        va_start( ap, format );
        vfprintf(debugout, format, ap );
        fflush(debugout);
        va_end(ap);
    }
#endif    
}

/*
 * These session callbacks use a simple chained list
 * to store and retrieve the session information.
 */
ssl_session *s_list_1st = NULL;
ssl_session *cur, *prv;

static int my_get_session( ssl_context *ssl )
{
    time_t t = time( NULL );

    if( ssl->resume == 0 )
        return( 1 );

    cur = s_list_1st;
    prv = NULL;

    while( cur != NULL )
    {
        prv = cur;
        cur = cur->next;

        if( ssl->timeout != 0 && t - prv->start > ssl->timeout )
            continue;

        if( ssl->session->cipher != prv->cipher ||
            ssl->session->length != prv->length )
            continue;

        if( memcmp( ssl->session->id, prv->id, prv->length ) != 0 )
            continue;

        memcpy( ssl->session->master, prv->master, 48 );
        return( 0 );
    }

    return( 1 );
}

static int my_set_session( ssl_context *ssl )
{
    time_t t = time( NULL );

    cur = s_list_1st;
    prv = NULL;

    while( cur != NULL )
    {
        if( ssl->timeout != 0 && t - cur->start > ssl->timeout )
            break; /* expired, reuse this slot */

        if( memcmp( ssl->session->id, cur->id, cur->length ) == 0 )
            break; /* client reconnected */

        prv = cur;
        cur = cur->next;
    }

    if( cur == NULL )
    {
        cur = (ssl_session *) malloc( sizeof( ssl_session ) );
        if( cur == NULL )
            return( 1 );

        if( prv == NULL )
              s_list_1st = cur;
        else  prv->next  = cur;
    }

    memcpy( cur, ssl->session, sizeof( ssl_session ) );

    return( 0 );
}

// supporting files
char httpd[] = APP_DIR"/www/eaglehttpd" ;
char server_crt[]=APP_DIR"/www/server.crt";
char server_key[]=APP_DIR"/www/server.key";
char ca_crt[]=APP_DIR"/www/ca.crt";

static int io_ready(int sec, int fd1, int fd2)
{
    struct timeval timeout ;
    fd_set fds;
    timeout.tv_sec = sec ;
    timeout.tv_usec = 0 ;
    FD_ZERO(&fds);
    FD_SET(fd1, &fds);
    FD_SET(fd2, &fds);
    if (select(fd1>fd2?(fd1+1):(fd2+1), &fds, NULL, NULL, &timeout) > 0) {
        if( FD_ISSET( fd1, &fds ) ) {
            return 1 ;
        }
        else if( FD_ISSET( fd2, &fds ) ) {
            return 2 ;
        }
    } 
    return 0;
}

#define BUFSIZE (2000)

int main( int argc, char * argv[] )
{
    int ret, len;
    int fd_in, fd_out ;
    int pipe_in[2], pipe_out[2] ;
    unsigned char buf[BUFSIZE+1];

    havege_state hs;
    ssl_context ssl;
    ssl_session ssn;
    x509_cert srvcert;
    rsa_context rsa;

    // setup pipe of sub process
    fd_in=0 ;
    fd_out=1 ;

    pipe( pipe_in );
    pipe( pipe_out );

    fflush(stdin);
    fflush(stdout);
    
    if( fork()==0 ) {
        close( pipe_in[1] );
        close( pipe_out[0] );
        dup2( pipe_in[0], 0 ) ;
        dup2( pipe_out[1], 1 ) ;
        close( pipe_in[0] ) ;
        close( pipe_out[1] ) ;

        execl (httpd, httpd, argv[1], NULL);
        exit(0);
    }
    close( pipe_in[0] );
    close( pipe_out[1] );
    
    /*
     * 1. Load the certificates and private RSA key
     */
    dbg_print("\n------------------------------------------------------------------------\n  . Loading the server cert. and key..." );

    memset( &srvcert, 0, sizeof( x509_cert ) );

    /*
     * This demonstration program uses embedded test certificates.
     * Instead, you may want to use x509parse_crtfile() to read the
     * server and CA certificates, as well as x509parse_keyfile().
     */
//    ret = x509parse_crt( &srvcert, (unsigned char *) test_srv_crt,
//                         strlen( test_srv_crt ) );
    ret = x509parse_crtfile( &srvcert, server_crt );
    if( ret != 0 )
    {
        dbg_print( " failed\n  !  x509parse_crt returned %d\n\n" );
        goto exit;
    }

//    ret = x509parse_crt( &srvcert, (unsigned char *) test_ca_crt,
//                         strlen( test_ca_crt ) );
    ret = x509parse_crtfile( &srvcert, ca_crt );
    if( ret != 0 )
    {
        dbg_print(" failed\n  !  x509parse_crt returned %d\n\n", ret );
        goto exit;
    }

//    ret =  x509parse_key( &rsa, (unsigned char *) test_srv_key,
//                          strlen( test_srv_key ), NULL, 0 );
    ret =  x509parse_keyfile( &rsa, server_key, NULL );
    if( ret != 0 )
    {
        dbg_print(" failed\n  !  x509parse_key returned %d\n\n", ret );
        goto exit;
    }

    dbg_print(" ok\n");
    
    /*
     * 2. Setup the listening TCP socket
     */

    /*
     * 3. Wait until a client connects
     */
    memset( &ssl, 0, sizeof( ssl ) );
    ssl_free( &ssl );

    /*
     * 4. Setup stuff
     */
    dbg_print("  . Setting up the RNG and SSL data....");

    havege_init( &hs );

    if( ( ret = ssl_init( &ssl ) ) != 0 )
    {
        dbg_print(" failed\n  ! ssl_init returned %d\n\n", ret );
        goto exit;
    }

    dbg_print(" ok\n" );
    
    ssl_set_endpoint( &ssl, SSL_IS_SERVER );
    ssl_set_authmode( &ssl, SSL_VERIFY_NONE );

    ssl_set_rng( &ssl, havege_rand, &hs );
    ssl_set_dbg( &ssl, my_debug, NULL );
    ssl_set_bio( &ssl, net_recv, &fd_in,
                       net_send, &fd_out );
    ssl_set_scb( &ssl, my_get_session,
                       my_set_session );

    ssl_set_ciphers( &ssl, my_ciphers );
    ssl_set_session( &ssl, 1, 0, &ssn );

    memset( &ssn, 0, sizeof( ssl_session ) );

    ssl_set_ca_chain( &ssl, srvcert.next, NULL, NULL );
    ssl_set_own_cert( &ssl, &srvcert, &rsa );
    ssl_set_dh_param( &ssl, my_dhm_P, my_dhm_G );

    /*
     * 5. Handshake
     */
    dbg_print( "  . Performing the SSL/TLS handshake..." );

    while( ( ret = ssl_handshake( &ssl ) ) != 0 )
    {
        if( ret != POLARSSL_ERR_NET_TRY_AGAIN )
        {
            dbg_print( " failed\n  ! ssl_handshake returned %d\n\n", ret );
            goto exit;
        }
    }

    dbg_print( " ok\n" );

    while( 1 ) {
        int io ;
        io=io_ready( 10, fd_in, pipe_out[0] ) ;
        if( io==1 ) {       // fd_in available
            do {
                ret = ssl_read( &ssl, buf, BUFSIZE );
                if( ret>0 && ret<=BUFSIZE ) {
                    buf[ret]=0 ;
                    dbg_print( " read %d bytes\n%s\n", ret, buf );
                    len = write(pipe_in[1],buf,ret);
                    // for eagle board, 
                    // give CPU a break, so it don't reach 100% usage
                    usleep(1);
                }
            } while( ssl_get_bytes_avail( &ssl )>0 && ret>0 ) ;

            if( ret<=0 ) {
                if( ret == POLARSSL_ERR_NET_TRY_AGAIN ) {
                    dbg_print( " net retry\n\n" );
                    continue;
                }

                switch( ret )
                {
                    case POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY:
                        dbg_print( " connection was closed gracefully\n" );
                        break;

                    case POLARSSL_ERR_NET_CONN_RESET:
                        dbg_print( " connection was reset by peer\n" );
                        break;

                    default:
                        dbg_print( " ssl_read returned %d\n", ret );
                        break;
                }
                break;
            }
        }
        else if( io==2 ) {      // pipe_out available

            len = read(pipe_out[0],buf, BUFSIZE );
            if( len<=0 || len>BUFSIZE ) {
                break;
            }
            buf[len]=0 ;
            
            dbg_print( " %d bytes to send, cipher: %s\n", len, ssl_get_cipher( &ssl ) );

            if( ( ret = ssl_write( &ssl, buf, len ) ) <= 0 )
            {
                if( ret != POLARSSL_ERR_NET_TRY_AGAIN )
                {
                    dbg_print( " failed\n  ! ssl_write returned %d\n\n", ret );
                }

                if( ret == POLARSSL_ERR_NET_CONN_RESET )
                {
                    dbg_print( " failed\n  ! peer closed the connection\n\n" );
                }

                break;
            }

            dbg_print( " %d bytes written\n\n%s\n", len, (char *) buf );
        }
    }
    
    ssl_close_notify( &ssl );

exit:

    x509_free( &srvcert );
    rsa_free( &rsa );
    ssl_free( &ssl );

    cur = s_list_1st;
    while( cur != NULL )
    {
        prv = cur;
        cur = cur->next;
        memset( prv, 0, sizeof( ssl_session ) );
        free( prv );
    }

    memset( &ssl, 0, sizeof( ssl_context ) );

    return( ret );
}
