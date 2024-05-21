#ifndef NET_SERVICES_H_
#define NET_SERVICES_H_

#define GET_RTN_OK "Ok"
#define GET_RTN_UNKNOWN "Unknown command"
#define GET_RTN_NOT_CHANGED "Not Changed"
#define GET_RTN_ERROR "Error"


#define CONTENT_JSON "Content-Type: application/json"
#define CONTENT_JS   "Content-Type: text/javascript"
#define CONTENT_TEXT  "Content-Type: text/plain"

//void main_server();
//void main_server_TEST(struct aqualinkdata *aqdata, char *s_http_port);
//bool start_web_server(struct mg_mgr *mgr, struct aqualinkdata *aqdata, char *port, char* web_root);
//bool start_net_services(struct mg_mgr *mgr, struct aqualinkdata *aqdata, struct aqconfig *aqconfig);

/*
#ifdef AQ_NO_THREAD_NETSERVICE
bool start_net_services(struct mg_mgr *mgr, struct aqualinkdata *aqdata);
void stop_net_services(struct mg_mgr *mgr);
time_t poll_net_services(struct mg_mgr *mgr, int timeout_ms);
void broadcast_aqualinkstate(struct mg_connection *nc);
void broadcast_aqualinkstate_error(struct mg_connection *nc, char *msg);
#else*/
bool start_net_services(struct aqualinkdata *aqdata);
void stop_net_services();
time_t poll_net_services(int timeout_ms);
void broadcast_aqualinkstate();
void broadcast_aqualinkstate_error(char *msg);
void broadcast_simulator_message();



// NSF Need to find a better way, this is not thread safe, so don;t like exposting it.
//void send_mqtt(struct mg_connection *nc, const char *toppic, const char *message);

// superseded with systemd/sd-journal
//void broadcast_log(char *msg);
//#endif


#endif // WEB_SERVER_H_