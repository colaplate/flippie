#ifndef UI_HANDLER_H
#define UI_HANDLER_H
#include "../flippie.h"
#include <ESP8266WebServer.h>

class UIHandler : public RequestHandler {
  public:
    UIHandler(Flippie* f);
    bool canHandle(HTTPMethod method, String uri) override;
    bool handle(ESP8266WebServer& server, HTTPMethod method, String uri) override;
  protected:
    static const char index_page[];
    static const char low_level_page[];
    static const char paint_page[];
    static const char tasks_page[];
    String config_page;
    Flippie* flippie;
};

#endif
