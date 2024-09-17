

void send_iaqualink_ack(int rs_fd, unsigned char *packet_buffer);

bool process_iaqualink_packet(unsigned char *packet, int length, struct aqualinkdata *aq_data);



// Send the below commands to turn on/off (toggle)
// This is the button in pButton. (byte 6 in below)
// iAq pButton | HEX: 0x10|0x02|0x00|0x24|0x73|0x01|0x21|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0x00|0xcb|0x10|0x03|

#define IAQ_PUMP     0x00
#define IAQ_POOL_HTR 0x01
#define IAQ_SPA      0x02
#define IAQ_SPA_HTR  0x03
//....... some missing ....
#define IAQ_AUX1     0x21
#define IAQ_AUX2     0x22
#define IAQ_AUX3     0x23
#define IAQ_AUX4     0x24
#define IAQ_AUX5     0x25
#define IAQ_AUX6     0x26
#define IAQ_AUX7     0x27
#define IAQ_AUXB1    0x28
#define IAQ_AUXB2    0x29
#define IAQ_AUXB3    0x2a
#define IAQ_AUXB4    0x2b
#define IAQ_AUXB5    0x2c
#define IAQ_AUXB6    0x2d
#define IAQ_AUXB7    0x2e
#define IAQ_AUXB8    0x2f
//... Looks like there are C & D buttons
/* I got this when sending  dec=53 hex=0x35 as the button, all of a sudden got extra buttons in the aux status message send to AqualinkTouch protocol
   Not sure on ordering BUT  dec=57 hex=0x39 = button D2 / dec=58 hex=0x3a = D3
Notice:  iAQ Touch: Label Aux C1 = On
Notice:  iAQ Touch: Label Aux C2 = Off
Notice:  iAQ Touch: Label Aux C3 = Off
Notice:  iAQ Touch: Label Aux C4 = Off
Notice:  iAQ Touch: Label Aux C5 = Off
Notice:  iAQ Touch: Label Aux C6 = On
Notice:  iAQ Touch: Label Aux C7 = On
Notice:  iAQ Touch: Label Aux C8 = On
Notice:  iAQ Touch: Label Aux D1 = On
Notice:  iAQ Touch: Label Aux D2 = On
Notice:  iAQ Touch: Label Aux D3 = On
Notice:  iAQ Touch: Label Aux D4 = On
Notice:  iAQ Touch: Label Aux D5 = On
Notice:  iAQ Touch: Label Aux D6 = On
Notice:  iAQ Touch: Label Aux D7 = On
Notice:  iAQ Touch: Label Aux D8 = On
*/
//... Need to add Vitrual buttons...




/*

Read  Jandy   packet To 0xa3 of type   Unknown '0x53' | HEX: 0x10|0x02|0xa3|0x53|0x08|0x10|0x03|
Read  Jandy   packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x3f|0x00|0x52|0x10|0x03|


Below get's sent to AqualinkTouch with iAqualink is enabled.
End of message is board cpu and panel type.
Read  Jandy   packet To 0x33 of type   Unknown '0x70' | HEX: 0x10|0x02|0x33|0x70|0x0d|0x00|0x01|0x02|0x03|0x05|0x06|0x07|0x0e|0x0f|0x1a|0x1d|0x20|0x21|0x00|0x00|0x00|0x00|0x00|0x48|0x00|0x66|0x00|0x50|0x00|0x00|0x00|0xff|0x42|0x30|0x33|0x31|0x36|0x38|0x32|0x33|0x20|0x52|0x53|0x2d|0x34|0x20|0x43|0x6f|0x6d|0x62|0x6f|0x00|0x00|0x4b|0x10|0x03|

*/