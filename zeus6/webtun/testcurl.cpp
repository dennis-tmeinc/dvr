#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <curl/multi.h>
 
static const char *urls[] = {
  "http://www.microsoft.com",
  "http://1.3.5.7",
  "http://www.google.com",
  "http://www.yahoo.com",
  "http://www.ibm.com",
  "http://www.mysql.com",
  "http://www.oracle.com",
  "http://www.ripe.net",
  "http://www.iana.org",
  "http://www.amazon.com",
  "http://www.netcraft.com",
  "http://www.heise.de",
  "http://www.chip.de",
    "http://www.google.com",
  "http://www.yahoo.com",
  "http://www.ibm.com",
  "http://www.mysql.com",
  "http://www.oracle.com",
  "http://www.ripe.net",
  "http://www.iana.org",
  "http://www.amazon.com",
  "http://www.netcraft.com",
  "http://www.heise.de",
  "http://www.chip.de",
  "http://www.ca.com",
  "http://www.cnet.com",
  "http://www.news.com",
  "http://www.cnn.com",
  "http://www.wikipedia.org",
  "http://www.dell.com",
  "http://www.hp.com",
  "http://www.cert.org",
  "http://www.mit.edu",
  "http://www.nist.gov",
  "http://www.ebay.com",
  "http://www.playstation.com",
  "http://www.uefa.com",
  "http://www.ieee.org",
  "http://www.apple.com",
  "http://www.sony.com",
  "http://www.symantec.com",
  "http://www.zdnet.com",
  "http://www.fujitsu.com",
  "http://www.supermicro.com",
  "http://www.hotmail.com",
  "http://www.ecma.com",
  "http://www.bbc.co.uk",
  "http://news.google.com",
  "http://www.foxnews.com",
  "http://www.msn.com",
  "http://www.wired.com",
  "http://www.sky.com",
  "http://www.usatoday.com",
  "http://www.cbs.com",
  "http://www.nbc.com",
  "http://slashdot.org",
  "http://www.bloglines.com",
  "http://www.techweb.com",
  "http://www.newslink.org",
  "http://www.un.org",
};
 
#define MAX 10  
#define CNT sizeof(urls)/sizeof(char*)  
 
static size_t cb(char *d, size_t n, size_t l, void *p)
{
  /* take care of the data here, ignored in this example */ 
  (void)d;
  (void)p;
  return n*l;
}
 
static void init(CURLM *cm, int i)
{
  CURL *eh = curl_easy_init();

  curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, cb);
  curl_easy_setopt(eh, CURLOPT_URL, urls[i]);
  curl_easy_setopt(eh, CURLOPT_PRIVATE, (char *)i);
  curl_easy_setopt(eh, CURLOPT_TIMEOUT, 30L);
 
  curl_multi_add_handle(cm, eh);
}
 
static void reinit(CURLM *cm, CURL *eh, int i)
{
	printf("Reinit %s\r\n", urls[i]);

 curl_multi_remove_handle(cm, eh );
	 
//	 curl_easy_reset( eh );
	 
	 curl_easy_cleanup(eh );
	 eh = curl_easy_init();
	 
  curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, cb);
  curl_easy_setopt(eh, CURLOPT_URL, urls[i]);
  curl_easy_setopt(eh, CURLOPT_PRIVATE, (char *)i);
  curl_easy_setopt(eh, CURLOPT_TIMEOUT, 30L);
  
 curl_multi_add_handle(cm, eh);

}
 
int main(void)
{
  CURLM *cm;
  CURLMsg *msg;
  long L;
  unsigned int C=0;
  int M, Q, U = -1;
  fd_set R, W, E;
  struct timeval T;
 
  curl_global_init(CURL_GLOBAL_ALL);
 
  cm = curl_multi_init();
 
  /* we can optionally limit the total amount of connections this multi handle
     uses */ 
  // curl_multi_setopt(cm, CURLMOPT_MAXCONNECTS, (long)5);

 
  for (C = 0; C < MAX; ++C) {
    init(cm, C);
  }
 
 
  while (U) {
    curl_multi_perform(cm, &U);

    while ((msg = curl_multi_info_read(cm, &Q))) {
      if (msg->msg == CURLMSG_DONE) {
		int priv ;
        const char *url;
        CURL *e = msg->easy_handle;
        curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &url);
        priv = (int)url ;
        url = urls[priv] ;
        long ec ;
        curl_easy_getinfo(msg->easy_handle, CURLINFO_NUM_CONNECTS , &ec);

        fprintf(stderr, "U: %d Q:%d ec: %d R: %d - %s <%s>\n", U, Q, (int)ec,
                msg->data.result, curl_easy_strerror(msg->data.result), url);
        
		  if ( 1 ) {
			 reinit( cm, e, priv );
			 U++;
		  }
		  else {
        fprintf(stderr, "new connections\r\n" );
                
				curl_multi_remove_handle(cm, e);
				curl_easy_cleanup(e);
		  }
        
      }
      else {
        fprintf(stderr, "E: CURLMsg (%d)\n", msg->msg);
      }

    }

 
    if (U) {
      FD_ZERO(&R);
      FD_ZERO(&W);
      FD_ZERO(&E);
 
      if (curl_multi_fdset(cm, &R, &W, &E, &M)) {
        fprintf(stderr, "E: curl_multi_fdset\n");
        return EXIT_FAILURE;
      }
 
      if (curl_multi_timeout(cm, &L)) {
        fprintf(stderr, "E: curl_multi_timeout\n");
        return EXIT_FAILURE;
      }
      if (L == -1)
        L = 100;
 
      if (M == -1) {
#ifdef WIN32
        Sleep(L);
#else
        sleep(L / 1000);
#endif
      } else {
        T.tv_sec = L/1000;
        T.tv_usec = (L%1000)*1000;
 
        if (0 > select(M+1, &R, &W, &E, &T)) {
          fprintf(stderr, "E: select(%i,,,,%li): %i: %s\n",
              M+1, L, errno, strerror(errno));
          return EXIT_FAILURE;
        }
      }
    }
 

  }
 
  curl_multi_cleanup(cm);
  curl_global_cleanup();
 
  return EXIT_SUCCESS;
}
