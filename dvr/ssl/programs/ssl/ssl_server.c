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

#ifdef WIN32
#include <windows.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "polarssl/config.h"

#include "polarssl/havege.h"
#include "polarssl/certs.h"
#include "polarssl/x509.h"
#include "polarssl/ssl.h"
#include "polarssl/net.h"

#define HTTP_RESPONSE \
    "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n" \
    "<h2>PolarSSL Test Server</h2>\r\n" \
    "<p>Successful connection using: %s</p>\r\n"

const char my_ca_crt[] =
"-----BEGIN CERTIFICATE-----\r\n"
"MIIDhzCCAm+gAwIBAgIBADANBgkqhkiG9w0BAQUFADA7MQswCQYDVQQGEwJOTDER\r\n"
"MA8GA1UEChMIUG9sYXJTU0wxGTAXBgNVBAMTEFBvbGFyU1NMIFRlc3QgQ0EwHhcN\r\n"
"MTEwMjEyMTQ0NDAwWhcNMjEwMjEyMTQ0NDAwWjA7MQswCQYDVQQGEwJOTDERMA8G\r\n"
"A1UEChMIUG9sYXJTU0wxGTAXBgNVBAMTEFBvbGFyU1NMIFRlc3QgQ0EwggEiMA0G\r\n"
"CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDA3zf8F7vglp0/ht6WMn1EpRagzSHx\r\n"
"mdTs6st8GFgIlKXsm8WL3xoemTiZhx57wI053zhdcHgH057Zk+i5clHFzqMwUqny\r\n"
"50BwFMtEonILwuVA+T7lpg6z+exKY8C4KQB0nFc7qKUEkHHxvYPZP9al4jwqj+8n\r\n"
"YMPGn8u67GB9t+aEMr5P+1gmIgNb1LTV+/Xjli5wwOQuvfwu7uJBVcA0Ln0kcmnL\r\n"
"R7EUQIN9Z/SG9jGr8XmksrUuEvmEF/Bibyc+E1ixVA0hmnM3oTDPb5Lc9un8rNsu\r\n"
"KNF+AksjoBXyOGVkCeoMbo4bF6BxyLObyavpw/LPh5aPgAIynplYb6LVAgMBAAGj\r\n"
"gZUwgZIwDAYDVR0TBAUwAwEB/zAdBgNVHQ4EFgQUtFrkpbPe0lL2udWmlQ/rPrzH\r\n"
"/f8wYwYDVR0jBFwwWoAUtFrkpbPe0lL2udWmlQ/rPrzH/f+hP6Q9MDsxCzAJBgNV\r\n"
"BAYTAk5MMREwDwYDVQQKEwhQb2xhclNTTDEZMBcGA1UEAxMQUG9sYXJTU0wgVGVz\r\n"
"dCBDQYIBADANBgkqhkiG9w0BAQUFAAOCAQEAuP1U2ABUkIslsCfdlc2i94QHHYeJ\r\n"
"SsR4EdgHtdciUI5I62J6Mom+Y0dT/7a+8S6MVMCZP6C5NyNyXw1GWY/YR82XTJ8H\r\n"
"DBJiCTok5DbZ6SzaONBzdWHXwWwmi5vg1dxn7YxrM9d0IjxM27WNKs4sDQhZBQkF\r\n"
"pjmfs2cb4oPl4Y9T9meTx/lvdkRYEug61Jfn6cA+qHpyPYdTH+UshITnmp5/Ztkf\r\n"
"m/UTSLBNFNHesiTZeH31NcxYGdHSme9Nc/gfidRa0FLOCfWxRlFqAI47zG9jAQCZ\r\n"
"7Z2mCGDNMhjQc+BYcdnl0lPXjdDK6V0qCg1dVewhUBcW5gZKzV7e9+DpVA==\r\n"
"-----END CERTIFICATE-----\r\n";

const char my_ca_key[] =
"-----BEGIN RSA PRIVATE KEY-----\r\n"
"Proc-Type: 4,ENCRYPTED\r\n"
"DEK-Info: DES-EDE3-CBC,A8A95B05D5B7206B\r\n"
"\r\n"
"9Qd9GeArejl1GDVh2lLV1bHt0cPtfbh5h/5zVpAVaFpqtSPMrElp50Rntn9et+JA\r\n"
"7VOyboR+Iy2t/HU4WvA687k3Bppe9GwKHjHhtl//8xFKwZr3Xb5yO5JUP8AUctQq\r\n"
"Nb8CLlZyuUC+52REAAthdWgsX+7dJO4yabzUcQ22Tp9JSD0hiL43BlkWYUNK3dAo\r\n"
"PZlmiptjnzVTjg1MxsBSydZinWOLBV8/JQgxSPo2yD4uEfig28qbvQ2wNIn0pnAb\r\n"
"GxnSAOazkongEGfvcjIIs+LZN9gXFhxcOh6kc4Q/c99B7QWETwLLkYgZ+z1a9VY9\r\n"
"gEU7CwCxYCD+h9hY6FPmsK0/lC4O7aeRKpYq00rPPxs6i7phiexg6ax6yTMmArQq\r\n"
"QmK3TAsJm8V/J5AWpLEV6jAFgRGymGGHnof0DXzVWZidrcZJWTNuGEX90nB3ee2w\r\n"
"PXJEFWKoD3K3aFcSLdHYr3mLGxP7H9ThQai9VsycxZKS5kwvBKQ//YMrmFfwPk8x\r\n"
"vTeY4KZMaUrveEel5tWZC94RSMKgxR6cyE1nBXyTQnDOGbfpNNgBKxyKbINWoOJU\r\n"
"WJZAwlsQn+QzCDwpri7+sV1mS3gBE6UY7aQmnmiiaC2V3Hbphxct/en5QsfDOt1X\r\n"
"JczSfpRWLlbPznZg8OQh/VgCMA58N5DjOzTIK7sJJ5r+94ZBTCpgAMbF588f0NTR\r\n"
"KCe4yrxGJR7X02M4nvD4IwOlpsQ8xQxZtOSgXv4LkxvdU9XJJKWZ/XNKJeWztxSe\r\n"
"Z1vdTc2YfsDBA2SEv33vxHx2g1vqtw8SjDRT2RaQSS0QuSaMJimdOX6mTOCBKk1J\r\n"
"9Q5mXTrER+/LnK0jEmXsBXWA5bqqVZIyahXSx4VYZ7l7w/PHiUDtDgyRhMMKi4n2\r\n"
"iQvQcWSQTjrpnlJbca1/DkpRt3YwrvJwdqb8asZU2VrNETh5x0QVefDRLFiVpif/\r\n"
"tUaeAe/P1F8OkS7OIZDs1SUbv/sD2vMbhNkUoCms3/PvNtdnvgL4F0zhaDpKCmlT\r\n"
"P8vx49E7v5CyRNmED9zZg4o3wmMqrQO93PtTug3Eu9oVx1zPQM1NVMyBa2+f29DL\r\n"
"1nuTCeXdo9+ni45xx+jAI4DCwrRdhJ9uzZyC6962H37H6D+5naNvClFR1s6li1Gb\r\n"
"nqPoiy/OBsEx9CaDGcqQBp5Wme/3XW+6z1ISOx+igwNTVCT14mHdBMbya0eIKft5\r\n"
"X+GnwtgEMyCYyyWuUct8g4RzErcY9+yW9Om5Hzpx4zOuW4NPZgPDTgK+t2RSL/Yq\r\n"
"rE1njrgeGYcVeG3f+OftH4s6fPbq7t1A5ZgUscbLMBqr9tK+OqygR4EgKBPsH6Cz\r\n"
"L6zlv/2RV0qAHvVuDJcIDIgwY5rJtINEm32rhOeFNJwZS5MNIC1czXZx5//ugX7l\r\n"
"I4sy5nbVhwSjtAk8Xg5dZbdTZ6mIrb7xqH+fdakZor1khG7bC2uIwibD3cSl2XkR\r\n"
"wN48lslbHnqqagr6Xm1nNOSVl8C/6kbJEsMpLhAezfRtGwvOucoaE+WbeUNolGde\r\n"
"P/eQiddSf0brnpiLJRh7qZrl9XuqYdpUqnoEdMAfotDOID8OtV7gt8a48ad8VPW2\r\n"
"-----END RSA PRIVATE KEY-----\r\n";

const char my_srv_crt[] =
"-----BEGIN CERTIFICATE-----\r\n"
"MIIDPzCCAiegAwIBAgIBATANBgkqhkiG9w0BAQUFADA7MQswCQYDVQQGEwJOTDER\r\n"
"MA8GA1UEChMIUG9sYXJTU0wxGTAXBgNVBAMTEFBvbGFyU1NMIFRlc3QgQ0EwHhcN\r\n"
"MTEwMjEyMTQ0NDA2WhcNMjEwMjEyMTQ0NDA2WjA8MQswCQYDVQQGEwJOTDERMA8G\r\n"
"A1UEChMIUG9sYXJTU0wxGjAYBgNVBAMTEVBvbGFyU1NMIFNlcnZlciAxMIIBIjAN\r\n"
"BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAqQIfPUBq1VVTi/027oJlLhVhXom/\r\n"
"uOhFkNvuiBZS0/FDUEeWEllkh2v9K+BG+XO+3c+S4ZFb7Wagb4kpeUWA0INq1UFD\r\n"
"d185fAkER4KwVzlw7aPsFRkeqDMIR8EFQqn9TMO0390GH00QUUBncxMPQPhtgSVf\r\n"
"CrFTxjB+FTms+Vruf5KepgVb5xOXhbUjktnUJAbVCSWJdQfdphqPPwkZvq1lLGTr\r\n"
"lZvc/kFeF6babFtpzAK6FCwWJJxK3M3Q91Jnc/EtoCP9fvQxyi1wyokLBNsupk9w\r\n"
"bp7OvViJ4lNZnm5akmXiiD8MlBmj3eXonZUT7Snbq3AS3FrKaxerUoJUsQIDAQAB\r\n"
"o00wSzAJBgNVHRMEAjAAMB0GA1UdDgQWBBQfdNY/KcF0dEU7BRIsPai9Q1kCpjAf\r\n"
"BgNVHSMEGDAWgBS0WuSls97SUva51aaVD+s+vMf9/zANBgkqhkiG9w0BAQUFAAOC\r\n"
"AQEAvc+WwZUemsJu2IiI2Cp6liA+UAvIx98dQe3kZs2zAoF9VwQbXcYzWQ/BILkj\r\n"
"NImKbPL9x0g2jIDn4ZvGYFywMwIO/d++YbwYiQw42/v7RiMy94zBPnzeHi86dy/0\r\n"
"jpOOJUx3IXRsGLdyjb/1T11klcFqGnARiK+8VYolMPP6afKvLXX7K4kiUpsFQhUp\r\n"
"E5VeM5pV1Mci2ETOJau2cO40FJvI/C9W/wR+GAArMaw2fxG77E3laaa0LAOlexM6\r\n"
"A4KOb5f5cGTM5Ih6tEF5FVq3/9vzNIYMa1FqzacBLZF8zSHYLEimXBdzjBoN4qDU\r\n"
"/WzRyYRBRjAI49mzHX6raleqnw==\r\n"
"-----END CERTIFICATE-----\r\n";

const char my_srv_key[] =
"-----BEGIN RSA PRIVATE KEY-----\r\n"
"MIIEogIBAAKCAQEAqQIfPUBq1VVTi/027oJlLhVhXom/uOhFkNvuiBZS0/FDUEeW\r\n"
"Ellkh2v9K+BG+XO+3c+S4ZFb7Wagb4kpeUWA0INq1UFDd185fAkER4KwVzlw7aPs\r\n"
"FRkeqDMIR8EFQqn9TMO0390GH00QUUBncxMPQPhtgSVfCrFTxjB+FTms+Vruf5Ke\r\n"
"pgVb5xOXhbUjktnUJAbVCSWJdQfdphqPPwkZvq1lLGTrlZvc/kFeF6babFtpzAK6\r\n"
"FCwWJJxK3M3Q91Jnc/EtoCP9fvQxyi1wyokLBNsupk9wbp7OvViJ4lNZnm5akmXi\r\n"
"iD8MlBmj3eXonZUT7Snbq3AS3FrKaxerUoJUsQIDAQABAoIBABaJ9eiRQq4Ypv+w\r\n"
"UTcVpLC0oTueWzcpor1i1zjG4Vzqe/Ok2FqyGToGKMlFK7Hwwa+LEyeJ3xyV5yd4\r\n"
"v1Mw9bDZFdJC1eCBjoUAHtX6k9HOE0Vd6woVQ4Vi6OPI1g7B5Mnr/58rNrnN6TMs\r\n"
"x58NF6euecwTU811QJrZtLbX7j2Cr28yB2Vs8qyYlHwVw5jbDOv43D7vU5gmlIDN\r\n"
"0JQRuWAnOuPzZNoJr4SfJKqHNGxYYY6pHZ1s0dOTLIDb/B8KQWapA2kRmZyid2EH\r\n"
"nwzgLbAsHJCf+bQnhXjXuxtUsrcIL8noZLazlOMxwNEammglVWW23Ud/QRnFgJg5\r\n"
"UgcAcRECgYEA19uYetht5qmwdJ+12oC6zeO+vXLcyD9gon23T5J6w2YThld7/OW0\r\n"
"oArQJGgkAdaq0pcTyOIjtTQVMFygdVmCEJmxh/3RutPcTeydqW9fphKDMej32J8e\r\n"
"GniGmNGiclbcfNOS8E5TGp445yZb9P1+7AHng16bGg3Ykj5EA4G+HCcCgYEAyHAl\r\n"
"//ekk8YjQElm+8izLtFkymIK0aCtEe9C/RIRhFYBeFaotC5dStNhBOncn4ovMAPD\r\n"
"lX/92yDi9OP8PPLN3a4B9XpW3k/SS5GrbT5cwOivBHNllZSmu/2qz5WPGcjVCOrB\r\n"
"LYl3YWr2h3EGKICT03kEoTkiDBvCeOpW7cCGl2cCgYBD5whoXHz1+ptPlI4YVjZt\r\n"
"Xh86aU+ajpVPiEyJ84I6xXmO4SZXv8q6LaycR0ZMbcL+zBelMb4Z2nBv7jNrtuR7\r\n"
"ZF28cdPv+YVr3esaybZE/73VjXup4SQPH6r3l7qKTVi+y6+FeJ4b2Xn8/MwgnT23\r\n"
"8EFrye7wmzpthrjOgZnUMQKBgE9Lhsz/5J0Nis6Y+2Pqn3CLKEukg9Ewtqdct2y0\r\n"
"5Dcta0F3TyCRIxlCDKTL/BslqMtfAdY4H268UO0+8IAQMn9boqzBrHIgs/pvc5kx\r\n"
"TbKHmw2wtWR6vYersBKVgVpbCGSRssDYHGFu1n74qM4HJ/RGcR1zI9QUe1gopSFD\r\n"
"xDtLAoGAVAdWvrqDwgoL2hHW3scGpxdE/ygJDOwHnf+1B9goKAOP5lf2FJaiAxf3\r\n"
"ectoPOgZbCmm/iiDmigu703ld3O+VoCLDD4qx3R+KyALL78gtVJYzSRiKhzgCZ3g\r\n"
"mKsIVRBq4IfwiwyMNG2BYZQAwbSDjjPtn/kPBduPzPj7eriByhI=\r\n"
"-----END RSA PRIVATE KEY-----\r\n";

/*
 * Computing a "safe" DH-1024 prime can take a very
 * long time, so a precomputed value is provided below.
 * You may run dh_genprime to generate a new value.
 */
char *my_dhm_P = 
    "CCBF40949DC46E26853B6D699E60D52D"\
	"E3F35C86C4F6C1C73520596D5D852ED5"\
	"47461C229CD6070246C9421562C00B15"\
	"3833740FAD8286FFC2ED31E9744D0542"\
	"48FD91A81271AA8845096D30BB6C6A27"\
    "1887E9D066D2F7C843FF16A2AAB3DEC7"\
	"1DDFA57FC3A144ED150CD2BB619B586E"\
	"27D5E2682BB310C58BD17D8E65FF51BF";

char *my_dhm_G = "4";

/*
 * Sorted by order of preference
 */
int my_ciphersuites[] =
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

void my_debug( void *ctx, int level, const char *str )
{
}

void my_debug_printf( const char *fmt, ...  )
{
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

        if( ssl->session->ciphersuite != prv->ciphersuite ||
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

static int io_ready(int usec, int fd)
{
    struct timeval timeout ;
    fd_set fds;
    timeout.tv_sec = 0 ;
    timeout.tv_usec = usec ;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
	return select(fd+1, &fds, NULL, NULL, &timeout) > 0 ;
}

// spawn a child process, and redirect child's stdin, stdout to pipeout, pipein
int my_spawn( int *pipein, int * pipeout, int argc, char * argv[] )
{    
    int pipe1[2], pipe2[2] ;
    int i;
    char * sargv[argc] ;

    pipe( pipe1 ); 	//  -> child
    pipe( pipe2 ); 	//  <- child
    
    if( fork()==0 ) {
        for( i=0; i<argc; i++ ) {
            sargv[i]=argv[i+1] ;
        }
        dup2( pipe1[0], 0 ) ;
        dup2( pipe2[1], 1 ) ;
        close( pipe1[0] );
        close( pipe1[1] );
        close( pipe2[0] );
        close( pipe2[1] );
        execv( sargv[0], sargv );
        // error!
        exit(0);
    }

    *pipeout=pipe1[1] ;
    *pipein=pipe2[0] ;
    close( pipe1[0] );
    close( pipe2[1] );
    return 1 ;
}

int main( int argc, char * argv[] )
{
    int ret;
    int pipein, pipeout ;
    int serverin, serverout ;
    unsigned char * ssl_buffer ;

    havege_state hs;
    ssl_context ssl;
    ssl_session ssn;
    x509_cert srvcert;
    rsa_context rsa;

    serverin=0 ;			// stdin
    serverout=1 ;			// stdout

    memset( &ssl, 0, sizeof( ssl ) );
    memset( &srvcert, 0, sizeof( srvcert ) );
    memset( &rsa, 0, sizeof( rsa ) );
    memset( &ssn, 0, sizeof( ssn ) );

    /*
     * Load the certificates and private RSA key
     */

    /*
     * This demonstration program uses embedded test certificates.
     * Instead, you may want to use x509parse_crtfile() to read the
     * server and CA certificates, as well as x509parse_keyfile().
     */
    ret = x509parse_crt( &srvcert, (unsigned char *) my_srv_crt,
                         strlen( my_srv_crt ) );
    if( ret != 0 )
    {
        my_debug_printf( " failed\n  !  x509parse_crt returned %d\n\n", ret );
        goto exit;
    }

    ret = x509parse_crt( &srvcert, (unsigned char *) my_ca_crt,
                         strlen( my_ca_crt ) );
    if( ret != 0 )
    {
        my_debug_printf( " failed\n  !  x509parse_crt returned %d\n\n", ret );
        goto exit;
    }

    rsa_init( &rsa, RSA_PKCS_V15, 0 );
    ret =  x509parse_key( &rsa, (unsigned char *) my_srv_key,
                          strlen( my_srv_key ), NULL, 0 );
    if( ret != 0 )
    {
        my_debug_printf( " failed\n  !  x509parse_key returned %d\n\n", ret );
        goto exit;
    }

    my_debug_printf( " ok\n" );

    ssl_free( &ssl );

    /*
     * Setup stuff
     */
    my_debug_printf( "  . Setting up the RNG and SSL data...." );

    havege_init( &hs );

    if( ( ret = ssl_init( &ssl ) ) != 0 )
    {
        my_debug_printf( " failed\n  ! ssl_init returned %d\n\n", ret );
        goto exit;
    }

    my_debug_printf( " ok\n" );

    ssl_set_endpoint( &ssl, SSL_IS_SERVER );
    ssl_set_authmode( &ssl, SSL_VERIFY_NONE );

    ssl_set_rng( &ssl, havege_rand, &hs );
    ssl_set_dbg( &ssl, my_debug, NULL );
    ssl_set_bio( &ssl, net_recv, &serverin,
                 net_send, &serverout );
    ssl_set_scb( &ssl, my_get_session,
                 my_set_session );

    ssl_set_ciphersuites( &ssl, my_ciphersuites );
    ssl_set_session( &ssl, 1, 0, &ssn );

    ssl_set_ca_chain( &ssl, srvcert.next, NULL, NULL );
    ssl_set_own_cert( &ssl, &srvcert, &rsa );
    ssl_set_dh_param( &ssl, my_dhm_P, my_dhm_G );

    /*
     * Handshake
     */
    my_debug_printf( "  . Performing the SSL/TLS handshake..." );

    while( ( ret = ssl_handshake( &ssl ) ) != 0 )
    {
        if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE )
        {
            my_debug_printf( " failed\n  ! ssl_handshake returned %d\n\n", ret );
            goto exit;
        }
    }

    my_debug_printf( " ok\n" );

    /*
  * spawn child process (httpd)
  */
    my_spawn( &pipein, &pipeout, argc, argv );

    net_set_nonblock( pipein );
    net_set_block( pipeout );
    net_set_nonblock( serverin );
    net_set_block( serverout );

    ssl_buffer = (unsigned char *)malloc( SSL_BUFFER_LEN ) ;

    do {

        if( io_ready(0, pipein) ) {
            ret = net_recv( &pipein, ssl_buffer, SSL_BUFFER_LEN );
            if( ret>0 ) {
                ssl_write( &ssl, ssl_buffer, ret );
                continue ;
            }
            else if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE ) {
                break ;
            }
        }

        if( ssl_get_bytes_avail( &ssl )>0 || io_ready(1000, serverin) ) {
            ret = ssl_read( &ssl, ssl_buffer, SSL_BUFFER_LEN );
            if( ret>0 ) {
                ssl_buffer[ret]=0 ;
                net_send( &pipeout, ssl_buffer, ret );
            }
            else if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE ) {
                break ;
            }
        }

    } while( 1 );

    free(ssl_buffer);

    ssl_close_notify( &ssl );

    close( pipeout );
    close( pipein );

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

    return( ret );
}

