/*

	   Definitions for RTP packet manipulation routines

*/

struct rtcp_sdes_request_item {
    unsigned char r_item;
    char *r_text;
};

struct rtcp_sdes_request {
    int nitems; 		      /* Number of items requested */
    unsigned char ssrc[4];	      /* Source identifier */
    struct rtcp_sdes_request_item item[10]; /* Request items */
};
