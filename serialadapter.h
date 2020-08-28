#ifndef SERIALADAPTER_H_
#define SERIALADAPTER_H_


bool push_rssa_cmd(unsigned char *cmd);
unsigned char *get_rssa_cmd(unsigned char source_message_type);
void remove_rssa_cmd();
bool process_rssadapter_packet(unsigned char *packet, int length, struct aqualinkdata *aq_data);

//void rssadapter_device_on(unsigned char devID);
//void rssadapter_device_off(unsigned char devID);

void set_aqualink_rssadapter_aux_state(int buttonIndex, bool turnOn);

void get_aqualink_rssadapter_setpoints();
void set_aqualink_rssadapter_pool_setpoint(char *args, struct aqualinkdata *aqdata);
void set_aqualink_rssadapter_spa_setpoint(char *args, struct aqualinkdata *aqdata);

void increase_aqualink_rssadapter_pool_setpoint(char *args, struct aqualinkdata *aqdata);
void increase_aqualink_rssadapter_spa_setpoint(char *args, struct aqualinkdata *aqdata);
/*

CAN ONLY REPLY WITH BELOW TO STATUS MESSAGE, UNLESS FOLLOWON 0x07


Message is reply ACK as below
--------- Send ACK  -----------
0x10|0x02|0x00|0x01|DeviceID|What|0xXX|0x10|0x03|
DeviceID = Below
What = 0x05 Query Value | 0x35 Set Value on next command.
--------- Reply Msg -------------
0x10|0x02|0x48|ReplyType|DeviceID|????|Value|0x00|0xXX|0x10|0x03|
ReplyType 0x13=status, 0x07=set value next message
DeviceID
???? Not sure 0x00 modst of the time, 0x02 for (not set or vbat return)
Value  What it's set to, except VBAT. or 0x07 set on next command
--------- Send reply to above if 0x07 ---------
0x10|0x02|0x00|0x01|0x00|Value|0x4f|0x10|0x03|
Value to be set
--------- Send received from above ---------
0x10|0x02|0x48|ReplyType|DeviceID|????|Value|0x00|0xXX|0x10|0x03|
Same as previous reply.

*/

#define RS_SA_DEVSTATUS 0x13

// Setpoints changes are in this group./
#define RS_SA_MODEL     0x00
#define RS_SA_OPTIONS   0x01
#define RS_SA_POOLSP    0x05  
#define RS_SA_POOLSP2   0x06  
#define RS_SA_SPASP     0x07  
#define RS_SA_POOLTMP   0x08
#define RS_SA_AIRTMP    0x09
#define RS_SA_UNITS     0x0a 
#define RS_SA_SPATMP    0x0b
#define RS_SA_SOLTMP    0x0c   
#define RS_SA_OPMODE    0x0d 
#define RS_SA_VBAT      0x0e  
#define RS_SA_WFALL     0x0f


/*
// Set & Query options
Query or Change simple on/off
--------- Send ACK --------------
0x10|0x02|0x00|0x01|What|DeviceID|0xXX|0x10|0x03|
What     - Byte 4 = What   (0x00 Query | 0x81 On | 0x80 off )
DeviceID - Byte 5 = (ID's below)

--------- Reply Msg -------------
In return
0x10|0x02|0x48|0x13   |0x02   |0x00   |0x0d|0x10|0x8c|0x10|0x03| 
0x10|0x02|0x48|MsgType|Status1|Status2|0x0e|DeviceID|0xXX|0x10|0x03|
MsgType  - Byte 3 = 0x13 (some state message)??
StatType - Byte 4 = 0x02 or 0x03 (not sure meaning)  Status Type ????
Status1  - Byte 5 = 0x00 0x01  (???)  if Byte4 is 0x02 then this is state 0x00=off 0x01=on /
Status2  - Byte 6 = 0x00 0x01   0x0e(option switch set can't change???)   if byte4 is 0x03, this this looks to be state
DeviceID - Byte 7 = Should match request.
*/
// These are duplicate for some above, so be careful
#define RS_SA_PUMP      0x0c  
#define RS_SA_PUMPLO    0x0d  
#define RS_SA_SPA       0x0e 
// Unique again
#define RS_SA_CLEANR    0x10    
#define RS_SA_POOLHT    0x11  
#define RS_SA_POOLHT2   0x12  
#define RS_SA_SOLHT     0x14  
#define RS_SA_SPAHT     0x13 
#define RS_SA_AUX1      0x15    
#define RS_SA_AUX2      0x16   
#define RS_SA_AUX3      0x17    
#define RS_SA_AUX4      0x18    
#define RS_SA_AUX5      0x19 
#define RS_SA_AUX6      0x1a
#define RS_SA_AUX7      0x1b
#define RS_SA_AUX8      0x1c
#define RS_SA_AUX9      0x1d
#define RS_SA_AUX10     0x1e
#define RS_SA_AUX11     0x1f
#define RS_SA_AUX12     0x20
#define RS_SA_AUX13     0x21
#define RS_SA_AUX14     0x22
#define RS_SA_AUX15     0x23

#endif // SERIALADAPTER_H_